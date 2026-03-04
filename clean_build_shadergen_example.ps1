# Clean + rebuild helper for ShaderGen and BasicLitSunDirectionECS
# Run from repository root (e:\ULRE)

$ErrorActionPreference = 'Stop'

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Clean and Rebuild (ShaderGen + BasicLitSunDirectionECS)" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

if (-not (Test-Path "build")) {
    Write-Host "ERROR: build directory not found. Please configure CMake first." -ForegroundColor Red
    exit 1
}

$commands = @(
    @{ Name = "clean"; Cmd = "cmake --build build --config Debug --target clean" },
    @{ Name = "ULRE.ShaderGen"; Cmd = "cmake --build build --config Debug --target ULRE.ShaderGen -- /m" },
    @{ Name = "03_BasicLitSunDirectionECS"; Cmd = "cmake --build build --config Debug --target 03_BasicLitSunDirectionECS -- /m" }
)

foreach ($step in $commands) {
    Write-Host "[Step] $($step.Name)" -ForegroundColor Yellow
    Write-Host "[Cmd ] $($step.Cmd)" -ForegroundColor DarkGray

    Invoke-Expression $step.Cmd

    if ($LASTEXITCODE -ne 0) {
        Write-Host "FAILED at step: $($step.Name) (exit code: $LASTEXITCODE)" -ForegroundColor Red
        exit $LASTEXITCODE
    }

    Write-Host "OK: $($step.Name)" -ForegroundColor Green
    Write-Host ""
}

Write-Host "========================================" -ForegroundColor Green
Write-Host "All build steps completed successfully." -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
