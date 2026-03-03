param(
    [string]$BuildDir = "build",
    [string]$Config = "Release",
    [string]$OutputDir = "shadergen_materialpreset_dump",
    [string]$Generator = "",
    [switch]$CleanBuildDir
)

$ErrorActionPreference = "Stop"

function Write-Step([string]$msg) {
    Write-Host "`n========== $msg ==========" -ForegroundColor Cyan
}

function Fail([string]$msg) {
    Write-Host "[FAIL] $msg" -ForegroundColor Red
    exit 1
}

function Resolve-ExePath([string]$dir, [string]$cfg) {
    $candidates = @(
        (Join-Path $dir "test\$cfg\test_MaterialPresetExhaustiveCompile.exe"),
        (Join-Path $dir "test_MaterialPresetExhaustiveCompile.exe"),
        (Join-Path $dir "$cfg\test_MaterialPresetExhaustiveCompile.exe")
    )

    foreach ($path in $candidates) {
        if (Test-Path $path) { return $path }
    }

    return $null
}

Write-Step "Environment Check"
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Fail "cmake not found in PATH"
}

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $root
Write-Host "Root: $root"

if ($CleanBuildDir -and (Test-Path $BuildDir)) {
    Write-Step "Clean Build Directory"
    Remove-Item -Recurse -Force $BuildDir
}

Write-Step "Configure"
$configureArgs = @("-S", ".", "-B", $BuildDir)
if ($Generator -and $Generator.Trim().Length -gt 0) {
    $configureArgs += @("-G", $Generator)
}

Write-Host "cmake $($configureArgs -join ' ')"
& cmake @configureArgs
if ($LASTEXITCODE -ne 0) { Fail "cmake configure failed" }

Write-Step "Build Target"
$buildArgs = @("--build", $BuildDir, "--config", $Config, "--target", "test_MaterialPresetExhaustiveCompile")
Write-Host "cmake $($buildArgs -join ' ')"
& cmake @buildArgs
if ($LASTEXITCODE -ne 0) { Fail "cmake build failed" }

Write-Step "Run Exhaustive Test"
$exe = Resolve-ExePath -dir $BuildDir -cfg $Config
if (-not $exe) {
    Fail "cannot find test_MaterialPresetExhaustiveCompile.exe under $BuildDir"
}

$outAbs = Join-Path $root $OutputDir
if (-not (Test-Path $outAbs)) {
    New-Item -ItemType Directory -Path $outAbs | Out-Null
}

Write-Host "Executable: $exe"
Write-Host "OutputDir : $outAbs"
& $exe $outAbs
$testExit = $LASTEXITCODE

Write-Step "Validate Output"
$summaryTxt = Join-Path $outAbs "summary.txt"
$summaryCsv = Join-Path $outAbs "summary.csv"

if (-not (Test-Path $summaryTxt)) { Fail "missing summary.txt at $summaryTxt" }
if (-not (Test-Path $summaryCsv)) { Fail "missing summary.csv at $summaryCsv" }

Write-Host "summary.txt:"
Get-Content $summaryTxt

$kv = @{}
Get-Content $summaryTxt | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') {
        $kv[$matches[1]] = $matches[2]
    }
}

$total = [int]($kv["total_cases"] | ForEach-Object { if ($_){$_} else {"0"} })
$createFail = [int]($kv["create_fail_count"] | ForEach-Object { if ($_){$_} else {"0"} })
$compileFail = [int]($kv["compile_fail_count"] | ForEach-Object { if ($_){$_} else {"0"} })
$mirrorFail = [int]($kv["mirror_fail_count"] | ForEach-Object { if ($_){$_} else {"0"} })
$diffMismatch = [int]($kv["diff_mismatch_count"] | ForEach-Object { if ($_){$_} else {"0"} })

Write-Step "Diff Summary"
Write-Host "total_cases=$total"
Write-Host "create_fail_count=$createFail"
Write-Host "compile_fail_count=$compileFail"
Write-Host "mirror_fail_count=$mirrorFail"
Write-Host "diff_mismatch_count=$diffMismatch"

if ($testExit -ne 0 -or $createFail -ne 0 -or $compileFail -ne 0 -or $mirrorFail -ne 0) {
    Fail "exhaustive compile reported failure(s), see $summaryTxt and $summaryCsv"
}

Write-Host "`n[PASS] exhaustive compile completed successfully. Output: $outAbs" -ForegroundColor Green
exit 0
