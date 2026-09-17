# Contributing to Quokka

First off, thank you for considering contributing to Quokka! It's people like you that make Quokka such a great language.

## Code of Conduct
By participating in this project, you are expected to uphold our Code of Conduct. Please be welcoming, inclusive, and respectful to all community members.

## How Can I Contribute?

### Reporting Bugs
If you find a bug, please create an issue on GitHub. Include:
* Your operating system and Quokka version.
* A minimal, reproducible example `.qk` script.
* The expected vs actual behavior.

### Suggesting Enhancements
We love discussing new language features! However, Quokka is strictly constrained by its design philosophy (deterministic, self-hosted, exception-free). Please open a Discussion or an Issue detailing your proposal before writing any code.

### Developing
1. **Fork the repository** and clone it locally.
2. **Compile the Stage-0 runner**: `gcc -std=c11 src/bootstrap/*.c -o build/qk_bootstrap.exe`
3. **Make your changes** in the `src/` directory (the canonical Quokka source).
4. **Bootstrap**: Run `quokka bootstrap` to regenerate the interpreter.
5. **Test**: Run `quokka test` and `quokka check-self` to ensure you haven't broken the deterministic bootstrap process.
6. **Submit a Pull Request**: Provide a clear description of the problem and your solution.

## Architectural Guidelines
* **No C additions unless strictly necessary**: Features should be implemented in Quokka (Stage-1). C is reserved only for low-level OS bridging (Stage-0).
* **Maintain Determinism**: Do not introduce any features that rely on undefined behaviors, unseeded random number generation without explicit monads, or unsafe thread sharing.
* **Joey ML Changes**: Python backend changes in `joey_backend.py` must maintain the strict JSON IPC protocol. No string-concatenated shell commands are permitted.

Thank you for contributing to Quokka!
