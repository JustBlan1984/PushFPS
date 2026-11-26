@echo off
setlocal
title VIctory - conservative comparison - 1440p RTX 5070 Ti
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Start-VIctory-NVIDIA.ps1" -Mode Safe
echo.
pause
