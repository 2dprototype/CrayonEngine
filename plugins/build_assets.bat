@echo off
setlocal
rem Builds the assets plugin (assets.dll) standalone.
rem
rem Usage:
rem   plugins\build_assets.bat

set "HERE=%~dp0"
set "BUILD=%HERE%..\build\plugins"
set "OUT=%HERE%bin"

echo [*] Configuring plugins...
cmake -S "%HERE%." -B "%BUILD%" -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCRAYON_PLUGIN_OUTPUT_DIR="%OUT%"
if errorlevel 1 exit /b 1

echo [*] Building assets plugin...
cmake --build "%BUILD%" --target assets -j4
if errorlevel 1 (
    echo [ERROR] assets plugin build failed.
    exit /b 1
)

for %%D in ("%HERE%..\build" "%HERE%..") do (
    if exist "%%~D\crayon.exe" (
        copy /Y "%OUT%\assets.dll" "%%~D\" >nul 2>&1
        echo [OK] Copied assets.dll next to %%~D\crayon.exe
    )
)
echo [OK] assets.dll in %OUT%
