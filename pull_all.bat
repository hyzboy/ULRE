@echo off
SET REPO="hyzgame"
SET BRANCH=master

call pull.bat CMAssetsManage %REPO% %BRANCH%
call pull.bat CMCMakeModule %REPO% %BRANCH%
call pull.bat CMCore %REPO% %BRANCH%
call pull.bat CMPlatform %REPO% %BRANCH%
call pull.bat CMSceneGraph %REPO% %BRANCH%
call pull.bat CMUtil %REPO% %BRANCH%
