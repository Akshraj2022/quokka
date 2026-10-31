"""
ATSRS — Adaptive Tiered Storage for Resource-Starved training
Enables QLoRA on models larger than VRAM + RAM combined.

Memory Hierarchy:
  - T0 VRAM: Active layer (dequantized fp16), trainable LoRA adapters,
             optimizer states (AdamW), current activations.
  - T1 Pinned RAM: Next layers staged as 4-bit quantized blocks in pinned host memory.
  - T2 NVMe: Cold layers stored as read-only mmap / direct I/O blocks on SSD.

Strict Invariants:
  1. Base weights are strictly read-only.
  2. Fill order: T0 -> T1 -> T2.
  3. Never write to T2 during training.
  4. Prefetch depth: 2 (prefetch next 2 layers ahead).
  5. Dequantize directly on GPU.
  6. Overlap I/O with compute via asynchronous streams and background staging.
  7. Fallback: If model fits in T0 + T1, automatically disable T2.
  8. Metrics: io_wait_pct, tok/s, step_time emitted per training step.
"""

import os
import sys
import math
import time
import json
import socket
import threading
from typing import List, Dict, Optional, Tuple, Union

try:
    import torch
    import torch.nn as nn
    import torch.optim as optim
    HAS_TORCH = True
except ImportError:
    HAS_TORCH = False


class ATSRSConfig:
    """Configuration for Adaptive Tiered Storage for Resource-Starved training."""

    def __init__(
        self,
        ram_budget_gb: float = 4.0,
        nvme_path: str = "./.atsrs_nvme",
        min_tokens_per_step: int = 8192,
        batch_size: int = 2,
        seq_len: int = 512,
        grad_accum: Union[int, str] = "auto",
        prefetch_depth: int = 2,
        quant_type: str = "nf4",
        device: Optional[str] = None,
        lora_r: int = 16,
        lora_alpha: int = 32,
        lora_dropout: float = 0.05,
        learning_rate: float = 2e-4,
    ):
        self.ram_budget_gb = float(ram_budget_gb)
        self.nvme_path = os.path.abspath(nvme_path)
        self.min_tokens_per_step = int(min_tokens_per_step)
        self.batch_size = int(batch_size)
        self.seq_len = int(seq_len)
        self.prefetch_depth = int(prefetch_depth)
        self.quant_type = quant_type
        self.lora_r = int(lora_r)
        self.lora_alpha = int(lora_alpha)
        self.lora_dropout = float(lora_dropout)
        self.learning_rate = float(learning_rate)

        if device:
            self.device = device
        elif HAS_TORCH and torch.cuda.is_available():
            self.device = "cuda"
        else:
            self.device = "cpu"

        # Resolve auto gradient accumulation
        if grad_accum == "auto" or grad_accum is None:
            tokens_per_micro_batch = self.batch_size * self.seq_len
            if tokens_per_micro_batch > 0:
                self.grad_accum = max(1, math.ceil(self.min_tokens_per_step / tokens_per_micro_batch))
            else:
                self.grad_accum = 1
        else:
            self.grad_accum = int(grad_accum)

    @property
    def effective_tokens_per_step(self) -> int:
        return self.batch_size * self.seq_len * self.grad_accum

    @property
    def ram_budget_bytes(self) -> int:
        return int(self.ram_budget_gb * (1024 ** 3))

    def to_dict(self) -> dict:
        return {
            "ram_budget_gb": self.ram_budget_gb,
            "nvme_path": self.nvme_path,
            "min_tokens_per_step": self.min_tokens_per_step,
            "batch_size": self.batch_size,
            "seq_len": self.seq_len,
            "grad_accum": self.grad_accum,
            "effective_tokens_per_step": self.effective_tokens_per_step,
            "prefetch_depth": self.prefetch_depth,
            "quant_type": self.quant_type,
            "device": self.device,
            "lora_r": self.lora_r,
            "lora_alpha": self.lora_alpha,
            "lora_dropout": self.lora_dropout,
            "learning_rate": self.learning_rate,
        }


