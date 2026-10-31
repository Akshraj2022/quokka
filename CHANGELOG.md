# Changelog

## [0.9.3] - 2026-10-31 - ATSRS (Adaptive Tiered Storage for Resource-Starved Training)

### ATSRS Engine Architecture (runtime/joey/atsrs.py)
- **Extreme Scale QLoRA**: Enabled fine-tuning of multi-billion parameter foundation models exceeding combined VRAM + host RAM capacity.
- **Three-Tier Storage Hierarchy**:
  - **T0 VRAM**: Active layer (dequantized fp16), trainable LoRA adapters ($A$ and $B$ low-rank matrices), AdamW optimizer states, and forward activations.
  - **T1 Pinned RAM**: Next layers staged as 4-bit quantized blocks in pinned host memory up to `ram_budget_gb`.
  - **T2 NVMe SSD**: Cold layers stored in read-only binary blocks at `nvme_path`.
- **Strict Invariants**:
  - Base weights strictly read-only throughout lifecycle.
  - Fill order strictly enforced: `T0 -> T1 -> T2`.
  - Zero writes to T2 during training (adapters and updates stay exclusively in T0).
- **Automatic Fallback Detection**: Automatically checks total 4-bit model size against combined T0+T1 memory. If the model fits in T0 + T1, T2 NVMe spillover is completely disabled, avoiding SSD overhead.
- **Prefetch Depth 2 Pipeline**: Pipelined asynchronous prefetching overlapping layer I/O with GPU forward/backward compute using background workers and dedicated CUDA streams.
- **On-GPU 4-Bit Dequantization**: Vectorized GPU unpacking and dequantization directly on device at memory bandwidth speed.
- **Real-Time Telemetry & Progress Metrics**: Tracks and emits `io_wait_pct` (% step time blocked on I/O), `tok/s` (effective token throughput), and `step_time` (step wall-clock latency) over TCP port 9001.

### Joey ML Framework & Monitor Integration
- **Quokka Standard Library API**: Added `joey_pipeline()`, `joey_atsrs()`, and `JoeyPipeline` struct in `src/std/joey.qk`.
- **Python ML Backend**: Integrated ATSRS step and trainer execution into `runtime/joey/engine.py` and `src/std/joey_backend.py`.
- **Native Monitor Dashboard**: Upgraded `joey_watch.c` TUI dashboard to parse and render live ATSRS I/O wait percentages and step times.
- **Native C Backend Hardening**: Optimized `native_emit_progress` with non-blocking socket I/O and included `#include <fcntl.h>` for POSIX compliance.
- **Zero-Leak Validation**: Resolved list refcount leak in `builtin_joey_run_native`; verified 100% clean under Linux Valgrind (`1,296 allocs, 1,296 frees, 0 bytes in 0 blocks, 0 errors`).

## [0.9.2] - 2026-10-31 - Modules, User-Defined Structs, Sockets, and FFI

### Modular Code Architecture
- **Module Import System**: Implemented `import "module/path"` and `import "module" as alias` keywords and parser grammar.
- **Deduplicated Import Cache**: Built-in interpreter tracks loaded modules, preventing multiple executions or cyclic import overhead.
- **Resolution Pathing**: Automatically resolves relative filepaths, standard library paths (`src/std/`), and `.qk`/`.qka` extensions.

### User-Defined Structs & Object System
- **Struct Definitions**: First-class `struct Name { field1, field2, ... }` AST declarations and constructor functions.
- **Property Access**: Postfix dot-syntax (`struct.field`) evaluator for dynamic, zero-leak field extraction.
- **Mutable Field Updates**: Postfix field assignment (`struct.field = value`) with type preservation and intermediate cleanup.
- **Zero-Leak Lifetime**: Integrated `VAL_STRUCT` with reference-counted instances (`QkStructInstance`) and recursive GC.

### Cross-Platform Network Sockets
- **Socket Primitives**: Implemented `socket_connect(host, port)`, `socket_listen(port)`, `socket_accept(fd)`, `socket_send(fd, data)`, `socket_recv(fd, max_bytes)`, and `socket_close(fd)`.
- **Cross-Platform Abstraction**: Seamlessly operates on Winsock2 on Windows (`-lws2_32`) and standard BSD Sockets on Linux and macOS.
- **Deterministic Error Handling**: All socket primitives return `Result<Int/String, String>` monadic types (`Ok` / `Err`).

