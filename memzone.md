# Memzone allocator

A simple non-moving memory allocator that uses a linked list of blocks to manage allocated blocks of memory.

This is a custom implementation following the ideas of the memory allocator used in DOOM and explained it in the [Fabien Sanglard's Game Engine Black Book for Doom](http://fabiensanglard.net/gebb/index.html).

Include the `#define SHL_MEMORY_ZONE_IMPLEMENTATION` before one of the `#include memzone.h` to get the implementation of the functions. Otherwise, only the declarations will be included.

Publicly, `memzone_t` is an opaque handle. Introspection goes through helper functions such as `mz_maxSize()`, `mz_usedSize()`, `mz_blockCount()`, `mz_usableFreeSize()`, `mz_fragmentation()`, and `mz_validate()`.

A memory _zone_ is defined with the following information:

* `usedSize`: how much space is currently unavailable for new allocations, including allocator metadata
* `maxSize`: the max allowed size that can be allocated
* `rover`: a pointer to a free block that is used when allocating
* `blockList`: list of blocks, here is where the requested memory begins

It maintains a circular double linked-list of _blocks_ with minimal information necessary to allocate and deallocate memory. Each block has the following information:

* `size`: size of the block
* `user`: the pointer returned to the user for allocated blocks, or `NULL` when the block is free
* `next`: pointer to the next block in the list
* `prev`: pointers to previous block in the list

The information of the headers of the _zone_ and _blocks_ are part of the `maxSize` passed at initialization time. Requested allocation sizes are rounded up to the allocator alignment returned by `mz_alignment()`, and block headers are aligned as well so returned pointers are suitable for general runtime data. Callers that need stricter placement can use `mz_allocAligned()` with `16`, `32`, or `64` byte alignment.

At the beginning, when the _zone_ is initialized there is only one _block_ with all the free memory, except for the information of the header of the _zone_ and the header of the _block_. When the user allocates memory, the allocator finds the next suitable free block, splits it only when the remainder can form a valid aligned block, and returns a stable pointer to the payload. When the user deallocates a pointer, the allocator finds the _block_ for that pointer, marks it as free, and merges it with sibling _blocks_ if they are also empty. Invalid frees are ignored for allocator safety, but they are now reported through the runtime diagnostics hook.

## Memory layout scenarios

Legend:

* `ZH`: `memzone_t` header up to `blockList`
* `BH`: `memblock_t` header for one block
* `F`: free payload bytes
* `A`: allocated payload bytes visible to the caller
* `P`: padding
* `pad`: alignment padding that belongs to an allocated block

### `mz_init()` / `mz_reset()`

The zone starts as one free block covering the whole payload span.

```text
| ZH | BH FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF |
       ^ rover
```

### `mz_alloc()`

If a free block is larger than needed, the allocator splits it and leaves a new free tail block.

```text
before:
| ZH | BH FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF |

after:
| ZH | BH AAAAAAAA | BH FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF |
          ^ user     ^ rover
```

If the remainder would be too small to hold `BH + mz_alignment()` bytes, the allocation takes the entire free block instead of creating an unusable tail.

```text
before:
| ZH | BH FFFFFFFFFFFFFFF |

after:
| ZH | BH AAAAAAAAAAAAAAA |
       ^ rover/user
```

### `mz_allocAligned()`

Aligned allocations may place padding between the block header and the returned pointer. That padding still belongs to the same live block.

```text
| ZH | BH PPP AAAAAAAA | BH FFFFFFFFFFFFFFFFFFFFFFFFF |
       ^ block start
              ^ user
```

### `mz_free()`

Freeing a block between live neighbors leaves a standalone free span.

```text
before:
| ZH | BH AAAAA | BH BBBBB | BH CCCCC |

after mz_free(B):
| ZH | BH AAAAA | BH FFFFF | BH CCCCC |
                  ^ rover
```

If either neighbor is already free, `mz_free()` coalesces immediately so the list never retains adjacent free blocks.

Deleting block B:
```text
merge with next before:
| ZH | BH AAAAA | BH BBBBB | BH FFFFF |
merge with next after:
| ZH | BH AAAAA | BH FFFFFFFFFFFFFF |

merge with previous before:
| ZH | BH FFFFF | BH BBBBB | BH CCCCC |
merge with previous after:
| ZH | BH FFFFFFFFFFFFFF | BH CCCCC |

merge with both sides before:
| ZH | BH FFFFF | BH BBBBB | BH FFFFF |
merge with both sides after:
| ZH | BH FFFFFFFFFFFFFFFFFFFFFFFFFF |
```

### `mz_realloc()`

Case 1: if the current block already has enough capacity, the pointer and layout stay unchanged.

```text
| ZH | BH AAAAAAAAAAAAAAA | BH FFFFFFFFFFFFFFFFFFFFFFFFF |
          ^ user unchanged
```

Case 2a: if the next block is free and large enough, `mz_realloc()` grows in place. The allocator only consumes the extra bytes required for the new request. If the remainder can still form a valid aligned block, that tail stays free.

```text
before:
| ZH | BH AAAAA | BH FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF |
          ^ user  ^ rover

after grow:
| ZH | BH AAAAAAAAAAAA | BH FFFFFFFFFFFFFFFFFFFFFFFFFFF |
          ^ same user    ^ rover
```

This is the important rule for the recent fix: growing an allocation into a free block at the end of the zone must not make the allocation appear to own the entire tail unless that tail is too small to remain a valid block.

Case 2b: if the leftover bytes would be too small to hold a valid free block, `mz_realloc()` absorbs the whole adjacent free block.

```text
before:
| ZH | BH AAAAA | BH FFFFFFF |

after grow:
| ZH | BH AAAAAAAAAAAAAAAA |
          ^ same user
```

Case 3: if in-place growth is not possible, `mz_realloc()` allocates a new block, copies the old payload, and frees the previous block.

```text
before:
| ZH | BH AAAAA | BH CCCCC | BH FFFFFFFFFFFFFFFFFFFFFFF |

after:
| ZH | BH FFFFF | BH CCCCC | BH AAAAAAAAAAAA | BH FFFF |
                                ^ new user
```

The validated structural invariants are:

* blocks cover the zone payload span exactly with no gaps or overlaps
* there are never two contiguous free `memblock_t` entries because free-time coalescing must merge them
* all block headers and returned pointers satisfy the allocator alignment

This allocator is intended to be used as a per-thread runtime allocator. It is not internally synchronized, and it does not move live allocations.

Example:
```c
#define MZ_MALLOC(sz) custom_malloc(sz)
#define MZ_FREE(p) custom_free(p)
#define MZ_DEBUG
#define SHL_MEMORY_ZONE_IMPLEMENTATION
#include "memzone.h"
```

Customization points:

* `MZ_MALLOC(sz)`: replaces the allocator used by `mz_init()`
* `MZ_FREE(p)`: replaces the deallocator used by `mz_destroy()`
* `MZ_DEBUG`: enables internal `MZ_ASSERT(...)` checks after allocator mutations
* `MZ_ASSERT(expr)`: optional custom assert hook used when `MZ_DEBUG` is enabled

The memory allocator allows the following operations:

| Function | Description | Return type |
| --- | --- | --- |
| `mz_alignment`(void) | Returns the allocator alignment used for payloads and headers. | size_t |
| `mz_maxSize`(const memzone_t* zone) | Returns the total zone size passed at initialization, or `0` for `NULL`. | size_t |
| `mz_usedSize`(const memzone_t* zone) | Returns the currently unavailable bytes including allocator metadata, or `0` for `NULL`. | size_t |
| `mz_init`(size_t maxSize) | Creates and initializes a memory zone with the specified `maxSize`. | memzone_t* |
| `mz_destroy`(memzone_t* zone) | Releases a zone previously created with `mz_init`. | void |
| `mz_reset`(memzone_t* zone) | Resets a zone back to a single free block without releasing its backing memory. | void |
| `mz_alloc`(memzone_t* zone, size_t size) | Allocates a zero-initialized block of memory from the specified zone. Size `0` returns `NULL`. | void* |
| `mz_allocAligned`(memzone_t* zone, size_t size, size_t alignment) | Allocates a zero-initialized block with explicit `16`, `32`, or `64` byte pointer alignment. Invalid alignments return `NULL`. | void* |
| `mz_setReporter`(memzone_t* zone, mz_reporter_t reporter, void* userData) | Replaces the default `stderr` diagnostics hook for allocation failures, invalid frees, and validation failures. Pass `NULL` to silence reports. | void |
| `mz_free`(memzone_t* zone, void* p) | Frees a previously allocated block of memory. Unknown pointers are ignored. | void |
| `mz_realloc`(memzone_t* zone, void* p, size_t size) | Resizes a live allocation. `NULL` pointer behaves like `mz_alloc`. Zero size frees `p` and returns `NULL`. Returns the (possibly new) pointer, or `NULL` on failure. | void* |
| `mz_contains`(const memzone_t* zone, const void* p) | Returns whether `p` currently belongs to a live allocation in the zone. | bool |
| `mz_allocationSize`(const memzone_t* zone, const void* p) | Returns the aligned size reserved for a live allocation, or `0` if not found. | size_t |
| `mz_validate`(const memzone_t* zone) | Validates the zone internal structure and accounting invariants. | bool |
| `mz_blockCount`(const memzone_t* zone) | Gets the number of blocks in the allocator. | int32_t |
| `mz_usableFreeSize`(const memzone_t* zone) | Gets the usable free size on the allocator. | size_t |
| `mz_fragmentation`(const memzone_t* zone) | Gets the fragmentation percentage of the allocator. | float |

The diagnostics hook receives an `mz_report_t` category plus a context pointer. Allocation failures use a `NULL` context, invalid frees report the rejected pointer as context, and validation failures report the block or zone that failed the invariant being checked.

When `MZ_DEBUG` is enabled, allocator mutations assert `mz_validate(zone)` internally after initialization, reset, allocation, and free.

Example:
```c
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define SHL_MEMORY_ZONE_IMPLEMENTATION
#include "memzone.h"

typedef enum
{
    ENTITY_TYPE_1,
    ENTITY_TYPE_2,
    ENTITY_TYPE_3,

    ENTITY_TYPE_COUNT
} EntityType;

typedef struct
{
    int32_t id;
    EntityType type;
} Entity;

#define ENTITY_COUNT 30000
#define randabi(a, b) (int32_t)((a) + ((float)rand() / RAND_MAX) * ((b) - (a)))

int main()
{
    srand(time(NULL));

    size_t zoneSize = 1024 * 1024; // 1MB
    memzone_t* zone = mz_init(zoneSize);
    if (!zone)
    {
        printf("ERROR: Couldn't allocate %d bytes\n", zoneSize);
        return -1;
    }

    Entity** entities = (Entity**)mz_alloc(zone, sizeOfArray);
    for (int32_t i = 0; i < ENTITY_COUNT; i++)
    {
        entities[i] = (Entity*)mz_alloc(zone, sizeof(Entity));
        entities[i]->id = i;
    }

    for (int32_t i = 0; i < ENTITY_COUNT / 2; i++)
    {
        int32_t index = randabi(0, ENTITY_COUNT - 1);
        if (entities[index])
        {
            mz_free(zone, entities[index]);
            entities[index] = NULL;
        }
    }

    mz_destroy(zone);
    return 0;
}
```
