@echo off
cd %1
git.exe pull -v --no-rebase -- %2 %3
cd ..
