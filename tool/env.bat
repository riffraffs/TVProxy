@echo off
REM ===== TVProxy tool environment =====
REM Usage:  call tool\env.bat
REM Then java / adb / ndk-build / sdkmanager / emulator / gradle / gost are on PATH.

set "TOOL_ROOT=%~dp0"
if "%TOOL_ROOT:~-1%"=="\" set "TOOL_ROOT=%TOOL_ROOT:~0,-1%"
REM Emulator/SDK break on non-ASCII paths. Prefer the ASCII junction if present.
if exist "C:\TVProxy\tool\env.bat" set "TOOL_ROOT=C:\TVProxy\tool"

set "JAVA_HOME=%TOOL_ROOT%\jdk\17"
set "ANDROID_HOME=%TOOL_ROOT%\android-sdk"
set "ANDROID_SDK_ROOT=%ANDROID_HOME%"
set "NDK_HOME=%ANDROID_HOME%\ndk\27.3.13750724"
set "ANDROID_NDK_HOME=%NDK_HOME%"
set "ANDROID_AVD_HOME=%TOOL_ROOT%\avd"
set "ANDROID_USER_HOME=%TOOL_ROOT%\android-user"
set "GRADLE_HOME=%TOOL_ROOT%\gradle"
set "STUDIO_HOME=%TOOL_ROOT%\android-studio"

set "PATH=%JAVA_HOME%\bin;%ANDROID_HOME%\platform-tools;%ANDROID_HOME%\emulator;%ANDROID_HOME%\cmdline-tools\latest\bin;%NDK_HOME%;%GRADLE_HOME%\bin;%TOOL_ROOT%\gost;%STUDIO_HOME%\bin;%PATH%"
