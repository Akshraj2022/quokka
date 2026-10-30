# Changelog

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

# Changelog

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