class QuantizedBlock:
    """Encapsulates 4-bit packed weights and scale tensors for a base layer."""

    def __init__(self, packed_bytes: bytes, scale_bytes: bytes, shape: Tuple[int, ...]):
        self.packed_bytes = packed_bytes
        self.scale_bytes = scale_bytes
        self.shape = shape
        self.size_bytes = len(packed_bytes) + len(scale_bytes)

    @staticmethod
    def from_tensor(weight_tensor: "torch.Tensor") -> "QuantizedBlock":
        """Quantizes an fp32/fp16 weight tensor to 4-bit packed representation."""
        if not HAS_TORCH:
            raise RuntimeError("PyTorch is required for QuantizedBlock")

        flat = weight_tensor.detach().float().flatten()
        numel = flat.numel()
        if numel % 2 != 0:
            flat = torch.nn.functional.pad(flat, (0, 1))

        # Block-wise or per-tensor symmetric scaling
        abs_max = flat.abs().max().clamp(min=1e-8)
        scale = abs_max / 7.0  # 4-bit signed representation [-7, 7] mapped to [0, 15]

        # Quantize to integer indices [0..15]
        quantized = torch.clamp(torch.round(flat / scale) + 7.0, 0.0, 15.0).to(torch.uint8)

        # Pack 2 4-bit integers per uint8 byte
        low = quantized[0::2]
        high = quantized[1::2]
        packed = low | (high << 4)

        packed_bytes = packed.cpu().numpy().tobytes()
        scale_bytes = scale.to(torch.float16).cpu().numpy().tobytes()

        return QuantizedBlock(packed_bytes, scale_bytes, tuple(weight_tensor.shape))

    def dequantize_on_gpu(self, device: str = "cuda") -> "torch.Tensor":
        """Transfers packed 4-bit bytes to GPU and unpacks/dequantizes directly on GPU."""
        if not HAS_TORCH:
            raise RuntimeError("PyTorch is required for dequantize_on_gpu")

        # Load raw bytes onto GPU as uint8 tensor (use bytearray to avoid non-writable buffer warning)
        packed_tensor = torch.frombuffer(bytearray(self.packed_bytes), dtype=torch.uint8).to(device, non_blocking=True)
        scale_tensor = torch.frombuffer(bytearray(self.scale_bytes), dtype=torch.float16).to(device, non_blocking=True).float()

        # Unpack on GPU
        low = (packed_tensor & 0x0F).float() - 7.0
        high = ((packed_tensor >> 4) & 0x0F).float() - 7.0

        unpacked = torch.empty(packed_tensor.numel() * 2, dtype=torch.float16, device=device)
        unpacked[0::2] = (low * scale_tensor).half()
        unpacked[1::2] = (high * scale_tensor).half()

        total_needed = 1
        for d in self.shape:
            total_needed *= d

        return unpacked[:total_needed].view(self.shape)


