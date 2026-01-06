@echo off
SET REPO="hyzgame"
SET BRANCH=master

git pull
call pull.bat CM2D %REPO% %BRANCH%
call pull.bat CMAssetsManage %REPO% %BRANCH%
call pull.bat CMCMakeModule %REPO% %BRANCH%
call pull.bat CMCore %REPO% %BRANCH%
call pull.bat CMCoreType %REPO% %BRANCH%
call pull.bat CMMath %REPO% %BRANCH%
call pull.bat CMUtil %REPO% %BRANCH%
