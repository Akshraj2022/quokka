# Quokka Self-Hosting & Bootstrap Architecture

Quokka is a strictly self-hosted programming language. This means that the core compilation and execution semantics are implemented in Quokka itself. C is only used to start the process, and Python is only used optionally for Machine Learning endpoints.

## 1. Stage-0 Limits
The Stage-0 compiler (`src/bootstrap/`) is a minimal C host. Its **only** purpose is to parse a severely constrained subset of Quokka and immediately launch the Stage-1 (Quokka-written) interpreter. 
- Stage-0 does **not** support ML features, advanced string manipulation, comprehensive error reporting, or the `?` operator. 
- It treats Lists as mutable reference types and natively exposes OS functions (`file_read`, `exec`).
- Once Stage-1 starts, Stage-0 is merely a virtual machine executing the AST.

## 2. Canonical Interpreter
The true "Canonical Interpreter" is written entirely in Quokka. These files live in `src/` and form the official language semantics:
- `lexer.qk`: Tokenizes strings into Quokka Tokens.
- `parser.qk`: Parses tokens into the canonical Quokka AST using recursive descent.
- `ast.qk`: Defines the AST schema.
- `evaluator.qk`: Traverses the AST and executes semantics safely.
- `environment.qk`: Implements lexical bindings and variable lookups.

## 3. The Optional Joey Boundary
The `joey.qk` module defines ML pipeline bindings using a standard Quokka API. When executing ML functions, Joey writes the structured jobs to a `.json` IPC manifest and spawns `joey_backend.py` externally. Python is never embedded directly, preserving Quokka's memory safety and independence.

## 4. The Bootstrap Process
To bootstrap Quokka from scratch:
1. Compile the C host: `gcc -std=c11 src/bootstrap/*.c -o build/qk_bootstrap.exe`
2. Run `quokka bootstrap`. This tells the current Quokka interpreter to read its own source files, parse them, and generate the final `quokka_interpreter.qk` unified file.

## 5. Self-Hosting Verifiability
Quokka provides verifiable self-hosting via deterministic reproducibility testing.
Command: `quokka check-self`
**The Lifecycle:**
1. The C host executes Stage-1 (`quokka_interpreter.qk`).
2. Stage-1 is instructed to lex, parse, and evaluate *itself* (reading its own source files).
3. Stage-1 generates an output artifact (Stage-2).
4. Stage-2 must bit-for-bit match Stage-1, guaranteeing that the language semantics correctly comprehend and compile themselves without mutating or relying on undocumented C behaviors.
5. All conformance tests are executed against Stage-2.

## CLI Usage
* `quokka run <script>`: Runs a Quokka file through the self-hosted canonical interpreter.
* `quokka bootstrap`: Rebuilds the unified canonical interpreter.
* `quokka check-self`: Runs the reproducibility verifier.
* `quokka test`: Executes the Quokka standard test suite.
