#!/bin/sh
set -e
echo "Running negative test for check-self..."
cp src/quokka_interpreter.qka /tmp/corrupted.qka
# Corrupt one token: change a "let" to "lex"
sed -i 's/let /lex /g' /tmp/corrupted.qka

# Run check-self on corrupted file
# Disable set -e temporarily because we expect failure
set +e
./build/qk_bootstrap /tmp/corrupted.qka check-self > /dev/null 2>&1
status=$?
set -e

if [ $status -eq 0 ]; then
    echo "Negative test failed: check-self should have returned non-zero exit code on corrupted file."
    exit 1
else
    echo "Negative test passed: check-self correctly failed on corrupted source (exit code $status)."
    exit 0
fi
