#!/bin/bash
FAIL=0
for f in examples/*.qk tests/conformance/*.qk; do
    echo "Testing $f..."
    ./build/qk_bootstrap "$f" > /tmp/c_out 2>&1
    ./build/qk_bootstrap src/quokka_interpreter.qka run "$f" > /tmp/qk_out 2>&1
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
