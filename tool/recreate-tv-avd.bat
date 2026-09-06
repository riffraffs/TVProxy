@echo off
setlocal
call "%~dp0env.bat"
echo TOOL_ROOT=%TOOL_ROOT%
echo ANDROID_HOME=%ANDROID_HOME%
echo ANDROID_AVD_HOME=%ANDROID_AVD_HOME%

if not exist "%ANDROID_AVD_HOME%" mkdir "%ANDROID_AVD_HOME%"

if exist "%ANDROID_AVD_HOME%\tvproxy-tv-1080p.avd.bak" rmdir /s /q "%ANDROID_AVD_HOME%\tvproxy-tv-1080p.avd.bak"
if exist "%ANDROID_AVD_HOME%\tvproxy-tv-1080p.ini.bak" del /q "%ANDROID_AVD_HOME%\tvproxy-tv-1080p.ini.bak"
if exist "%ANDROID_AVD_HOME%\tvproxy-tv-1080p.avd" move /y "%ANDROID_AVD_HOME%\tvproxy-tv-1080p.avd" "%ANDROID_AVD_HOME%\tvproxy-tv-1080p.avd.bak"
if exist "%ANDROID_AVD_HOME%\tvproxy-tv-1080p.ini" move /y "%ANDROID_AVD_HOME%\tvproxy-tv-1080p.ini" "%ANDROID_AVD_HOME%\tvproxy-tv-1080p.ini.bak"

echo no | avdmanager create avd -n tvproxy-tv-1080p -k system-images;android-36;android-tv;x86_64 -d tv_1080p -f
if errorlevel 1 (
  echo FAILED: avdmanager create
  exit /b 1
)

set "CFG=%ANDROID_AVD_HOME%\tvproxy-tv-1080p.avd\config.ini"
powershell -NoProfile -Command ^
  "$p='%CFG%'; $c=Get-Content -Raw $p;" ^
  "$c=$c -replace 'firstboot.bootFromDownloadableSnapshot=yes','firstboot.bootFromDownloadableSnapshot=no';" ^
  "$c=$c -replace 'hw.gpu.enabled=no','hw.gpu.enabled=yes';" ^
  "$c=$c -replace 'hw.gpu.mode=auto','hw.gpu.mode=host';" ^
  "$c=$c -replace 'hw.initialOrientation=portrait','hw.initialOrientation=landscape';" ^
  "$c=$c -replace 'hw.keyboard=no','hw.keyboard=yes';" ^
  "Set-Content -Path $p -Value $c -NoNewline -Encoding ASCII"

echo ----- created avd -----
type "%ANDROID_AVD_HOME%\tvproxy-tv-1080p.ini"
echo ----- config.ini -----
type "%ANDROID_AVD_HOME%\tvproxy-tv-1080p.avd\config.ini"
exit /b 0