class TieredStorageManager:
    """
    Coordinates T0 (VRAM), T1 (Pinned RAM), and T2 (NVMe).
    Strictly enforces:
      - Read-only base weights.
      - Fill order: T0 -> T1 -> T2.
      - Never write to T2 during training.
      - Fallback: If model fits in T0 + T1, disable T2.
    """

    def __init__(self, config: ATSRSConfig, layer_blocks: List[QuantizedBlock]):
        self.config = config
        self.layer_blocks = layer_blocks
        self.num_layers = len(layer_blocks)
        self.total_base_bytes = sum(b.size_bytes for b in layer_blocks)

        # Memory allocations
        self.t1_pinned_cache: Dict[int, QuantizedBlock] = {}
        self.t1_allocated_bytes = 0
        self.t2_file_paths: Dict[int, str] = {}
        self.t2_enabled = False

        self._check_fallback_and_initialize()

    def _check_fallback_and_initialize(self):
        """Initializes storage tiers and applies fallback check."""
        os.makedirs(self.config.nvme_path, exist_ok=True)

        # Estimate T0 capacity for active layers in VRAM
        avg_layer_bytes = self.total_base_bytes / max(1, self.num_layers)
        active_layers_in_t0 = 1 + self.config.prefetch_depth
        t0_active_capacity = int(avg_layer_bytes * active_layers_in_t0)

        # Layers that cannot reside concurrently in T0 must fit in T1 RAM
        ram_needed = max(0, self.total_base_bytes - t0_active_capacity)

        print(f"[ATSRS] Total Base Weights: {self.total_base_bytes / (1024**2):.2f} MB across {self.num_layers} layers")
        print(f"[ATSRS] T1 RAM Budget: {self.config.ram_budget_gb:.4f} GB ({self.config.ram_budget_bytes / (1024**2):.2f} MB)")

        # Fallback Check: If model fits in T0 + T1, disable T2
        if ram_needed <= self.config.ram_budget_bytes:
            self.t2_enabled = False
            print("[ATSRS Fallback] Model fits within T0 + T1 budget! Disabling T2 NVMe spillover.")
            # Fill T1 entirely
            for i, block in enumerate(self.layer_blocks):
                self.t1_pinned_cache[i] = block
                self.t1_allocated_bytes += block.size_bytes
        else:
            self.t2_enabled = True
            print(f"[ATSRS] Model exceeds T0 + T1 budget (needs {ram_needed / (1024**2):.2f} MB in RAM, budget is {self.config.ram_budget_bytes / (1024**2):.2f} MB). T2 NVMe enabled at: {self.config.nvme_path}")
            # Serialize cold layers to T2 NVMe (read-only base weights)
            for i, block in enumerate(self.layer_blocks):
                layer_path = os.path.join(self.config.nvme_path, f"layer_{i:04d}.bin")
                self.t2_file_paths[i] = layer_path
                # Write initial read-only weight block to disk
                with open(layer_path, "wb") as f:
                    meta = json.dumps({"shape": block.shape, "packed_len": len(block.packed_bytes)}).encode("utf-8")
                    f.write(len(meta).to_bytes(4, byteorder="little"))
                    f.write(meta)
                    f.write(block.packed_bytes)
                    f.write(block.scale_bytes)

            # Stage initial layers into T1 up to ram_budget
            curr_bytes = 0
            for i, block in enumerate(self.layer_blocks):
                if curr_bytes + block.size_bytes <= self.config.ram_budget_bytes:
                    self.t1_pinned_cache[i] = block
                    curr_bytes += block.size_bytes
            self.t1_allocated_bytes = curr_bytes
            print(f"[ATSRS] Staged {len(self.t1_pinned_cache)}/{self.num_layers} layers into T1 ({self.t1_allocated_bytes / (1024**2):.2f} MB)")

    def get_layer_block(self, layer_idx: int) -> QuantizedBlock:
        """Retrieves a layer block, fetching from T1 or loading from T2 (never writing T2)."""
        if layer_idx in self.t1_pinned_cache:
            return self.t1_pinned_cache[layer_idx]

        if not self.t2_enabled:
            return self.layer_blocks[layer_idx]

        # Read-only fetch from T2 NVMe
        path = self.t2_file_paths[layer_idx]
        with open(path, "rb") as f:
            meta_len = int.from_bytes(f.read(4), byteorder="little")
            meta = json.loads(f.read(meta_len).decode("utf-8"))
            packed_len = meta["packed_len"]
            shape = tuple(meta["shape"])
            packed_bytes = f.read(packed_len)
            scale_bytes = f.read()

        block = QuantizedBlock(packed_bytes, scale_bytes, shape)

        # Cache in T1 if space permits, or evict FIFO
        if self.t1_allocated_bytes + block.size_bytes > self.config.ram_budget_bytes:
            # Evict a non-active layer from T1
            keys = list(self.t1_pinned_cache.keys())
            if keys:
                evicted_idx = keys[0]
                evicted_block = self.t1_pinned_cache.pop(evicted_idx)
                self.t1_allocated_bytes -= evicted_block.size_bytes

        self.t1_pinned_cache[layer_idx] = block
        self.t1_allocated_bytes += block.size_bytes
        return block

    def cleanup(self):
        """Clean up temporary NVMe files."""
        self.t1_pinned_cache.clear()
        if self.t2_enabled and os.path.exists(self.config.nvme_path):
            try:
                for f in os.listdir(self.config.nvme_path):
                    if f.startswith("layer_") and f.endswith(".bin"):
                        os.remove(os.path.join(self.config.nvme_path, f))
            except Exception:
                pass


