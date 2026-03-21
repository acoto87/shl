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

#define ENTITY_COUNT 30000
#define randabi(a, b) (int32_t)((a) + ((float)rand() / RAND_MAX) * ((b) - (a)))

static void edgeCaseMergeTest()
{
    memzone_t* zone = mzInit(4096);
    assert(zone);

    void* a = mzAlloc(zone, 64);
    void* b = mzAlloc(zone, 64);
    void* c = mzAlloc(zone, 64);
    assert(a && b && c);

    size_t usedBeforeFree = zone->usedSize;

    mzFree(zone, b);
    assert(zone->usedSize == usedBeforeFree - 64);

    mzFree(zone, c);
    assert(zone->rover);
    assert(mzGetUsableFreeSize(zone) == zone->maxSize - zone->usedSize);

    mzFree(zone, a);
    assert(zone->usedSize == sizeof(memzone_t));
    assert(mzGetNumberOfBlocks(zone) == 1);
    assert(mzGetUsableFreeSize(zone) == zone->maxSize - zone->usedSize);

    free(zone);
}

int main()
{
    srand(time(NULL));

    printf("sizeof(Entity) = %u\n", sizeof(Entity));
    printf("sizeof(memzone_t) = %u\n", sizeof(memzone_t));
    printf("sizeof(memblock_t) = %u\n", sizeof(memblock_t));
    printf("\n");

    size_t zoneSize = 2 * 1024 * 1024; // 2MB
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
        entities[i]->id = i;
        assert(entities[i]);

        size_t partialSizeOfArrayElements = (i + 1) * (sizeof(memblock_t) + sizeof(Entity));
        assert(zone->usedSize == sizeof(memzone_t) + sizeOfArray + partialSizeOfArrayElements + sizeof(memblock_t));
    }

    size_t sizeOfArrayElements = ENTITY_COUNT * (sizeof(memblock_t) + sizeof(Entity));
    assert(zone->usedSize == sizeof(memzone_t) + sizeOfArray + sizeOfArrayElements + sizeof(memblock_t));
    assert(mzGetUsableFreeSize(zone) == zone->maxSize - zone->usedSize);
    assert(mzGetNumberOfBlocks(zone) == 1 + ENTITY_COUNT + 1); // 1 for the array itself, ENTITY_COUNT for the elements, 1 for the empty block

    mzPrint(zone, false, true);

    int32_t k = 0;
    for (int32_t i = 0; i < ENTITY_COUNT / 2; i++)
    {
        int32_t index = randabi(0, ENTITY_COUNT - 1);
        if (entities[index])
        {
            int32_t blocksBeforeFree = mzGetNumberOfBlocks(zone);
            size_t usedSizeBeforeFree = zone->usedSize;
            mzFree(zone, entities[index]);
            entities[index] = NULL;

            int32_t blocksAfterFree = mzGetNumberOfBlocks(zone);
            size_t reclaimedMetadata = (size_t)(blocksBeforeFree - blocksAfterFree) * sizeof(memblock_t);
            assert(zone->usedSize == usedSizeBeforeFree - sizeof(Entity) - reclaimedMetadata);
            k++;
        }
    }

    size_t sizeOfDeletedArrayElements = k * sizeof(Entity);
    sizeOfArrayElements -= sizeOfDeletedArrayElements;
    assert(zone->usedSize == zone->maxSize - mzGetUsableFreeSize(zone));
    assert(mzGetNumberOfBlocks(zone) <= 1 + ENTITY_COUNT + 1); // 1 for the array itself, ENTITY_COUNT for the elements, 1 for the empty block

    mzPrint(zone, false, true);

    free(zone);

    edgeCaseMergeTest();

    printf("Done\n");
    return 0;
}
