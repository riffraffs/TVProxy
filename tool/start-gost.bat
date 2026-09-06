@echo off
call "%~dp0env.bat"
if "%~1"=="" (
  echo Starting gost SOCKS5 on :10800  ^(emulator host is 10.0.2.2^)
  echo Port 1080 is often reserved by Hyper-V on Windows.
  echo Also HTTP:  tool\start-gost.bat -L http://:8080
  gost -L "socks5://:10800?udp=true"
) else (
  gost %*
)