class PrefetchPipeline:
    """
    Asynchronous prefetch pipeline with prefetch depth 2.
    Overlaps I/O with computation using background thread & GPU streams.
    Tracks io_wait_pct, tok/s, step_time.
    """

    def __init__(self, storage_manager: TieredStorageManager, config: ATSRSConfig):
        self.storage = storage_manager
        self.config = config
        self.prefetch_depth = config.prefetch_depth
        self.device = config.device

        # GPU Stream for async transfer and dequantization
        if HAS_TORCH and self.device.startswith("cuda") and torch.cuda.is_available():
            self.cuda_stream = torch.cuda.Stream()
        else:
            self.cuda_stream = None

        # Prefetched T0 tensor queue: layer_idx -> torch.Tensor
        self.t0_cache: Dict[int, "torch.Tensor"] = {}
        self.prefetch_lock = threading.Lock()
        self.prefetch_thread: Optional[threading.Thread] = None

        # Metrics tracking
        self.total_io_wait_time = 0.0
        self.total_compute_time = 0.0

    def start_prefetch(self, target_layers: List[int]):
        """Launches asynchronous prefetch of the given layers into T0."""
        def _worker():
            for idx in target_layers:
                if idx < 0 or idx >= self.storage.num_layers:
                    continue
                with self.prefetch_lock:
                    if idx in self.t0_cache:
                        continue

                # Stage to T1 / load block
                block = self.storage.get_layer_block(idx)

                # Transfer to T0 and dequantize on GPU
                if self.cuda_stream:
                    with torch.cuda.stream(self.cuda_stream):
                        tensor_t0 = block.dequantize_on_gpu(self.device)
                        self.cuda_stream.synchronize()
                elif HAS_TORCH:
                    tensor_t0 = block.dequantize_on_gpu(self.device)
                else:
                    tensor_t0 = None

                with self.prefetch_lock:
                    self.t0_cache[idx] = tensor_t0

        self.prefetch_thread = threading.Thread(target=_worker, daemon=True)
        self.prefetch_thread.start()

    def acquire_layer(self, layer_idx: int) -> "torch.Tensor":
        """
        Retrieves the dequantized T0 layer weight.
        Measures I/O wait time if computation has to wait for transfer.
        """
        t_start = time.perf_counter()
        while True:
            with self.prefetch_lock:
                if layer_idx in self.t0_cache:
                    tensor = self.t0_cache[layer_idx]
                    break
            # If thread not running or finished without prefetching this layer, fetch synchronously
            if self.prefetch_thread is None or not self.prefetch_thread.is_alive():
                block = self.storage.get_layer_block(layer_idx)
                tensor = block.dequantize_on_gpu(self.device)
                with self.prefetch_lock:
                    self.t0_cache[layer_idx] = tensor
                break
            time.sleep(0.0005)

        wait_duration = time.perf_counter() - t_start
        self.total_io_wait_time += wait_duration
        return tensor

    def release_layer(self, layer_idx: int):
        """Evicts layer from T0 VRAM to free active memory for the next layer."""
        with self.prefetch_lock:
            if layer_idx in self.t0_cache:
                del self.t0_cache[layer_idx]
        if HAS_TORCH and self.device.startswith("cuda") and torch.cuda.is_available():
            torch.cuda.empty_cache()


