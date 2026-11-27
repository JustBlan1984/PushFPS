@echo off
setlocal
title VIctory - timed CPU sampling - 1080p RTX 5070 Ti
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Start-VIctory-NVIDIA.ps1" -Profile
echo.
pause
