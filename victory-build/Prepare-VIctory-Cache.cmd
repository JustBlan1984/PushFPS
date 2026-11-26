@echo off
setlocal
title VIctory - prepare shader and pipeline cache
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Start-VIctory-NVIDIA.ps1" -Action PrepareCache
echo.
pause
