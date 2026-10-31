#!/usr/bin/env bash
# ============================================================
#  Quokka Language -- Build Script (Linux & macOS)
#  Requires GCC or Clang
# ============================================================
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$DIR"

mkdir -p build

CC="${CC:-gcc}"
if ! command -v "$CC" >/dev/null 2>&1; then
    if command -v clang >/dev/null 2>&1; then
        CC="clang"
    else
        echo "Error: neither gcc nor clang found in PATH."
        exit 1
    fi
fi

OS="$(uname -s)"
CFLAGS="-std=c11 -Wall -Wextra -Wno-unused-parameter -O2"
LIBS="-lm -lpthread -ldl"

echo "Building Quokka for $OS using $CC..."

$CC $CFLAGS \
    -o build/quokka \
    src/bootstrap/main.c \
    src/bootstrap/lexer.c \
    src/bootstrap/parser.c \
    src/bootstrap/interpreter.c \
    src/bootstrap/sha256.c \
    src/bootstrap/concurrency.c \
    src/bootstrap/typecheck.c \
    src/bootstrap/compiler.c \
    src/bootstrap/vm.c \
    src/joey/joey_native.c \
    src/joey/joey_watch.c \
    src/joey/joey_gui.c \
    $LIBS

echo ""
echo "Build successful: build/quokka"
echo ""
echo "Usage:"
echo "  build/quokka                    Start REPL"
echo "  build/quokka examples/hello.qk  Run a .qk file"
