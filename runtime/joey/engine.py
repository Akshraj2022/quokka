"""
Joey Engine — core ML operations on top of PyTorch.
This is what the Quokka interpreter calls under the hood.
"""

import os
import sys
import json

def load_model(model_name, quantize=None):
    """Load a model from HuggingFace with optional quantization."""
    from transformers import AutoModelForCausalLM, AutoTokenizer

    print(f"[Joey] Loading model: {model_name}")
    load_kwargs = {"device_map": "auto", "trust_remote_code": True}

    if quantize == "4bit":
        from transformers import BitsAndBytesConfig
        import torch
        load_kwargs["quantization_config"] = BitsAndBytesConfig(
            load_in_4bit=True,
            bnb_4bit_compute_dtype=torch.float16,
            bnb_4bit_quant_type="nf4",
            bnb_4bit_use_double_quant=True,
        )
        print("[Joey] Using 4-bit quantization (QLoRA-ready)")
    elif quantize == "8bit":
        from transformers import BitsAndBytesConfig
        load_kwargs["quantization_config"] = BitsAndBytesConfig(load_in_8bit=True)
        print("[Joey] Using 8-bit quantization")

    model = AutoModelForCausalLM.from_pretrained(model_name, **load_kwargs)
    tokenizer = AutoTokenizer.from_pretrained(model_name, trust_remote_code=True)

    if tokenizer.pad_token is None:
        tokenizer.pad_token = tokenizer.eos_token
        model.config.pad_token_id = tokenizer.pad_token_id

    param_count = sum(p.numel() for p in model.parameters())
    print(f"[Joey] Model loaded: {param_count:,} parameters")
    return {"model": model, "tokenizer": tokenizer, "name": model_name}


def apply_lora(bundle, r=16, alpha=32, dropout=0.05, targets=None):
    """Apply LoRA adapter to a model."""
    from peft import LoraConfig, get_peft_model, TaskType

    model = bundle["model"]

    if targets is None:
        targets = _find_linear_modules(model)

    config = LoraConfig(
        task_type=TaskType.CAUSAL_LM,
        r=r,
        lora_alpha=alpha,
        lora_dropout=dropout,
        target_modules=targets,
        bias="none",
    )

    peft_model = get_peft_model(model, config)
    trainable = sum(p.numel() for p in peft_model.parameters() if p.requires_grad)
    total = sum(p.numel() for p in peft_model.parameters())
    pct = 100 * trainable / total
    print(f"[Joey] LoRA applied: {trainable:,} trainable / {total:,} total ({pct:.2f}%)")

    bundle["model"] = peft_model
    bundle["peft_config"] = config
    return bundle


def apply_qlora(bundle, r=16, alpha=32, dropout=0.05, targets=None):
    """Apply QLoRA (4-bit quantized LoRA) — just LoRA on a quantized model."""
    print("[Joey] QLoRA = LoRA on quantized model (ensure model was loaded with quantize='4bit')")
    return apply_lora(bundle, r=r, alpha=alpha, dropout=dropout, targets=targets)


def load_dataset(name, split="train", subset=None):
    """Load a dataset from HuggingFace."""
    from datasets import load_dataset as hf_load

    print(f"[Joey] Loading dataset: {name}")
    kwargs = {}
    if subset:
        kwargs["name"] = subset

    ds = hf_load(name, split=split, **kwargs)
    print(f"[Joey] Dataset loaded: {len(ds)} examples")
    return ds


def format_dataset(dataset, template, tokenizer, max_length=512):
    """Format and tokenize a dataset using a template string.

    Template uses {column_name} placeholders, e.g.:
    "### Instruction:\\n{instruction}\\n### Response:\\n{output}"
    """
    print(f"[Joey] Formatting dataset with max_length={max_length}")

    def _format_and_tokenize(examples):
        texts = []
        keys = list(examples.keys())
        n = len(examples[keys[0]])
        for i in range(n):
            row = {k: examples[k][i] for k in keys}
            text = template.format(**row)
            if not text.endswith(tokenizer.eos_token):
                text += tokenizer.eos_token
            texts.append(text)

        tokenized = tokenizer(
            texts,
            truncation=True,
            max_length=max_length,
            padding="max_length",
        )
        tokenized["labels"] = tokenized["input_ids"].copy()
        return tokenized

    formatted = dataset.map(_format_and_tokenize, batched=True, remove_columns=dataset.column_names)
    print(f"[Joey] Dataset formatted: {len(formatted)} examples, max_length={max_length}")
    return formatted


def train(bundle, dataset, epochs=3, lr=2e-4, batch_size=4, gradient_steps=4,
          warmup_ratio=0.03, max_steps=-1, logging_steps=10, save_steps=100,
          output_dir="./joey_output"):
    """Train/fine-tune the model."""
    from transformers import TrainingArguments, Trainer, DataCollatorForLanguageModeling
    import torch

    model = bundle["model"]
    tokenizer = bundle["tokenizer"]

    print(f"[Joey] Starting training:")
    print(f"  Epochs: {epochs}")
    print(f"  Learning rate: {lr}")
    print(f"  Batch size: {batch_size}")
    print(f"  Gradient accumulation: {gradient_steps}")
    print(f"  Effective batch: {batch_size * gradient_steps}")
    print(f"  Output: {output_dir}")

    training_args = TrainingArguments(
        output_dir=output_dir,
        num_train_epochs=epochs,
        per_device_train_batch_size=batch_size,
        gradient_accumulation_steps=gradient_steps,
        learning_rate=lr,
        warmup_steps=10,
        max_steps=max_steps,
        logging_steps=logging_steps,
        save_steps=save_steps,
        save_total_limit=3,
        fp16=torch.cuda.is_available(),
        optim="adamw_torch",
        report_to="none",
        remove_unused_columns=False,
    )

    collator = DataCollatorForLanguageModeling(tokenizer=tokenizer, mlm=False)

    trainer = Trainer(
        model=model,
        args=training_args,
        train_dataset=dataset,
        data_collator=collator,
    )

    print("[Joey] Training...")
    result = trainer.train()
    print(f"[Joey] Training complete! Loss: {result.training_loss:.4f}")
    bundle["trainer"] = trainer
    return bundle


