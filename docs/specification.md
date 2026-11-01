# Quokka Language Behavior Specification (v0.9.5)

## 1. Overview & Execution Architecture
Quokka is an expression-oriented, deterministic, self-hosted programming language designed for clarity, safety, and modern systems and AI workflows. Quokka features multiple execution modes:
- **Canonical Tree-Walking Interpreter**: Evaluates AST nodes with deep lexical closures, reference counting, and garbage collection (`quokka <file.qk>` or `quokka run <file.qk>`).
- **Bytecode Compiler & Virtual Machine (VM)**: Fast stack-based virtual machine executing linear chunk-based bytecode with local stack offsets (`quokka --vm <file.qk>`).
- **Static Type Checker**: Compile-time static semantic analysis verifying type safety, arity, struct properties, and monads ahead of execution (`quokka --check <file.qk>`).

The canonical Quokka interpreter is verifiably self-hosted. C is only used as a stage-0 bootstrap host.

---

## 2. Variables, Scoping & Mutability
- **Immutability by Default**: Variables defined with `let x = value` are strictly immutable.
- **Explicit Mutability**: Rebinding requires explicit opt-in with `let mut x = value`, followed by reassignment `x = new_value`.
- **Explicit Shadowing**: The `shadow x = value` keyword allows redefining a variable with the same name in the same scope, safely rebinding names and types without mutation.
- **Lexical Scoping**: Blocks `{ ... }` introduce local scopes. Variables allocated within blocks are cleaned up automatically upon exit. Blocks are expressions that evaluate to the value of their final statement.

---

## 3. Type System & Values
Quokka operates a hybrid dynamic runtime and static type checking system. Supported types include:
- **Int**: 64-bit signed integer (`int64_t`).
- **Float**: 64-bit IEEE 754 double-precision floating-point number.
- **Bool**: `true` or `false`.
- **String**: Immutable UTF-8 character sequences. String concatenation uses the dedicated `++` operator.
- **Unit**: The `()` value representing absence of meaningful payload.
- **Option**: Monadic `Some(value)` or `None`.
- **Result**: Monadic `Ok(value)` or `Err(error)`.
- **List**: Dynamic reference-counted array (`[item1, item2, ...]`). Indexed via `list[index]`.
- **Struct**: User-defined structured data types with named fields and property mutation.
- **Channel**: Thread-safe communication queue (`QkChannel`).
- **Thread**: Native OS worker thread handle (`QkThread`).
- **Function / Builtin**: First-class closures and native runtime functions.

---

## 4. Control Flow
- **`if` / `else`**: Expression-based conditional branching. Condition must evaluate to `Bool`.
- **`while`**: Loop execution while condition evaluates to `true`.
- **`for var in iter`**: Iterates through elements of a `List` (e.g. `for x in range(0, 10)`).
- **`break` / `continue`**: Loop controls for premature termination and cycle continuation.
- **`return`**: Early exit from functions returning an expression or `Unit`.
- **`match`**: Structural pattern matching on Monads:
  ```quokka
  match res {
      Ok(value) => println("Success: " ++ to_string(value)),
      Err(err)  => println("Failed: " ++ err),
      _         => println("Fallback")
  }
  ```

---

## 5. Modules & Imports
Code can be organized into modular multi-file architectures:
```quokka
import "path/to/module"
import "math_utils" as math
```
- **Cycle-Safe Resolution**: Automatically resolves relative file paths and standard library paths (`src/std/`).
- **Deduplicated Import Cache**: Modules are loaded and evaluated once; subsequent imports reuse the environment bindings without re-evaluating side effects.

---

## 6. User-Defined Structs
Structs are first-class data types defined with named schemas:
```quokka
struct Point { x, y }

let mut p = Point(10, 20)
println(p.x)       // Dot property access: 10
p.x = 99           // Field assignment mutation
println(p.x)       // 99
```

---

## 7. Concurrency: Native Threads & Channels
Quokka features cross-platform OS concurrency with message-passing semantics:
```quokka
let ch = channel_new(4) // Buffered channel (capacity 4)

let worker = thread_spawn(fn() {
    channel_send(ch, 42)
    100
})

let msg = channel_recv(ch)     // Ok(42)
let ret = thread_join(worker)  // Ok(100)
channel_close(ch)
```
- **Thread Safety**: Supported by Win32 `CRITICAL_SECTION` / `CONDITION_VARIABLE` on Windows and POSIX `pthread_mutex_t` / `pthread_cond_t` on Linux/macOS.
- **Deterministic Monadic Interface**: `channel_recv` returns `Ok(val)` on message receipt, or `Err("Channel closed")` when closed.

---

## 8. Networking & Foreign Function Interface (FFI)
- **Sockets**: Cross-platform networking via Winsock2 on Windows and standard BSD sockets on POSIX systems:
  - `socket_connect(host, port)`
  - `socket_listen(port)`
  - `socket_accept(fd)`
  - `socket_send(fd, data)`
  - `socket_recv(fd, max_bytes)`
  - `socket_close(fd)`
- **FFI**: Dynamic library inspection and symbols binding:
  - `ffi_open(path)`
  - `ffi_available(handle, symbol_name)`
  - `ffi_close(handle)`

---

## 9. Error Handling Principles
- **No Exceptions or Null**: The language eliminates runtime exceptions and null pointers.
- **Monads**: Errors are passed as explicit `Result` and `Option` values.
- **The Postfix `?` Operator**: Unwraps an `Ok(v)` or `Some(v)`, or performs an immediate return from the enclosing function if an `Err` or `None` is encountered.

---

## 10. Joey ML & ATSRS Architecture
Quokka isolates heavy machine learning workloads into a strictly separated process boundary via the Joey API and the **ATSRS (Adaptive Tiered Storage for Resource-Starved Training)** engine:
- **T0 VRAM**: Active forward layer, LoRA adapters, optimizer states, activations.
- **T1 Pinned RAM**: Next layers staged as 4-bit quantized blocks.
- **T2 NVMe SSD**: Cold model layers stored read-only on disk.
- **Asynchronous Prefetch**: Overlaps layer disk/RAM I/O with GPU forward/backward passes at prefetch depth 2.
- **Telemetry**: Exposes `io_wait_pct`, `tok/s`, and `step_time` monitoring metrics over non-blocking sockets.

---

## 11. Builtin Functions Reference
- **I/O & String**: `print(val)`, `println(val)`, `to_string(val)`, `to_int(val)`, `to_float(val)`, `substring(s, start, len)`, `input()`.
- **Collections**: `len(coll)`, `push(list, val)`, `list_set(list, idx, val)`, `range(start, end)`.
- **Option / Result**: `unwrap_or(monad, default)`, `map(monad, fn)`, `and_then(monad, fn)`, `map_err(res, fn)`.
- **File System**: `file_read(path)`, `file_write(path, content)`.
- **Concurrency**: `channel_new(cap)`, `channel_send(ch, val)`, `channel_recv(ch)`, `channel_close(ch)`, `thread_spawn(fn)`, `thread_join(th)`.
- **Execution & Tooling**: `typecheck(code)`, `vm_eval(code)`, `exit(code)`.
