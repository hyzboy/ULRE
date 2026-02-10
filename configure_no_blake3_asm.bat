@echo off
REM This script reconfigures the ULRE project with BLAKE3 assembly disabled
REM Run this from the workspace root (e:\ULRE)

setlocal enabledelayedexpansion

echo.
echo ========================================
echo Reconfiguring ULRE with BLAKE3 Assembly Disabled
echo ========================================
echo.

REM Check if build directory exists
if not exist "build" (
    echo Creating build directory...
    mkdir build
)

cd /d "build"

REM Remove CMake cache to force reconfiguration
if exist "CMakeCache.txt" (
    echo Clearing CMake cache...
    del /f CMakeCache.txt
)

if exist "CMakeFiles" (
    echo Removing CMakeFiles directory...
    rmdir /s /q CMakeFiles
)

echo.
echo Running CMake with BLAKE3_SIMD_TYPE=none...
echo.

REM Run CMake with BLAKE3_SIMD_TYPE disabled
REM Note: Update VCPKG_ROOT path if vcpkg is installed in a different location
if not defined VCPKG_ROOT (
    set "VCPKG_ROOT=%USERPROFILE%\vcpkg"
)
if not exist "%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" (
    set "VCPKG_ROOT=E:\vcpkg"
)
if not exist "%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" (
    set "VCPKG_ROOT=C:\vcpkg"
)
if not exist "%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" (
    set "VCPKG_ROOT=C:\src\vcpkg"
)

cmake.exe -G "Visual Studio 18 2026" -A x64 ^
    -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" ^
    -DBLAKE3_SIMD_TYPE=none ^
    ..

if %errorlevel% neq 0 (
    echo.
    echo ERROR: CMake configuration failed!
    echo.
    cd ..
    exit /b 1
)

echo.
echo ========================================
echo Configuration complete!
echo.
echo Next steps:
echo 1. Open: ULRE.sln in Visual Studio
echo 2. Build the project (Ctrl+Shift+B)
echo ========================================
echo.

cd ..
