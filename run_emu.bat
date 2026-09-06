@echo off
call C:\TVProxy\tool\env.bat
cd /d C:\TVProxy
emulator -avd tvproxy-tv-1080p -no-boot-anim
echo EMU_EXITCODE=%ERRORLEVEL%
