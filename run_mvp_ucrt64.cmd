@echo off
setlocal

set "MSYS2_ROOT=D:\msys64"
set "APP_EXE=D:\poke\build-msys2-2\PokeBurstCaptureMVP.exe"

if not exist "%APP_EXE%" (
    echo App not found: %APP_EXE%
    echo Run build_mvp_ucrt64.cmd first.
    exit /b 1
)

set "PATH=%MSYS2_ROOT%\ucrt64\bin;%PATH%"
start "" "%APP_EXE%"

endlocal
