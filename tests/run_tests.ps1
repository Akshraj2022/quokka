$ErrorActionPreference = "Stop"
$exe = "..\build\qk_bootstrap.exe"
if (-not (Test-Path $exe)) {
    Write-Host "Please build first."
    exit 1
}

$tests = Get-ChildItem ".\conformance\*.qk"
$all_passed = $true

foreach ($t in $tests) {
    Write-Host "Running $($t.Name)... " -NoNewline
    $output = & $exe $t.FullName 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "FAILED" -ForegroundColor Red
        Write-Host $output
        $all_passed = $false
    } else {
        Write-Host "OK" -ForegroundColor Green
    }
}

if ($all_passed) {
    Write-Host "All tests passed!" -ForegroundColor Green
} else {
    Write-Host "Some tests failed." -ForegroundColor Red
    exit 1
}
