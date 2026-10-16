@echo off
setlocal
rem Builds physics4D.dll standalone.
set "HERE=%~dp0"
call "%HERE%..\build_physics4D.bat" %*
