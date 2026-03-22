@echo off
setlocal

set COMMAND=%1
if "%COMMAND%"=="" set COMMAND=build

pushd %~dp0\..
gcc -std=c99 -Wall -Wextra nob.c -o nob.exe || exit /b 1
.\nob.exe %COMMAND% || exit /b 1
popd
