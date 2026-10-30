@echo off
set QUOKKA_DIR=%~dp0
if exist "%QUOKKA_DIR%build\quokka_standalone.exe" (
    "%QUOKKA_DIR%build\quokka_standalone.exe" %*
) else if exist "%QUOKKA_DIR%build\quokka.exe" (
    "%QUOKKA_DIR%build\quokka.exe" %*
) else if exist "%QUOKKA_DIR%build\qk_bootstrap.exe" (
    "%QUOKKA_DIR%build\qk_bootstrap.exe" %*
) else (
    echo Error: Quokka executable not found. Run build.bat first.
    exit /b 1
)
