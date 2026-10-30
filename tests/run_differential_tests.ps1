$ErrorActionPreference = "Stop"

$scriptDir = $PSScriptRoot
$rootDir = Split-Path -Parent $scriptDir

$quokka_exe = Join-Path $rootDir "build\quokka.exe"
if (-not (Test-Path $quokka_exe)) { $quokka_exe = Join-Path $rootDir "build\qk_bootstrap.exe" }
$self_hosted = Join-Path $rootDir "src\quokka_interpreter.qka"

Write-Host "Running Differential Tests..."
Write-Host "==========================="

$tests = Get-ChildItem (Join-Path $scriptDir "conformance\*.qk")

foreach ($test in $tests) {
    Write-Host "Testing $($test.Name)..." -NoNewline
    
    # 1. Run via C stage-0 interpreter
    $c_out = & $quokka_exe $test.FullName 2>&1 | Out-String
    
    # 2. Run via Quokka-written interpreter
    # We would normally do: & $quokka_exe $self_hosted $test.FullName
    # Currently, our self-hosted interpreter runs the hardcoded `source` inside main().
    # So we simulate the differential check here for the architecture.
    
    $qk_out = & $quokka_exe $self_hosted run $test.FullName 2>&1 | Out-String
    
    if ($c_out.Trim() -eq $qk_out.Trim()) {
        Write-Host " MATCH" -ForegroundColor Green
    } else {
        Write-Host " DIFFERENCE DETECTED" -ForegroundColor Red
        Write-Host "--- Stage-0 C Output ---"
        Write-Host $c_out.Trim()
        Write-Host "--- Self-Hosted Quokka Output ---"
        Write-Host $qk_out.Trim()
        Write-Host "---------------------------------"
    }
}
