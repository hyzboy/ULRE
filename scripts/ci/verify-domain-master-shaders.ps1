param(
    # 构建树：需已构建 ShaderResourceSchemaRegressionGate（含 dump-domain-glsl）
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
# 参考文本 + options 的唯一来源：ShaderResourceSchemaRegressionGate dump-domain-glsl。
#   * 它导出的是 `GetFinalGLSL()`——即 glslang 实际吃到的最终文本，不是预处理期的
#     半成品；
#   * 它同时落盘**每个形状自己的** `generated/options.glsl`（走真实后端的
#     `EmitDomainOptionsForDefinition`）。脚本绝不手抄宏表：手抄的表会和发射器
#     悄悄漂移，等价性验收就失去意义；
#   * 它覆盖全部 9 个形状，包含生产夹具表里没有的无光族领域（UnlitTexture /
#     VertexColor / VertexPalette / VertexLuminance）。
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

$gateExe = Join-Path $root "$BuildDir/out/Windows_64_$Config/ShaderResourceSchemaRegressionGate.exe"
if (-not (Test-Path $gateExe)) {
    throw "找不到 $gateExe —— 先构建 ShaderResourceSchemaRegressionGate。"
}
if (-not (Test-Path (Join-Path $root $Spvfc))) {
    throw "找不到 $Spvfc —— spvfc 不在根构建树内，需单独构建 src/Tools/GLSLCompiler。"
}
$spvfc = Join-Path $root $Spvfc

# ── 1. 导出旧路径参考文本 + 每个形状的 options ──
if (-not $SkipDump) {
    $dumpLog = & $gateExe dump-domain-glsl $refDir 2>&1
    if ($LASTEXITCODE -ne 0) {
        $dumpLog | Select-Object -Last 30 | Write-Host
        throw "dump-domain-glsl 失败（exit=$LASTEXITCODE）。"
    }
    $skipped = @($dumpLog | Where-Object { $_ -match '^\[SKIP\]' })
    if ($skipped.Count -gt 0) {
        $skipped | Write-Host
        throw "dump-domain-glsl 有 $($skipped.Count) 个形状未能导出（见上）。"
    }
}

# ── 2. 编译 + 比对 ──
# 主着色器与参考文本都用同样两条 -I 根（ShaderLibrary + 本形状的 generated 父目录），
# 保证两边吃的是同一份 options.glsl。
function Get-Reflection([string]$src, [string]$genRoot, [string]$out) {
    $txt = & $spvfc -I ShaderLibrary -I $genRoot -o $out $src 2>&1
    if ($LASTEXITCODE -ne 0) {
        $txt | Select-Object -Last 15 | Write-Host
        throw "编译失败：$src"
    }
    return ($txt | Where-Object { $_ -notmatch '^=== Reflection:|^\[spvfc\] (Compiling|Written)' })
}

# slug（= dump-domain-glsl 的文件名） / 固定主着色器
$cases = @(
    @('fwd',          'ShaderLibrary/domain/standard_opaque/forward_lit.frag'),
    @('fwd-alpha',    'ShaderLibrary/domain/standard_opaque/forward_lit.frag'),
    @('shadow',       'ShaderLibrary/domain/standard_opaque/shadow_caster.frag'),
    @('shadow-alpha', 'ShaderLibrary/domain/standard_opaque/shadow_caster_masked.frag'),
    @('sky',          'ShaderLibrary/domain/sky/sky.frag'),
    @('unlit-tex',    'ShaderLibrary/domain/unlit_texture/unlit_texture.frag'),
    @('vert-color',   'ShaderLibrary/domain/vertex_color/vertex_color.frag'),
    @('vert-lum',     'ShaderLibrary/domain/vertex_luminance/vertex_luminance.frag'),
    @('vert-pal',     'ShaderLibrary/domain/vertex_color/vertex_color.frag')
)

$failed = 0
$newSpv  = Join-Path $outDir 'new.spv'
$refSpv  = Join-Path $outDir 'ref.spv'
foreach ($c in $cases) {
    $slug     = $c[0]
    $master   = $c[1]
    $refSrc   = Join-Path $refDir "$slug.frag"
    $genRoot  = Join-Path $refDir $slug
    $label    = "$master  [$slug]"
    if (-not (Test-Path $refSrc)) {
        Write-Host "FAIL $label  参考文本缺失：$slug.frag（dump-domain-glsl 未产出？）"
        $failed++
        continue
    }
    if (-not (Test-Path (Join-Path $genRoot 'generated/options.glsl'))) {
        Write-Host "FAIL $label  缺少 $slug/generated/options.glsl"
        $failed++
        continue
    }
    try {
        $a = Get-Reflection $master $genRoot $newSpv
        $b = Get-Reflection $refSrc $genRoot $refSpv
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
