@echo off
REM Wi-Fi HDC connect. ASCII-only so cmd.exe does not break.
REM Usage from this folder:
REM   hdc-wifi.bat 192.168.3.16
setlocal
set "HDC=%~dp0hdc\hdc.exe"
if not exist "%HDC%" set "HDC=D:\DevEcoStudio\sdk\default\openharmony\toolchains\hdc.exe"
if exist "%HDC%" goto :have_hdc
echo hdc.exe not found
pause
exit /b 1

:have_hdc
if not "%~1"=="" goto :connect
echo Usage: hdc-wifi.bat TV_IP
echo Example: hdc-wifi.bat 192.168.3.16
echo TV_IP is the LAN address on the TVProxy status card.
pause
exit /b 1

:connect
set "IP=%~1"
echo hdc=%HDC%
echo ip=%IP%
"%HDC%" kill >nul 2>nul
"%HDC%" start >nul 2>nul
echo tconn %IP%:5555
"%HDC%" tconn %IP%:5555
echo tconn %IP%:8710
"%HDC%" tconn %IP%:8710
echo tconn %IP%:10178
"%HDC%" tconn %IP%:10178
echo list targets
"%HDC%" list targets -v
echo.
echo If still Empty, this TV is not listening on Wi-Fi HDC.
pause
exit /b 0
