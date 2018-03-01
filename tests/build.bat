@echo off

SET CommonCompilerFlags=-Od -MTd -nologo -fp:fast -fp:except- -Gm- -GR- -EHa- -Zo -Oi -FC -Z7 -I -TC
SET CommonLinkerFlags= -incremental:no -opt:ref -out:wave_writer_test.exe -pdb:wave_writer_test.pdb

IF NOT EXIST build mkdir build
pushd build

REM 64-bit build
REM Optimization switches /wO2
cl %CommonCompilerFlags% ../wave_writer_test.cpp /link %CommonLinkerFlags%
popd
