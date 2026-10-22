param(
    [string]$BuildDir = "build",
    [string]$Config   = "Debug",
    # 默认 8：2026-10-18 实测 354 个 exe 在 /m:8 下全部重链，rc=0、零错误、零 MSB3073。
    # 此前「并行 /m 有 vcpkg applocal 竞态（MSB3073 exit 32）」的结论在现版 vcpkg
    # （E:\vcpkg，走 z-applocal 而非 legacy applocal.ps1）下不复现，/m:1 已无必要。
    [int]$Jobs        = 8,
    [string]$LogDir   = "",
    [switch]$SkipConfigure,
    [switch]$SkipBuild,
    [switch]$SkipTest,
    # 发行形态验收：ULRE_ENABLE_SHADERGEN=OFF 的第二棵树能否只链 ULRE::Runtime 就跑通
    # 烘焙好的 shader-cache。默认关闭 —— 它要额外一棵构建树，耗时不可忽略，且 cutter
    # 回归频率低于常规基线。改动 ULRE::Runtime 边界 / ShaderMaterialData / 清单格式
    # 时必须打开。
    [switch]$VerifyReleaseForm
)

# ─────────────────────────────────────────────────────────────────────────────
# CMake 阶段 1 收尾（2026-10）：**A 层基线** —— 把「构建绿 + ULRE 自有测试绿」变成一条
# 可重复执行、有明确退出码的动作，作为阶段 2~5 每次改动的回归判据。
#
# 为什么必须分层（本仓 738 个 target / 183 个 ctest 用例）：
#   A 层 = ULRE 自有的 src/ + example/ + inc/ + ShaderLibrary/ 产物，以及 ULRE 自有测试
#          （ctest 标签 ecs / csm / shadergen，共 25 个用例）
#   B 层 = CMCore / CMMath / CMAudio / CM2D / CMUtil / CMCoreType / CMAssetsManage 等
#          submodule 上游仓库的 examples 与 tests（共 158 个用例）
# 上游（B 层）若有断点属另一条任务线，登记不阻塞 ——
# 否则「CMake 模块化」永远卡在修别人的仓库上。
# 注意：此前观察到的「CMCore.lib / CMMath.lib / CMP.AudioClient.lib 大片 LNK1104」
# 全部是 Visual Studio 常驻 MSBuild node 与本次构建抢文件造成的假错误，加
# /nodeReuse:false 后即消失；真实错误只有 TexImage 跨配置链接一条，已由 [2/5] 自动前置。
# 更正（2026-10-18）：曾记录「CMP.AudioClient 在 Debug 下从未编译」，经取证为误判 ——
#   CMP.AudioClient.dll / CMAudio.lib 均正常产出，成因是并发构建把 .tlog 目录清空/重写。
#
# 判据（A 层全绿才算过，退出码 0）：
#   1) 全量构建中**不存在归属于 A 层路径**的编译/链接错误；
#   2) ctest -L "ecs|csm|shadergen" 全部通过（0 failed）。
# B 层错误只统计、写入 summary.md，不影响退出码。
#
# 用法：
#   pwsh scripts/ci/build-baseline.ps1                     # 全量 Debug 串行构建 + A 层测试
#   pwsh scripts/ci/build-baseline.ps1 -SkipBuild          # 只跑测试（用已有产物）
#   pwsh scripts/ci/build-baseline.ps1 -Config Release -Jobs 4
#   pwsh scripts/ci/build-baseline.ps1 -VerifyReleaseForm  # 追加「无生成器形态」验收
# ─────────────────────────────────────────────────────────────────────────────

$ErrorActionPreference = "Continue"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot  = (Resolve-Path (Join-Path $scriptDir "..\..")).Path
$buildPath = Join-Path $repoRoot $BuildDir
$buildDirName = Split-Path $buildPath -Leaf

if ($LogDir -eq "") { $LogDir = Join-Path $buildPath "baseline-logs" }
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null

$stamp     = Get-Date -Format "yyyyMMdd-HHmmss"
$buildLog  = Join-Path $LogDir "build-$Config-$stamp.log"
$testLog   = Join-Path $LogDir "ctest-$Config-$stamp.log"
$summaryMd = Join-Path $LogDir "summary-$Config-$stamp.md"

# A 层路径前缀（相对仓库根，统一用正斜杠比较）
$aLayerPrefixes = @(
    "src/",
    "example/",
    "inc/",
    "ShaderLibrary/",
    "CMakeLists.txt",
    "example/CMakeLists.txt"
)

