@echo off
setlocal
rem Builds devKit.dll standalone from inside plugins\devKit\.
set "HERE=%~dp0"
call "%HERE%..\build_devKit.bat" %*
