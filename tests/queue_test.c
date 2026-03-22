#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>

#include "../queue.h"
#include "test_common.h"

#if defined(SHL_LEAK_CHECK)
static const int32_t count = 5000;
#else
static const int32_t count = 100000;
#endif

static float getTime()
{
    return (float)clock() / CLOCKS_PER_SEC;
}

// value type tests
bool intEquals(const int x, const int y)
{
    return x == y;
}

shlDeclareQueue(IntQueue, int)
shlDefineQueue(IntQueue, int)

void edgeCaseValueTypeTest()
{
    IntQueueOptions options = {0};
    options.defaultValue = -1;
    options.equalsFn = intEquals;

    IntQueue queue;
    IntQueueInit(&queue, options);

    assert(IntQueuePeek(&queue) == -1);
    assert(IntQueuePop(&queue) == -1);

    for (int i = 0; i < 6; i++)
        IntQueuePush(&queue, i);
    for (int i = 0; i < 4; i++)
        assert(IntQueuePop(&queue) == i);

    for (int i = 6; i < 18; i++)
        IntQueuePush(&queue, i);

    for (int i = 4; i < 18; i++)
        assert(IntQueuePop(&queue) == i);

    assert(IntQueuePop(&queue) == -1);
    IntQueueFree(&queue);

    IntQueueOptions noEqualsOptions = {0};
    noEqualsOptions.defaultValue = -1;
    IntQueueInit(&queue, noEqualsOptions);
    IntQueuePush(&queue, 1);
    assert(!IntQueueContains(&queue, 1));
    IntQueueFree(&queue);
}

