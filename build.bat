@echo off
REM ============================================================
REM  Quokka Language — Build Script (Windows)
REM  Requires GCC (MinGW-w64 / WinLibs)
REM ============================================================

echo Building Quokka...

if not exist "build" mkdir build

gcc -std=c11 -Wall -Wextra -Wno-unused-parameter ^
    -o build\quokka.exe ^
    src\bootstrap\main.c src\bootstrap\lexer.c src\bootstrap\parser.c src\bootstrap\interpreter.c src\bootstrap\sha256.c src\joey\joey_native.c src\joey\joey_watch.c -lws2_32

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Build successful: build\quokka.exe
    echo.
    echo Usage:
    echo   build\quokka                    Start REPL
    echo   build\quokka examples\hello.qk  Run a .qk file
) else (
    echo.
    echo Build FAILED.
    exit /b 1
)
