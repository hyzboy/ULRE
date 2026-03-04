param(
    [ValidateSet('smoke','extended')]
    [string]$Mode = 'smoke',

    [string]$BuildDir = 'build/windows-msvc-debug',

    [ValidateSet('Debug','Release')]
    [string]$Config = 'Debug'
)

$ErrorActionPreference = 'Stop'

$target = if ($Mode -eq 'extended') {
    'test_ShaderGenProfileParityExtended'
} else {
    'test_ShaderGenProfileParitySmoke'
}

Write-Host "[ShaderGenParity] Mode   : $Mode" -ForegroundColor Cyan
Write-Host "[ShaderGenParity] Target : $target" -ForegroundColor Cyan
Write-Host "[ShaderGenParity] Build  : $BuildDir ($Config)" -ForegroundColor Cyan

if (-not (Test-Path $BuildDir)) {
    Write-Error "Build directory not found: $BuildDir"
}

$cmd = @('cmake', '--build', $BuildDir, '--config', $Config, '--target', $target)
Write-Host "[ShaderGenParity] Command: $($cmd -join ' ')" -ForegroundColor DarkGray

& $cmd[0] $cmd[1..($cmd.Length-1)]
if ($LASTEXITCODE -ne 0) {
    throw "Parity run failed with exit code $LASTEXITCODE"
}

$reportPath = if ($Mode -eq 'extended') {
    'doc/shader-system/baseline/shadergen_profile_parity_extended_latest.json'
} else {
    'doc/shader-system/baseline/shadergen_profile_parity_latest.json'
}

Write-Host "[ShaderGenParity] PASS" -ForegroundColor Green
Write-Host "[ShaderGenParity] Report : $reportPath" -ForegroundColor Green
