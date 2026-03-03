@echo off
setlocal

set BUILD_DIR=build
set CONFIG=Release
set OUTPUT_DIR=shadergen_materialpreset_dump
set GENERATOR=

if not "%~1"=="" set BUILD_DIR=%~1
if not "%~2"=="" set CONFIG=%~2
if not "%~3"=="" set OUTPUT_DIR=%~3
if not "%~4"=="" set GENERATOR=%~4

echo.
echo ========== Run MaterialPreset Exhaustive ==========
echo BUILD_DIR=%BUILD_DIR%
echo CONFIG=%CONFIG%
echo OUTPUT_DIR=%OUTPUT_DIR%
if not "%GENERATOR%"=="" echo GENERATOR=%GENERATOR%
echo.

set SCRIPT_DIR=%~dp0
if "%GENERATOR%"=="" (
    powershell -ExecutionPolicy Bypass -File "%SCRIPT_DIR%run_materialpreset_exhaustive.ps1" -BuildDir "%BUILD_DIR%" -Config "%CONFIG%" -OutputDir "%OUTPUT_DIR%"
) else (
    powershell -ExecutionPolicy Bypass -File "%SCRIPT_DIR%run_materialpreset_exhaustive.ps1" -BuildDir "%BUILD_DIR%" -Config "%CONFIG%" -OutputDir "%OUTPUT_DIR%" -Generator "%GENERATOR%"
)

if errorlevel 1 (
    echo.
    echo [FAIL] run_materialpreset_exhaustive.ps1 failed.
    exit /b 1
)

echo.
echo [PASS] run_materialpreset_exhaustive finished.
exit /b 0
