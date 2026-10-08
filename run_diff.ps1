$fail = $false
foreach ($f in Get-ChildItem -Path "examples/*.qk", "tests/conformance/*.qk") {
    Write-Output "Testing $($f.Name)..."
    ./build/qk_bootstrap.exe $f.FullName > $env:TEMP/c_out.txt 2>&1
    ./build/qk_bootstrap.exe src/quokka_interpreter.qka run $f.FullName > $env:TEMP/qk_out.txt 2>&1
    
    $c_out = Get-Content $env:TEMP/c_out.txt -Raw
    $qk_out = Get-Content $env:TEMP/qk_out.txt -Raw
    if ($c_out -ne $qk_out) {
        Write-Output "DIFFERENCE in $($f.Name)!"
        Write-Output "C out: $c_out"
        Write-Output "Q out: $qk_out"
        $fail = $true
    }
}
if ($fail) { exit 1 }
exit 0
