$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$rootDir = Split-Path -Parent $scriptDir

$exe = Join-Path $rootDir "build\quokka.exe"
if (-not (Test-Path $exe)) {
    $exe = Join-Path $rootDir "build\qk_bootstrap.exe"
}
if (-not (Test-Path $exe)) {
    Write-Host "Please build first (build\quokka.exe not found)."
    exit 1
}

$testPattern = Join-Path $scriptDir "conformance\*.qk"
$tests = Get-ChildItem $testPattern
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