### Foreign Function Interface (FFI)
- **Dynamic Library Inspection**: Implemented `ffi_open(path)`, `ffi_available(handle, symbol_name)`, and `ffi_close(handle)`.
- **Cross-Platform Dynamic Linker**: Built with `LoadLibraryA`/`GetProcAddress` on Windows and `dlopen`/`dlsym` (`-ldl`) on POSIX systems.

### Hardening & Zero-Leak Auditing
- **Header Collision Resolution**: Renamed `TokenType` to `QkTokenType` across `quokka.h`, `lexer.c`, and `parser.c` to prevent collisions with Windows SDK `winnt.h`.
- **AST Tracker Protection**: Eliminated duplicate AST tracking calls on imported module trees, preventing double-frees on termination.
- **Valgrind Verification**: 100% memory clean (`477 allocs, 477 frees, 0 bytes in 0 blocks, 0 errors`).

## [0.9.1] - 2026-10-30 - Comprehensive Hardening & Zero-Leak Release

### Zero-Leak Memory Engine (Valgrind-Verified 0 Leaks)
- **Complete AST Garbage Collection**: Implemented full recursive AST destructor `node_free()` and root tracking registry (`qk_ast_track`, `qk_ast_free_all`). Hooked AST destruction into arena resets, REPL executions, and program termination.
- **Function Call Argument Leak Elimination**: Fixed double-cloning memory leaks in `call_value()` parameter binding.
- **Pattern Match Binding Leak Elimination**: Fixed value leaks in `NODE_MATCH` arm bindings by properly freeing cloned subject values.
- **Loop Iteration Leak Elimination**: Fixed double-cloning memory leaks in `for` loop iteration variable bindings.
- **Destructor & Refcount Hardening**: Added reference counting to `QkList` and `Env`, differentiating closure snapshots from lexical scope chains to prevent double-frees.
- **Intermediate Expression Cleanup**: Systematically reclaimed intermediate values in binary expressions, unary operations, string concatenations, variable assignments (`NODE_LET`, `NODE_ASSIGN`, `NODE_SHADOW`), list lookups, and block statements.
- **Builtin Return Value Cleanup**: Fixed leaks in `builtin_capture_println()` and updated `builtin_list_set()` to free overwritten values.
- **Parser Assignment Leak Fix**: Fixed statement expression node leak in `parser.c` assignment parsing.
- **Verified 0 Leaks**: 100% clean Valgrind memory audits on Linux across all conformance suites and example programs (`0 bytes in 0 blocks`).

### Cross-Platform Architecture & Portability (Linux, macOS, Windows)
- **Hardware Architecture Detection**: Added `arch_name()` builtin supporting `x86_64`, `arm64`, `x86`, and `arm`.
- **POSIX Conformance**: Added `_DEFAULT_SOURCE` and `_POSIX_C_SOURCE 200809L` headers across Joey native and monitor modules, resolving 64-bit pointer truncation warnings and segfaults on Linux and macOS.
- **Compiler Compatibility**: Guarded MSVC `#pragma comment` directives to eliminate compiler warnings under MinGW GCC and Clang.
- **Standardized Headers**: Replaced non-standard `<memory.h>` with standard `<string.h>` in `sha256.c`.
- **Universal Build System**: Created universal `build.sh` (supporting GCC and Clang on Linux/macOS) and standard cross-platform `Makefile`.
- **Cross-Platform Test Harness**: Created universal POSIX test harness `tests/run_tests.sh` and modernized `tests/run_differential.sh` and `tests/test_negative.sh`.

### Runtime Stability & Stack Safety
- **Call Stack Recursion Guard**: Implemented recursion depth tracking (`QK_MAX_CALL_DEPTH 2500`) in `call_value()`, transforming runaway recursive stack overflows into deterministic runtime errors.
- **Lexer Escape Line Synchronization**: Fixed lexer source line desynchronization when string literals contain escaped newlines.
- **String Quote Lexing Repair**: Fixed string literal parsing across `src/lexer.qka`, `src/quokka_interpreter.qka`, and `src/main.qka`.
- **Terminal Cursor Signal Handling**: Installed `SIGINT` and `SIGTERM` signal handlers in `joey_watch.c` to guarantee terminal cursor restoration.
- **Sparkline Visualization**: Fixed inverted sparkline scaling bug in Joey watch terminal monitor.
- **Joey C/Python Robustness**: Added bounds checks to `ftell` and `fread` returns, NULL guards to `localtime`/`gmtime`, deduplicated CUDA error declarations, and protected Python backend argument unpacking.

