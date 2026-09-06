@echo off
REM Launch Android Studio with this repo's JDK / SDK / AVD paths.
call "%~dp0env.bat"
if not exist "%STUDIO_HOME%\bin\studio64.exe" (
  echo Android Studio not found at "%STUDIO_HOME%\bin\studio64.exe"
  exit /b 1
)
start "Android Studio" "%STUDIO_HOME%\bin\studio64.exe" %*
