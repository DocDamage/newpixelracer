@echo off
setlocal
cd /d "%~dp0.."
where py >nul 2>nul
if %ERRORLEVEL%==0 (
  py -3 Tools\quick_check.py
) else (
  python Tools\quick_check.py
)
endlocal
