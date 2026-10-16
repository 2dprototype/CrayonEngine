@echo off
setlocal
rem Builds simple_ui.dll standalone from inside plugins\simple_ui\.
set "HERE=%~dp0"
call "%HERE%..\build_simple_ui.bat" %*
