@echo off
REM Clean + rebuild helper for ShaderGen and BasicLitSunDirectionECS
REM Run from repository root (e:\ULRE)

setlocal enabledelayedexpansion

echo.
echo ========================================
echo Clean and Rebuild (ShaderGen + BasicLitSunDirectionECS)
echo ========================================
echo.

if not exist "build" (
    echo ERROR: build directory not found. Please configure CMake first.
    exit /b 1
)

echo [Step] clean
echo [Cmd ] cmake --build build --config Debug --target clean
cmake --build build --config Debug --target clean
if %errorlevel% neq 0 (
    echo FAILED at step: clean (exit code: %errorlevel%)
    exit /b %errorlevel%
)
echo OK: clean
echo.

echo [Step] ULRE.ShaderGen
echo [Cmd ] cmake --build build --config Debug --target ULRE.ShaderGen -- /m
cmake --build build --config Debug --target ULRE.ShaderGen -- /m
if %errorlevel% neq 0 (
    echo FAILED at step: ULRE.ShaderGen (exit code: %errorlevel%)
    exit /b %errorlevel%
)
echo OK: ULRE.ShaderGen
echo.

echo [Step] 03_BasicLitSunDirectionECS
echo [Cmd ] cmake --build build --config Debug --target 03_BasicLitSunDirectionECS -- /m
cmake --build build --config Debug --target 03_BasicLitSunDirectionECS -- /m
if %errorlevel% neq 0 (
    echo FAILED at step: 03_BasicLitSunDirectionECS (exit code: %errorlevel%)
    exit /b %errorlevel%
)
echo OK: 03_BasicLitSunDirectionECS
echo.

echo ========================================
echo All build steps completed successfully.
echo ========================================

exit /b 0
