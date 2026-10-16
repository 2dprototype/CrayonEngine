@echo off
setlocal
rem Builds the physics4D plugin (physics4D.dll) standalone.
rem
rem Usage:
rem   plugins\build_physics4D.bat

set "HERE=%~dp0"
call "%HERE%build.bat" physics4D %*
