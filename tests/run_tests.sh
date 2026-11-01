#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$DIR/.." && pwd)"

EXE=""
if [ -f "$ROOT/build/quokka.exe" ]; then
    EXE="$ROOT/build/quokka.exe"
elif [ -f "$ROOT/quokka.exe" ]; then
    EXE="$ROOT/quokka.exe"
elif [ -f "$ROOT/build/quokka" ]; then
    EXE="$ROOT/build/quokka"
elif [ -f "$ROOT/build/quokka_linux" ]; then
    EXE="$ROOT/build/quokka_linux"
elif [ -f "$ROOT/build/qk_bootstrap" ]; then
    EXE="$ROOT/build/qk_bootstrap"
fi
if [ ! -f "$EXE" ]; then
    echo "Executable not found. Please build first."
    exit 1
fi

FAILED=0
for test_file in "$ROOT"/tests/conformance/*.qk; do
    name="$(basename "$test_file")"
    printf "Running %s... " "$name"
    if "$EXE" "$test_file" > /dev/null 2>&1; then
        echo "OK"
    else
        echo "FAILED"
        FAILED=1
    fi
done

for ex_file in "$ROOT"/examples/*.qk; do
    name="$(basename "$ex_file")"
    printf "Running example %s... " "$name"
    if "$EXE" "$ex_file" > /dev/null 2>&1; then
        echo "OK"
    else
        echo "FAILED"
        FAILED=1
    fi
done

if [ $FAILED -ne 0 ]; then
    echo "Some tests failed!"
    exit 1
fi

if command -v python3 >/dev/null 2>&1; then
    python3 "$ROOT/tests/test_100_ways.py"
elif command -v python >/dev/null 2>&1; then
    python "$ROOT/tests/test_100_ways.py"
fi

echo "All tests passed successfully!"
exit 0