### CLI & Infrastructure
- **Standard CLI Flags**: Added `--version` (`-v`) and `--help` (`-h`) flags to Quokka CLI.
- **CI Workflows**: Modernized GitHub Actions workflows (`release.yml`, `linux-build.yml`, `macos-build.yml`, `macos-cross-build.yml`) with automated test runners and pthread linkage.

## [0.9.0] - 2026-10-30 - Milestone Release

### Summary
Milestone v0.9.0 delivers comprehensive stability, security, memory management, and build infrastructure upgrades across all layers of the Quokka language and Joey AI/ML framework.

### Highlights
- **Zero Mojibake & Emojis**: Complete eradication of double-encoded UTF-8 artifacts and emojis across documentation, TUI monitors, and code.
- **Robust Memory Management**: Introduction of `qk_malloc`/`qk_realloc`/`qk_free` with fatal out-of-memory protections, recursive `val_free()` destructors, and closure environment isolation.
- **Mathematical Safety**: Hardware `SIGFPE` exception prevention on signed 64-bit integer overflow (`INT64_MIN / -1` and `INT64_MIN % -1`).
- **Complete CLI Capabilities**: Native support for `quokka run <file>` command structure, auto-detecting test harness binaries, and robust fallback chains in `quokka.bat`.
- **Parser & Type System**: Full 64-bit bounds checking, string escape character processing (`\n`, `\t`, `\r`, `\\`, `\"`), and monadic `Result` support in `and_then()`.
- **GPU Safety**: Rank clamping and CUDA kernel error tracking for Joey LoRA tensor operations.
- **Cross-Platform Tooling**: Unified build scripts and workflows across Windows, macOS (arm64/x86_64), and Linux.

## [0.8.9] - 2026-10-30

### Fixed
- Fixed immutable variable reassignment bug in `src/std/joey.qk` by declaring `let mut result`.
- Optimized string concatenation in `interpreter.c` using bounded `memcpy` and `qk_malloc`.
- Added NULL body guard for match arms in `parser.c` to prevent corrupted match arm execution.

## [0.8.8] - 2026-10-30

### Fixed
- Implemented full interpreter shutdown cleanup in `interp_free()` to release global scope and return values.
- Added temporary evaluation result cleanup in REPL loop via `val_free(result)` to prevent unbounded memory growth.
- Added negative file size validation in `read_file()` to avoid invalid allocations on corrupted file descriptors.

## [0.8.7] - 2026-10-30

### Fixed
- Added string escape sequence processing (`\n`, `\t`, `\r`, `\\`, `\"`) in parser string literals.
- Guarded `block_expr` and `parser_parse` against appending NULL statement nodes on parse errors.
- Secured `env_snapshot` allocations with `qk_malloc` to prevent silent allocation failures.
- Added executable auto-detection in `tests/test_negative.sh` across standard build binaries.

## [0.8.6] - 2026-10-30 - Comprehensive 100-Bug Fix Release

### Memory & Allocation Safety
- Added `qk_malloc`, `qk_realloc`, and `qk_free` wrappers with out-of-memory fatal guards.
- Protected `val_some`, `val_ok`, and `val_err` against NULL pointer returns from `malloc`.
- Guarded `val_list` allocation against heap exhaustion.
- Added NULL checks on `Env` and `Binding` allocations in `env_define` and `env_snapshot`.
- Implemented `val_free` recursive destructor for strings, options, results, and lists.
- Fixed scope memory leak in `env_free` by recursively releasing all variable values.
- Replaced unchecked allocations in `match_expr.arms` with checked memory reallocations.
- Guarded argument array allocation in function call evaluation.

