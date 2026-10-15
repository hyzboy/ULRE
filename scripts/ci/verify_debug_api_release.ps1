param(
    [string]$BuildDir          = "build_debugapi_release",
    [string]$ReferenceBuildDir = "build",
    [string]$Config            = "Release",
    [int]$Jobs                 = 2,
    [switch]$SkipBuild,
    [switch]$Clean
)

# ─────────────────────────────────────────────────────────────────────────────
# A3②（2026-10-14）：**带校验跑一次 Release 验收** —— 独立构建树 + `-DULRE_ECS_DEBUG_API=ON`。
#
# 为什么需要：`ULRE_ECS_DEBUG_API` 默认 AUTO（Debug 开 / Release 关）⇒ 对拍器（掩码↔行号、
# 实体行表↔组件行、可见性镜像）与越界守卫在 Release 里**根本不编译** ⇒ 长期不验证会 rot。
# 本脚本按「独立树 + 强制开 + 只编需要的目标 + 跑判据」把它变成一条可执行的验收动作。
#
# 判据（全绿才算过）：
#   1) 编得过（这本身就在证明整个调试面在 Release 下可编译）；
#   2) `ShaderResourceSchemaRegressionGate` 退出 0 且 0 FAIL；
#   3) `TestCSMIncrementalPass` 退出 0（源码契约针：存在性/计数/禁复活）；
#   4) `PBRSpheres`（`PBR_SELFCHECK=1`）退出 0；
#   5) `GPUDrivenCull`（只给 `GC_SELFCHECK=1`，不给 `GC_STATIC=1`）退出 0 且打印"已自动冻结相机"加固行。
# 退出码 127 = 无窗口会话下示例早退（环境问题，已知）⇒ 记 SKIP，不算 FAIL。
# ─────────────────────────────────────────────────────────────────────────────

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot  = (Resolve-Path (Join-Path $scriptDir "..\..")).Path
$buildPath = Join-Path $repoRoot $BuildDir
$refCache  = Join-Path (Join-Path $repoRoot $ReferenceBuildDir) "CMakeCache.txt"
$outDir    = Join-Path $buildPath ("out\Windows_64_" + $Config)

# 生成器沿用主构建树（本仓 = "Visual Studio 18 2026"，多配置）
$generator = "Visual Studio 18 2026"
if (Test-Path $refCache) {
    $m = Select-String -Path $refCache -Pattern '^CMAKE_GENERATOR:INTERNAL=(.+)$' | Select-Object -First 1
    if ($m) { $generator = $m.Matches[0].Groups[1].Value.Trim() }
}

$targets = @("ULRE.ECS", "ShaderResourceSchemaRegressionGate", "TestCSMIncrementalPass", "PBRSpheres", "GPUDrivenCull")

# 依赖解析机制沿用主构建树：本仓靠 vcpkg 工具链（`CMAKE_TOOLCHAIN_FILE`）找 absl 等；
# 独立树若不带它 ⇒ configure 直接失败（Could not find a package configuration file provided by "absl"）。
$toolchain = ""
if (Test-Path $refCache) {
    $t = Select-String -Path $refCache -Pattern '^CMAKE_TOOLCHAIN_FILE:[A-Z]+=(.+)$' | Select-Object -First 1
    if ($t) { $toolchain = $t.Matches[0].Groups[1].Value.Trim() }
}
$triplet = ""
if (Test-Path $refCache) {
    $tr = Select-String -Path $refCache -Pattern '^VCPKG_TARGET_TRIPLET:[A-Z]+=(.+)$' | Select-Object -First 1
    if ($tr) { $triplet = $tr.Matches[0].Groups[1].Value.Trim() }
}

Write-Host "[1/3] 构建树 = $buildPath"
Write-Host "      generator = '$generator' / config = $Config / -DULRE_ECS_DEBUG_API=ON"
if ($toolchain -ne "") { Write-Host "      toolchain = '$toolchain'   triplet = '$triplet'" }
else { Write-Host "      [!] 参考树没给出 CMAKE_TOOLCHAIN_FILE ⇒ 若 configure 报缺包，检查依赖来源" }

