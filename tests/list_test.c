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

int32_t intCompare(const int x, const int y)
{
    return x - y;
}

shlDeclareList(IntList, int)
shlDefineList(IntList, int)

void valueTypeTest()
{
    float start, end;

    IntListOptions options = IntListDefaultOptions();
    options.equalsFn = intEquals;

    IntList list;
    IntListInit(&list, options);

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
        IntListRemoveAt(&list, index);
        assert(list.items[index] != value);
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

    printf("\n");

    const int rangeCount = 50000;
    const int rangeAt = 100;

    int* rangeValues = (int*)malloc(rangeCount*sizeof(int));
    for(int i = 0; i < rangeCount; i++)
        rangeValues[i] = -i;

    printf("--- Start test 6: insert range %d objects ---\n", rangeCount);
    start = getTime();
    IntListInsertRange(&list, rangeAt, rangeCount, rangeValues);
    end = getTime();
    for(int i = 0; i < rangeCount; i++)
        assert(list.items[rangeAt + i] == rangeValues[i]);
    printf("List count and capacity: (%d, %d)\n", list.count, list.capacity);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 6: insert range %d objects ---\n", rangeCount);

    printf("\n");

    printf("--- Start test 7: remove range %d objects ---\n", rangeCount);
    start = getTime();
    IntListRemoveAtRange(&list, rangeAt, rangeCount);
    end = getTime();
    for(int i = 0; i < rangeCount; i++)
        assert(list.items[rangeAt + i] != rangeValues[i]);
    printf("List count and capacity: (%d, %d)\n", list.count, list.capacity);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 7: remove range %d objects ---\n", rangeCount);

    free(rangeValues);

    printf("\n");

    printf("--- Start test 8: sorting %d objects ---\n", list.count);
    start = getTime();
    IntListSort(&list, intCompare);
    end = getTime();
    for(int i = 1; i < list.count; i++)
        assert(list.items[i - 1] <= list.items[i]);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 8: sorting %d objects ---\n", list.count);

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
    return e1 == e2;
}

int32_t EntryCompare(const Entry *e1, const Entry *e2)
{
    if (e1->index == e2->index)
        return strcmp(e1->name, e2->name);
    return e1->index - e2->index;
}

void EntryFree(Entry* e)
{
    free(e);
}

shlDeclareList(EntriesList, Entry*)
shlDefineList(EntriesList, Entry*)

void referenceTypeTest()
{
    float start, end;
    Entry** entries;

    EntriesListOptions options = EntriesListDefaultOptions();
    options.defaultValue = NULL;
    options.equalsFn = EntryEquals;
    options.freeFn = EntryFree;

    EntriesList list;
    EntriesListInit(&list, options);

    printf("--- Start reference type tests ---\n");

    printf("--- Start test 1: add %d objects ---\n", count);
    entries = malloc(count * sizeof(Entry*));
    start = getTime();
    for(int i = 0; i < count; i++)
    {
        Entry *entry = (Entry*)malloc(sizeof(Entry));
        entry->index = i;
        entry->name = "entry";
        EntriesListAdd(&list, entry);
        assert(list.items[list.count - 1]->index == i);
        entries[i] = entry;
    }
    end = getTime();
    printf("List count and capacity: (%d, %d)\n", list.count, list.capacity);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 1: add %d objects ---\n", count);

    printf("\n");

    printf("--- Start test 2: contains %d objects ---\n", count / 2);
    start = getTime();
    for(int i = 0; i < count / 2; i++)
    {
        Entry *entry = entries[rand() % count];
        assert(EntriesListContains(&list, entry));
    }
    end = getTime();
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 2: contains %d objects ---\n", count / 2);

    free(entries);

    printf("\n");

    printf("--- Start test 3: insert %d objects ---\n", count / 2);
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
    printf("--- End test 3: insert %d objects ---\n", count / 2);

    printf("\n");

    printf("--- Start test 4: remove by index %d objects ---\n", count / 2);
    start = getTime();
    for(int i = 0; i < count/2; i++)
    {
        int index = rand() % (list.count - 1);
        Entry* entry = list.items[index + 1];
        EntriesListRemoveAt(&list, index);
        assert(EntryEquals(list.items[index], entry));
    }
    end = getTime();
    printf("List count and capacity: (%d, %d)\n", list.count, list.capacity);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 4: remove by index %d objects ---\n", count / 2);

    printf("\n");

    printf("--- Start test 5: remove by value %d objects ---\n", count / 2);
    
    entries = malloc((count / 2) * sizeof(Entry*));
    for(int i = 0; i < count / 2; i++)
    {
        Entry* entry = (Entry*)malloc(sizeof(Entry));
        entry->index = rand() % list.count;
        entry->name = "negative";
        EntriesListInsert(&list, entry->index, entry);
        entries[i] = entry;
    }
    start = getTime();
    for(int i = 0; i < count / 2; i++)
        EntriesListRemove(&list, entries[i]);
    end = getTime();
    for(int i = 0; i < list.count; i++)
        assert(strcmp(list.items[i]->name, "negative") != 0);
    printf("List count and capacity: (%d, %d)\n", list.count, list.capacity);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 5: remove by value %d objects ---\n", count / 2);

    free(entries);

    printf("\n");

    const int rangeCount = 50000;
    const int rangeAt = 100;

    Entry** rangeValues = (Entry**)malloc(rangeCount * sizeof(Entry*));
    for(int i = 0; i < rangeCount; i++)
    {
        rangeValues[i] = malloc(sizeof(Entry));
        rangeValues[i]->index = -i;
        rangeValues[i]->name = "insert range";
    }

    printf("--- Start test 6: insert range %d objects ---\n", rangeCount);
    start = getTime();
    EntriesListInsertRange(&list, rangeAt, rangeCount, rangeValues);
    end = getTime();
    for(int i = 0; i < rangeCount; i++)
        assert(EntryEquals(list.items[rangeAt + i], rangeValues[i]));
    printf("List count and capacity: (%d, %d)\n", list.count, list.capacity);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 6: insert range %d objects ---\n", rangeCount);

    printf("\n");

    printf("--- Start test 7: remove range %d objects ---\n", rangeCount);
    start = getTime();
    EntriesListRemoveAtRange(&list, rangeAt, rangeCount);
    end = getTime();
    for(int i = 0; i < rangeCount; i++)
        assert(list.items[rangeAt + i]->index >= 0);
    printf("List count and capacity: (%d, %d)\n", list.count, list.capacity);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 7: remove range %d objects ---\n", rangeCount);

    free(rangeValues);

    printf("\n");

    printf("--- Start test 8: sorting %d objects ---\n", list.count);
    start = getTime();
    EntriesListSort(&list, EntryCompare);
    end = getTime();
    for(int i = 1; i < list.count; i++)
        assert(EntryCompare(list.items[i - 1], list.items[i]) <= 0);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 8: sorting %d objects ---\n", list.count);

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
