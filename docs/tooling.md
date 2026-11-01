# Quokka Developer Tooling

Quokka v0.9.5 integrates a developer suite directly into the single binary CLI.

---

## 1. Code Formatter (`quokka fmt`)

Quokka includes an opinionated, zero-config source code formatter.

### Usage
```bash
# Preview formatted source to stdout
quokka fmt src/main.qk

# Format and overwrite in place
quokka fmt -w src/main.qk
quokka fmt --write src/main.qk

# CI check: returns exit 0 if formatted, exit 1 if differences exist
quokka fmt --check src/main.qk
```

### Formatting Rules
* 4-space indentation per block nesting level.
* Standardized binary operator spacing (`+`, `-`, `*`, `/`, `==`, `!=`, `<=`, `>=`, `->`, `=>`, `++`).
* Consistent comma separation (`a, b, c`).
* Preservation of single blank lines between logical sections; elimination of redundant blank lines.

---

## 2. Language Server Protocol (`quokka lsp`)

Quokka ships with an embedded Language Server Protocol (LSP) server communicating over JSON-RPC 2.0 on standard input and output.

### Capabilities
* **Diagnostics (`textDocument/publishDiagnostics`)**: Real-time syntax and parse error reporting with line/column pointers directly inside the editor.
* **Document Formatting (`textDocument/formatting`)**: Instant formatting triggered by editor "Format Document" commands.
* **Hover Documentation (`textDocument/hover`)**: Contextual hover information for Quokka keywords and runtime structures.

### Integration
To connect with VS Code, Neovim, or Helix, configure the language server command:
```json
{
  "command": "quokka",
  "args": ["lsp"]
}
```

---

## 3. Interactive Debugger (`quokka debug`)

Quokka includes an interactive terminal debugger (`qkdb`) for line-by-line stepping and variable inspection.

### Usage
```bash
quokka debug path/to/script.qk
```

### Commands
| Command | Shorthand | Description |
|---------|-----------|-------------|
| `step` | `s` | Step into the next statement or expression |
| `next` | `n` | Step over to the next source line |
| `continue` | `c` | Resume execution until next breakpoint |
| `break <line>` | `b <line>` | Set a breakpoint at line number |
| `delete <line>` | `d <line>` | Remove breakpoint at line number |
| `breakpoints` | `bl` | List active breakpoints |
| `print <var>` | `p <var>` | Inspect variable value in current scope |
| `locals` | - | Display all variables bound in the active environment |
| `list` | `l` | Show source code around the current line |
| `backtrace` | `bt` | Show call stack depth |
| `help` | `h` | Show help commands |
| `quit` | `q` | Abort debugging session |

---

## 4. Package Manager (`quokka pkg`)

Package manager CLI for project scaffolding and package discovery.

### Commands
```bash
# Initialize a new package with quokka.json manifest
quokka pkg init [package_name]

# List standard library modules and capabilities
quokka pkg list
```

---

## 5. Self-Hosting Reproducibility Verifier (`quokka check-self`)

Verifies the cryptographic reproducibility and compiler equivalence of Quokka.

### Usage
```bash
# Verify self-hosting conformance against test suite
quokka check-self

# Verify against custom target
quokka check-self path/to/target.qk
```

### Verification Pipeline
1. **Stage 0 (C Host AST)**: Parses source and serializes AST to SHA-256 hash.
2. **Interpreter Execution**: Executes AST across tree-walking interpreter.
3. **Bytecode Compilation**: Compiles AST to bytecode instructions.
4. **Bytecode VM Execution**: Executes bytecode in the VM and verifies execution parity.
