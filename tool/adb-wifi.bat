@echo off
REM Connect HarmonyOS TV over Wi-Fi ADB, then grant VPN.
REM Usage: adb-wifi.bat 192.168.3.51
setlocal
set "ADB=%~dp0android-sdk\platform-tools\adb.exe"
if not exist "%ADB%" set "ADB=%~dp0..\tool\android-sdk\platform-tools\adb.exe"
if exist "%ADB%" goto :ok
echo adb.exe not found
pause
exit /b 1

:ok
if "%~1"=="" (
  echo Usage: adb-wifi.bat TV_IP
  echo Example: adb-wifi.bat 192.168.3.51
  pause
  exit /b 1
)
set "IP=%~1"
echo adb=%ADB%
echo ip=%IP%
"%ADB%" start-server
echo connect %IP%:5555
"%ADB%" connect %IP%:5555
echo.
echo devices
"%ADB%" devices -l
echo.
echo If status is unauthorized, look at the TV and allow debugging.
echo Then run this bat again.
echo.
echo If status is device, granting ACTIVATE_VPN ...
"%ADB%" -s %IP%:5555 shell cmd appops set com.tvproxy ACTIVATE_VPN allow
"%ADB%" -s %IP%:5555 shell appops set com.tvproxy ACTIVATE_VPN allow
echo.
echo appops:
"%ADB%" -s %IP%:5555 shell cmd appops get com.tvproxy ACTIVATE_VPN
pause
exit /b 0
