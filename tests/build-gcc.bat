@echo off

SET CommonCompilerFlags=-Wall

IF NOT EXIST build mkdir build
pushd build

gcc %CommonCompilerFlags% -std=c99 ../tests/wave_writer_test.c -o wave_writer_test.exe
gcc %CommonCompilerFlags% -std=c99 ../tests/list_test.c -o list_test.exe
gcc %CommonCompilerFlags% -std=c99 ../tests/map_test.c -o map_test.exe
popd