class LoRAAdapter(nn.Module if HAS_TORCH else object):
    """Trainable LoRA adapter residing permanently in T0 VRAM."""

    def __init__(self, in_features: int, out_features: int, r: int = 16, alpha: int = 32, dropout: float = 0.05):
        if HAS_TORCH:
            super().__init__()
            self.r = r
            self.scaling = alpha / r
            self.lora_A = nn.Parameter(torch.empty(r, in_features))
            self.lora_B = nn.Parameter(torch.zeros(out_features, r))
            self.dropout = nn.Dropout(p=dropout) if dropout > 0.0 else nn.Identity()
            # Kaiming uniform init for A, zeros for B
            nn.init.kaiming_uniform_(self.lora_A, a=math.sqrt(5))
        else:
            self.r = r
            self.scaling = alpha / r

    def forward(self, x: "torch.Tensor") -> "torch.Tensor":
        if not HAS_TORCH:
            return x
        # Match tensor dtype for matrix multiplication
        dropped = self.dropout(x).to(self.lora_A.dtype)
        adapter_out = (dropped @ self.lora_A.t() @ self.lora_B.t()) * self.scaling
        return adapter_out.to(x.dtype)


class ATSRSLayer(nn.Module if HAS_TORCH else object):
    """
    A single model layer managed by ATSRS.
    Base weights are frozen & managed across tiers (T0/T1/T2).
    LoRA adapters are trainable and live in T0 VRAM.
    """

    def __init__(self, layer_idx: int, in_features: int, out_features: int, config: ATSRSConfig):
        if HAS_TORCH:
            super().__init__()
        self.layer_idx = layer_idx
        self.in_features = in_features
        self.out_features = out_features
        self.config = config
        self.adapter = LoRAAdapter(
            in_features=in_features,
            out_features=out_features,
            r=config.lora_r,
            alpha=config.lora_alpha,
            dropout=config.lora_dropout,
        )
        if HAS_TORCH:
            self.adapter.to(config.device)

    def forward(self, x: "torch.Tensor", base_weight_t0: "torch.Tensor") -> "torch.Tensor":
        """Compute layer forward: Base(x) + LoRA(x)."""
        if not HAS_TORCH:
            return x
        # Cast input to match base weight dtype
        x_cast = x.to(base_weight_t0.dtype)
        base_out = torch.nn.functional.linear(x_cast, base_weight_t0)
        adapter_out = self.adapter(x)
        return (base_out + adapter_out.to(base_out.dtype)).to(x.dtype)


class ATSRSModelWrapper(nn.Module if HAS_TORCH else object):
    """
    Full sequence of layers wrapped under ATSRS pipelined execution.
    Handles forward and backward layer execution with depth-2 prefetching.
    """

    def __init__(self, layers: List[ATSRSLayer], storage: TieredStorageManager, config: ATSRSConfig):
        if HAS_TORCH:
            super().__init__()
            self.layer_list = nn.ModuleList(layers)
        else:
            self.layer_list = layers
        self.storage = storage
        self.config = config
        self.pipeline = PrefetchPipeline(storage, config)

    def forward(self, x: "torch.Tensor") -> "torch.Tensor":
        """
        Executes forward pass with prefetch depth 2.
        For layer i:
          - Prefetches i+1, i+2
          - Computes layer i
          - Releases layer i base weights from T0
        """
        if not HAS_TORCH:
            return x

        h = x
        num_layers = len(self.layer_list)

        for i, layer in enumerate(self.layer_list):
            # Prefetch ahead (depth 2)
            prefetch_targets = [i + 1, i + 2]
            self.pipeline.start_prefetch([tgt for tgt in prefetch_targets if tgt < num_layers])

            # Acquire active layer base weight in T0
            t_comp_start = time.perf_counter()
            base_w = self.pipeline.acquire_layer(i)

            # Compute forward
            h = layer(h, base_w)
            h = torch.relu(h)  # Non-linearity

            self.pipeline.total_compute_time += time.perf_counter() - t_comp_start

            # Release active layer base weights from T0
            self.pipeline.release_layer(i)

        return h


