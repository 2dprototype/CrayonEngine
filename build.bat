@echo off
setlocal enabledelayedexpansion

if not exist "build\build.ninja" (
    echo [*] Configuring CMake...
    cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
    if %errorlevel% neq 0 exit /b %errorlevel%
)

set TARGET=%1

if "%TARGET%"=="" (
    echo [*] Building both crayon.exe and editor.exe...
    cmake --build build -j4
) else (
    echo [*] Building %TARGET%...
    cmake --build build --target %TARGET% -j4
)

if %errorlevel% equ 0 (
    echo [OK] Build successful!
    echo   Player: .\build\crayon.exe (or .\crayon.exe)
    echo   Editor: .\build\editor.exe (or .\editor.exe)
) else (
    echo [ERROR] Build failed.
)
