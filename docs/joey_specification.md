# Joey Quokka Extension Specification

Joey is an optional ML extension for Quokka, designed to integrate seamlessly with standard ML frameworks (PyTorch, Unsloth, Transformers, LoRA, QLoRA) without embedding Python or ML-specific code into the core Quokka language.

## Architecture

1. **Quokka Standard Library API**: The `joey.qk` module provides a builder API (`joey.pipeline()`) that safely constructs an ML job configuration.
2. **JSON IPC Protocol**: The configuration is serialized into JSON and written to a temporary job file (e.g., `.joey_job.json`).
3. **Structured Process Execution**: Quokka spawns the Python backend (`joey_backend.py`) as an external process, passing the JSON file as a structured argument rather than relying on unsafe shell string concatenation.
4. **Offline Resilience**: Quokka operates independently. If Python or the Joey backend dependencies are not installed, Joey calls gracefully fail with a structured `Err`, while the rest of Quokka remains unaffected.

## API Usage Example

```quokka
import joey

fn train_model() {
    let mut pipeline = joey.pipeline()

    pipeline.load_model("meta-llama/Llama-3-8B", "4bit")?
    pipeline.qlora(16, 32, 0.05)?
    pipeline.dataset("quokka_data", 512)?
    pipeline.train(3, 0.0002, 4, 4)?
    pipeline.save("./joey_quokka_model")?
    pipeline.run()?

    return Ok(true)
}
```

*Note: Joey calls return `Ok` or `Err` (the `?` postfix operator handles errors safely).*

## The Joey-to-Python Protocol

Data is passed to Python via a strict JSON schema. The job is an array of operation objects.

**JSON Schema:**
```json
[
  { "load_model": ["meta-llama/Llama-3-8B", "4bit"] },
  { "qlora": [16, 32, 0.05] },
  { "dataset": ["quokka_data", 512] },
  { "train": [3, 0.0002, 4, 4] },
  { "save": ["./joey_quokka_model"] }
]
```

## Backend Installation Requirements

The backend requires the following external Python dependencies:
- `torch` (PyTorch)
- `transformers`
- `unsloth`
- `peft` (For LoRA/QLoRA)
- `datasets`

**Installation:**
```bash
pip install torch transformers unsloth peft datasets
```

## Security Limitations
- **No String Concatenation**: Command line arguments are strictly passed to the process spawner as arrays (e.g., `["python", "joey_backend.py", job_file]`).
- **Validated Inputs**: `joey.qk` must strictly validate that inputs correspond to expected types (e.g., integers for epochs, numerical ranges for learning rates) before JSON serialization.
- **Isolated Execution**: The python script runs as a child process. It cannot mutate Quokka's memory or runtime environment directly.

## Error Conversion

The Quokka `exec` builtin captures the return code of the child process.
- **Exit Code 0**: Converted to `Ok(true)` in Quokka. Logs are optionally accessible from the standard output.
- **Non-zero Exit Code**: Converted to `Err("Joey backend failed with status X")`. Python exceptions and errors are isolated and safely mapped to Quokka's Option/Result monad.

## Offline Behavior

Because Joey is dynamically invoked via `exec`, Quokka functions perfectly without it. If `python` is unavailable or `joey_backend.py` is missing, `pipeline.run()` will capture the OS process failure and return an `Err`. Quokka will not crash, allowing developers to handle the absence of ML capabilities gracefully in their application logic.
