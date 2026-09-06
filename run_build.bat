@echo off
call C:\TVProxy\tool\env.bat
cd /d C:\TVProxy
gradlew.bat -Pabi=x86_64 assembleDebug --console=plain
echo BUILD_EXITCODE=%ERRORLEVEL%
