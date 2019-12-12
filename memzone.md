# Memzone allocator

A simple memory allocator that uses a linked list of blocks to manage allocated blocks of memory.

This is a custom implementation following the ideas of the memory allocator used in DOOM and explained it in the [Fabien Sanglard's Game Engine Black Book for Doom](http://fabiensanglard.net/gebb/index.html).

Include the `#define SHL_MEMORY_ZONE_IMPLEMENTATION` before one of the `#include memzone.h` to get the implementation of the functions. Otherwise, only the declarations will be included.

A memory _zone_ is defined with the following information:

* `usedSize`: how much space is used without including blocks data
* `maxSize`: the max allowed size that can be allocated
* `rover`: a pointer to a free block that is used when allocating
* `blockList`: list of blocks, here is where the requested memory begins

It maintains a circular double linked-list of _blocks_ with minimal information necessary to allocate and deallocate memory. Each block as the following information:

* `size`: size of the block
* `type`: the type of the block
* `user`: a pointer to the pointer returned to the user
* `next`: pointer to the next block in the list
* `prev`: pointers to previous block in the list

The type of a block is one of the following values:

* `MEM_STATIC`: default block type, represent a normal block. _For now this is the only type of blocks used, in further development the other types of blocks will be consider.
* `MEM_PURGE`: block type that can be reused when allocating, even if it's used, normally this type of block is created when free some STATIC block
* `MEM_FIXED`: block type that is fixed at the start of the app and will last the entire app life-cycle and it can't be defragmented

The information of the headers of the _zone_ and _blocks_ are part of the `maxSize` passed at initialization time. That's it, if the user is considering a 1MB zone, the information of the headers will occupy part of that 1MB. The size of the header of the _zone_ is 20 bytes and the size of the header of each block is 32 bytes. However, when the user try to allocate a pointer of a given size, the _block_ that hold that pointer will have enough space to hold the requested size. For instance, if the user allocates 256 bytes, the _block_ holding the pointer for that size is 32 + 256 bytes long.

At the beginning, when the _zone_ is initialized there is only one _block_ with all the free memory, except for the information of the header of the _zone_ and the header of the _block_. When a the user try to allocate memory, the allocator finds the first block that is big enough to hold the space needed, split the _block_ if necessary and return the pointer to the user. When the user try to deallocate a pointer, the allocator finds the _block_ for that pointer, and mark it as free, merging it with the sibilings _blocks_ if they are also empty.

Example:
```c
#define SHL_MEMORY_ZONE_IMPLEMENTATION
#include "memzone.h"
```

The memory allocator allows the following operations:

| Function | Description | Return type |
| --- | --- | --- |
| `mzInit`(size_t maxSize) | Creates and initialize a new memory allocator structure with the specified `maxSize`. | memzone_t* |
| `mzAlloc`(memzone_t* zone, size_t size) | Allocates a block of memory on the specified `memzone_t` object and a given `size` | void* |
| `mzFree`(memzone_t* zone, void* p) | Free a previously allocated block of memory. | void |
| `mzIsBlockEmpty`(memblock_t* block) | A macro to determine if the specified block is empty. It will expand to `((block)->user == NULL)` | bool |
| `mzGetNumberOfBlocks`(memzone_t* zone) | Gets the number of blocks in the allocator. | int32_t |
| `mzGetUsableFreeSize`(memzone_t* zone) | Gets the usable free size on the allocator. |size_t |
| `mzGetFragPercentage`(memzone_t* zone) | Gets the fragmentation percentage of the allocator. | float |
| `mzDefrag`(memzone_t* zone) | Defragment the allocator. _This functionality is still in development_. | void |
| `mzPrint`(memzone_t* zone, bool printBlocks, bool printMap) | Prints the allocator data to `stdout`. | void |

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
    memzone_t* zone = mzInit(zoneSize);
    if (!zone)
    {
        printf("ERROR: Couldn't allocate %d bytes\n", zoneSize);
        return -1;
    }

    Entity** entities = (Entity**)mzAlloc(zone, sizeOfArray);
    for (int32_t i = 0; i < ENTITY_COUNT; i++)
    {
        entities[i] = (Entity*)mzAlloc(zone, sizeof(Entity));
        entities[i]->id = i;
    }

    mzPrint(zone, false, true);

    for (int32_t i = 0; i < ENTITY_COUNT / 2; i++)
    {
        int32_t index = randabi(0, ENTITY_COUNT - 1);
        if (entities[index])
        {
            mzFree(zone, entities[index]);
            entities[index] = NULL;
        }
    }

    mzPrint(zone, false, true);

    free(zone);
    return 0;
}
```