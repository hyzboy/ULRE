@echo off
setlocal

set MODE=smoke
set BUILDDIR=build\windows-msvc-debug
set CONFIG=Debug

if /I "%~1"=="extended" set MODE=extended
if /I "%~1"=="smoke" set MODE=smoke
if not "%~2"=="" set BUILDDIR=%~2
if not "%~3"=="" set CONFIG=%~3

if /I "%MODE%"=="extended" (
    set TARGET=test_ShaderGenProfileParityExtended
    set REPORT=doc\shader-system\baseline\shadergen_profile_parity_extended_latest.json
) else (
    set TARGET=test_ShaderGenProfileParitySmoke
    set REPORT=doc\shader-system\baseline\shadergen_profile_parity_latest.json
)

echo [ShaderGenParity] Mode   : %MODE%
echo [ShaderGenParity] Target : %TARGET%
echo [ShaderGenParity] Build  : %BUILDDIR% (%CONFIG%)
echo [ShaderGenParity] Command: cmake --build %BUILDDIR% --config %CONFIG% --target %TARGET%

cmake --build %BUILDDIR% --config %CONFIG% --target %TARGET%
if errorlevel 1 (
    echo [ShaderGenParity] FAIL
    exit /b 1
)

echo [ShaderGenParity] PASS
echo [ShaderGenParity] Report : %REPORT%
exit /b 0
