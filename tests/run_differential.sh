#!/bin/bash
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$DIR/.." && pwd)"
cd "$ROOT"

EXE=""
if [ -f "./build/quokka" ]; then EXE="./build/quokka";
elif [ -f "./build/quokka_linux" ]; then EXE="./build/quokka_linux";
elif [ -f "./build/quokka.exe" ]; then EXE="./build/quokka.exe";
elif [ -f "./build/qk_bootstrap" ]; then EXE="./build/qk_bootstrap";
fi

if [ -z "$EXE" ]; then
    echo "Quokka binary not found. Please build first."
    exit 1
fi

FAIL=0
for f in tests/conformance/*.qk; do
    echo "Testing $f..."
    "$EXE" "$f" > /tmp/c_out 2>&1
    "$EXE" src/quokka_interpreter.qka run "$f" > /tmp/qk_out 2>&1
    if ! cmp -s /tmp/c_out /tmp/qk_out; then
        echo "DIFFERENCE in $f!"
        diff -u /tmp/c_out /tmp/qk_out
        FAIL=1
    fi
done
if [ $FAIL -ne 0 ]; then
    echo "Differential testing failed!"
    exit 1
fi
echo "Differential testing passed!"
exit 0
