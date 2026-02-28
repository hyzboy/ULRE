param(
    [string]$BuildDir = "build/windows-msvc-debug",
    [string]$Config = "Debug",
    [string]$ArtifactDir = ""
)

$ErrorActionPreference = "Stop"

Write-Host "[Gate] BuildDir=$BuildDir Config=$Config"

if ([string]::IsNullOrWhiteSpace($ArtifactDir)) {
    $ArtifactDir = Join-Path $BuildDir "gate-artifacts"
}
New-Item -ItemType Directory -Force -Path $ArtifactDir | Out-Null

$allOutput = New-Object System.Collections.Generic.List[string]

$targets = @(
    "test_FSHelperConsistencyValidation",
    "test_DescriptorSetLifecycleRegression",
    "test_ShaderLogicValidation",
    "test_BridgeValidation3Materials",
    "test_HelperInjectionConflict",
    "test_HelperInjectionConflictMatrix",
    "test_ComposedDiagnosticsAggregation",
    "test_TextureBlinnPhongTemplateConformance"
)

Write-Host "[Gate] Building targets..."
cmake --build $BuildDir --config $Config --target $targets -j 8
if ($LASTEXITCODE -ne 0) {
    throw "[Gate] Build failed"
}

Write-Host "[Gate] Running focused ctest set..."
Push-Location $BuildDir
$prevErrorAction = $ErrorActionPreference
$ErrorActionPreference = "Continue"
$ctestPattern = 'test_(FSHelperConsistencyValidation|DescriptorSetLifecycleRegression|ShaderLogicValidation|BridgeValidation3Materials|HelperInjectionConflict|HelperInjectionConflictMatrix|ComposedDiagnosticsAggregation|TextureBlinnPhongTemplateConformance)'
$ctestCommand = 'ctest -C {0} -R "{1}" --output-on-failure' -f $Config, $ctestPattern
$ctestOutput = & cmd /c $ctestCommand 2>&1
$ctestExit = $LASTEXITCODE
$ErrorActionPreference = $prevErrorAction
$ctestText = ($ctestOutput | Out-String)
$ctestOutput
$ctestOutput | ForEach-Object { $allOutput.Add($_.ToString()) }
Pop-Location

if ($ctestText -match "No tests were found") {
    Write-Host "[Gate] ctest has no registered tests; fallback to direct test executables..."

    foreach ($target in $targets) {
        $exe = Get-ChildItem -Path $BuildDir -Recurse -File -Filter "$target.exe" | Select-Object -First 1
        if (-not $exe) {
            throw "[Gate] Cannot find executable for $target"
        }

        Write-Host "[Gate] Running $($exe.FullName)"
        $exeOutput = & $exe.FullName 2>&1
        $exeExit = $LASTEXITCODE
        $exeOutput
        $exeOutput | ForEach-Object { $allOutput.Add($_.ToString()) }
        if ($exeExit -ne 0) {
            throw "[Gate] $target failed with exit code $exeExit"
        }
    }
}
elseif ($ctestExit -ne 0) {
    throw "[Gate] Tests failed via ctest"
}
else {
    Write-Host "[Gate] Capturing diagnostics from composed aggregation test (verbose)..."
    Push-Location $BuildDir
    $prevErrorAction = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $diagOutput = & cmd /c ('ctest -C {0} -R "^test_ComposedDiagnosticsAggregation$" -V' -f $Config) 2>&1
    $diagExit = $LASTEXITCODE
    $ErrorActionPreference = $prevErrorAction
    $diagOutput
    $diagOutput | ForEach-Object { $allOutput.Add($_.ToString()) }
    Pop-Location

    if ($diagExit -ne 0) {
        throw "[Gate] Failed to capture composed diagnostics output"
    }
}

$rawLogPath = Join-Path $ArtifactDir "gate-output.log"
$allOutput | Set-Content -Path $rawLogPath -Encoding UTF8

$diagnosticsPath = Join-Path $ArtifactDir "composed-diagnostics.jsonl"
$diagnosticsLines = New-Object System.Collections.Generic.List[string]
foreach ($line in $allOutput) {
    if ($line -match '\[ComposedBusiness\]\[Diagnostics\]\s*(\{.*\})\s*$') {
        $diagnosticsLines.Add($matches[1])
    }
}

if ($diagnosticsLines.Count -gt 0) {
    $diagnosticsLines | Set-Content -Path $diagnosticsPath -Encoding UTF8
} else {
    "" | Set-Content -Path $diagnosticsPath -Encoding UTF8
}

Write-Host "[Gate] Artifact log: $rawLogPath"
Write-Host "[Gate] Artifact diagnostics: $diagnosticsPath (count=$($diagnosticsLines.Count))"

Write-Host "[Gate] PASS"
