#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <string.h>

#include "../list.h"

static float getTime()
{
    return (float)clock() / CLOCKS_PER_SEC;
}

bool intEquals(const int x, const int y)
{
    return x == y;
}

shlDeclareList(IntList, int)
shlDefineList(IntList, int, intEquals, 0)

typedef struct
{
    int index;
    char *name;
} Entry;

bool EntryEquals(const Entry *e1, const Entry *e2)
{
    return e1->index == e2->index &&
           strcmp(e1->name, e2->name) == 0;
}

shlDeclareList(EntriesList, Entry*)
shlDefineList(EntriesList, Entry*, EntryEquals, NULL)

int main(int argc, char **argv)
{
    const int32_t count = 100000;
    float start, end;

    /* initialize random seed: */
    srand(time(NULL));

    IntList intList;
    IntListInit(&intList);

    EntriesList entriesList;
    EntriesListInit(&entriesList);

    printf("--- Start test 1: add %d value type objects ---\n", count);
    start = getTime();
    for(int i = 0; i < count; i++)
    {
        int value = rand() % count + 1;
        IntListAdd(&intList, value);
    }
    end = getTime();
    printf("List count and capacity: (%d, %d)\n", intList.count, intList.capacity);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 1: add %d value type objects ---\n", count);

    printf("\n");

    printf("--- Start test 1: add %d reference type objects ---\n", count);
    start = getTime();
    for(int i = 0; i < count; i++)
    {
        Entry *entry = (Entry*)malloc(sizeof(Entry));
        entry->index = rand() % count + 1;
        entry->name = "entry";
        EntriesListAdd(&entriesList, entry);
    }
    end = getTime();
    printf("List count and capacity: (%d, %d)\n", entriesList.count, entriesList.capacity);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 1: add %d reference type objects ---\n", count);

    printf("\n\n");

    printf("--- Start test 2: contains %d value type objects ---\n", count / 2);
    start = getTime();
    int32_t containsCount = 0;
    for(int i = 0; i < count/2; i++)
    {
        int value = rand() % count + 1;
        if (IntListContains(&intList, value))
            containsCount++;
    }
    end = getTime();
    printf("Contains count: %d\n", containsCount);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 2: contains %d value type objects ---\n", count / 2);

    printf("\n");

    printf("--- Start test 2: contains %d reference type objects ---\n", count / 2);
    start = getTime();
    containsCount = 0;
    for(int i = 0; i < count/2; i++)
    {
        Entry *entry = (Entry*)malloc(sizeof(Entry));
        entry->index = rand() % count + 1;
        entry->name = "entry";
        if (EntriesListContains(&entriesList, entry))
            containsCount++;
        free(entry);
    }
    end = getTime();
    printf("Contains count: %d\n", containsCount);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 2: contains %d reference type objects ---\n", count / 2);

    printf("\n\n");

    printf("--- Start test 3: remove by index %d value type objects ---\n", count / 2);
    start = getTime();
    for(int i = 0; i < count/2; i++)
    {
        int index = rand() % count;
        IntListRemoveAt(&intList, index);
    }
    end = getTime();
    printf("List count and capacity: (%d, %d)\n", intList.count, intList.capacity);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 3: remove by index %d value type objects ---\n", count / 2);

    printf("\n");

    printf("--- Start test 3: remove by index %d reference type objects ---\n", count / 2);
    start = getTime();
    for(int i = 0; i < count/2; i++)
    {
        int index = rand() % count;
        EntriesListRemoveAt(&entriesList, index);
    }
    end = getTime();
    printf("List count and capacity: (%d, %d)\n", entriesList.count, entriesList.capacity);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 3: remove by index %d reference type objects ---\n", count / 2);

    printf("\n\n");

    printf("--- Start test 4: remove by value %d value type objects ---\n", count / 2);
    start = getTime();
    for(int i = 0; i < count/2; i++)
    {
        int value = rand() % count;
        IntListRemove(&intList, value);
    }
    end = getTime();
    printf("List count and capacity: (%d, %d)\n", intList.count, intList.capacity);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 3: remove by index %d value type objects ---\n", count / 2);

    printf("\n");

    printf("--- Start test 4: remove by value %d reference type objects ---\n", count / 2);
    start = getTime();
    for(int i = 0; i < count/2; i++)
    {
        Entry *entry = (Entry*)malloc(sizeof(Entry));
        entry->index = rand() % count + 1;
        entry->name = "entry";
        EntriesListRemove(&entriesList, entry);
        free(entry);
    }
    end = getTime();
    printf("List count and capacity: (%d, %d)\n", entriesList.count, entriesList.capacity);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 3: remove by index %d reference type objects ---\n", count / 2);
    
    return 0;
}