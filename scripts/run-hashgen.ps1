<#
.SYNOPSIS
    EnumGen 代码生成与校验执行脚本。

.DESCRIPTION
    重新运行 Clang AST 枚举注册表生成器（src/Tools/ASTTooling/enum_gen.py）。
    目前服务的唯一目标是 CMHash 的 `enum class Algorithm`：它的成员列表、
    数量和顺序会被派生为 `CMHash/inc/hgl/util/hash/generated/AlgorithmRegistry.gen.h`，
    严格名称解析与算法清单因此不可能与枚举声明脱节。

.PARAMETER Write
    生成并落盘覆盖目标文件（默认模式）。

.PARAMETER Check
    只校验派生产物与 C++ 枚举是否一致，不写入文件（CI 模式）。

.PARAMETER Only
    仅处理指定的单个 target（例如 -Only HashAlgorithm）。

.PARAMETER Clang
    指定 clang++ 路径。缺省自动查找环境变量、PATH 或常见安装目录。

.EXAMPLE
    .\scripts\run-hashgen.ps1
    .\scripts\run-hashgen.ps1 -Check
    .\scripts\run-hashgen.ps1 -Only HashAlgorithm
#>
param(
    [switch]$Check,
    [switch]$Write,
    [string]$Only  = "",
    [string]$Clang = ""
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$tool     = Join-Path $repoRoot "src\Tools\ASTTooling\enum_gen.py"
$config   = Join-Path $repoRoot "src\Tools\ASTTooling\enum_gen.toml"

if (-not (Test-Path -LiteralPath $tool)) {
    Write-Error "[EnumGen] 找不到生成器脚本: $tool"
}
if (-not (Test-Path -LiteralPath $config)) {
    Write-Error "[EnumGen] 找不到配置文件: $config"
}

# 1. 探测 Clang
if ([string]::IsNullOrWhiteSpace($Clang)) {
    $Clang = $env:HGL_CLANG
}
if ([string]::IsNullOrWhiteSpace($Clang)) {
    $cmd = Get-Command clang++ -ErrorAction SilentlyContinue
    if ($cmd) { $Clang = $cmd.Source }
}
if ([string]::IsNullOrWhiteSpace($Clang)) {
    $candidates = @(
        "C:\Program Files\LLVM\bin\clang++.exe",
        "C:\LLVM\bin\clang++.exe",
        "D:\LLVM\bin\clang++.exe"
    )
    foreach ($cand in $candidates) {
        if (Test-Path -LiteralPath $cand) { $Clang = $cand; break }
    }
}
if ([string]::IsNullOrWhiteSpace($Clang)) {
    Write-Error "[EnumGen] 找不到 clang++。请通过 -Clang 参数指定或配置 HGL_CLANG 环境变量。"
}

# 2. 探测 Python
$python = ""
$pyCmd = Get-Command python -ErrorAction SilentlyContinue
if ($pyCmd) { $python = $pyCmd.Source }
if ([string]::IsNullOrWhiteSpace($python)) {
    Write-Error "[EnumGen] 找不到 python（需要 3.11+，因为使用 tomllib）。"
}

# 3. 组装参数
$arguments = @($tool, "--config", $config, "--clang", $Clang)
if ($Only) { $arguments += @("--only", $Only) }
if ($Check) { $arguments += "--check" }

Write-Host "[EnumGen] python : $python"
Write-Host "[EnumGen] clang++: $Clang"

& $python @arguments
exit $LASTEXITCODE
