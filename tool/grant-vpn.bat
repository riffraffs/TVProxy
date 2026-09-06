@echo off
REM Grant ACTIVATE_VPN. ASCII-only so cmd.exe does not break.
REM Usage:
REM   grant-vpn.bat
REM   grant-vpn.bat 192.168.3.16
setlocal EnableDelayedExpansion
set "HDC=%~dp0hdc\hdc.exe"
if not exist "%HDC%" set "HDC=D:\DevEcoStudio\sdk\default\openharmony\toolchains\hdc.exe"
if exist "%HDC%" goto :ready
echo hdc.exe not found
pause
exit /b 1

:ready
if "%~1"=="" goto :grant
echo tconn %~1
"%HDC%" tconn %~1
"%HDC%" tconn %~1:5555
"%HDC%" tconn %~1:8710

:grant
echo list targets
"%HDC%" list targets -v
echo.
echo grant ACTIVATE_VPN
"%HDC%" shell cmd appops set com.tvproxy ACTIVATE_VPN allow
if errorlevel 1 "%HDC%" shell appops set com.tvproxy ACTIVATE_VPN allow
echo.
echo current appops:
"%HDC%" shell cmd appops get com.tvproxy ACTIVATE_VPN
pause
exit /b 0
