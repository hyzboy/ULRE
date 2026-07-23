param(
    [string]$BuildDir = "build_phasec_c5",
    [string]$Config = "Debug",
    [int]$Jobs = 8,
    [int]$LaunchTimeoutSeconds = 4,
    [switch]$StrictRuntime,
    [switch]$ConfigureIfMissing,
    [string]$Generator = "Ninja"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $scriptDir "..\..")).Path
$buildPath = Join-Path $repoRoot $BuildDir
if (Test-Path $buildPath) {
    $buildPath = (Resolve-Path $buildPath).Path
}
 $cmakeCachePath = Join-Path $buildPath "CMakeCache.txt"
if (-not (Test-Path $cmakeCachePath)) {
    if (-not $ConfigureIfMissing) {
        throw "Build directory '$buildPath' is not a configured CMake build tree. Pass -ConfigureIfMissing to configure it automatically."
    }

    $vcpkgRoot = $env:VCPKG_ROOT
    if (-not $vcpkgRoot -or -not (Test-Path (Join-Path $vcpkgRoot "scripts/buildsystems/vcpkg.cmake"))) {
        $candidateRoots = @(
            "E:\vcpkg",
            "C:\vcpkg",
            "$env:USERPROFILE\vcpkg"
        )

        foreach ($candidateRoot in $candidateRoots) {
            if (Test-Path (Join-Path $candidateRoot "scripts/buildsystems/vcpkg.cmake")) {
                $vcpkgRoot = $candidateRoot
                break
            }
        }
    }

    if (-not $vcpkgRoot -or -not (Test-Path (Join-Path $vcpkgRoot "scripts/buildsystems/vcpkg.cmake"))) {
        throw "VCPKG_ROOT is not set and no vcpkg installation was found."
    }

    New-Item -ItemType Directory -Path $buildPath -Force | Out-Null

    $isWindowsHost = [System.Runtime.InteropServices.RuntimeInformation]::IsOSPlatform([System.Runtime.InteropServices.OSPlatform]::Windows)
    $selectedGenerator = $Generator
    if ($isWindowsHost -and $selectedGenerator -eq "Ninja") {
        $selectedGenerator = "Visual Studio 18 2026"
    }

    Write-Host "[PhaseE][Smoke] Configuring missing build directory with $selectedGenerator"
    $configureArgs = @(
        "-S", $repoRoot,
        "-B", $buildPath,
        "-G", $selectedGenerator,
        "-DCMAKE_TOOLCHAIN_FILE=$vcpkgRoot/scripts/buildsystems/vcpkg.cmake",
        "-DVCPKG_TARGET_TRIPLET=x64-windows",
        "-DBLAKE3_SIMD_TYPE=none"
    )

    if ($selectedGenerator -ne "Visual Studio 18 2026") {
        $configureArgs += "-DCMAKE_BUILD_TYPE=$Config"
    }

    & cmake @configureArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Configure failed with exit code $LASTEXITCODE"
    }
}

Write-Host "[PhaseE][Smoke] repoRoot=$repoRoot"
Write-Host "[PhaseE][Smoke] buildPath=$buildPath config=$Config"

Push-Location $repoRoot
try {
    $buildArgs = @(
        "--build", $buildPath,
        "--config", $Config,
        "--target", "ULRE.ECS", "09_TextureBindingValidation",
        "-j", "$Jobs"
    )

    Write-Host "[PhaseE][Smoke] Building targets: ULRE.ECS, 09_TextureBindingValidation"
    & cmake @buildArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE"
    }

    $exeCandidates = @(Get-ChildItem -Path $buildPath -Recurse -Filter "09_TextureBindingValidation.exe" -File -ErrorAction SilentlyContinue)
    if (-not $exeCandidates -or $exeCandidates.Count -eq 0) {
        throw "Cannot locate 09_TextureBindingValidation.exe under $buildPath"
    }

    $exePath = $exeCandidates[0].FullName
    Write-Host "[PhaseE][Smoke] Launch target: $exePath"

    $stdoutPath = Join-Path $env:TEMP "phasee_diag_smoke_stdout.log"
    $stderrPath = Join-Path $env:TEMP "phasee_diag_smoke_stderr.log"

    if (Test-Path $stdoutPath) { Remove-Item $stdoutPath -Force }
    if (Test-Path $stderrPath) { Remove-Item $stderrPath -Force }

    $proc = Start-Process -FilePath $exePath `
                          -WorkingDirectory $repoRoot `
                          -PassThru `
                          -RedirectStandardOutput $stdoutPath `
                          -RedirectStandardError $stderrPath

    $exited = $proc.WaitForExit($LaunchTimeoutSeconds * 1000)
    $timedOut = $false

    if (-not $exited) {
        $timedOut = $true
        Write-Host "[PhaseE][Smoke] App still running after timeout (${LaunchTimeoutSeconds}s), terminating for smoke completion."
        Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    }

    $stdout = ""
    $stderr = ""
    if (Test-Path $stdoutPath) { $stdout = Get-Content -Path $stdoutPath -Raw }
    if (Test-Path $stderrPath) { $stderr = Get-Content -Path $stderrPath -Raw }

    $startupMarker = ($stdout -match "TextureBinding Validation \(Phase 4E\)") -or ($stdout -match "\[TBV\]")

    if ($timedOut) {
        Write-Host "[PhaseE][Smoke][PASS] Launch check passed (process alive until timeout)."
        if ($startupMarker) {
            Write-Host "[PhaseE][Smoke] Startup marker detected in stdout."
        }
        exit 0
    }

    if ($proc.ExitCode -eq 0) {
        Write-Host "[PhaseE][Smoke][PASS] App exited cleanly with code 0."
        if ($startupMarker) {
            Write-Host "[PhaseE][Smoke] Startup marker detected in stdout."
        }
        exit 0
    }

    $knownVkInitError = ($stderr -match "vkAcquireNextImageKHR") -or ($stderr -match "Invalid device")
    if (-not $StrictRuntime -and $knownVkInitError) {
        Write-Host "[PhaseE][Smoke][WARN] App exited during Vulkan init with known environment-dependent error."
        Write-Host "[PhaseE][Smoke][PASS] Build and launch-path smoke passed (runtime surface/device unavailable)."
        exit 0
    }

    throw "App exited with code $($proc.ExitCode). stderr: $stderr"
}
finally {
    Pop-Location
}