# B 层（上游 submodule）顶层目录
$bLayerTops = @(
    "CMCore", "CMCoreType", "CMMath", "CMUtil", "CMAudio",
    "CM2D", "CMAssetsManage", "CMGUI", "CMQT", "3rdpty"
)

# ULRE 自有测试标签
$ulreLabelRegex = "ecs|csm|shadergen"

Write-Host "=============================================================="
Write-Host "ULRE 构建基线（A 层）"
Write-Host "  仓库根   = $repoRoot"
Write-Host "  构建树   = $buildPath"
Write-Host "  配置     = $Config   并行度 = $Jobs"
Write-Host "  日志目录 = $LogDir"
Write-Host "=============================================================="

# ── [1/5] configure ──────────────────────────────────────────────────────────
if (-not $SkipConfigure -and -not $SkipBuild) {
    Write-Host "[1/5] configure"
    cmake -S $repoRoot -B $buildPath 2>&1 | Tee-Object -FilePath (Join-Path $LogDir "configure-$stamp.log")
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  configure 失败（rc=$LASTEXITCODE）"
        exit 2
    }
} else {
    Write-Host "[1/5] configure 已跳过"
}

# ── [2/5] 前置：Release 版 TexImage ──────────────────────────────────────────
# TexImage 恒按 Release 构建：ImageMagick 只提供 /MT 版 import lib，Debug 编译必然
# 撞 RuntimeLibrary / _ITERATOR_DEBUG_LEVEL 不匹配。Debug 宿主（TexConvCore）因此
# 必须链 out/..._Release/TexImage.lib —— 干净树里这个文件不存在，是 Debug 全量构建
# 唯一的固定 LNK1104 来源。这里把它变成基线的自动前置，而不是靠人记住。
if (-not $SkipBuild -and $Config -ne "Release") {
    $texImageLib = Get-ChildItem -Path $buildPath -Filter "TexImage.lib" -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '_Release\\' } | Select-Object -First 1
    if (-not $texImageLib) {
        Write-Host "[2/5] 前置：构建 Release 版 TexImage"
        cmake --build $buildPath --config Release --target TexImage -- /m:1 /nodeReuse:false /nologo 2>&1 |
            Tee-Object -FilePath (Join-Path $LogDir "bootstrap-teximage-$stamp.log") | Out-Null
        if ($LASTEXITCODE -ne 0) {
            Write-Host "  Release TexImage 构建失败（rc=$LASTEXITCODE），无法继续"
            exit 3
        }
        # EXISTS() 在 configure 期求值，产出 import lib 后必须重新配置，Debug 链接项才会生效
        cmake -S $repoRoot -B $buildPath 2>&1 | Out-Null
    } else {
        Write-Host "[2/5] 前置：Release 版 TexImage 已存在"
    }
} else {
    Write-Host "[2/5] 前置：已跳过（-SkipBuild 或 Release 配置）"
}

