#!/bin/sh

# Output path (relative to build sh file)
OutputPath="./build/arm32"

# Compiler flags
# ProfilerFlags="-pg"
# OptimizeFlags="-O2"
# AssemblyFlags="-g -Wa,-ahl"
# DebugFlags="-g -O1 -D __DEBUG__"
CommonCompilerFlags="-std=c99 -Wall -x c $ProfilerFlags $OptimizeFlags $AssemblyFlags $DebugFlags"
CommonLinkerFlags="-l m"

# Create output path if doesn't exists
mkdir -p $OutputPath
cd $OutputPath

# Empty the build folder
rm -f *

# Compile the tests
gcc $CommonCompilerFlags $CommonLinkerFlags ../../wave_writer_test.c -o wave_writer_test
gcc $CommonCompilerFlags $CommonLinkerFlags ../../array_test.c -o array_test
gcc $CommonCompilerFlags $CommonLinkerFlags ../../binary_heap_test.c -o binary_heap_test
gcc $CommonCompilerFlags $CommonLinkerFlags ../../list_test.c -o list_test
gcc $CommonCompilerFlags $CommonLinkerFlags ../../map_test.c -o map_test
gcc $CommonCompilerFlags $CommonLinkerFlags ../../set_test.c -o set_test
gcc $CommonCompilerFlags $CommonLinkerFlags ../../queue_test.c -o queue_test
gcc $CommonCompilerFlags $CommonLinkerFlags ../../stack_test.c -o stack_test
gcc $CommonCompilerFlags $CommonLinkerFlags ../../memory_buffer_test.c -o memory_buffer_test
gcc $CommonCompilerFlags $CommonLinkerFlags ../../xmi2mid.c -o xmi2mid
gcc $CommonCompilerFlags $CommonLinkerFlags ../../flic_test.c -o flic_test
