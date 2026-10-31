"""
Comprehensive unit and integration test suite for ATSRS.
Tests:
  1. ATSRSConfig & auto grad_accum calculation.
  2. Fallback mechanism (T2 disabled when model fits T0+T1).
  3. Tiered storage (T0 VRAM, T1 Pinned RAM, T2 NVMe).
  4. 4-bit Quantization and GPU dequantization.
  5. Prefetch pipeline (depth 2) and I/O overlap.
  6. End-to-end QLoRA training step and metrics (io_wait_pct, tok/s, step_time).
"""

import os
import sys
import math
import shutil
import tempfile
import unittest

# Ensure quokka root is in path
ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
if ROOT_DIR not in sys.path:
    sys.path.insert(0, ROOT_DIR)

from runtime.joey.atsrs import (
    ATSRSConfig,
    QuantizedBlock,
    TieredStorageManager,
    PrefetchPipeline,
    ATSRSLayer,
    ATSRSModelWrapper,
    ATSRSTrainer,
    build_synthetic_atsrs_model,
    HAS_TORCH,
)

if HAS_TORCH:
    import torch


class TestATSRS(unittest.TestCase):

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp(prefix="atsrs_test_")

    def tearDown(self):
        if os.path.exists(self.temp_dir):
            shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_01_config_auto_grad_accum(self):
        """Verify min_tokens_per_step=8192 and auto grad_accum calculation."""
        cfg = ATSRSConfig(
            min_tokens_per_step=8192,
            batch_size=2,
            seq_len=512,
            grad_accum="auto",
            nvme_path=self.temp_dir,
        )
        # 2 * 512 = 1024 tokens per micro-batch
        # 8192 / 1024 = 8 grad_accum steps
        self.assertEqual(cfg.grad_accum, 8)
        self.assertEqual(cfg.effective_tokens_per_step, 8192)

        # Test non-exact division rounds up (ceil)
        cfg2 = ATSRSConfig(
            min_tokens_per_step=8192,
            batch_size=3,
            seq_len=512,
            grad_accum="auto",
            nvme_path=self.temp_dir,
        )
        # 3 * 512 = 1536 tokens per micro-batch
        # ceil(8192 / 1536) = 6 steps
        self.assertEqual(cfg2.grad_accum, 6)
        self.assertGreaterEqual(cfg2.effective_tokens_per_step, 8192)

    def test_02_quantization_and_dequantization(self):
        """Verify 4-bit packing and GPU/CPU dequantization."""
        if not HAS_TORCH:
            self.skipTest("PyTorch not installed")

        tensor = torch.randn(64, 64, dtype=torch.float32)
        block = QuantizedBlock.from_tensor(tensor)

        # 64*64 = 4096 elements. At 4-bits (2 per byte), packed should be 2048 bytes
        self.assertEqual(len(block.packed_bytes), 2048)
        self.assertEqual(block.shape, (64, 64))

        # Dequantize
        device = "cuda" if torch.cuda.is_available() else "cpu"
        dequant = block.dequantize_on_gpu(device=device)

        self.assertEqual(dequant.shape, (64, 64))
        # Check finite values and reasonable scale preservation
        self.assertTrue(torch.isfinite(dequant).all())
        corr = torch.cosine_similarity(tensor.flatten().to(device), dequant.flatten(), dim=0)
        self.assertGreater(corr.item(), 0.90)  # Strong 4-bit reconstruction fidelity

    def test_03_fallback_mechanism(self):
        """Verify fallback disables T2 when model fits T0+T1."""
        if not HAS_TORCH:
            self.skipTest("PyTorch not installed")

        # Huge RAM budget (100 GB) -> model easily fits in T0+T1
        cfg_large_ram = ATSRSConfig(
            ram_budget_gb=100.0,
            nvme_path=self.temp_dir,
        )
        model, storage, _ = build_synthetic_atsrs_model(num_layers=4, hidden_dim=128, config=cfg_large_ram)
        self.assertFalse(storage.t2_enabled)
        # Verify no files written to NVMe directory
        nvme_files = [f for f in os.listdir(self.temp_dir) if f.endswith(".bin")]
        self.assertEqual(len(nvme_files), 0)
        storage.cleanup()

    def test_04_t2_nvme_spillover_when_exceeding_budget(self):
        """Verify cold layers spill to read-only T2 NVMe when exceeding budget."""
        if not HAS_TORCH:
            self.skipTest("PyTorch not installed")

        # Very tiny RAM budget (0.000001 GB ~ 1 KB) to force T2 usage
        cfg_tiny_ram = ATSRSConfig(
            ram_budget_gb=0.000001,
            nvme_path=self.temp_dir,
            device="cpu",  # Force CPU budget limit
        )
        model, storage, _ = build_synthetic_atsrs_model(num_layers=6, hidden_dim=256, config=cfg_tiny_ram)
        self.assertTrue(storage.t2_enabled)
        # Verify NVMe files created
        nvme_files = [f for f in os.listdir(self.temp_dir) if f.endswith(".bin")]
        self.assertEqual(len(nvme_files), 6)

        # Verify read-only retrieval
        block_0 = storage.get_layer_block(0)
        self.assertEqual(block_0.shape, (256, 256))
        storage.cleanup()

    def test_05_prefetch_depth_2_and_metrics(self):
        """Verify prefetch pipeline with depth 2 and metric calculation."""
        if not HAS_TORCH:
            self.skipTest("PyTorch not installed")

        cfg = ATSRSConfig(
            ram_budget_gb=1.0,
            nvme_path=self.temp_dir,
            min_tokens_per_step=512,
            batch_size=1,
            seq_len=64,
            grad_accum=2,
            prefetch_depth=2,
        )

        model, storage, _ = build_synthetic_atsrs_model(num_layers=4, hidden_dim=128, config=cfg)
        trainer = ATSRSTrainer(model, cfg)

        # Create micro-batch data
        micro_batches = [
            torch.randn(1, 64, 128) for _ in range(cfg.grad_accum)
        ]

        metrics = trainer.train_step(micro_batches)

        # Check required metrics
        self.assertIn("io_wait_pct", metrics)
        self.assertIn("tok/s", metrics)
        self.assertIn("step_time", metrics)
        self.assertIn("loss", metrics)

        self.assertGreaterEqual(metrics["io_wait_pct"], 0.0)
        self.assertLessEqual(metrics["io_wait_pct"], 100.0)
        self.assertGreater(metrics["tok/s"], 0.0)
        self.assertGreater(metrics["step_time"], 0.0)

        # Check LoRA weights received updates
        for layer in model.layer_list:
            self.assertIsNotNone(layer.adapter.lora_A.grad)
            self.assertIsNotNone(layer.adapter.lora_B.grad)

        storage.cleanup()


if __name__ == "__main__":
    unittest.main()
