@echo off
setlocal
rem Builds the activation plugin (activation.dll) and tools/crayon_keygen.exe standalone.
rem
rem Usage:
rem   plugins\build_activation.bat
rem   plugins\build_activation.bat "your-secret-key"
rem   set CRAYON_ACTIVATION_SECRET=your-secret-key && plugins\build_activation.bat

set "HERE=%~dp0"
set "BUILD=%HERE%..\build\plugins"
set "OUT=%HERE%bin"

set "SECRET=%~1"
if "%SECRET%"=="" if defined CRAYON_ACTIVATION_SECRET set "SECRET=%CRAYON_ACTIVATION_SECRET%"

set "EXTRA_FLAGS="
if not "%SECRET%"=="" (
    set "EXTRA_FLAGS=-DCRAYON_ACTIVATION_SECRET=%SECRET%"
)

echo [*] Configuring plugins...
cmake -S "%HERE%." -B "%BUILD%" -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCRAYON_PLUGIN_OUTPUT_DIR="%OUT%" %EXTRA_FLAGS%
if errorlevel 1 exit /b 1

echo [*] Building activation plugin...
cmake --build "%BUILD%" --target activation -j4
if errorlevel 1 (
    echo [ERROR] activation plugin build failed.
    exit /b 1
)

for %%D in ("%HERE%..\build" "%HERE%..") do (
    if exist "%%~D\crayon.exe" (
        copy /Y "%OUT%\activation.dll" "%%~D\" >nul 2>&1
        echo [OK] Copied activation.dll next to %%~D\crayon.exe
    )
)
echo [OK] activation.dll in %OUT%
if exist "%OUT%\tools\crayon_keygen.exe" (
    echo [OK] crayon_keygen in %OUT%\tools\crayon_keygen.exe
)
