@echo off
cd %1
git.exe pull -v --no-rebase -- github %2
git.exe pull -v --no-rebase -- hyzgame %2
cd ..
