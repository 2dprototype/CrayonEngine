@echo off
setlocal
rem Builds assets.dll standalone from inside plugins\assets\.
set "HERE=%~dp0"
call "%HERE%..\build_assets.bat" %*
