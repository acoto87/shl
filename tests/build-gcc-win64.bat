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
gcc %CommonCompilerFlags% ..\..\wave_writer_test.c -o wave_writer_test.exe
gcc %CommonCompilerFlags% ..\..\array_test.c -o array_test.exe
gcc %CommonCompilerFlags% ..\..\binary_heap_test.c -o binary_heap_test.exe
gcc %CommonCompilerFlags% ..\..\list_test.c -o list_test.exe
gcc %CommonCompilerFlags% ..\..\map_test.c -o map_test.exe
gcc %CommonCompilerFlags% ..\..\set_test.c -o set_test.exe
gcc %CommonCompilerFlags% ..\..\queue_test.c -o queue_test.exe
gcc %CommonCompilerFlags% ..\..\stack_test.c -o stack_test.exe
gcc %CommonCompilerFlags% ..\..\memory_buffer_test.c -o memory_buffer_test.exe
gcc %CommonCompilerFlags% ..\..\xmi2mid.c -o xmi2mid.exe
gcc %CommonCompilerFlags% ..\..\flic_test.c -o flic_test.exe
gcc %CommonCompilerFlags% ..\..\memzone_test.c -o memzone_test.exe

POPD
