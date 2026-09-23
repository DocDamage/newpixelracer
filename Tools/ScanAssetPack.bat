@echo off
setlocal
if "%~1"=="" (
  echo Drag a racing asset pack ZIP or folder onto this BAT file.
  pause
  exit /b 1
)
py -3 "%~dp0import_asset_pack.py" "%~1"
if errorlevel 1 python "%~dp0import_asset_pack.py" "%~1"
pause
