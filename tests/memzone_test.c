#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <assert.h>

#define SHL_MEMORY_ZONE_IMPLEMENTATION
#include "../memzone.h"

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

#define ENTITY_COUNT 25000
#define randabi(a, b) (int32_t)(a + ((float)rand() / RAND_MAX) * (b - a));

int main()
{
    srand(time(NULL));

    printf("sizeof(memzone_t) = %u\n", sizeof(memzone_t));
    printf("sizeof(memblock_t) = %u\n", sizeof(memblock_t));
    printf("\n");

    size_t zoneSize = 1024 * 1024; // 1MB
    memzone_t* zone = mzInit(zoneSize);
    if (!zone)
    {
        printf("ERROR: Couldn't allocate %d bytes\n", zoneSize);
        return -1;
    }

    assert(zone->maxSize == zoneSize);
    assert(zone->usedSize == sizeof(memzone_t));

    size_t sizeOfArray = ENTITY_COUNT * sizeof(Entity*);
    Entity** entities = (Entity**)mzAlloc(zone, sizeOfArray);
    assert(entities);
    assert(zone->usedSize == sizeof(memzone_t) + sizeOfArray + sizeof(memblock_t));

    for (int32_t i = 0; i < ENTITY_COUNT; i++)
    {
        entities[i] = (Entity*)mzAlloc(zone, sizeof(Entity));
        assert(entities[i]);

        size_t partialSizeOfArrayElements = (i + 1) * (sizeof(memblock_t) + sizeof(Entity));
        assert(zone->usedSize == sizeof(memzone_t) + sizeOfArray + partialSizeOfArrayElements + sizeof(memblock_t));
    }

    size_t sizeOfArrayElements = ENTITY_COUNT * (sizeof(memblock_t) + sizeof(Entity));
    assert(zone->usedSize == sizeof(memzone_t) + sizeOfArray + sizeOfArrayElements + sizeof(memblock_t));
    assert(mzGetUsableFreeSize(zone) == zone->maxSize - zone->usedSize);
    assert(mzGetNumberOfBlocks(zone) == 1 + ENTITY_COUNT + 1); // 1 for the array itself, ENTITY_COUNT for the elements, 1 for the empty block

    int32_t k = 0;
    for (int32_t i = 0; i < ENTITY_COUNT / 2; i++)
    {
        int32_t index = randabi(0, ENTITY_COUNT);
        if (entities[index])
        {
            mzFree(zone, entities[index]);
            entities[index] = NULL;

            assert(zone->usedSize == sizeof(memzone_t) + sizeOfArray + sizeOfArrayElements - ((k + 1) * sizeof(Entity)) + sizeof(memblock_t));
            k++;
        }
    }

    size_t sizeOfDeletedArrayElements = k * sizeof(Entity);
    assert(zone->usedSize == sizeof(memzone_t) + sizeOfArray + sizeOfArrayElements - sizeOfDeletedArrayElements + sizeof(memblock_t));
    assert(mzGetUsableFreeSize(zone) == zone->maxSize - zone->usedSize);
    assert(mzGetNumberOfBlocks(zone) == 1 + ENTITY_COUNT + 1); // 1 for the array itself, ENTITY_COUNT for the elements, 1 for the empty block

    mzPrint(zone, false);

    free(zone);

    printf("Done\n");
    return 0;
}