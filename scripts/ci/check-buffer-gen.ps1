<#
.SYNOPSIS
    BufferGen 一致性校验：仓库里的 UBO/SSBO 派生产物是否与当前 C++ 结构一致。

.DESCRIPTION
    结构（`inc/hgl/graph/ubo/*.h`、`inc/hgl/graph/ssbo/*.h`）是唯一真源。派生产物——

        ShaderLibrary/module/descriptor/ubo/*.glsl
        ShaderLibrary/module/descriptor/ssbo/*.glsl
        inc/hgl/graph/generated/BufferToml.gen.h
        inc/hgl/graph/generated/buffer_schema.fbs

    一律由 `src/Tools/BufferGen/buffer_gen.py` 生成。本脚本用 `--check` 只比对不落盘：
    任何一侧手改、或改了 C++ 结构却忘了重新生成，都会在这里失败。

    依赖：Python 3.11+（tomllib）、Clang（能解析本仓库头文件）、glm 等第三方头。
    第三方头目录用 -IncludeDir 追加，或设环境变量 HGL_BUFFERGEN_INCLUDE_DIRS。

.PARAMETER Clang
    clang++ 可执行文件。缺省取 $env:HGL_CLANG，再缺省按 PATH 查找 clang++。

.PARAMETER IncludeDir
    额外 include 目录（可重复），例如 D:\vcpkg\installed\x64-windows\include。

.EXAMPLE
    pwsh scripts/ci/check-buffer-gen.ps1 -IncludeDir D:\vcpkg\installed\x64-windows\include
#>
param(
    [string]$RepoRoot  = ".",
    [string]$Clang     = "",
    [string[]]$IncludeDir = @()
)

$ErrorActionPreference = "Stop"

$repo = (Resolve-Path -LiteralPath $RepoRoot).Path
$tool = Join-Path $repo "src/Tools/BufferGen/buffer_gen.py"
if (-not (Test-Path -LiteralPath $tool)) {
    Write-Error "[BufferGen] 找不到生成器：$tool"
}

if ([string]::IsNullOrWhiteSpace($Clang)) {
    $Clang = $env:HGL_CLANG
}
if ([string]::IsNullOrWhiteSpace($Clang)) {
    $found = Get-Command clang++ -ErrorAction SilentlyContinue
    if ($found) {
        $Clang = $found.Source
    }
}
if ([string]::IsNullOrWhiteSpace($Clang)) {
    # Windows 上 LLVM 的默认安装位置；找不到就明确报错，不做静默跳过。
    $fallback = "C:\Program Files\LLVM\bin\clang++.exe"
    if (Test-Path -LiteralPath $fallback) {
        $Clang = $fallback
    }
}
if ([string]::IsNullOrWhiteSpace($Clang)) {
    Write-Error "[BufferGen] 找不到 clang++。请用 -Clang 指定，或设置环境变量 HGL_CLANG。"
}

$extra = @()
foreach ($dir in $IncludeDir) {
    if (-not [string]::IsNullOrWhiteSpace($dir)) {
        $extra += @("--include-dir", $dir)
    }
}

Write-Host "[BufferGen] clang = $Clang"
Write-Host "[BufferGen] 一致性校验中（--check，不落盘）…"

& python $tool --repo-root $repo --clang $Clang --check @extra
$code = $LASTEXITCODE

if ($code -ne 0) {
    Write-Host ""
    Write-Host "[BufferGen] 派生产物与 C++ 结构不一致。请重新生成后提交：" -ForegroundColor Red
    Write-Host "    python src/Tools/BufferGen/buffer_gen.py --repo-root . --clang `"$Clang`" --write" -ForegroundColor Yellow
    exit $code
}

Write-Host "[BufferGen] OK：派生产物与 C++ 结构一致。" -ForegroundColor Green
exit 0
