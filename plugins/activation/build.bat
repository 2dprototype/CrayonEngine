@echo off
setlocal
rem Builds activation.dll and tools/crayon_keygen.exe standalone.
set "HERE=%~dp0"
call "%HERE%..\build_activation.bat" %*