if (-not $SkipBuild) {
    # `-Clean`：删掉独立树里的 obj（⚠ 改了头文件/struct 字段集合后**必须**带它 —— 独立树是另一份
    # CMake 树，MSBuild 的增量依赖跟踪不可靠 ⇒ 陈旧 TU 与旧布局混链 ⇒ 示例起手就 0xC0000005 崩）。
    if ($Clean) {
        Write-Host "      [i] -Clean：清独立树的 obj（struct/头改动后必带）"
        Get-ChildItem -Path $buildPath -Directory -Recurse -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -like "*.dir" } |
            ForEach-Object {
                foreach ($cfg in @("Debug", "Release")) {
                    $sub = Join-Path $_.FullName $cfg
                    if (Test-Path $sub) { Remove-Item -Recurse -Force $sub -ErrorAction SilentlyContinue }
                }
            }
        Get-ChildItem -Path $buildPath -Filter "*.module.json" -Recurse -ErrorAction SilentlyContinue |
            Remove-Item -Force -ErrorAction SilentlyContinue
    }

    # 陈旧/不兼容的独立树（例如上次没带工具链建的）⇒ 直接重建，避免 CMake 拒绝换工具链
    $staleCache = Join-Path $buildPath "CMakeCache.txt"
    if (Test-Path $staleCache) {
        $old = Select-String -Path $staleCache -Pattern '^CMAKE_TOOLCHAIN_FILE:[A-Z]+=(.*)$' | Select-Object -First 1
        $oldVal = if ($old) { $old.Matches[0].Groups[1].Value.Trim() } else { "" }
        if ($oldVal -ne $toolchain) {
            Write-Host "      [i] 已有树工具链('$oldVal')与参考树不一致 ⇒ 重建该树"
            Remove-Item -Recurse -Force $buildPath
        }
    }

    Write-Host "[2/3] configure + build（只编需要的目标）"
    $cfgArgs = @("-S", $repoRoot, "-B", $buildPath, "-G", $generator, "-DULRE_ECS_DEBUG_API=ON")
    if ($toolchain -ne "") { $cfgArgs += ("-DCMAKE_TOOLCHAIN_FILE=" + $toolchain) }
    if ($triplet   -ne "") { $cfgArgs += ("-DVCPKG_TARGET_TRIPLET=" + $triplet) }

    cmake @cfgArgs
    if ($LASTEXITCODE -ne 0) { throw "configure 失败（rc=$LASTEXITCODE）" }

    cmake --build $buildPath --config $Config --parallel $Jobs --target @targets
    if ($LASTEXITCODE -ne 0) { throw "build 失败（rc=$LASTEXITCODE）" }
} else {
    Write-Host "[2/3] -SkipBuild：跳过 configure/build（用已有输出跑判据）"
}

function Invoke-Checked {
    param(
        [string]$ExeName,
        [hashtable]$EnvVars = @{},
        [string[]]$RequireLines = @()
    )

    $exe = Join-Path $outDir $ExeName
    if (-not (Test-Path $exe)) {
        Write-Host ("  [!] 找不到 {0}（先去掉 -SkipBuild 编一次）" -f $ExeName)
        return "MISSING"
    }

    $log = Join-Path $buildPath ($ExeName + ".log")
    foreach ($k in $EnvVars.Keys) { Set-Item -Path ("Env:" + $k) -Value $EnvVars[$k] }

    & $exe 2>&1 | Tee-Object -FilePath $log | Out-Null
    $rc = $LASTEXITCODE

    foreach ($k in $EnvVars.Keys) { Remove-Item -Path ("Env:" + $k) -ErrorAction SilentlyContinue }
    Get-Process -Name ([IO.Path]::GetFileNameWithoutExtension($ExeName)) -ErrorAction SilentlyContinue |
        Stop-Process -Force -ErrorAction SilentlyContinue

    $hit = @{}
    foreach ($l in $RequireLines) {
        $hit[$l] = [bool](Select-String -Path $log -Pattern ([regex]::Escape($l)) -Quiet)
    }
    $fnv = (Select-String -Path $log -Pattern 'fnv1a64=0x[0-9A-F]+' | Select-Object -Last 1)
    $fnvText = if ($fnv) { $fnv.Matches[0].Value } else { "(无摘要行)" }

    $miss = @($RequireLines | Where-Object { -not $hit[$_] })

    $verdict = "FAIL"
    if ($rc -eq 0 -and $miss.Count -eq 0) { $verdict = "PASS" }
    elseif ($rc -eq 127) { $verdict = "SKIP(无窗口会话早退)" }

    Write-Host ("  {0,-22} rc={1,-4} {2}  {3}" -f $ExeName, $rc, $verdict, $fnvText)
    foreach ($l in $miss) { Write-Host ("      [缺] " + $l) }

    if ($verdict -eq "PASS") { return "PASS" }
    if ($verdict -like "SKIP*") { return "SKIP" }
    return "FAIL"
}

Write-Host "[3/3] 跑判据"

$r1 = Invoke-Checked -ExeName "ShaderResourceSchemaRegressionGate.exe"
$r2 = Invoke-Checked -ExeName "TestCSMIncrementalPass.exe"
$r3 = Invoke-Checked -ExeName "PBRSpheres.exe" -EnvVars @{ PBR_SELFCHECK = "1" } `
        -RequireLines @("配置口径：Release 不钉像素摘要")
$r4 = Invoke-Checked -ExeName "GPUDrivenCull.exe" -EnvVars @{ GC_SELFCHECK = "1" } `
        -RequireLines @("已**自动冻结相机**")

Write-Host ""
Write-Host ("汇总：gate={0}  csm={1}  pbr={2}  gc={3}" -f $r1, $r2, $r3, $r4)

$failed = @(@($r1, $r2, $r3, $r4) | Where-Object { $_ -ne "PASS" -and $_ -ne "SKIP" })
if ($failed.Count -gt 0) {
    Write-Host "带宏 Release 验收：FAIL"
    exit 1
}

Write-Host "带宏 Release 验收：PASS（SKIP = 无窗口会话早退，已知环境问题）"
exit 0
