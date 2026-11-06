@echo off
setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set REPO_ROOT=%SCRIPT_DIR%..
set TOOLS_DIR=%REPO_ROOT%\Tools\TexConv
set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Release

set OUT_DIR=%REPO_ROOT%\build\out\Windows_64_%CONFIG%

if not exist "%OUT_DIR%" (
    echo [ERROR] Build directory not found: %OUT_DIR%
    exit /b 1
)

if not exist "%TOOLS_DIR%" mkdir "%TOOLS_DIR%"

echo Copying TexConv binaries from %OUT_DIR% to %TOOLS_DIR% ...

for %%F in (TexConv.exe TexConvCore.dll TexImage.dll TexEncAMD.dll TexEncIntel.dll TexConvQt.exe) do (
    if exist "%OUT_DIR%\%%F" (
        copy /y "%OUT_DIR%\%%F" "%TOOLS_DIR%\%%F" >nul
        echo   Copied %%F
    )
)

if exist "%OUT_DIR%\modules" (
    xcopy /y /e /i /q "%OUT_DIR%\modules" "%TOOLS_DIR%\modules" >nul
    echo   Copied modules/
)

echo Done.
exit /b 0
