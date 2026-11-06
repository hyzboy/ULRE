@echo off
rem EnumGen wrapper: Clang AST enum -> registry header (CMHash Algorithm etc.)
rem Keep this file ASCII-only: cmd.exe does not reliably honour a UTF-8 BOM.

setlocal
set "SCRIPT_DIR=%~dp0"
set "PS_SCRIPT=%SCRIPT_DIR%scripts\run-hashgen.ps1"

if not exist "%PS_SCRIPT%" (
    echo [EnumGen] script not found: %PS_SCRIPT%
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%PS_SCRIPT%" %*
exit /b %ERRORLEVEL%
