@echo off
setlocal enabledelayedexpansion

:: Setup Java 17, Android SDK and Gradle paths
if exist "D:\jdk-17.0.12" (
    set "JAVA_HOME=D:\jdk-17.0.12"
) else if exist "C:\jdk-17.0.12" (
    set "JAVA_HOME=C:\jdk-17.0.12"
)

if exist "D:\Android\Sdk" (
    set "ANDROID_HOME=D:\Android\Sdk"
) else if exist "C:\Android\Sdk" (
    set "ANDROID_HOME=C:\Android\Sdk"
)

if exist "D:\gradle-8.7\bin" (
    set "PATH=D:\gradle-8.7\bin;%PATH%"
) else if exist "C:\gradle-8.7\bin" (
    set "PATH=C:\gradle-8.7\bin;%PATH%"
)

if defined JAVA_HOME set "PATH=%JAVA_HOME%\bin;%PATH%"
if defined ANDROID_HOME set "PATH=%ANDROID_HOME%\platform-tools;%PATH%"

set "ROOT_DIR=%~dp0"
set "APK_PATH=%ROOT_DIR%android\app\build\outputs\apk\debug\app-debug.apk"

echo ===================================================
echo   Crayon Engine - Android Build
echo ===================================================
echo [*] JAVA_HOME:    %JAVA_HOME%
echo [*] ANDROID_HOME: %ANDROID_HOME%
echo.

:: Ensure local.properties exists
if not exist "%ROOT_DIR%android\local.properties" (
    echo [*] Creating android\local.properties...
    (
        echo sdk.dir=%ANDROID_HOME:\=\\%
        echo ndk.dir=%ANDROID_HOME:\=\\%\\ndk\\27.1.12297006
    ) > "%ROOT_DIR%android\local.properties"
)

:: Build action
set "ACTION=%1"

if /i "%ACTION%"=="install" (
    echo [*] Building and installing APK to connected device...
    pushd "%ROOT_DIR%android"
    call gradle installDebug
    set "BUILD_STATUS=!errorlevel!"
    popd
) else if /i "%ACTION%"=="clean" (
    echo [*] Cleaning Android build...
    pushd "%ROOT_DIR%android"
    call gradle clean
    set "BUILD_STATUS=!errorlevel!"
    popd
) else (
    echo [*] Building debug APK (assembleDebug)...
    pushd "%ROOT_DIR%android"
    call gradle assembleDebug
    set "BUILD_STATUS=!errorlevel!"
    popd
)

if !BUILD_STATUS! equ 0 (
    echo.
    echo [OK] Android build successful!
    if exist "%APK_PATH%" (
        echo [OK] APK: %APK_PATH%
    )
    if /i "%ACTION%"=="install" (
        echo [OK] Installed on connected device.
    ) else (
        echo [*] To install on your phone, connect USB and run:
        echo     build_android.bat install
        echo     or: adb install -r "%APK_PATH%"
    )
) else (
    echo.
    echo [ERROR] Android build failed.
    exit /b !BUILD_STATUS!
)
