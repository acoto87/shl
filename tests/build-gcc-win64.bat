@echo off

REM Output path (relative to build bat file)
SET OutputPath=.\build\win64

REM Compiler flags
REM ProfilerFlags="-pg"
REM OptimizeFlags="-O2"
REM AssemblyFlags="-g -Wa,-ahl"
REM DebugFlags="-g -O1 -D __DEBUG__"
SET CommonCompilerFlags=-std=c99 -Wall -x c %ProfilerFlags% %OptimizeFlags% %AssemblyFlags% %DebugFlags%

REM Create output path if doesn't exists
IF NOT EXIST %OutputPath% MKDIR %OutputPath%
PUSHD %OutputPath%

REM Empty the build folder
DEL /Q *

REM Compile the tests
ECHO Compiling wave_writer_test
gcc %CommonCompilerFlags% ..\..\wave_writer_test.c -o wave_writer_test.exe
ECHO Compiling array_test
gcc %CommonCompilerFlags% ..\..\array_test.c -o array_test.exe
ECHO Compiling binary_heap_test
gcc %CommonCompilerFlags% ..\..\binary_heap_test.c -o binary_heap_test.exe
ECHO Compiling list_test
gcc %CommonCompilerFlags% ..\..\list_test.c -o list_test.exe
ECHO Compiling map_test
gcc %CommonCompilerFlags% ..\..\map_test.c -o map_test.exe
ECHO Compiling set_test
gcc %CommonCompilerFlags% ..\..\set_test.c -o set_test.exe
ECHO Compiling queue_test
gcc %CommonCompilerFlags% ..\..\queue_test.c -o queue_test.exe
ECHO Compiling stack_test
gcc %CommonCompilerFlags% ..\..\stack_test.c -o stack_test.exe
ECHO Compiling memory_buffer_test
gcc %CommonCompilerFlags% ..\..\memory_buffer_test.c -o memory_buffer_test.exe
ECHO Compiling xmi2mid
gcc %CommonCompilerFlags% ..\..\xmi2mid.c -o xmi2mid.exe
ECHO Compiling flic_test
gcc %CommonCompilerFlags% ..\..\flic_test.c -o flic_test.exe
ECHO Compiling memzone_test
gcc %CommonCompilerFlags% ..\..\memzone_test.c -o memzone_test.exe

POPD
