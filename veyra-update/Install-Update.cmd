@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Update-Veyra.ps1" %*
pause
