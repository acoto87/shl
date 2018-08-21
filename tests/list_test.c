#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>

#include "../list.h"

static const int32_t count = 100000;

static float getTime()
{
    return (float)clock() / CLOCKS_PER_SEC;
}

// value type test
bool intEquals(const int x, const int y)
{
    return x == y;
}

shlDeclareList(IntList, int)
shlDefineList(IntList, int, intEquals, 0)

void valueTypeTest()
{
    float start, end;

    IntList list;
    IntListInit(&list);

    printf("--- Start value type tests ---\n");

    printf("--- Start test 1: add %d objects ---\n", count);
    start = getTime();
    for(int i = 0; i < count; i++)
    {
        IntListAdd(&list, i);
        assert(list.items[list.count - 1] == i);
    }
    end = getTime();
    printf("List count and capacity: (%d, %d)\n", list.count, list.capacity);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 1: add %d objects ---\n", count);

    printf("\n");

    printf("--- Start test 2: contains %d objects ---\n", count / 2);
    start = getTime();
    for(int i = 0; i < count/2; i++)
    {
        int value = rand() % count;
        assert(IntListContains(&list, value));
    }
    end = getTime();
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 2: contains %d objects ---\n", count / 2);

    printf("\n");

    printf("--- Start test 3: remove by index %d objects ---\n", count / 2);
    start = getTime();
    for(int i = 0; i < count/2; i++)
    {
        int index = rand() % list.count;
        int value = list.items[index];
        int deletedValue = IntListRemoveAt(&list, index);
        assert(deletedValue == value);
    }
    end = getTime();
    printf("List count and capacity: (%d, %d)\n", list.count, list.capacity);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 3: remove by index %d objects ---\n", count / 2);

    printf("\n");

    printf("--- Start test 4: remove by value %d objects ---\n", count / 2);
    start = getTime();
    for(int i = 0; i < count/2; i++)
    {
        int value = rand() % count;
        IntListRemove(&list, value);
        assert(!IntListContains(&list, value));
    }
    end = getTime();
    printf("List count and capacity: (%d, %d)\n", list.count, list.capacity);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 4: remove by value %d objects ---\n", count / 2);

    printf("\n");

    printf("--- Start test 5: insert %d objects ---\n", count / 2);
    start = getTime();
    for(int i = 0; i < count/2; i++)
    {
        int index = rand() % list.count;
        IntListInsert(&list, index, index);
        assert(list.items[index] == index);
    }
    end = getTime();
    printf("List count and capacity: (%d, %d)\n", list.count, list.capacity);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 5: insert %d objects ---\n", count / 2);

    printf("--- End value type tests ---\n");
}

// reference type test
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

void referenceTypeTest()
{
    float start, end;

    EntriesList list;
    EntriesListInit(&list);

    printf("--- Start reference type tests ---\n");

    printf("--- Start test 1: add %d objects ---\n", count);
    start = getTime();
    for(int i = 0; i < count; i++)
    {
        Entry *entry = (Entry*)malloc(sizeof(Entry));
        entry->index = i;
        entry->name = "entry";
        EntriesListAdd(&list, entry);
        assert(list.items[list.count - 1]->index == i);
    }
    end = getTime();
    printf("List count and capacity: (%d, %d)\n", list.count, list.capacity);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 1: add %d objects ---\n", count);

    printf("\n");

    printf("--- Start test 2: contains %d objects ---\n", count / 2);
    start = getTime();
    for(int i = 0; i < count/2; i++)
    {
        Entry *entry = (Entry*)malloc(sizeof(Entry));
        entry->index = rand() % count;
        entry->name = "entry";
        assert(EntriesListContains(&list, entry));
        free(entry);
    }
    end = getTime();
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 2: contains %d objects ---\n", count / 2);

    printf("\n");

    printf("--- Start test 3: remove by index %d objects ---\n", count / 2);
    start = getTime();
    for(int i = 0; i < count/2; i++)
    {
        int index = rand() % list.count;
        Entry *entry = list.items[index];
        Entry *deletedEntry = EntriesListRemoveAt(&list, index);
        assert(deletedEntry->index == entry->index);
        free(deletedEntry);
    }
    end = getTime();
    printf("List count and capacity: (%d, %d)\n", list.count, list.capacity);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 3: remove by index %d objects ---\n", count / 2);

    printf("\n");

    printf("--- Start test 4: remove by value %d objects ---\n", count / 2);
    start = getTime();
    for(int i = 0; i < count/2; i++)
    {
        Entry *entry = (Entry*)malloc(sizeof(Entry));
        entry->index = rand() % count;
        entry->name = "entry";
        Entry *deletedEntry = EntriesListRemove(&list, entry);
        assert(!EntriesListContains(&list, deletedEntry));
        free(deletedEntry);
        free(entry);
    }
    end = getTime();
    printf("List count and capacity: (%d, %d)\n", list.count, list.capacity);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 4: remove by value %d objects ---\n", count / 2);

    printf("\n");

    printf("--- Start test 5: insert %d objects ---\n", count / 2);
    start = getTime();
    for(int i = 0; i < count/2; i++)
    {
        Entry *entry = (Entry*)malloc(sizeof(Entry));
        entry->index = rand() % list.count;
        entry->name = "entry";
        EntriesListInsert(&list, entry->index, entry);
        assert(list.items[entry->index]->index == entry->index);
    }
    end = getTime();
    printf("List count and capacity: (%d, %d)\n", list.count, list.capacity);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 5: insert %d objects ---\n", count / 2);

    printf("--- End reference type tests ---\n");
}

int main(int argc, char **argv)
{
    /* initialize random seed: */
    srand(time(NULL));

    valueTypeTest();

    printf("\n\n");

    referenceTypeTest();

    return 0;
}