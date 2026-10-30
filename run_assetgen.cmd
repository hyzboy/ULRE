@echo off
setlocal
chcp 65001 >nul

set "SCRIPT_DIR=%~dp0"
set "PS_SCRIPT=%SCRIPT_DIR%scripts\run-assetgen.ps1"

if not exist "%PS_SCRIPT%" (
    echo [错误] 找不到脚本: %PS_SCRIPT%
    pause
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%PS_SCRIPT%" %*
set "EXIT_CODE=%ERRORLEVEL%"

rem 如果是双击直接打开（cmdcmdline中包含/c），且发生错误时暂停以查看日志
echo %cmdcmdline% | findstr /i /c:"%~nx0" >nul
if %errorlevel% equ 0 (
    if %EXIT_CODE% neq 0 (
        echo.
        echo 执行遇到错误，按任意键退出...
        pause >nul
    )
)

exit /b %EXIT_CODE%
