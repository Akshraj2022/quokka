# Quokka Self-Hosting & Bootstrap Architecture

Quokka is a strictly self-hosted programming language. This means that the core compilation and execution semantics are implemented in Quokka itself. C is only used to start the process, and Python is only used optionally for Machine Learning endpoints.

## 1. Stage-0 Limits
The Stage-0 compiler (`src/bootstrap/`) is a minimal C host. Its **only** purpose is to parse a severely constrained subset of Quokka and immediately launch the Stage-1 (Quokka-written) interpreter. 
- Stage-0 does **not** support ML features, advanced string manipulation, comprehensive error reporting, or the `?` operator. 
- It treats Lists as mutable reference types and natively exposes OS functions (`file_read`, `exec`).
- Once Stage-1 starts, Stage-0 is merely a virtual machine executing the AST.

## 2. Canonical Interpreter
The true "Canonical Interpreter" is written entirely in Quokka. These files live in `src/` and form the official language semantics:
- `lexer.qka`: Tokenizes strings into Quokka Tokens.
- `parser.qka`: Parses tokens into the canonical Quokka AST using recursive descent.
- `ast.qka`: Defines the AST schema.
- `evaluator.qka`: Traverses the AST and executes semantics safely.
- `environment.qka`: Implements lexical bindings and variable lookups.

## 3. The Optional Joey Boundary
The `joey.qka` module defines ML pipeline bindings using a standard Quokka API. When executing ML functions, Joey writes the structured jobs to a `.json` IPC manifest and spawns `joey_backend.py` externally. Python is never embedded directly, preserving Quokka's memory safety and independence.

## 4. The Bootstrap Process
To bootstrap Quokka from scratch:
1. Compile the C host: `gcc -std=c11 src/bootstrap/*.c -o build/qk_bootstrap.exe`
2. Run `quokka bootstrap`. This tells the current Quokka interpreter to read its own source files, parse them, and generate the final `quokka_interpreter.qka` unified file.

## 5. Self-Hosting Verifiability

Quokka provides verifiable self-hosting via deterministic reproducibility testing.
Command: `quokka check-self`

**The Protocol:**
1. **Stage 0 (C bootstrap)** compiles and runs **Stage 1** (`quokka_interpreter.qka`, written in Quokka).
2. Stage 1, now running, is used to compile/interpret ITSELF (**Stage 2**) — i.e. Stage 1 reads its own source and produces an AST, then interprets a fixed test program.
3. Stage 2's serialized AST and the test program's printed output are hashed using SHA-256.
4. This repeats: Stage 2 interprets the same interpreter source again to produce **Stage 3**, and its AST/output are hashed.
5. If Stage 2's hashes and Stage 3's hashes are identical, the interpreter is confirmed to produce byte-identical results when run on itself repeatedly — that's what "self-hosting is verified" actually means.

This is a genuine multi-generation bootstrap where each stage's INPUT is the ACTUAL OUTPUT/AST of the previous stage interpreting the source.

**Example Output:**
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

5. All conformance tests are executed against Stage-2.

## CLI Usage
* `quokka run <script>`: Runs a Quokka file through the self-hosted canonical interpreter.
* `quokka bootstrap`: Rebuilds the unified canonical interpreter.
* `quokka check-self`: Runs the reproducibility verifier.
* `quokka test`: Executes the Quokka standard test suite.
