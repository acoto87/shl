@echo off

pushd .
call "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
popd
set path=D:\Work\shl\tests;%path%
