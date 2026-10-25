param(
    # 构建树：需已构建 ShaderDocumentProductionRegression（含 --dump-source）
    [string]$BuildDir = "build",
    [string]$Config   = "Debug",
    # spvfc 不在根构建树内，只在 GLSLCompiler 预置 Release 产物里
    [string]$Spvfc    = "src/Tools/GLSLCompiler/build/out/Windows_64_Release/spvfc.exe",
    # 跳过重新导出旧路径参考文本（复用上次结果）
    [switch]$SkipDump
)

$ErrorActionPreference = 'Stop'

# ─────────────────────────────────────────────────────────────────────────────
# 物理固定主着色器（ShaderLibrary/domain/**）与旧 ShaderGen 产物的**等价性验收**。
#
# 背景：阶段五要物理删除 src/ShaderGen。删除之前必须证明「新的固定主着色器 + 极简
#       options.glsl」编译出的东西，和旧生成器那条路编译出的东西是同一个着色器。
#
# 判据（踩过坑，别改）：
#   * SPIR-V **逐字节比较无效**——同一份源码编译两次都差 ~50 字节（内嵌内容哈希与
#     标识符名），而不同但等价的源码差数千字节。字节差异量不携带任何信息。
#   * 因此比较 **reflection 文本**（稳定）：等价源码的 reflection 完全一致。
#   * 再加一道 **SPIR-V 字节数全等**：能抓住 reflection 看不见的偏差
#     （例如多声明一个未被使用的 varying / 多一个未用常量）。
#
# 参考文本的来源：ShaderDocumentProductionRegression --dump-source，它导出的是
# `GetFinalGLSL()`——即 glslang 实际吃到的最终文本，不是预处理期的半成品。
#
# 用法：pwsh -File scripts/ci/verify-domain-master-shaders.ps1
# 退出码 0 = 全部等价。
# ─────────────────────────────────────────────────────────────────────────────

$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
Set-Location $root

$work    = Join-Path $root 'build/domain-master-equiv'
$refDir  = Join-Path $work 'ref'
$outDir  = Join-Path $work 'out'
New-Item -ItemType Directory -Force $work, $refDir, $outDir | Out-Null

$regExe = Join-Path $root "$BuildDir/out/Windows_64_$Config/ShaderDocumentProductionRegression.exe"
if (-not (Test-Path $regExe)) {
    throw "找不到 $regExe —— 先构建 ShaderDocumentProductionRegression。"
}
if (-not (Test-Path (Join-Path $root $Spvfc))) {
    throw "找不到 $Spvfc —— spvfc 不在根构建树内，需单独构建 src/Tools/GLSLCompiler。"
}
$spvfc = Join-Path $root $Spvfc

# ── 1. 导出旧路径参考文本 ──
if (-not $SkipDump) {
    $env:ULRE_SHADER_CACHE_PATH = Join-Path $work 'cache'
    $dumpLog = & $regExe --full --dump-source $refDir 2>&1
    if ($LASTEXITCODE -ne 0) {
        $dumpLog | Select-Object -Last 30 | Write-Host
        throw "ShaderDocumentProductionRegression --dump-source 失败（exit=$LASTEXITCODE）。"
    }
}

# ── 2. 每个 pass 形状的 options.glsl ──
# **由生产者自己生成**：DomainOptionsRegression --write-options 直接调用
# EmitDomainOptions（与将来运行时同一条代码路径），脚本不再手抄宏表 ——
# 手抄的表会和发射器悄悄漂移，等价性验收就失去意义。
$optExe = Join-Path $root "$BuildDir/out/Windows_64_$Config/DomainOptionsRegression.exe"
if (-not (Test-Path $optExe)) {
    throw "找不到 $optExe —— 先构建 DomainOptionsRegression。"
}
$variants = & $optExe --write-options $work 2>&1
if ($LASTEXITCODE -ne 0) {
    $variants | Select-Object -Last 20 | Write-Host
    throw "DomainOptionsRegression --write-options 失败（exit=$LASTEXITCODE）。"
}

# ── 3. 编译 + 比对 ──
# 主着色器：两条 -I 根（ShaderLibrary + 本形状的 generated 父目录）
# 参考文本：无 generated/options.glsl 的 include，多给一条根无副作用
function Get-Reflection([string]$src, [string]$profile, [string]$out) {
    $gen = Join-Path $work $profile
    $txt = & $spvfc -I ShaderLibrary -I $gen -o $out $src 2>&1
    if ($LASTEXITCODE -ne 0) { throw "编译失败：$src（profile=$profile）" }
    return ($txt | Where-Object { $_ -notmatch '^=== Reflection:|^\[spvfc\] (Compiling|Written)' })
}

$cases = @(
    @('ShaderLibrary/domain/standard_opaque/forward_lit.frag',          'fwd',           'lit-forward.frag'),
    @('ShaderLibrary/domain/standard_opaque/forward_lit.frag',          'fwd-alpha',     'lit-forward-alpha.frag'),
    @('ShaderLibrary/domain/standard_opaque/shadow_caster.frag',        'shadow',        'lit-shadow.frag'),
    @('ShaderLibrary/domain/standard_opaque/shadow_caster_masked.frag', 'shadow-masked', 'lit-shadow-alpha.frag'),
    @('ShaderLibrary/domain/sky/sky.frag',                              'sky',           'sky-cube.frag')
)

$failed = 0
$newSpv  = Join-Path $outDir 'new.spv'
$refSpv  = Join-Path $outDir 'ref.spv'
foreach ($c in $cases) {
    $master   = $c[0]; $profile = $c[1]
    $refSrc   = Join-Path $refDir $c[2]
    $label    = "$master  [$profile]"
    if (-not (Test-Path $refSrc)) {
        Write-Host "FAIL $label  参考文本缺失：$($c[2])（--dump-source 未产出该 fixture？）"
        $failed++
        continue
    }
    try {
        $a = Get-Reflection $master $profile $newSpv
        $b = Get-Reflection $refSrc $profile $refSpv
        $diff = Compare-Object $a $b
        if ($diff) {
            Write-Host "FAIL $label  reflection 差异行数=$($diff.Count)"
            $diff | Select-Object -First 12 | Format-Table -AutoSize | Out-String | Write-Host
            $failed++
            continue
        }
        $newLen = (Get-Item $newSpv).Length
        $refLen = (Get-Item $refSpv).Length
        if ($newLen -ne $refLen) {
            Write-Host "FAIL $label  reflection 相同但 SPIR-V 尺寸不同：new=$newLen ref=$refLen"
            $failed++
            continue
        }
        Write-Host "PASS $label  reflection=$($a.Count) 行  spv=$newLen 字节"
    } catch {
        Write-Host "FAIL $label  $_"
        $failed++
    }
}

Write-Host ""
if ($failed -eq 0) {
    Write-Host "全部通过：$($cases.Count) 个 pass 形状与旧 ShaderGen 产物等价。"
    exit 0
}
Write-Host "失败 $failed / $($cases.Count)。"
exit 1
