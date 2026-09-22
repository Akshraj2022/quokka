<div align="center">
  <img src="assets/cfbl.png" alt="Quokka Logo" width="150" />
  <h1>Quokka Programming Language</h1>
  <p><strong>A deterministic, strictly self-hosted language built for modern AI workflows.</strong></p>

  <p><a href="https://quokka.space">?? Visit quokka.space</a></p>

  [![Version](https://img.shields.io/badge/version-v0.3.0-blue.svg)](https://quokka.space)
  [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
  [![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux-lightgrey.svg)](https://quokka.space)
</div>

<br/>

Quokka is a modern, expression-oriented programming language designed with an unyielding commitment to **verifiable self-hosting** and **deterministic semantics**. It provides a pure, exception-free core language perfectly isolated from its optional, high-performance Machine Learning extensions.

## ✨ Key Features

* **Strictly Self-Hosted**: The canonical lexer, parser, AST, and evaluator are written entirely in Quokka. The C-based bootstrap host (`src/bootstrap`) is only a minimal VM that boots the language and gets out of the way.
* **Deterministic & Safe**: Exceptions do not exist. Errors are handled safely via Monadic `Option` and `Result` types (`Ok` / `Err`), paired with powerful `match` expressions and the `?` postfix operator.
* **Immutable by Default**: Variables (`let`) are immutable. Mutation (`let mut`) and explicit shadowing (`shadow`) must be opted into.
* **The Joey ML Extension**: Quokka includes `joey.qka`, a modular extension that orchestrates complex Python/PyTorch/Unsloth workflows (like Llama-3 QLoRA fine-tuning) via a secure JSON IPC boundary. Python is never embedded in the Quokka core.
* **Native Windows Support**: Manual installation via a standalone zip archive (no automated installer, PATH configuration, or file associations are included).

## 🚀 Getting Started

### Installation

**Windows**
Quokka is distributed as a standalone zip archive containing the `quokka.exe` binary and the Quokka standard library source. There is no automated installer.

1. Download `quokka-windows-x64.zip` from the [Releases](https://github.com/Akshraj2022/quokka/releases) page.
2. Extract the archive to a folder (e.g., `C:\Quokka`).
3. Manually add that folder to your system's `PATH` environment variable.
4. Open a new terminal and type `quokka`.
*(Note: If you want VS Code syntax highlighting, you must manually install the extension from `editors/vscode`.)*

**macOS (Apple Silicon & Intel)**
Quokka is natively supported on both Apple Silicon (arm64) and Intel (x86_64) macOS.

For Apple Silicon, you can use Homebrew:
```bash
brew install akshraj2022/quokka/quokka
```

For Intel Macs, or as a manual alternative, download the respective tarball (`quokka-macos-x64.tar.gz` or `quokka-macos-arm64.tar.gz`) from the [Releases](https://github.com/Akshraj2022/quokka/releases) page, extract it, and add it to your `PATH`.
*(Note: Joey's CUDA-accelerated ML features are not available on macOS hardware.)*

**Linux (Ubuntu / x86_64)**
Quokka is distributed as a standalone tarball for Linux.
1. Download `quokka-linux-x64.tar.gz` from the [Releases](https://github.com/Akshraj2022/quokka/releases) page.
2. Extract the archive: `tar -xzf quokka-linux-x64.tar.gz`
3. Add the extracted folder to your `PATH`.

### Writing your first Quokka script

Create a file called `hello.qka`:

```quokka
// hello.qka
fn greet(name) {
    return "Hello, " ++ name ++ "!"
}

let message = greet("World")
println(message)
```

Run it using the CLI:
```bash
quokka run hello.qka
```

## 🏗️ Architecture & Self-Hosting

Quokka is engineered to mathematically prove it understands its own semantics.

* **Stage 0**: The tiny C host (`build/qk_bootstrap.exe`) executes `quokka_interpreter.qka`.
* **Stage 1**: The Quokka interpreter reads, lexes, and parses Quokka source code natively.

You can verify Quokka's determinism using the built-in verifier:
```bash
quokka check-self
```
This performs a real SHA-256 cryptographic verification across three bootstrap stages. Stage 0 parses and executes the Quokka interpreter, which in turn parses and evaluates itself (Stage 1), and then again (Stage 2). Quokka guarantees that the AST generated and output produced by the interpreter compiling itself matches perfectly byte-for-byte:

```text
Running reproducibility tests...
Stage 0 -> Stage 1: bootstrapping interpreter...
Values test passed
Stage 1 AST hash: 2e047632c6ed548e164b64a9cd0135a44232dc2cd96af75234af58ddc08c2a37
Stage 1 -> Stage 2: interpreter compiling itself...
Values test passed
Stage 2 AST hash: 2e047632c6ed548e164b64a9cd0135a44232dc2cd96af75234af58ddc08c2a37
Stage 2 -> Stage 3: repeating for verification...
Values test passed
Stage 3 AST hash: 2e047632c6ed548e164b64a9cd0135a44232dc2cd96af75234af58ddc08c2a37
v Self-hosting verified: hashes match across stages.
```

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
*Note: Joey writes the configuration to a secure `.json` IPC manifest and spawns the Python backend, ensuring Quokka remains pure and memory-safe. Joey's CUDA-accelerated training path is Windows/Linux only — CUDA is not available on macOS hardware.*

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
