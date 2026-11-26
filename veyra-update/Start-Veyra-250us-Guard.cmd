@echo off
setlocal
rem Close Veyra normally before using this comparison launcher.
set "VEYRA_TEST_CONTENT_PHASE_GUARD_US=250"
if not exist "%LOCALAPPDATA%\Programs\Veyra (NVIDIA)\Veyra.exe" (
  echo Veyra installation not found.
  pause
  exit /b 1
)
start "" "%LOCALAPPDATA%\Programs\Veyra (NVIDIA)\Veyra.exe"
endlocal
