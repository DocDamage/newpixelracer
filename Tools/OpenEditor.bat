@echo off
setlocal
cd /d "%~dp0.."
set "PROJECT=%CD%\PixelRacer.uproject"
if not "%UE58_ROOT%"=="" set "UE_ROOT=%UE58_ROOT%"
if "%UE_ROOT%"=="" set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"
set "EDITOR=%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor.exe"
if not exist "%EDITOR%" (
  echo Unreal Editor 5.8 was not found at:
  echo   %EDITOR%
  echo Set UE58_ROOT to your Unreal Engine 5.8 install folder and run again.
  exit /b 1
)
start "Pixel Racer" "%EDITOR%" "%PROJECT%"
endlocal