# ── [3/5] 全量构建（并行，日志落盘） ─────────────────────────────────────────
if (-not $SkipBuild) {
    Write-Host "[3/5] build（--config $Config，/m:$Jobs，日志 -> $buildLog）"
    # -nodeReuse:false：Visual Studio 常驻的 MSBuild worker node 会与本次构建抢同一批
    # .lib/.dll（表现为大片假的 LNK1104），必须用独立节点构建。
    # 并行：本仓输出目录把 exe/dll/lib 全平铺在一个目录，vcpkg applocal 会并发往同一目录
    # 拷同一批 DLL。实测 354 个 exe 全量重链 /m:8 零失败，故竞态不是系统性的；偶发的
    # MSB3073(exit 32) 由下面的「串行补跑」兜底，不构成把 /m 降回 1 的理由。
    cmake --build $buildPath --config $Config -- /m:$Jobs /nodeReuse:false /nologo 2>&1 |
        Tee-Object -FilePath $buildLog | Out-Null
    $buildRc = $LASTEXITCODE
    Write-Host "  build rc=$buildRc"

    # vcpkg applocal 的 POST_BUILD 偶发 MSB3073（exit 32），成因是并发拷贝时的共享冲突。
    # z-applocal 本身幂等，把日志里记录的那几条命令串行重放一遍即可修复 —— 这样既保留了
    # 并行构建的速度，又不让基线因一次偶发而误判。
    # 只在确实出现 MSB3073 时触发：日志里出现 z-applocal 行本身是正常现象（有 target 重链
    # 就会有），无条件重放等于给每次增量构建平白加上几十次进程启动。
    $appLocalRepaired = 0
    if ((Test-Path $buildLog) -and
        (Select-String -Path $buildLog -Pattern 'MSB3073.*exited with code 32' -Quiet)) {
        $replay = Select-String -Path $buildLog `
            -Pattern 'z-applocal\s+--target-binary=(\S+)\s+--installed-bin-dir=(\S+)' |
            ForEach-Object { "$($_.Matches[0].Groups[1].Value)|$($_.Matches[0].Groups[2].Value)" } |
            Sort-Object -Unique
        if ($replay) {
            Write-Host ("  检测到 {0} 条 applocal POST_BUILD，串行重放修复" -f $replay.Count)
            $vcpkgExe = (Select-String -Path (Join-Path $buildPath "CMakeCache.txt") `
                -Pattern '^CMAKE_TOOLCHAIN_FILE:FILEPATH=(.+)/scripts/buildsystems/vcpkg\.cmake$').Matches.Groups[1].Value
            $vcpkgExe = if ($vcpkgExe) { Join-Path $vcpkgExe "vcpkg.exe" } else { "" }
            if (Test-Path $vcpkgExe) {
                foreach ($r in $replay) {
                    $parts = $r.Split('|')
                    # 共享冲突是瞬时的：占用方（并发链接/拷贝）通常几百毫秒内就释放，
                    # 故退避重试而不是一次失败即放弃。
                    for ($try = 1; $try -le 3; $try++) {
                        & $vcpkgExe z-applocal "--target-binary=$($parts[0])" "--installed-bin-dir=$($parts[1])" 2>&1 | Out-Null
                        if ($LASTEXITCODE -eq 0) { $appLocalRepaired++; break }
                        if ($try -lt 3) { Start-Sleep -Milliseconds 500 }
                    }
                    if ($LASTEXITCODE -ne 0) {
                        Write-Host ("    重放 3 次仍失败(rc={0})：{1}" -f $LASTEXITCODE, $parts[0])
                    }
                }
                Write-Host ("  修复 {0}/{1}" -f $appLocalRepaired, $replay.Count)
            } else {
                Write-Host "  未找到 vcpkg.exe，跳过重放（请手工检查输出目录依赖 DLL）"
            }
        }
    }
} else {
    $buildRc = 0
    $appLocalRepaired = 0
    Write-Host "[3/5] build 已跳过（-SkipBuild）"
}

