param(
    # 开发形态构建树：含 ShaderResourceSchemaRegressionGate（只能在生成器 ON 的树里构建）
    [string]$DevBuildDir  = "build",
    # 发行形态构建树：ULRE_ENABLE_SHADERGEN=OFF，树内不存在 ULRE.ShaderGen
    [string]$ProdBuildDir = "build_probe_nosg",
    [string]$Config       = "Debug",
    [int]$Jobs            = 8,
    [string]$FixtureDir   = "build/release-cache-fixture",
    [switch]$SkipConfigure,
    [switch]$SkipBake,
    [switch]$SkipBuild
)

# ─────────────────────────────────────────────────────────────────────────────
# ShaderGen 物理切割的**发行形态验收**（2026-10）。
#
# 判据不是「代码分成了两个 target」，而是这条跨进程链路能真跑通：
#
#   [开发形态]  ULRE.ShaderGen 在场
#        └─ RegressionGate emit-release-cache  → 烘焙 program_manifest.pmf + stage SPV
#   [发行形态]  ULRE.ShaderGen 不存在于链接图
#        └─ RuntimeFormProbe（只链 ULRE::Runtime）→ 只读查表 + 装配 build context
#
# 三步各自能独立失败，且失败原因互不相同：
#   1) 烘焙失败      → 生成期数据面写不出来（manifest schema / 语义键 / stage 落盘）
#   2) 探针链接失败  → 运行时反向引用了生成器符号（LNK2019）
#   3) 探针运行失败  → 发行期算不出同一个语义键，或清单装载/校验不一致
#
# 注意：探针**不能**注册成 ctest。夹具不是构建产物，在干净树里 ctest 只会恒红。
# 用法：
#   pwsh scripts/ci/verify-release-form.ps1
#   pwsh scripts/ci/verify-release-form.ps1 -SkipBuild    # 复用已有两棵树产物
# ─────────────────────────────────────────────────────────────────────────────

$ErrorActionPreference = "Continue"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot  = (Resolve-Path (Join-Path $scriptDir "..\..")).Path
$devPath   = Join-Path $repoRoot $DevBuildDir
$prodPath  = Join-Path $repoRoot $ProdBuildDir
$fixture   = Join-Path $repoRoot $FixtureDir
$logDir    = Join-Path $devPath "release-form-logs"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

$stamp    = Get-Date -Format "yyyyMMdd-HHmmss"
$log      = Join-Path $logDir "verify-$Config-$stamp.log"
$failures = New-Object System.Collections.Generic.List[string]

function Write-Step([string]$text)
{
    Write-Host ""
    Write-Host "==> $text" -ForegroundColor Cyan
    Add-Content -Path $log -Value "==> $text"
}

function Invoke-Logged([string]$title, [scriptblock]$action)
{
    $out  = & $action 2>&1
    $code = $LASTEXITCODE
    Add-Content -Path $log -Value $out
    if ($code -ne 0)
    {
        $failures.Add("$title (exit $code)")
        Write-Host "[FAIL] $title (exit $code)" -ForegroundColor Red
        $out | Select-Object -Last 25 | ForEach-Object { Write-Host "       $_" }
        return $false
    }
    Write-Host "[ OK ] $title" -ForegroundColor Green
    return $true
}