def save_model(bundle, path):
    """Save the adapter (LoRA weights) to disk."""
    model = bundle["model"]
    tokenizer = bundle["tokenizer"]

    os.makedirs(path, exist_ok=True)
    model.save_pretrained(path)
    tokenizer.save_pretrained(path)
    print(f"[Joey] Model saved to: {path}")
    return path


def merge_and_save(bundle, path):
    """Merge LoRA weights into base model and save full model."""
    from peft import PeftModel

    model = bundle["model"]
    tokenizer = bundle["tokenizer"]

    print("[Joey] Merging LoRA weights into base model...")
    if hasattr(model, "merge_and_unload"):
        merged = model.merge_and_unload()
    else:
        merged = model

    os.makedirs(path, exist_ok=True)
    merged.save_pretrained(path)
    tokenizer.save_pretrained(path)
    print(f"[Joey] Merged model saved to: {path}")
    return path


def generate(bundle, prompt, max_tokens=256, temperature=0.7, top_p=0.9):
    """Generate text from a prompt."""
    import torch

    model = bundle["model"]
    tokenizer = bundle["tokenizer"]

    inputs = tokenizer(prompt, return_tensors="pt").to(model.device)

    with torch.no_grad():
        outputs = model.generate(
            **inputs,
            max_new_tokens=max_tokens,
            temperature=temperature,
            top_p=top_p,
            do_sample=temperature > 0,
            pad_token_id=tokenizer.pad_token_id,
        )

    result = tokenizer.decode(outputs[0], skip_special_tokens=True)
    return result


def _find_linear_modules(model):
    """Auto-detect linear modules for LoRA targeting."""
    import torch.nn as nn
    from peft.tuners.lora import Linear as LoraLinear

    targets = set()
    for name, module in model.named_modules():
        if isinstance(module, (nn.Linear,)):
            parts = name.split(".")
            targets.add(parts[-1])

    # Remove common non-targetable modules
    targets.discard("lm_head")
    targets.discard("embed_tokens")

    result = list(targets)
    if not result:
        result = ["q_proj", "v_proj"]  # safe default

    print(f"[Joey] Auto-detected LoRA targets: {result}")
    return result


# ============================================================
# Script mode: called by the Quokka interpreter
# Reads a JSON config and executes the ML pipeline
# ============================================================
def run_from_config(config_path):
    """Execute an ML pipeline from a JSON config file (generated by Quokka)."""
    with open(config_path, "r") as f:
        config = json.load(f)

    steps = config.get("steps", [])
    state = {}

    for step in steps:
        op = step["op"]

        if op == "load_model":
            state["bundle"] = load_model(
                step["model_name"],
                quantize=step.get("quantize"),
            )

        elif op == "lora":
            state["bundle"] = apply_lora(
                state["bundle"],
                r=step.get("r", 16),
                alpha=step.get("alpha", 32),
                dropout=step.get("dropout", 0.05),
                targets=step.get("targets"),
            )

        elif op == "qlora":
            state["bundle"] = apply_qlora(
                state["bundle"],
                r=step.get("r", 16),
                alpha=step.get("alpha", 32),
                dropout=step.get("dropout", 0.05),
                targets=step.get("targets"),
            )

        elif op == "load_dataset":
            state["dataset_raw"] = load_dataset(
                step["name"],
                split=step.get("split", "train"),
                subset=step.get("subset"),
            )

        elif op == "format_dataset":
            state["dataset"] = format_dataset(
                state["dataset_raw"],
                template=step["template"],
                tokenizer=state["bundle"]["tokenizer"],
                max_length=step.get("max_length", 512),
            )

        elif op == "train":
            state["bundle"] = train(
                state["bundle"],
                state["dataset"],
                epochs=step.get("epochs", 3),
                lr=step.get("lr", 2e-4),
                batch_size=step.get("batch_size", 4),
                gradient_steps=step.get("gradient_steps", 4),
                warmup_ratio=step.get("warmup_ratio", 0.03),
                max_steps=step.get("max_steps", -1),
                logging_steps=step.get("logging_steps", 10),
                save_steps=step.get("save_steps", 100),
                output_dir=step.get("output_dir", "./joey_output"),
            )

        elif op == "save":
            save_model(state["bundle"], step["path"])

        elif op == "merge_save":
            merge_and_save(state["bundle"], step["path"])

        elif op == "generate":
            result = generate(
                state["bundle"],
                step["prompt"],
                max_tokens=step.get("max_tokens", 256),
                temperature=step.get("temperature", 0.7),
            )
            print(f"\n{result}\n")

        else:
            print(f"[Joey] Unknown operation: {op}")

    print("[Joey] Pipeline complete.")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python -m joey.engine <config.json>")
        sys.exit(1)
    run_from_config(sys.argv[1])
