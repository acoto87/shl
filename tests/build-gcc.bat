@echo off

SET CommonCompilerFlags=-Wall

IF NOT EXIST build mkdir build
pushd build

gcc %CommonCompilerFlags% ../wave_writer_test.cpp -o wave_writer_test.exe
popd
