@echo off
call "%~dp0env.bat"
echo Starting AVD tvproxy-tv-1080p ...
echo SDK/AVD path: %ANDROID_HOME%
echo Remote keys: emulator sidebar DPAD / keyboard arrows.
emulator -avd tvproxy-tv-1080p -no-metrics %*
