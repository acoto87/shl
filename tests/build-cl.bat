@echo off

pushd .
call "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
popd
SET path=D:\Work\shl\tests;%path%

REM Include paths
SET GLEWIncludePath=D:\Work\libs\glew32\include
SET GLFWIncludePath=D:\Work\libs\glfw-3.2.1_x64\include
SET GLMIncludePath=D:\Work\cglm\include\cglm
SET STBIncludePath=D:\Work\stb
SET NVGIncludePath=D:\Work\nanovg\src

REM Lib paths
SET GLEWLibPath=D:\Work\libs\glew32\lib
SET GLFWLibPath=D:\Work\libs\glfw-3.2.1_x64\lib-vc2015

SET CommonCompilerFlags=-Od -MTd -nologo -fp:fast -fp:except- -Gm- -GR- -EHa- -Zo -Oi -FC -Z7 -TC -I %GLEWIncludePath% -I %GLFWIncludePath% -I %GLMIncludePath% -I %STBIncludePath% -I %NVGIncludePath%
SET CommonLinkerFlags= -incremental:no -opt:ref -libpath:%GLEWLibPath% -libpath:%GLFWLibPath%

IF NOT EXIST build mkdir build
pushd build

cl %CommonCompilerFlags% ../tests/wave_writer_test.c /link -out:wave_writer_test.exe -pdb:wave_writer_test.pdb %CommonLinkerFlags%
cl %CommonCompilerFlags% ../tests/list_test.c /link %CommonLinkerFlags% -out:list_test.exe -pdb:list_test.pdb
cl %CommonCompilerFlags% ../tests/stack_test.c /link %CommonLinkerFlags% -out:stack_test.exe -pdb:stack_test.pdb
cl %CommonCompilerFlags% ../tests/queue_test.c /link %CommonLinkerFlags% -out:queue_test.exe -pdb:queue_test.pdb
cl %CommonCompilerFlags% ../tests/map_test.c /link %CommonLinkerFlags% -out:map_test.exe -pdb:map_test.pdb
cl %CommonCompilerFlags% ../tests/canvas2d_test.c /link %CommonLinkerFlags% user32.lib gdi32.lib opengl32.lib glew32.lib glfw3dll.lib -out:canvas2d_test.exe -pdb:canvas2d_test.pdb
popd
