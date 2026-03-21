#!/bin/sh

# Output path (relative to build sh file)
OutputPath="./build/arm32"

# Compiler flags
# ProfilerFlags="-pg"
# OptimizeFlags="-O2"
# AssemblyFlags="-g -Wa,-ahl"
# DebugFlags="-g"
CommonCompilerFlags="-std=c99 -Wall -x c $ProfilerFlags $OptimizeFlags $AssemblyFlags $DebugFlags"
CommonLinkerFlags="-l m"

# Create output path if doesn't exists
mkdir -p $OutputPath
cd $OutputPath

# Empty the build folder
rm -f *

# Compile the tests
gcc $CommonCompilerFlags ../../wave_writer_test.c -o wave_writer_test $CommonLinkerFlags
gcc $CommonCompilerFlags ../../array_test.c -o array_test $CommonLinkerFlags
gcc $CommonCompilerFlags ../../binary_heap_test.c -o binary_heap_test $CommonLinkerFlags
gcc $CommonCompilerFlags ../../list_test.c -o list_test $CommonLinkerFlags
gcc $CommonCompilerFlags ../../map_test.c -o map_test $CommonLinkerFlags
gcc $CommonCompilerFlags ../../set_test.c -o set_test $CommonLinkerFlags
gcc $CommonCompilerFlags ../../queue_test.c -o queue_test $CommonLinkerFlags
gcc $CommonCompilerFlags ../../stack_test.c -o stack_test $CommonLinkerFlags
gcc $CommonCompilerFlags ../../memory_buffer_test.c -o memory_buffer_test $CommonLinkerFlags
gcc $CommonCompilerFlags ../../xmi2mid.c -o xmi2mid $CommonLinkerFlags
gcc $CommonCompilerFlags ../../flic_test.c -o flic_test $CommonLinkerFlags
gcc $CommonCompilerFlags ../../memzone_test.c -o memzone_test $CommonLinkerFlags
