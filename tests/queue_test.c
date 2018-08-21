#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>

#include "../queue.h"

static const int32_t count = 100000;

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
shlDefineQueue(IntQueue, int, intEquals, 0)

void valueTypeTest()
{
    float start, end;
    int peek;

    IntQueue queue;
    IntQueueInit(&queue);

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

shlDeclareQueue(EntriesQueue, Entry*)
shlDefineQueue(EntriesQueue, Entry*, EntryEquals, NULL)

void referenceTypeTest()
{
    float start, end;
    Entry *peek;

    EntriesQueue queue;
    EntriesQueueInit(&queue);

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
        }
    }
    end = getTime();
    printf("Queue count and capacity: (%d, %d)\n", queue.count, queue.capacity);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 4: push/pop %d objects ---\n", count / 2);

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