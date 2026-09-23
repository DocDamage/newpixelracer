@echo off
setlocal
cd /d "%~dp0.."
set "PROJECT=%CD%\PixelRacer.uproject"
if not "%UE58_ROOT%"=="" set "UE_ROOT=%UE58_ROOT%"
if "%UE_ROOT%"=="" set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"
set "BUILD=%UE_ROOT%\Engine\Build\BatchFiles\Build.bat"
if not exist "%BUILD%" (
  echo Unreal Engine 5.8 Build.bat was not found.
  echo Set UE58_ROOT to your Unreal Engine 5.8 install folder.
  exit /b 1
)
call "%BUILD%" UnrealEditor Win64 Development -Project="%PROJECT%" -WaitMutex -NoHotReloadFromIDE
endlocal
