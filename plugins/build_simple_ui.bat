@echo off
setlocal
rem Builds the simple_ui plugin (simple_ui.dll) standalone.
rem
rem Usage:
rem   plugins\build_simple_ui.bat

set "HERE=%~dp0"
set "BUILD=%HERE%..\build\plugins"
set "OUT=%HERE%bin"

echo [*] Configuring plugins...
cmake -S "%HERE%." -B "%BUILD%" -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCRAYON_PLUGIN_OUTPUT_DIR="%OUT%"
if errorlevel 1 exit /b 1

echo [*] Building simpleUI plugin...
cmake --build "%BUILD%" --target simpleUI -j4
if errorlevel 1 (
    echo [ERROR] simpleUI plugin build failed.
    exit /b 1
)

for %%D in ("%HERE%..\build" "%HERE%..") do (
    if exist "%%~D\crayon.exe" (
        copy /Y "%OUT%\simpleUI.dll" "%%~D\" >nul 2>&1
        echo [OK] Copied simpleUI.dll next to %%~D\crayon.exe
    )
)
echo [OK] simpleUI.dll in %OUT%
