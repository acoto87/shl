# Single Header Libraries

These are single header libraries that I use in my code, much in the style of Sean Barret stb libraries.

* list.h: A generic list implementation (see [list.md](https://github.com/acoto87/shl/blob/master/list.md)).
* stack.h: A generic stack implementation (see [stack.md](https://github.com/acoto87/shl/blob/master/stack.md)).
* queue.h: A generic queue implementation (see [queue.md](https://github.com/acoto87/shl/blob/master/queue.md)).
* binary_heap.h: A generic binary heap implementation (see [binary_heap.md](https://github.com/acoto87/shl/blob/master/binary_heap.md))
* map.h: A generic hash-table implementation (see [map.md](https://github.com/acoto87/shl/blob/master/map.md)).
* set.h: A generic hash-set implementation (see [set.md](https://github.com/acoto87/shl/blob/master/set.md))
* array.h: A generic helper to work with multi-dimentional arrays.
* wstr.h: String views and heap strings (see [wstr.md](https://github.com/acoto87/shl/blob/master/wstr.md)).
* wave_writer.h: Contains functionalities to write `.wav` files (see [wave_writer.md](https://github.com/acoto87/shl/blob/master/wave_writer.md)).
* memory_buffer.h: An in-memory buffer implementation with random access (see [memory_buffer.md](https://github.com/acoto87/shl/blob/master/memory_buffer.md)).
* flic.h: Contains functionalities to read FLIC files (see [flic.md](https://github.com/acoto87/shl/blob/master/flic.md)). It's a C port of the C++ implementation by David Capello's Aseprite FLIC Library: https://github.com/aseprite/flic
* memzone.h: A simple memory allocator. (see [memzone.md](https://github.com/acoto87/shl/blob/master/memzone.md))
* memzone_audit.h: Companion header for memzone.h that records every allocator mutation to a structured log file. (see [memzone_audit.md](https://github.com/acoto87/shl/blob/master/memzone_audit.md))

See the tests/*_tests.c files to see how to use them.

These are work in progress, and I'm using it in my games, so use at your own risk.

Any tips/suggestions are welcome.

## Building and running tests

The test suite is built with [nob](https://github.com/tsoding/nob.h) and uses the Unity C test framework.

```sh
cc -std=c99 -Wall -Wextra nob.c -o nob
./nob test
./nob test all
./nob test wstr_test
./nob asan
./nob asan array_test
./nob valgrind
```
