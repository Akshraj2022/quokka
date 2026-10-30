#!/bin/sh
set -e

EXE=""
if [ "$(uname -s 2>/dev/null)" = "Linux" ] || [ "$(uname -s 2>/dev/null)" = "Darwin" ]; then
    if [ -f "./build/quokka" ]; then EXE="./build/quokka";
    elif [ -f "./build/quokka_linux" ]; then EXE="./build/quokka_linux";
    elif [ -f "./build/qk_bootstrap" ]; then EXE="./build/qk_bootstrap";
    fi
fi
if [ -z "$EXE" ]; then
    if [ -f "./build/quokka.exe" ]; then EXE="./build/quokka.exe";
    elif [ -f "./build/quokka" ]; then EXE="./build/quokka";
    elif [ -f "./build/quokka_standalone.exe" ]; then EXE="./build/quokka_standalone.exe";
    elif [ -f "./build/qk_bootstrap.exe" ]; then EXE="./build/qk_bootstrap.exe";
    elif [ -f "./build/qk_bootstrap" ]; then EXE="./build/qk_bootstrap";
    fi
fi

PYTHON="python3"
if ! command -v python3 >/dev/null 2>&1; then
    PYTHON="python"
fi

echo "Running negative test for check-self..."
mkdir -p build
cp src/quokka_interpreter.qka build/corrupted.qka

$PYTHON -c "
with open('build/corrupted.qka', 'r', encoding='utf-8') as f:
    c = f.read()
c = c.replace('+', '-', 1)
with open('build/corrupted.qka', 'w', encoding='utf-8') as f:
    f.write(c)
"

set +e
$EXE build/corrupted.qka check-self > build/negative.out 2>&1
status=$?
set -e

if [ $status -eq 0 ]; then
    echo "Negative test failed: expected non-zero exit code, got 0"
    cat build/negative.out
    exit 1
fi

echo "Negative test passed: check-self correctly failed on corrupted source (exit code $status)."
exit 0
