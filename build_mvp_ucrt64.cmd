@echo off
setlocal

set "MSYS2_ROOT=D:\msys64"
set "BUILD_DIR=D:\poke\build-msys2-2"

if not exist "%MSYS2_ROOT%\msys2_shell.cmd" (
    echo MSYS2 not found at %MSYS2_ROOT%
    exit /b 1
)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

call "%MSYS2_ROOT%\msys2_shell.cmd" -defterm -no-start -ucrt64 -here -c "cd /d/poke/build-msys2-2 && cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build . --config Release"

endlocal
