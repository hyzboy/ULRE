<#
.SYNOPSIS
    AssetGen 代码生成与校验执行脚本。

.DESCRIPTION
    一键重新运行 Clang AST 资产代码生成工具（asset_gen.py）。
    自动探测 LLVM Clang、FlatBuffers flatc、vcpkg 及 Vulkan SDK 路径。

.PARAMETER Write
    生成并落盘覆盖目标文件（默认模式）。

.PARAMETER Check
    只校验派生产物与 C++ 结构是否一致，不写入文件（CI 模式）。

.PARAMETER Only
    仅生成指定的单个分组（例如 -Only Geometry）。

.PARAMETER Clang
    指定 clang++ 路径。缺省自动查找环境变量、PATH 或常见安装目录。

.PARAMETER Flatc
    指定 flatc 路径。缺省自动查找环境变量、PATH 或 vcpkg 目录。

.PARAMETER IncludeDir
    追加第三方头文件 include 目录。

.PARAMETER SkipTest
    跳过生成器单元测试。

.PARAMETER NoFlatc
    跳过 flatc 编译，只生成 .fbs 和 C++ 序列化头。

.EXAMPLE
    .\scripts\run-assetgen.ps1
    .\scripts\run-assetgen.ps1 -Check
    .\scripts\run-assetgen.ps1 -Only Geometry
#>
param(
    [switch]$Check,
    [switch]$Write,
    [string]$Only       = "",
    [string]$Clang      = "",
    [string]$Flatc      = "",
    [string[]]$IncludeDir = @(),
    [switch]$SkipTest,
    [switch]$NoFlatc
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$tool = Join-Path $repoRoot "src\Tools\AssetGen\asset_gen.py"
$testScript = Join-Path $repoRoot "src\Tools\AssetGen\test_asset_gen.py"

if (-not (Test-Path -LiteralPath $tool)) {
    Write-Error "[AssetGen] 找不到生成器脚本: $tool"
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
        if (Test-Path -LiteralPath $cand) {
            $Clang = $cand
            break
        }
    }
}
if ([string]::IsNullOrWhiteSpace($Clang)) {
    Write-Error "[AssetGen] 找不到 clang++。请通过 -Clang 参数指定或配置 HGL_CLANG 环境变量。"
}

# 2. 探测 Flatc
if (-not $NoFlatc) {
    if ([string]::IsNullOrWhiteSpace($Flatc)) {
        $Flatc = $env:HGL_FLATC
    }
    if ([string]::IsNullOrWhiteSpace($Flatc)) {
        $cmd = Get-Command flatc -ErrorAction SilentlyContinue
        if ($cmd) { $Flatc = $cmd.Source }
    }
    if ([string]::IsNullOrWhiteSpace($Flatc)) {
        $candidates = @(
            "D:\vcpkg\installed\x64-windows\tools\flatbuffers\flatc.exe",
            "C:\vcpkg\installed\x64-windows\tools\flatbuffers\flatc.exe"
        )
        if (-not [string]::IsNullOrWhiteSpace($env:VCPKG_ROOT)) {
            $candidates += (Join-Path $env:VCPKG_ROOT "installed\x64-windows\tools\flatbuffers\flatc.exe")
        }
        foreach ($cand in $candidates) {
            if (Test-Path -LiteralPath $cand) {
                $Flatc = $cand
                break
            }
        }
    }
    if ([string]::IsNullOrWhiteSpace($Flatc)) {
        Write-Error "[AssetGen] 找不到 flatc。请通过 -Flatc 参数指定或配置 HGL_FLATC 环境变量。"
    }
}

# 3. 收集 Include 路径
$allIncludes = @()
if ($IncludeDir) {
    $allIncludes += $IncludeDir
}
if (-not [string]::IsNullOrWhiteSpace($env:HGL_ASSETGEN_INCLUDE_DIRS)) {
    $allIncludes += ($env:HGL_ASSETGEN_INCLUDE_DIRS -split ';')
}
# 自动探测 vcpkg 与 Vulkan SDK
$defaultIncludes = @(
    "D:\vcpkg\installed\x64-windows\include",
    "C:\vcpkg\installed\x64-windows\include"
)
if (-not [string]::IsNullOrWhiteSpace($env:VCPKG_ROOT)) {
    $defaultIncludes += (Join-Path $env:VCPKG_ROOT "installed\x64-windows\include")
}
if (-not [string]::IsNullOrWhiteSpace($env:VULKAN_SDK)) {
    $defaultIncludes += (Join-Path $env:VULKAN_SDK "Include")
}
foreach ($inc in $defaultIncludes) {
    if ((Test-Path -LiteralPath $inc) -and ($allIncludes -notcontains $inc)) {
        $allIncludes += $inc
    }
}

Write-Host "==================================================" -ForegroundColor Cyan
Write-Host " [AssetGen] ULRE 资产代码生成与校验" -ForegroundColor Cyan
Write-Host "==================================================" -ForegroundColor Cyan
Write-Host " Clang : $Clang"
if ($NoFlatc) {
    Write-Host " Flatc : (跳过 flatc 编译)"
} else {
    Write-Host " Flatc : $Flatc"
}
foreach ($inc in $allIncludes) {
    Write-Host " Inc   : $inc"
}
Write-Host "--------------------------------------------------"

# 4. 执行生成器单元测试
if (-not $SkipTest) {
    Write-Host "[1/2] 运行生成器单元测试..." -ForegroundColor Yellow
    & python $testScript
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[AssetGen] 单元测试失败！"
        exit $LASTEXITCODE
    }
} else {
    Write-Host "[1/2] 跳过生成器单元测试。"
}

# 5. 构建命令行参数
$argsList = @(
    $tool,
    "--repo-root", $repoRoot,
    "--clang", $Clang
)

if ($NoFlatc) {
    $argsList += "--no-flatc"
} else {
    $argsList += @("--flatc", $Flatc)
}

foreach ($inc in $allIncludes) {
    $argsList += @("--include-dir", $inc)
}

if (-not [string]::IsNullOrWhiteSpace($Only)) {
    $argsList += @("--only", $Only)
}

if ($Check) {
    Write-Host "[2/2] 执行一致性比对校验 (--check)..." -ForegroundColor Yellow
    $argsList += "--check"
} else {
    Write-Host "[2/2] 执行生成并落盘 (--write)..." -ForegroundColor Green
    $argsList += "--write"
}

& python @argsList
$exitCode = $LASTEXITCODE

if ($exitCode -eq 0) {
    if ($Check) {
        Write-Host "==================================================" -ForegroundColor Green
        Write-Host " [AssetGen] 校验成功：C++ 结构与派生产物完全一致。" -ForegroundColor Green
        Write-Host "==================================================" -ForegroundColor Green
    } else {
        Write-Host "==================================================" -ForegroundColor Green
        Write-Host " [AssetGen] 代码生成完毕并已成功落盘！" -ForegroundColor Green
        Write-Host "==================================================" -ForegroundColor Green
    }
} else {
    Write-Host "==================================================" -ForegroundColor Red
    Write-Host " [AssetGen] 执行失败 (退出码 $exitCode)" -ForegroundColor Red
    Write-Host "==================================================" -ForegroundColor Red
}

exit $exitCode
