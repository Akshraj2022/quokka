#!/bin/sh
set -e

echo "Preparing small target for check-self..."
cat src/lexer.qka src/parser.qka > build/small_target.qka
echo 'parse(make_parser(lex(match file_read(sys_args[2]) { Ok(s) => s, Err(e) => "" })))' >> build/small_target.qka

echo "Running positive control for check-self..."
./build/qk_bootstrap.exe src/quokka_interpreter.qka check-self build/small_target.qka > build/positive.out 2>&1 || status=$?
if [ "${status:-0}" -ne 0 ]; then
    echo "Positive control failed! Output:"
    cat build/positive.out
    exit 1
fi
echo "Positive control passed."

echo "Running negative test for check-self..."
cp build/small_target.qka build/corrupted.qka

python -c "
with open('build/corrupted.qka', 'r', encoding='utf-8') as f:
    c = f.read()
c = c.replace('+', '-', 1)
with open('build/corrupted.qka', 'w', encoding='utf-8') as f:
    f.write(c)
"

set +e
./build/qk_bootstrap.exe src/quokka_interpreter.qka check-self build/corrupted.qka > build/negative.out 2>&1
status=$?
set -e

if [ $status -eq 0 ]; then
    echo "Negative test failed: expected non-zero exit code, got 0"
    cat build/negative.out
    exit 1
fi

if ! grep -q "MISMATCH" build/negative.out; then
    echo "Negative test failed: output did not contain MISMATCH"
    cat build/negative.out
    exit 1
fi

echo "Negative test passed: correctly failed and detected MISMATCH."
exit 0
