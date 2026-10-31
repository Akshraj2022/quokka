# ATSRS — Adaptive Tiered Storage for Resource-Starved Training

## Overview
**ATSRS** is Joey's memory virtualization engine designed to enable parameter-efficient fine-tuning (QLoRA) on large language models that exceed combined system VRAM and host RAM capacities.

---

## Three-Tier Storage Architecture

```text
┌────────────────────────────────────────────────────────┐
│  Tier 0: VRAM (GPU Device Memory)                     │
│  - Active layer weights (dequantized to FP16)          │
│  - Trainable LoRA adapter matrices (A and B)           │
│  - Optimizer states (AdamW FP32 moments)              │
│  - Forward activations and gradients                  │
└──────────────────────────────────▲─────────────────────┘
                                   │ On-GPU Dequantization
┌──────────────────────────────────▼─────────────────────┐
│  Tier 1: Pinned Host RAM (CPU Memory)                 │
│  - Next layers staged as 4-bit quantized blocks        │
│  - Direct DMA transfers up to `ram_budget_gb`          │
└──────────────────────────────────▲─────────────────────┘
                                   │ Async Prefetch (Depth 2)
┌──────────────────────────────────▼─────────────────────┐
│  Tier 2: NVMe SSD (Local Storage)                     │
│  - Cold base model layers (read-only binary blocks)   │
│  - Zero write overhead during training lifecycle      │
└────────────────────────────────────────────────────────┘
```

---

## Core Invariants & Rules

1. **Base Weights Read-Only**: Base model weights are strictly immutable throughout the training run.
2. **Deterministic Fill Hierarchy**: Layer storage allocation fills strictly in priority order:
   $$\text{Tier 0} \longrightarrow \text{Tier 1} \longrightarrow \text{Tier 2}$$
3. **Zero Writes to Tier 2**: Disk storage is strictly read-only. LoRA adapters, optimizer moments, and activations reside exclusively in Tier 0.
4. **Automatic Fallback**: If the total 4-bit quantized model fits within Tier 0 + Tier 1 combined memory, Tier 2 NVMe spillover is automatically disabled, bypassing SSD latency completely.

---

## Pipelining & Asynchronous Prefetching
- **Prefetch Depth 2**: Background worker threads prefetch layers $N+1$ and $N+2$ from NVMe to Pinned RAM while GPU compute evaluates layer $N$.
- **Overlapped I/O and Compute**: CUDA async copy streams transfer 4-bit blocks concurrently with Tensor Core GEMM operations.
- **On-GPU Vectorized Dequantization**: 4-bit blocks are unpacked and dequantized directly inside GPU memory at device bandwidth speeds (>1 TB/s).

---

## Configuration & Telemetry

### Quokka Pipeline API
```quokka
import joey

let mut pipe = joey.pipeline()
pipe.load_model("meta-llama/Llama-3-8B", "4bit")
pipe.qlora(16, 32, 0.05)
pipe.atsrs(8192, 16.0, "./nvme_cache") // min_tokens, ram_budget_gb, nvme_path
pipe.train("dataset", 512)
pipe.run()
```

### Telemetry Metrics (TCP Port 9001)
- `io_wait_pct`: Percentage of step time where GPU compute was stalled waiting on storage I/O.
- `tok/s`: Effective training throughput in tokens processed per second.
- `step_time`: End-to-end wall-clock latency per optimization step in milliseconds.