### Buffer Overflows & String Bounds
- Prevented hardware `SIGFPE` crash on signed 64-bit integer division overflow (`INT64_MIN / -1`).
- Prevented hardware `SIGFPE` crash on signed 64-bit integer modulo overflow (`INT64_MIN % -1`).
- Guarded unary negation against `INT64_MIN` signed integer overflow.
- Replaced unsafe `sprintf` calls with bounded `snprintf` in Joey TUI and GUI modules.
- Replaced unbounded `strcpy` with safe string copies in file path operations.
- Eliminated out-of-bounds byte read bug in function lookup error messages.
- Fixed trailing backslash advancing past EOF in lexer string scanner.
- Added LoRA rank clamp (maximum 128) in CUDA tensor kernels to prevent GPU stack overflow.
- Added strict bounds checking on command line assembly in `builtin_exec`.

### Standard Library & Type System
- Added `VAL_LIST` and `VAL_AST_NODE` handling in `val_type_name` to prevent undefined behavior.
- Added NULL pointer safety checks in `val_equal` and `val_string`.
- Fixed `builtin_print` to not emit trailing newlines, differentiating it from `builtin_println`.
- Fixed `builtin_input` to strip both `\r` and `\n` carriage returns on Windows.
- Expanded `builtin_and_then` to support monadic `Result` types (`Ok`/`Err`).
- Fixed `builtin_file_write` to verify full buffer write byte counts.
- Converted array and string indexing bounds checks to full 64-bit comparisons.
- Fixed `builtin_substring` to handle 64-bit bounds and check allocation success.
- Added `builtin_map_err` support to self-hosted evaluator dispatch.

### Tooling, Runtime & Infrastructure
- Fixed CLI `quokka run <file>` command-line parsing in `main.c`.
- Fixed `build.bat` missing Joey GUI dependencies and increased stack limit to 16MB.
- Dynamically resolved `CustomTkinter` path in `build_installer.bat`.
- Added return code verification in `joey_cli.c`.
- Added explicit UTF-8 encoding across Python job configuration loaders.
- Fixed syntax and indentation errors in installer helper scripts.
- Cleaned corrupted mixed UTF-16 bytes in `.gitignore`.
- Removed developer scratch notes from `parser.c`.
- Added CUDA error checking following kernel launch and synchronization.

## [0.8.9] - 2026-10-30

### Fixed
- Fixed immutable variable reassignment bug in `src/std/joey.qk` by declaring `let mut result`.
- Optimized string concatenation in `interpreter.c` using bounded `memcpy` and `qk_malloc`.
- Added NULL body guard for match arms in `parser.c` to prevent corrupted match arm execution.

## [0.8.8] - 2026-10-30

### Fixed
- Implemented full interpreter shutdown cleanup in `interp_free()` to release global scope and return values.
- Added temporary evaluation result cleanup in REPL loop via `val_free(result)` to prevent unbounded memory growth.
- Added negative file size validation in `read_file()` to avoid invalid allocations on corrupted file descriptors.

## [0.8.7] - 2026-10-30

### Fixed
- Added string escape sequence processing (`\n`, `\t`, `\r`, `\\`, `\"`) in parser string literals.
- Guarded `block_expr` and `parser_parse` against appending NULL statement nodes on parse errors.
- Secured `env_snapshot` allocations with `qk_malloc` to prevent silent allocation failures.
- Added executable auto-detection in `tests/test_negative.sh` across standard build binaries.

## [v0.5.0]
### Fixed
- Fixed check-self verification memory usage and memory limits (OOM/Stack Overflow) using a smaller target and larger stack.
- Re-enabled parsing of block expressions and full language features in Quokka self-hosted parser.
- Corrected check-self error handling and exit codes.
- Rewrote tests/test_negative.sh and bootstrap smoke tests.
- Updated documentation and README with correct claims and self-hosting verification logs.
- Fixed bootstrap command for cross-platform compatibility (Windows, macOS, Linux).

### Added
- quokka watch (alias quokka joey status): A cross-platform TUI monitor for live visualization of Joey (LoRA fine-tuning) jobs.
- Structured progress event emission over TCP (port 9001) from both Python and native Joey backends.
- Visual sparkline for loss, throughput tracking, VRAM monitoring (nvidia-smi), and ETA.
- Flat local file joey_history.log for training run history persistence.
