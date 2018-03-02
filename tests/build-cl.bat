@echo off

SET CommonCompilerFlags=-Od -MTd -nologo -fp:fast -fp:except- -Gm- -GR- -EHa- -Zo -Oi -FC -Z7 -TC
SET CommonLinkerFlags= -incremental:no -opt:ref -out:wave_writer_test.exe -pdb:wave_writer_test.pdb

IF NOT EXIST build mkdir build
pushd build

cl %CommonCompilerFlags% ..\wave_writer_test.cpp /link %CommonLinkerFlags%
popd