function Invoke-Verification
{
    Write-Host "verify-release-form" -ForegroundColor White
    Write-Host "  dev tree  : $devPath"
    Write-Host "  prod tree : $prodPath"
    Write-Host "  fixture   : $fixture"
    Add-Content -Path $log -Value "dev=$devPath prod=$prodPath fixture=$fixture config=$Config"

    # ── [1/5] 配置两棵树 ─────────────────────────────────────────────────────
    if ($SkipConfigure)
    {
        Write-Step "[1/5] configure skipped"
    }
    else
    {
        Write-Step "[1/5] configure dev + prod trees"

        if (-not (Test-Path (Join-Path $devPath "CMakeCache.txt")))
        {
            if (-not (Invoke-Logged "configure dev tree (ULRE_ENABLE_SHADERGEN=ON)" {
                        cmake -S $repoRoot -B $devPath -DULRE_ENABLE_SHADERGEN=ON }))
            { return }
        }

        if (-not (Test-Path (Join-Path $prodPath "CMakeCache.txt")))
        {
            if (-not (Invoke-Logged "configure prod tree (ULRE_ENABLE_SHADERGEN=OFF)" {
                        cmake -S $repoRoot -B $prodPath -DULRE_ENABLE_SHADERGEN=OFF }))
            { return }
        }
    }

    $gateExe  = Join-Path $devPath  "out\Windows_64_$Config\ShaderResourceSchemaRegressionGate.exe"
    $probeExe = Join-Path $prodPath "out\Windows_64_$Config\RuntimeFormProbe.exe"

    # ── [2/5] 开发形态：构建回归门 ───────────────────────────────────────────
    if ($SkipBuild -and (Test-Path $gateExe))
    {
        Write-Step "[2/5] gate build skipped"
    }
    else
    {
        Write-Step "[2/5] build regression gate in dev tree"
        if (-not (Invoke-Logged "build ShaderResourceSchemaRegressionGate" {
                    cmake --build $devPath --config $Config --target ShaderResourceSchemaRegressionGate -- /m:$Jobs /nodeReuse:false /nologo /v:m }))
        { return }
    }

    if (-not (Test-Path $gateExe))
    {
        $failures.Add("gate executable missing: $gateExe")
        return
    }

    # ── [3/5] 开发形态：烘焙发行形态夹具 ─────────────────────────────────────
    if ($SkipBake)
    {
        Write-Step "[3/5] fixture bake skipped"
    }
    else
    {
        Write-Step "[3/5] bake release-form fixture (dev tree, generator present)"
        if (Test-Path $fixture) { Remove-Item -Recurse -Force $fixture -ErrorAction SilentlyContinue }

        Push-Location $repoRoot
        $baked = Invoke-Logged "emit-release-cache $FixtureDir" {
            & $gateExe emit-release-cache $FixtureDir }
        Pop-Location

        if (-not $baked) { return }
    }

    # ── [4/5] 发行形态：构建探针（链接即断言）────────────────────────────────
    if ($SkipBuild -and (Test-Path $probeExe))
    {
        Write-Step "[4/5] probe build skipped"
    }
    else
    {
        Write-Step "[4/5] build runtime probe in prod tree (link is the assertion)"
        if (-not (Invoke-Logged "build RuntimeFormProbe (ULRE::Runtime only)" {
                    cmake --build $prodPath --config $Config --target RuntimeFormProbe -- /m:$Jobs /nodeReuse:false /nologo /v:m }))
        { return }
    }

    if (-not (Test-Path $probeExe))
    {
        $failures.Add("probe executable missing: $probeExe")
        return
    }

    # 发行形态树里不该有任何 ULRE.ShaderGen 产物；出现即意味着 OFF 开关失效。
    $sgArtifacts = Get-ChildItem $prodPath -Recurse -Filter "ULRE.ShaderGen.*" -File -ErrorAction SilentlyContinue
    if ($sgArtifacts)
    {
        $failures.Add("prod tree contains ULRE.ShaderGen artifacts")
        foreach ($item in $sgArtifacts)
        {
            $failures.Add("  " + $item.FullName)
            Write-Host "[FAIL] $($item.FullName)" -ForegroundColor Red
        }
        return
    }
    Write-Host "[ OK ] prod tree has no ULRE.ShaderGen artifacts" -ForegroundColor Green

    # ── [5/5] 发行形态：跑探针 ───────────────────────────────────────────────
    Write-Step "[5/5] run runtime probe in prod tree"
    Push-Location $repoRoot
    $ran = Invoke-Logged "RuntimeFormProbe $FixtureDir" {
        & $probeExe $FixtureDir }
    Pop-Location
    if (-not $ran) { return }
}

Invoke-Verification

Write-Host ""
if ($failures.Count -eq 0)
{
    Write-Host "[PASS] release form verified: baked cache resolved with zero generator symbols" -ForegroundColor Green
    Add-Content -Path $log -Value "[PASS] release form verified"
    exit 0
}

Write-Host "[FAIL] release form verification: $($failures.Count) problem(s)" -ForegroundColor Red
foreach ($item in $failures)
{
    Write-Host "  - $item"
    Add-Content -Path $log -Value "  - $item"
}
Write-Host "log: $log"
exit 1
