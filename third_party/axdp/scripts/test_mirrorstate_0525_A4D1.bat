@echo off
setlocal

set "ROOT=%~dp0.."
pushd "%ROOT%" || exit /b 1

set "VID=1317"
set "PID=42193"
set "TOOL=build\tools\Release\axdptool.exe"

if not exist "%TOOL%" (
  echo axdptool not found: %TOOL%
  echo Please build target axdptool first.
  popd
  exit /b 1
)

set "PATH=%CD%\build\Release;%PATH%"

echo VID=0x0525 PID=0xA4D1 ^(VID=%VID% PID=%PID%^) 
echo ---
echo getmirrorstate
"%TOOL%" -v %VID% -p %PID% -m getmirrorstate
echo ---
echo setmirrorstate 0
"%TOOL%" -v %VID% -p %PID% -m setmirrorstate -a 0
echo ---
echo getmirrorstate
"%TOOL%" -v %VID% -p %PID% -m getmirrorstate
echo ---
echo setmirrorstate 1
"%TOOL%" -v %VID% -p %PID% -m setmirrorstate -a 1
echo ---
echo getmirrorstate
"%TOOL%" -v %VID% -p %PID% -m getmirrorstate

popd
endlocal
