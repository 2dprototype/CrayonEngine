@echo off
setlocal enabledelayedexpansion

:: ===================================================
::   Crayon Engine - Android Build & Dev Utility
:: ===================================================

:: 1. Setup Java 17 path
if not defined JAVA_HOME (
    if exist "D:\jdk-17.0.12" (
        set "JAVA_HOME=D:\jdk-17.0.12"
    ) else if exist "C:\jdk-17.0.12" (
        set "JAVA_HOME=C:\jdk-17.0.12"
    )
)

:: 2. Setup Android SDK path
if not defined ANDROID_HOME (
    if exist "D:\Android\Sdk" (
        set "ANDROID_HOME=D:\Android\Sdk"
    ) else if exist "C:\Android\Sdk" (
        set "ANDROID_HOME=C:\Android\Sdk"
    )
)

:: 3. Setup Gradle path
if exist "D:\gradle-8.7\bin" (
    set "PATH=D:\gradle-8.7\bin;%PATH%"
) else if exist "C:\gradle-8.7\bin" (
    set "PATH=C:\gradle-8.7\bin;%PATH%"
)

if defined JAVA_HOME set "PATH=%JAVA_HOME%\bin;%PATH%"
if defined ANDROID_HOME set "PATH=%ANDROID_HOME%\platform-tools;%PATH%"

set "ROOT_DIR=%~dp0"
set "APK_PATH=%ROOT_DIR%android\app\build\outputs\apk\debug\app-debug.apk"
set "PACKAGE_NAME=com.crayonengine.player"
set "ACTIVITY_NAME=.CrayonActivity"

:: Check for direct logcat command
if /i "%1"=="log" goto :do_log
if /i "%1"=="logcat" goto :do_log

:: Check for push command
if /i "%1"=="push" goto :do_push

:: Check for help command
if /i "%1"=="help" goto :do_help
if /i "%1"=="-h" goto :do_help
if /i "%1"=="/?" goto :do_help

echo ===================================================
echo   Crayon Engine - Android Build
echo ===================================================
echo [*] JAVA_HOME:    %JAVA_HOME%
echo [*] ANDROID_HOME: %ANDROID_HOME%
echo.

:: Ensure local.properties exists
if not exist "%ROOT_DIR%android\local.properties" (
    echo [*] Generating android\local.properties...
    (
        echo sdk.dir=%ANDROID_HOME:\=\\%
    ) > "%ROOT_DIR%android\local.properties"
)

set "ACTION=%1"
set "PARAM=%2"

if /i "%ACTION%"=="clean" (
    echo [*] Cleaning Android build...
    pushd "%ROOT_DIR%android"
    call gradle clean
    set "BUILD_STATUS=!errorlevel!"
    popd
    exit /b !BUILD_STATUS!
)

:: Build debug APK
echo [*] Building debug APK (assembleDebug)...
pushd "%ROOT_DIR%android"
call gradle assembleDebug
set "BUILD_STATUS=!errorlevel!"
popd

if !BUILD_STATUS! neq 0 (
    echo.
    echo [ERROR] Android build failed.
    exit /b !BUILD_STATUS!
)

echo.
echo [OK] Android build successful!
if exist "%APK_PATH%" (
    echo [OK] APK Output: %APK_PATH%
)

:: Handle Install or Run actions
if /i "%ACTION%"=="install" goto :do_install
if /i "%ACTION%"=="run" goto :do_run

echo.
echo [*] Handy commands:
echo     build_android.bat install         - Install APK to phone via USB
echo     build_android.bat run [path]      - Install and launch game on phone
echo     build_android.bat log             - Stream engine logs / print() from phone
echo     build_android.bat push ^<folder^>   - Copy game folder to /sdcard/Crayon/ on phone
goto :eof

:do_install
echo.
echo [*] Installing APK onto connected device...
adb install -r "%APK_PATH%"
if %errorlevel% equ 0 (
    echo [OK] Successfully installed to device!
) else (
    echo [ERROR] Failed to install via ADB. Ensure USB Debugging is ON and authorized.
)
goto :eof

:do_run
call :do_install
if %errorlevel% neq 0 goto :eof
echo.
if not "%PARAM%"=="" (
    echo [*] Launching Crayon Engine with target: %PARAM%
    adb shell am start -n %PACKAGE_NAME%/%ACTIVITY_NAME% -e game "%PARAM%"
) else (
    echo [*] Launching Crayon Engine...
    adb shell am start -n %PACKAGE_NAME%/%ACTIVITY_NAME%
)
echo [*] Streaming logcat (Ctrl+C to stop)...
adb logcat -s Crayon
goto :eof

:do_log
echo [*] Listening for Crayon logs (Ctrl+C to exit)...
adb logcat -s Crayon
goto :eof

:do_push
if "%PARAM%"=="" (
    echo [ERROR] Usage: build_android.bat push ^<local_game_folder^>
    echo Example: build_android.bat push examples\games\air_strike
    exit /b 1
)
for %%F in ("%PARAM%") do set "FOLDER_NAME=%%~nxF"
echo [*] Pushing "%PARAM%" to /sdcard/Crayon/%FOLDER_NAME%/ ...
adb shell mkdir -p "/sdcard/Crayon/%FOLDER_NAME%"
adb push "%PARAM%\." "/sdcard/Crayon/%FOLDER_NAME%/"
if %errorlevel% equ 0 (
    echo [OK] Game pushed successfully!
    echo [*] You can run it with:
    echo     build_android.bat run /sdcard/Crayon/%FOLDER_NAME%
) else (
    echo [ERROR] Failed to push files to device.
)
goto :eof

:do_help
echo ===================================================
echo   Crayon Engine Android Build Tool
echo ===================================================
echo Usage:
echo   build_android.bat                 Build the debug APK
echo   build_android.bat install         Build and install APK via ADB
echo   build_android.bat run [path]      Build, install, and launch on phone
echo   build_android.bat log             Stream engine logcat (crayon.print output)
echo   build_android.bat push ^<folder^>   Push a local game to /sdcard/Crayon/ on phone
echo   build_android.bat clean           Clean Android build files
goto :eof
