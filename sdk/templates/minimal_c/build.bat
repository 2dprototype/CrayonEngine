@echo off
setlocal
rem Builds myplugin.dll standalone using MSVC (cl) or GCC/Clang.
rem No engine build or CMake needed.

set "SDK=%~dp0..\.."

where cl >nul 2>&1
if %errorlevel% equ 0 (
    echo [*] Building with MSVC (cl)...
    cl /nologo /LD /O2 plugin.c /I"%SDK%\include" "%SDK%\lib\lua51.lib" /link /OUT:myplugin.dll
    goto :done
)

where gcc >nul 2>&1
if %errorlevel% equ 0 (
    echo [*] Building with GCC...
    gcc -shared -O3 plugin.c -I"%SDK%\include" -L"%SDK%\lib" -llua51 -s -o myplugin.dll
    goto :done
)

where clang >nul 2>&1
if %errorlevel% equ 0 (
    echo [*] Building with Clang...
    clang -shared -O3 plugin.c -I"%SDK%\include" -L"%SDK%\lib" -llua51 -s -o myplugin.dll
    goto :done
)

echo [ERROR] No suitable C compiler (cl, gcc, or clang) found in PATH.
exit /b 1

:done
if exist myplugin.dll (
    echo [OK] myplugin.dll built successfully!
    echo Put myplugin.dll next to crayon.exe or in your project folder, and add:
    echo   function crayon.config(t) t.plugins.myplugin = true end
)
