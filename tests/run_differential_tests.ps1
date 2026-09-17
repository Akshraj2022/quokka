$ErrorActionPreference = "Stop"
$quokka_exe = "..\build\qk_bootstrap.exe"
$self_hosted = "..\src\quokka_interpreter.qk"

Write-Host "Running Differential Tests..."
Write-Host "==========================="

# Note: In a real differential test, we would load the self-hosted interpreter 
# and have it execute the target script instead of running main().
# Since our self-hosted evaluator is a stub (effort 0.5), we will set up the harness
# but note that the self-hosted interpreter currently just parses standard input or a hardcoded script.

$tests = Get-ChildItem "conformance\*.qk"

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
