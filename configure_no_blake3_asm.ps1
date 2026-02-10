# This script reconfigures the ULRE project with BLAKE3 assembly disabled
# Run this from the workspace root (e:\ULRE)

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Reconfiguring ULRE with BLAKE3 Assembly Disabled" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Check if build directory exists
if (-not (Test-Path "build")) {
    Write-Host "Creating build directory..." -ForegroundColor Yellow
    New-Item -ItemType Directory -Path "build" -Force | Out-Null
}

Push-Location "build"

# Remove CMake cache to force reconfiguration
if (Test-Path "CMakeCache.txt") {
    Write-Host "Clearing CMake cache..." -ForegroundColor Yellow
    Remove-Item "CMakeCache.txt" -Force
}

if (Test-Path "CMakeFiles") {
    Write-Host "Removing CMakeFiles directory..." -ForegroundColor Yellow
    Remove-Item "CMakeFiles" -Recurse -Force
}

Write-Host ""
Write-Host "Running CMake with BLAKE3_SIMD_TYPE=none..." -ForegroundColor Cyan
Write-Host ""

# Detect vcpkg installation path
$vcpkgRoot = $env:VCPKG_ROOT
if (-not $vcpkgRoot -or -not (Test-Path "$vcpkgRoot\scripts\buildsystems\vcpkg.cmake")) {
    $possiblePaths = @(
        "E:\vcpkg",
        "$env:USERPROFILE\vcpkg",
        "C:\vcpkg",
        "C:\src\vcpkg"
    )
    foreach ($path in $possiblePaths) {
        if (Test-Path "$path\scripts\buildsystems\vcpkg.cmake") {
            $vcpkgRoot = $path
            break
        }
    }
}

if (-not $vcpkgRoot -or -not (Test-Path "$vcpkgRoot\scripts\buildsystems\vcpkg.cmake")) {
    Write-Host "WARNING: vcpkg not found. Please set VCPKG_ROOT environment variable or install vcpkg." -ForegroundColor Yellow
    Write-Host "Continuing without vcpkg toolchain file..." -ForegroundColor Yellow
}

# Run CMake with BLAKE3_SIMD_TYPE disabled
$cmakeArgs = @(
    "-G", "Visual Studio 18 2026"
    "-A", "x64"
)

if ($vcpkgRoot -and (Test-Path "$vcpkgRoot\scripts\buildsystems\vcpkg.cmake")) {
    Write-Host "Using vcpkg from: $vcpkgRoot" -ForegroundColor Green
    $cmakeArgs += "-DCMAKE_TOOLCHAIN_FILE=$vcpkgRoot\scripts\buildsystems\vcpkg.cmake"
}

$cmakeArgs += @(
    "-DBLAKE3_SIMD_TYPE=none"
    ".."
)

& cmake.exe @cmakeArgs

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "ERROR: CMake configuration failed!" -ForegroundColor Red
    Write-Host ""
    Pop-Location
    exit 1
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "Configuration complete!" -ForegroundColor Green
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Cyan
Write-Host "1. Open: ULRE.sln in Visual Studio" -ForegroundColor White
Write-Host "2. Build the project (Ctrl+Shift+B)" -ForegroundColor White
Write-Host "========================================" -ForegroundColor Green
Write-Host ""

Pop-Location