class ATSRSTrainer:
    """
    End-to-end ATSRS QLoRA Trainer.
    Automates training loop, gradient accumulation, I/O metrics, and progress reporting.
    """

    def __init__(
        self,
        model_wrapper: ATSRSModelWrapper,
        config: ATSRSConfig,
        progress_sock: Optional[socket.socket] = None,
    ):
        self.model = model_wrapper
        self.config = config
        self.progress_sock = progress_sock

        if HAS_TORCH:
            # Optimizer only optimizes trainable LoRA parameters in T0
            trainable_params = [p for p in self.model.parameters() if p.requires_grad]
            self.optimizer = optim.AdamW(trainable_params, lr=config.learning_rate)
        else:
            self.optimizer = None

    def train_step(self, micro_batches: List["torch.Tensor"]) -> Dict[str, float]:
        """
        Executes one full global optimizer step across grad_accum micro-batches.
        Calculates and returns: io_wait_pct, tok/s, step_time, loss.
        """
        if not HAS_TORCH:
            return {"loss": 0.0, "io_wait_pct": 0.0, "tok/s": 0.0, "step_time": 0.01}

        self.optimizer.zero_grad()
        self.pipeline_reset_metrics()

        step_start = time.perf_counter()
        total_loss = 0.0

        for micro_x in micro_batches:
            micro_x = micro_x.to(self.config.device).half()
            out = self.model(micro_x)

            # Self-supervised next-token loss or dummy reconstruction loss
            loss = (out ** 2).mean() / self.config.grad_accum
            loss.backward()
            total_loss += loss.item()

        self.optimizer.step()
        step_duration = max(time.perf_counter() - step_start, 1e-6)

        total_tokens = self.config.effective_tokens_per_step
        tokens_per_sec = total_tokens / step_duration

        total_io = self.model.pipeline.total_io_wait_time
        total_comp = self.model.pipeline.total_compute_time
        denom = max(total_io + total_comp, 1e-6)
        io_wait_pct = (total_io / denom) * 100.0

        metrics = {
            "loss": float(total_loss),
            "io_wait_pct": float(io_wait_pct),
            "tok/s": float(tokens_per_sec),
            "step_time": float(step_duration),
        }

        # Emit to Joey monitor over TCP 9001 if connected
        self._emit_progress(metrics)
        return metrics

    def pipeline_reset_metrics(self):
        self.model.pipeline.total_io_wait_time = 0.0
        self.model.pipeline.total_compute_time = 0.0

    def _emit_progress(self, metrics: Dict[str, float]):
        if not self.progress_sock:
            return
        event = {
            "loss": metrics["loss"],
            "tokens_per_sec": metrics["tok/s"],
            "io_wait_pct": metrics["io_wait_pct"],
            "step_time": metrics["step_time"],
            "vram_mb": self._get_vram_mb(),
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        }
        try:
            self.progress_sock.sendall((json.dumps(event) + "\n").encode("utf-8"))
        except (socket.error, OSError):
            pass

    def _get_vram_mb(self) -> int:
        if HAS_TORCH and torch.cuda.is_available():
            try:
                return int(torch.cuda.memory_allocated() / (1024 ** 2))
            except Exception:
                return 0
        return 0


def build_synthetic_atsrs_model(
    num_layers: int = 8,
    hidden_dim: int = 1024,
    config: Optional[ATSRSConfig] = None,
) -> Tuple[ATSRSModelWrapper, TieredStorageManager, ATSRSConfig]:
    """Builds a verified synthetic ATSRS model for testing and benchmarking."""
    if config is None:
        config = ATSRSConfig()

    blocks: List[QuantizedBlock] = []
    layers: List[ATSRSLayer] = []

    for i in range(num_layers):
        if HAS_TORCH:
            # Create base weight tensor
            raw_w = torch.randn(hidden_dim, hidden_dim, dtype=torch.float32)
            block = QuantizedBlock.from_tensor(raw_w)
        else:
            block = QuantizedBlock(b"\x00" * (hidden_dim * hidden_dim // 2), b"\x00" * 2, (hidden_dim, hidden_dim))

        blocks.append(block)
        layers.append(ATSRSLayer(i, hidden_dim, hidden_dim, config))

    storage = TieredStorageManager(config, blocks)
    model = ATSRSModelWrapper(layers, storage, config)
    return model, storage, config