# ── [4/5] 错误分层 ───────────────────────────────────────────────────────────
function Get-LayerOf {
    param([string]$Text)
    $norm = $Text.Replace('\', '/')
    # MSBuild 错误行里给的是**构建树**路径（E:/ULRE/build/src/...），先归一化回源码树
    if ($buildDirName -ne "") { $norm = $norm.Replace("/ULRE/$buildDirName/", "/ULRE/") }
    foreach ($p in $aLayerPrefixes) {
        if ($norm -match [regex]::Escape("ULRE/$p")) { return "A" }
    }
    foreach ($t in $bLayerTops) {
        if ($norm -match [regex]::Escape("ULRE/$t/")) { return "B" }
    }
    return "?"
}

$aErrors = New-Object System.Collections.ArrayList
$bErrors = New-Object System.Collections.ArrayList
$uErrors = New-Object System.Collections.ArrayList

if (Test-Path $buildLog) {
    Write-Host "[4/5] 错误分层"
    # 只认真正的 MSBuild 错误行：形如 "... : error C2065: ..." 或 "LINK : fatal error LNK1104: ..."
    $errLines = Select-String -Path $buildLog -Pattern ':\s*(fatal\s+)?error\s+[A-Z]+\d+' |
        ForEach-Object { $_.Line.Trim() } |
        Sort-Object -Unique
    foreach ($l in $errLines) {
        switch (Get-LayerOf $l) {
            "A" { [void]$aErrors.Add($l) }
            "B" { [void]$bErrors.Add($l) }
            default { [void]$uErrors.Add($l) }
        }
    }
    # 已被 [3/5] 串行重放修复的 applocal MSB3073 不再计入 B 层错误
    if ($appLocalRepaired -gt 0) {
        $fixed = @($bErrors | Where-Object { $_ -match 'MSB3073' })
        if ($fixed.Count -gt 0) {
            foreach ($x in $fixed) { [void]$bErrors.Remove($x) }
            Write-Host ("  其中 {0} 条 applocal MSB3073 已由串行重放修复，不计入" -f $fixed.Count)
        }
    }
    Write-Host ("  A 层错误 = {0}   B 层错误 = {1}   未归类 = {2}" -f $aErrors.Count, $bErrors.Count, $uErrors.Count)
} else {
    Write-Host "[4/5] 无构建日志（-SkipBuild），跳过错误分层"
}

# ── [5/5] ULRE 自有测试 ──────────────────────────────────────────────────────
$testRc = 0
$ctestTotal = 0; $ctestPassed = 0; $ctestFailed = 0
if (-not $SkipTest) {
    Write-Host "[5/5] ctest -C $Config -L `"$ulreLabelRegex`""
    # 多配置生成器必须带 -C，否则 CTestTestfile 里 CTEST_CONFIGURATION_TYPE 为空，
    # 所有 add_test 落进 else() 分支变成 NOT_AVAILABLE（表现为 "Total Tests: 0"）。
    $ctestOut = ctest --test-dir $buildPath -C $Config -L $ulreLabelRegex --output-on-failure 2>&1
    $testRc = $LASTEXITCODE
    $ctestOut | Tee-Object -FilePath $testLog | Out-Null

    # 带 --output-on-failure 跑完后 ctest 只打印一行汇总，且**0 失败时省略失败子句**：
    #   "100% tests passed, 3 tests failed out of 25"   （有失败）
    #   "100% tests passed out of 25"                    （全通过，新版 ctest）
    # （"Total Tests: N" 只在 -N 模式下出现，别拿它解析）
    $line = ($ctestOut | Select-String 'tests passed.*out of \d+' | Select-Object -Last 1).Line
    if ($line -match '(\d+)%\s+tests passed,\s*(\d+) tests failed out of (\d+)') {
        $ctestTotal  = [int]$Matches[3]
        $ctestFailed = [int]$Matches[2]
        $ctestPassed = $ctestTotal - $ctestFailed
    } elseif ($line -match '(\d+)%\s+tests passed out of (\d+)') {
        $ctestTotal  = [int]$Matches[2]
        $ctestFailed = 0
        $ctestPassed = $ctestTotal
    }
    Write-Host ("  用例 {0}  通过 {1}  失败 {2}  rc={3}" -f $ctestTotal, $ctestPassed, $ctestFailed, $testRc)
} else {
    Write-Host "[5/5] ctest 已跳过（-SkipTest）"
}

# ── [extra] 发行形态验收 ─────────────────────────────────────────────────────
# 常规基线是「同一棵树能不能构建绿 + 测试绿」。这一节回答的是另一个问题：
# **在没有 ShaderGen 的树里，运行时还能不能靠烘焙好的 shader-cache 起得来。**
# 编号用 [extra] 而不占用 [n/5]，因为前面 5 个阶段是每次都要跑的，这一节按需开启。
$reasons = @()
if ($VerifyReleaseForm) {
    Write-Host "[extra] 发行形态验收（ULRE_ENABLE_SHADERGEN=OFF 树）"
    $verifyScript = Join-Path $scriptDir "verify-release-form.ps1"
    $verifyLog    = Join-Path $LogDir "verify-release-form-$Config-$stamp.log"
    & pwsh -NoProfile -File $verifyScript -DevBuildDir $buildDirName -Config $Config -Jobs $Jobs 2>&1 |
        Tee-Object -FilePath $verifyLog | ForEach-Object { Write-Host "  $_" }
    $verifyRc = $LASTEXITCODE
    Write-Host ("  rc={0}  日志 {1}" -f $verifyRc, $verifyLog)
    if ($verifyRc -ne 0) { $reasons += "发行形态验收失败（rc=$verifyRc）" }
} else {
    Write-Host "[extra] 发行形态验收已跳过（-VerifyReleaseForm 未开启）"
}

# ── 汇总 ─────────────────────────────────────────────────────────────────────
$verdict = "PASS"
if ($aErrors.Count -gt 0) { $verdict = "FAIL"; $reasons += ("A 层构建错误 {0} 条" -f $aErrors.Count) }
if ($uErrors.Count -gt 0) { $verdict = "FAIL"; $reasons += ("未归类构建错误 {0} 条（保守判失败，需人工归层）" -f $uErrors.Count) }
if ($testRc -ne 0 -or $ctestFailed -gt 0) { $verdict = "FAIL"; $reasons += ("ULRE 自有测试失败 {0} 个" -f $ctestFailed) }
if (-not $SkipTest -and $ctestTotal -eq 0) { $verdict = "FAIL"; $reasons += "ctest 一个用例都没选中（标签正则或 -C 配置有误）" }
if ($reasons.Count -gt 0) { $verdict = "FAIL" }

$md = New-Object System.Text.StringBuilder
[void]$md.AppendLine("# ULRE 构建基线报告")
[void]$md.AppendLine("")
[void]$md.AppendLine("- 时间：$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')")
[void]$md.AppendLine("- 配置：$Config    并行度：$Jobs")
[void]$md.AppendLine("- 构建树：``$buildPath``")
[void]$md.AppendLine("- 判定：**$verdict**")
if ($reasons.Count -gt 0) {
    foreach ($r in $reasons) { [void]$md.AppendLine("  - $r") }
}
[void]$md.AppendLine("")
[void]$md.AppendLine("| 层 | 构建错误数 | 含义 |")
[void]$md.AppendLine("|---|---|---|")
[void]$md.AppendLine("| A（ULRE 自有，阻塞） | $($aErrors.Count) | src/ example/ inc/ ShaderLibrary/ |")
[void]$md.AppendLine("| B（上游 submodule，不阻塞） | $($bErrors.Count) | CMCore/CMAudio/CMMath/... |")
[void]$md.AppendLine("| ?（未归类，阻塞） | $($uErrors.Count) | 需人工归层 |")
[void]$md.AppendLine("")
[void]$md.AppendLine("| ctest（-L `"$ulreLabelRegex`"） | 值 |")
[void]$md.AppendLine("|---|---|")
[void]$md.AppendLine("| 用例总数 | $ctestTotal |")
[void]$md.AppendLine("| 通过 | $ctestPassed |")
[void]$md.AppendLine("| 失败 | $ctestFailed |")
[void]$md.AppendLine("")
[void]$md.AppendLine("| 发行形态验收（ULRE_ENABLE_SHADERGEN=OFF） | 值 |")
[void]$md.AppendLine("|---|---|")
[void]$md.AppendLine("| 是否执行 | $([bool]$VerifyReleaseForm) |")
[void]$md.AppendLine("| 结果 | $(if (-not $VerifyReleaseForm) { '未执行' } elseif ($verifyRc -eq 0) { 'PASS' } else { "FAIL (rc=$verifyRc)" }) |")
[void]$md.AppendLine("")

if ($aErrors.Count -gt 0) {
    [void]$md.AppendLine("## A 层错误（必须修）")
    [void]$md.AppendLine("")
    [void]$md.AppendLine('```')
    foreach ($e in ($aErrors | Select-Object -First 60)) { [void]$md.AppendLine($e) }
    [void]$md.AppendLine('```')
    [void]$md.AppendLine("")
}
if ($uErrors.Count -gt 0) {
    [void]$md.AppendLine("## 未归类错误（需人工判定归属层）")
    [void]$md.AppendLine("")
    [void]$md.AppendLine('```')
    foreach ($e in ($uErrors | Select-Object -First 60)) { [void]$md.AppendLine($e) }
    [void]$md.AppendLine('```')
    [void]$md.AppendLine("")
}
if ($bErrors.Count -gt 0) {
    [void]$md.AppendLine("## B 层错误（上游 submodule，登记不修）")
    [void]$md.AppendLine("")
    [void]$md.AppendLine('```')
    foreach ($e in ($bErrors | Select-Object -First 60)) { [void]$md.AppendLine($e) }
    [void]$md.AppendLine('```')
    [void]$md.AppendLine("")
}

[System.IO.File]::WriteAllText($summaryMd, $md.ToString(), (New-Object System.Text.UTF8Encoding($false)))

Write-Host ""
Write-Host "=============================================================="
Write-Host "基线判定：$verdict"
foreach ($r in $reasons) { Write-Host "  - $r" }
Write-Host "  报告：$summaryMd"
Write-Host "  构建日志：$buildLog"
Write-Host "  测试日志：$testLog"
Write-Host "=============================================================="

if ($verdict -eq "PASS") { exit 0 }
exit 1
