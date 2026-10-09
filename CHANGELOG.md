# Changelog

## [Unreleased]
### Fixed
- Fixed check-self verification to be non-circular and properly validate AST hashes across stages.
- Re-enabled parsing of block expressions and full language features in Quokka self-hosted parser.
- Corrected check-self error handling and exit codes.
- Updated documentation and README with correct claims and self-hosting verification logs.
- Fixed bootstrap command for cross-platform compatibility (Windows, macOS, Linux).

## [v0.5.0]
### Added
- quokka watch (alias quokka joey status): A cross-platform TUI monitor for live visualization of Joey (LoRA fine-tuning) jobs.
- Structured progress event emission over TCP (port 9001) from both Python and native Joey backends.
- Visual sparkline for loss, throughput tracking, VRAM monitoring (nvidia-smi), and ETA.
- Flat local file joey_history.log for training run history persistence.
