@echo off
setlocal
rem Builds the Crayon native plugins (plugins\*\) on their own, without building the engine.
rem
rem   plugins\build.bat               build every plugin
rem   plugins\build.bat physics4D     build one plugin
rem
rem Output: plugins\bin\<name>.dll, copied next to crayon.exe (build\ and the repo root) when those exist.

set "HERE=%~dp0"
set "BUILD=%HERE%..\build\plugins"
set "OUT=%HERE%bin"

if not exist "%BUILD%\build.ninja" (
    echo [*] Configuring plugins...
    cmake -S "%HERE%." -B "%BUILD%" -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCRAYON_PLUGIN_OUTPUT_DIR="%OUT%"
    if errorlevel 1 exit /b 1
)

if "%~1"=="" (
    echo [*] Building all plugins...
    cmake --build "%BUILD%" -j4
) else (
    echo [*] Building plugin %~1...
    cmake --build "%BUILD%" --target %~1 -j4
)
if errorlevel 1 (
    echo [ERROR] Plugin build failed.
    exit /b 1
)

for %%D in ("%HERE%..\build" "%HERE%..") do (
    if exist "%%~D\crayon.exe" (
        copy /Y "%OUT%\*.dll" "%%~D\" >nul
        echo [OK] Copied plugins next to %%~D\crayon.exe
    )
)
echo [OK] Plugins in %OUT%