void valueTypeTest()
{
    float start, end;
    int peek;

    IntQueueOptions options = {0};
    options.defaultValue = 0;
    options.equalsFn = intEquals;

    IntQueue queue;
    IntQueueInit(&queue, options);

    printf("--- Start value type tests ---\n");

    printf("--- Start test 1: enqueue %d objects ---\n", count);
    start = getTime();
    for(int i = 0; i < count; i++)
    {
        IntQueuePush(&queue, i);
        assert(IntQueuePeek(&queue) == 0);
    }
    end = getTime();
    printf("Queue count and capacity: (%d, %d)\n", queue.count, queue.capacity);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 1: push %d objects ---\n", count);

    printf("\n");

    printf("--- Start test 2: contains %d objects ---\n", count / 2);
    start = getTime();
    for(int i = 0; i < count/2; i++)
    {
        int value = rand() % count;
        assert(IntQueueContains(&queue, value));
    }
    end = getTime();
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 2: contains %d objects ---\n", count / 2);

    printf("\n");

    printf("--- Start test 3: pop %d objects ---\n", count / 2);
    start = getTime();
    for(int i = 0; i < count/2; i++)
    {
        peek = IntQueuePeek(&queue);
        int value = IntQueuePop(&queue);
        assert(peek == value);
    }
    end = getTime();
    printf("Queue count and capacity: (%d, %d)\n", queue.count, queue.capacity);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 3: pop %d objects ---\n", count / 2);

    printf("\n");

    printf("--- Start test 4: push/pop %d objects ---\n", count / 2);
    start = getTime();
    peek = IntQueuePeek(&queue);
    for(int i = 0; i < count/2; i++)
    {
        if (rand() % 100 > 50)
        {
            IntQueuePush(&queue, i);
            assert(IntQueuePeek(&queue) == peek);
        }
        else
        {
            int value = IntQueuePop(&queue);
            assert(peek == value);
            peek = IntQueuePeek(&queue);
        }
    }
    end = getTime();
    printf("Queue count and capacity: (%d, %d)\n", queue.count, queue.capacity);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 4: push/pop %d objects ---\n", count / 2);

    printf("--- End value type tests ---\n");
    IntQueueFree(&queue);
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

void EntryFree(Entry* e)
{
    free(e);
}

shlDeclareQueue(EntriesQueue, Entry*)
shlDefineQueue(EntriesQueue, Entry*)

static int queueFreeCount = 0;

void EntryTrackFree(Entry* e)
{
    queueFreeCount++;
    free(e);
}

void edgeCaseReferenceTypeTest()
{
    EntriesQueueOptions options = {0};
    options.defaultValue = NULL;
    options.equalsFn = EntryEquals;
    options.freeFn = EntryTrackFree;

    EntriesQueue queue;
    EntriesQueueInit(&queue, options);

    for (int i = 0; i < 6; i++)
    {
        Entry *entry = (Entry*)malloc(sizeof(Entry));
        entry->index = i;
        entry->name = "entry";
        EntriesQueuePush(&queue, entry);
    }

    for (int i = 0; i < 4; i++)
        EntryFree(EntriesQueuePop(&queue));

    for (int i = 6; i < 12; i++)
    {
        Entry *entry = (Entry*)malloc(sizeof(Entry));
        entry->index = i;
        entry->name = "entry";
        EntriesQueuePush(&queue, entry);
    }

    queueFreeCount = 0;
    EntriesQueueClear(&queue);
    assert(queue.count == 0);
    assert(queue.head == 0);
    assert(queue.tail == 0);
    assert(queueFreeCount == 8);
    assert(EntriesQueuePeek(&queue) == NULL);

    EntriesQueueFree(&queue);
}

void referenceTypeTest()
{
    float start, end;
    Entry *peek;

    EntriesQueueOptions options = {0};
    options.defaultValue = NULL;
    options.equalsFn = EntryEquals;
    options.freeFn = EntryFree;

    EntriesQueue queue;
    EntriesQueueInit(&queue, options);

    printf("--- Start reference type tests ---\n");

    printf("--- Start test 1: enqueue %d objects ---\n", count);
    start = getTime();
    for(int i = 0; i < count; i++)
    {
        Entry *entry = (Entry*)malloc(sizeof(Entry));
        entry->index = i;
        entry->name = "entry";
        EntriesQueuePush(&queue, entry);
        assert(EntriesQueuePeek(&queue)->index == 0);
    }
    end = getTime();
    printf("Queue count and capacity: (%d, %d)\n", queue.count, queue.capacity);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 1: push %d objects ---\n", count);

    printf("\n");

    printf("--- Start test 2: contains %d objects ---\n", count / 2);
    start = getTime();
    for(int i = 0; i < count/2; i++)
    {
        Entry *entry = (Entry*)malloc(sizeof(Entry));
        entry->index = rand() % count;
        entry->name = "entry";
        assert(EntriesQueueContains(&queue, entry));
        free(entry);
    }
    end = getTime();
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 2: contains %d objects ---\n", count / 2);

    printf("\n");

    printf("--- Start test 3: pop %d objects ---\n", count / 2);
    start = getTime();
    for(int i = 0; i < count/2; i++)
    {
        peek = EntriesQueuePeek(&queue);
        Entry *entry = EntriesQueuePop(&queue);
        assert(peek->index == entry->index);
        EntryFree(entry);
    }
    end = getTime();
    printf("Queue count and capacity: (%d, %d)\n", queue.count, queue.capacity);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 3: pop %d objects ---\n", count / 2);

    printf("\n");

    printf("--- Start test 4: push/pop %d objects ---\n", count / 2);
    start = getTime();
    peek = EntriesQueuePeek(&queue);
    for(int i = 0; i < count/2; i++)
    {
        if (rand() % 100 > 50)
        {
            Entry *entry = (Entry*)malloc(sizeof(Entry));
            entry->index = rand() % count;
            entry->name = "entry";
            EntriesQueuePush(&queue, entry);
            assert(EntriesQueuePeek(&queue)->index == peek->index);
        }
        else
        {
            Entry *entry = EntriesQueuePop(&queue);
            assert(peek->index == entry->index);
            peek = EntriesQueuePeek(&queue);
            EntryFree(entry);
        }
    }
    end = getTime();
    printf("Queue count and capacity: (%d, %d)\n", queue.count, queue.capacity);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 4: push/pop %d objects ---\n", count / 2);

    printf("--- End reference type tests ---\n");
    EntriesQueueFree(&queue);
}

void setUp(void)
{
}

void tearDown(void)
{
}

int main(void)
{
    /* initialize random seed: */
    srand(time(NULL));

    UNITY_BEGIN();
    RUN_TEST(valueTypeTest);
    RUN_TEST(edgeCaseValueTypeTest);
    RUN_TEST(referenceTypeTest);
    RUN_TEST(edgeCaseReferenceTypeTest);
    return UNITY_END();
}
