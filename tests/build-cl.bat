@echo off

pushd .
call "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
popd
SET path=D:\Work\shl\tests;%path%

SET CommonCompilerFlags=-Od -MTd -nologo -fp:fast -fp:except- -Gm- -GR- -EHa- -Zo -Oi -FC -Z7 -TC
SET CommonLinkerFlags= -incremental:no -opt:ref

IF NOT EXIST build mkdir build
pushd build

cl %CommonCompilerFlags% ../tests/wave_writer_test.c /link -out:wave_writer_test.exe -pdb:wave_writer_test.pdb %CommonLinkerFlags%
cl %CommonCompilerFlags% ../tests/list_test.c /link %CommonLinkerFlags% -out:list_test.exe -pdb:list_test.pdb
cl %CommonCompilerFlags% ../tests/map_test.c /link %CommonLinkerFlags% -out:map_test.exe -pdb:map_test.pdb
popd
