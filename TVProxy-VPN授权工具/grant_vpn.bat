@echo off
setlocal
title TVProxy 一键跳过 VPN 授权工具
color 0A

set "PKG=com.tvproxy"
set "PORT=5555"

set "ADB=adb"
if exist "%~dp0adb.exe" set "ADB=%~dp0adb.exe"

echo.
echo  ==================================================
echo     TVProxy 电视端 VPN 授权一键工具
echo     (适用于无 VPN 授权弹窗的 HarmonyOS 电视)
echo  ==================================================
echo.

echo  [第 1 步] 在电视上开启"网络调试":
echo     华为智慧屏: 设置 - 系统 - 关于 - 连续点击版本号
echo     进入开发者选项后, 打开 "USB 调试" 与 "网络调试"
echo.
set /p "TVIP=  请输入电视 IP 地址 (默认端口 5555, 可输入 IP:端口): "
if "%TVIP%"=="" (
    echo  未输入 IP, 已退出.
    pause
    exit /b 1
)

REM 判断是否自带端口
echo %TVIP% | find ":" >nul
if errorlevel 1 (
    set "TARGET=%TVIP%:%PORT%"
) else (
    set "TARGET=%TVIP%"
)

echo.
echo  [第 2 步] 正在连接 %TARGET% ...
"%ADB%" connect %TARGET%

echo.
echo  [第 3 步] 请在电视屏幕上点击 "允许" 授权本电脑的调试请求.
echo     如果没有弹出提示, 请检查网络调试是否开启、IP 是否正确.
echo.
pause

echo  [第 4 步] 校验连接状态...
"%ADB%" -s %TARGET% shell echo __TVPROXY_LINK_OK__ > "%TEMP%\tvproxy_link.txt" 2>&1
findstr /C:"__TVPROXY_LINK_OK__" "%TEMP%\tvproxy_link.txt" >nul
if errorlevel 1 (
    echo.
    echo  [错误] 无法连接 %TARGET% .
    echo     常见原因: 电视未开启网络调试 / 未点允许 / IP 或端口错误.
    echo     电视系统版本: 
    "%ADB%" -s %TARGET% shell getprop ro.build.version.release 2>&1
    pause
    exit /b 1
)

echo  [第 5 步] 正在为 %PKG% 授予 VPN 权限...
"%ADB%" -s %TARGET% shell appops set %PKG% ACTIVATE_VPN allow >nul 2>&1
"%ADB%" -s %TARGET% shell cmd appops set %PKG% ACTIVATE_VPN allow >nul 2>&1

echo.
echo  授权结果 (应显示 ACTIVATE_VPN: allow):
"%ADB%" -s %TARGET% shell appops get %PKG% ACTIVATE_VPN

echo.
echo  ==================================================
echo   完成! 回到电视打开 TVProxy, 点击连接即可直接使用,
echo   不会再弹出 VPN 授权窗口.
echo.
echo   提示: 电视重启后若授权失效, 重新运行本脚本即可.
echo  ==================================================
echo.
pause
