<div align="center">
  <img src="assets/cfbl.png" alt="Quokka Logo" width="150" />
  <h1>Quokka Programming Language</h1>
  <p><strong>A deterministic, strictly self-hosted language built for modern AI workflows.</strong></p>

  <p><a href="https://quokka.space">?? Visit quokka.space</a></p>

  [![Version](https://img.shields.io/badge/version-1.0.0-blue.svg)](https://quokka.space)
  [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
  [![Platform](https://img.shields.io/badge/platform-Windows-lightgrey.svg)](https://quokka.space)
</div>

<br/>

Quokka is a modern, expression-oriented programming language designed with an unyielding commitment to **verifiable self-hosting** and **deterministic semantics**. It provides a pure, exception-free core language perfectly isolated from its optional, high-performance Machine Learning extensions.

## ✨ Key Features

* **Strictly Self-Hosted**: The canonical lexer, parser, AST, and evaluator are written entirely in Quokka. The C-based bootstrap host (`src/bootstrap`) is only a minimal VM that boots the language and gets out of the way.
* **Deterministic & Safe**: Exceptions do not exist. Errors are handled safely via Monadic `Option` and `Result` types (`Ok` / `Err`), paired with powerful `match` expressions and the `?` postfix operator.
* **Immutable by Default**: Variables (`let`) are immutable. Mutation (`let mut`) and explicit shadowing (`shadow`) must be opted into.
* **The Joey ML Extension**: Quokka includes `joey.qk`, a modular extension that orchestrates complex Python/PyTorch/Unsloth workflows (like Llama-3 QLoRA fine-tuning) via a secure JSON IPC boundary. Python is never embedded in the Quokka core.
* **Native Windows Support**: Ships with a custom GUI installer, automatic `.qk` file associations, and a VS Code extension for syntax highlighting.

## 🚀 Getting Started

### Installation
We provide a standalone, modern Windows installer that sets up Quokka, configures your PATH, and installs the VS Code extension automatically.

1. Download the latest `Quokka-Setup.exe` from the Releases page.
2. Run the installer and follow the GUI prompts.
3. Open a new terminal and type `quokka`.

### Writing your first Quokka script

Create a file called `hello.qk`:

```quokka
// hello.qk
fn greet(name) {
    return "Hello, " ++ name ++ "!"
}

let message = greet("World")
println(message)
```

Run it using the CLI:
```bash
quokka run hello.qk
```

## 🏗️ Architecture & Self-Hosting

Quokka is engineered to mathematically prove it understands its own semantics.

* **Stage 0**: The tiny C host (`build/qk_bootstrap.exe`) executes `quokka_interpreter.qk`.
* **Stage 1**: The Quokka interpreter reads, lexes, and parses Quokka source code natively.

You can verify Quokka's determinism using the built-in verifier:
```bash
quokka check-self
```
This forces Stage 0 to compile Stage 1, and Stage 1 to compile Stage 2, asserting that the cryptographic hashes of the AST and outputs match perfectly.

## 🧠 Joey: The AI / ML Boundary

Quokka isolates complex, heavy, and non-deterministic Machine Learning workflows into a strictly separated process via the Joey API.

```quokka
import joey

let mut pipeline = joey.pipeline()
pipeline.load_model("meta-llama/Llama-3-8B", "4bit")
pipeline.train("quokka_dataset", 512)
let result = pipeline.run()

match result {
    Ok(status) => println("Training complete!"),
    Err(e) => println("Training failed: " ++ e)
}
```
*Note: Joey writes the configuration to a secure `.json` IPC manifest and spawns the Python backend, ensuring Quokka remains pure and memory-safe.*

## 🛠️ Building from Source

If you wish to compile Quokka from scratch:

1. **Compile the Bootstrap**:
   ```bash
   gcc -std=c11 src/bootstrap/*.c -o build/qk_bootstrap.exe
   ```
2. **Re-bootstrap the Canonical Interpreter**:
   ```bash
   quokka bootstrap
   ```

## 🤝 Contributing
Contributions are welcome! Please read our [Contributing Guide](CONTRIBUTING.md) to learn about our development process, how to propose bugfixes and improvements, and how to build and test your changes.

## 📄 License
Quokka is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
