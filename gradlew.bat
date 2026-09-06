@echo off
setlocal
call "%~dp0tool\env.bat"
if errorlevel 1 exit /b 1
call "%GRADLE_HOME%\bin\gradle.bat" %*
exit /b %ERRORLEVEL%
