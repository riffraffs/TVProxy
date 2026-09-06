@echo off
setlocal
call "%~dp0env.bat"

echo ===== TVProxy dev-env check =====
echo TOOL_ROOT=%TOOL_ROOT%
echo.

set FAIL=0

echo [java]
java -version 2>&1 | findstr /C:"17." >nul
if errorlevel 1 (
  echo FAIL: java is not 17
  set FAIL=1
) else (
  java -version 2>&1
  echo OK
)
echo.

echo [adb]
adb --version
if errorlevel 1 (echo FAIL: adb && set FAIL=1) else echo OK
echo.

echo [ndk-build]
call ndk-build --version
if errorlevel 1 (echo FAIL: ndk-build && set FAIL=1) else echo OK
echo.

echo [emulator]
emulator -version 2>&1 | findstr /C:"Android emulator version"
if errorlevel 1 (echo FAIL: emulator && set FAIL=1) else echo OK
emulator -list-avds
echo.

echo [gradle]
call gradle -v
if errorlevel 1 (echo FAIL: gradle && set FAIL=1) else echo OK
echo.

echo [gost]
gost -V
if errorlevel 1 (echo FAIL: gost && set FAIL=1) else echo OK
echo.

echo [android studio]
if exist "%STUDIO_HOME%\bin\studio64.exe" (
  echo OK  %STUDIO_HOME%\bin\studio64.exe
) else (
  echo FAIL: studio64.exe missing
  set FAIL=1
)
echo.

echo [accel]
"%ANDROID_HOME%\emulator\emulator-check.exe" accel
echo.

if %FAIL%==0 (
  echo ===== ALL CHECKS PASSED =====
  exit /b 0
) else (
  echo ===== SOME CHECKS FAILED =====
  exit /b 1
)
