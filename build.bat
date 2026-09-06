@echo off
setlocal enabledelayedexpansion

if not exist "build\build.ninja" (
    echo [*] Configuring CMake...
    cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
    if %errorlevel% neq 0 exit /b %errorlevel%
)

echo [*] Building...
cmake --build build -j4

if %errorlevel% equ 0 (
    echo [OK] Build successful! Run: .\crayon_engine.exe
) else (
    echo [ERROR] Build failed.
)
