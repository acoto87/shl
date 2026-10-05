# Single Header Libraries

These are single header libraries that I use in my code, much in the style of Sean Barret stb libraries.

## Using a library

Copy the `.h` file for the module you want into your project. Each collection
header is self-contained and only requires standard C headers. For example,
`list.h` already contains the allocator type and its internal helpers; pass
`NULL` to `Init` for heap allocation without copying `alloc.h`.

You can include multiple SHL headers in the same translation unit. Each
collection owns its internal helpers and uses module-specific private names.
The embedded `shl_allocator_t` definition uses its own shared guard, independently
of the helper declarations in `alloc.h`. Repeated header includes are safe:

```c
#include "list.h"
#include "queue.h"

shlDeclareList(IntList, int)
shlDefineList(IntList, int)
shlDeclareQueue(IntQueue, int)
shlDefineQueue(IntQueue, int)

int main(void)
{
    IntList list;
    IntQueue queue;
    IntListInit(&list, NULL);
    IntQueueInit(&queue, NULL);
    IntListAdd(&list, 42);
    IntQueuePush(&queue, IntListGet(&list, 0));
    IntQueueFree(&queue);
    IntListFree(&list);
    return 0;
}
```

For multiple translation units, put collection `shlDeclare*` invocations in a
project header and the corresponding `shlDefine*` invocations in exactly one C
file. Give each generated collection type a unique name. Libraries with an
`*_IMPLEMENTATION` switch still require it in exactly one translation unit, as
documented in their headers.

### Initial capacities

Each collection's initial capacity defaults to `8`. Override these public macros
before including the header, or with compiler flags such as
`-DSHL_LIST_INITIAL_CAPACITY=32`:

| Header | Configuration macro |
| --- | --- |
| `list.h` | `SHL_LIST_INITIAL_CAPACITY` |
| `stack.h` | `SHL_STACK_INITIAL_CAPACITY` |
| `queue.h` | `SHL_QUEUE_INITIAL_CAPACITY` |
| `binary_heap.h` | `SHL_HEAP_INITIAL_CAPACITY` |
| `map.h` | `SHL_MAP_INITIAL_CAPACITY` |
| `set.h` | `SHL_SET_INITIAL_CAPACITY` |

```c
#define SHL_LIST_INITIAL_CAPACITY 32
#include "list.h"
```

List, stack, queue, and heap capacities must be positive `int32_t` values. Map
and set capacities must be powers of two from `2` through `1073741824`; their
initial hash shift and load threshold are derived from that capacity. Use the
same configuration wherever a collection implementation is compiled.

### Allocator helpers

A custom `shl_allocator_t` instance can be shared by all collection types. Its
context and `mallocFn`, `reallocFn`, and `freeFn` callbacks are supplied by the
caller; the allocator object must outlive the collections using it.

For the convenience helpers, copy `alloc.h` and define
`SHL_ALLOC_IMPLEMENTATION` in exactly one translation unit:

```c
#define SHL_ALLOC_IMPLEMENTATION
#include "alloc.h"
```

Include `alloc.h` normally in other translation units. `shl_heap_alloc()` returns
one stable system-heap allocator shared across translation units. Collection
headers only embed the type, so including them does not emit these public helpers.

To use `shl_zone_alloc(zone)`, also copy `memzone.h` and include both headers in
the allocator implementation translation unit and wherever the adapter is used:

```c
#define SHL_ALLOC_IMPLEMENTATION
#define SHL_MZ_IMPLEMENTATION
#include "alloc.h"
#include "memzone.h"  /* either include order works */
#include "list.h"
```

The zone implementation may also live in a separate translation unit. Keep both
the zone and the returned allocator object alive while their collections exist.
For audited allocator callbacks, use `memzone_audit.h` in the allocator
implementation translation unit and define `SHL_MZ_AUDIT_IMPLEMENTATION` alongside
`SHL_MZ_IMPLEMENTATION`. The audit companion also requires copying `memzone.h`.

## Libraries

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

The test suite is built with [nob](https://github.com/tsoding/nob.h) and uses the [Unity](https://github.com/ThrowTheSwitch/Unity) C test framework. Tests live in `tests/` and cover every public API of each header.

```sh
cc -std=c99 -Wall -Wextra nob.c -o nob

# Run all tests
./nob test
./nob test all

# Run a single test suite
./nob test wstr_test

# Check standalone headers and combinations only
./nob headers

# Run under AddressSanitizer
./nob asan
./nob asan array_test

# Run under Valgrind (Linux only)
./nob valgrind
```

`./nob test` also runs the standalone-header checks. These copy each header (and
the audit companion's zone dependency when needed) into an isolated build
directory, compile its implementation and a separate declarations-only
translation unit, and exercise repeated includes, mixed include orders,
capacity overrides, collection growth, and shared heap/zone allocators.

### Maintaining embedded dependencies

`alloc.h` is the source of truth for the marked `alloc.h:type` and `alloc.h:zone`
blocks. Collections embed only the allocator type; the zone headers embed the
optional adapter. After changing those blocks, refresh the embedded copies and
commit the updated headers together:

```sh
./nob sync-headers
./nob headers
```

The header checks fail if an embedded copy is stale. The distributed headers
are checked into the repository, so consumers do not need to run a generator.

## Benchmarks

Microbenchmarks live in `benchmarks/` and use [ubench.h](https://github.com/sheredom/ubench.h).
Each suite covers all major API operations in isolation, integrated scenarios, and — where applicable — direct comparisons against the stdlib equivalents (`malloc` / `realloc` / `free`).

| Suite | Source | What it covers |
|---|---|---|
| `list_bench` | `benchmarks/list_bench.c` | All `list.h` operations: add, insert, remove, search, sort |
| `memzone_bench` | `benchmarks/memzone_bench.c` | All `memzone.h` operations vs `malloc` / `realloc` / `free` |

```sh
# Build and run all benchmarks
./nob bench

# Build and run a single benchmark suite
./nob bench list_bench
./nob bench memzone_bench
```

> **Note on batched benchmarks:** O(1) operations (e.g. a single alloc+free or a direct array access) are too short to measure individually on Windows without hitting timer-resolution noise. Those benchmarks perform N = 128 operations per timed sample and report the aggregate mean; divide by 128 for the per-operation cost.
