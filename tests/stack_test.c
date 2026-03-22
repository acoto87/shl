#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>

#include "../stack.h"
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

shlDeclareStack(IntStack, int)
shlDefineStack(IntStack, int)

void edgeCaseValueTypeTest()
{
    IntStackOptions options = {0};
    options.defaultValue = -1;
    options.equalsFn = intEquals;

    IntStack stack;
    IntStackInit(&stack, options);

    assert(IntStackPeek(&stack) == -1);
    assert(IntStackPop(&stack) == -1);

    for (int i = 0; i < count * 2; i++)
        IntStackPush(&stack, i);

    for (int i = count * 2 - 1; i >= count; i--)
        assert(IntStackPop(&stack) == i);

    IntStackClear(&stack);
    assert(stack.count == 0);
    assert(IntStackPop(&stack) == -1);

    IntStackFree(&stack);
}

void valueTypeTest()
{
    float start, end;

    IntStackOptions options = {0};
    options.defaultValue = 0;
    options.equalsFn = intEquals;

    IntStack stack;
    IntStackInit(&stack, options);

    printf("--- Start value type tests ---\n");

    printf("--- Start test 1: push %d objects ---\n", count);
    start = getTime();
    for(int i = 0; i < count; i++)
    {
        IntStackPush(&stack, i);
        assert(IntStackPeek(&stack) == i);
    }
    end = getTime();
    printf("Stack count and capacity: (%d, %d)\n", stack.count, stack.capacity);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 1: push %d objects ---\n", count);

    printf("\n");

    printf("--- Start test 2: contains %d objects ---\n", count / 2);
    start = getTime();
    for(int i = 0; i < count/2; i++)
    {
        int value = rand() % count;
        assert(IntStackContains(&stack, value));
    }
    end = getTime();
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 2: contains %d objects ---\n", count / 2);

    printf("\n");

    printf("--- Start test 3: pop %d objects ---\n", count / 2);
    start = getTime();
    for(int i = 0; i < count/2; i++)
    {
        int peek = IntStackPeek(&stack);
        int value = IntStackPop(&stack);
        assert(peek == value);
    }
    end = getTime();
    printf("Stack count and capacity: (%d, %d)\n", stack.count, stack.capacity);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 3: pop %d objects ---\n", count / 2);

    printf("--- End value type tests ---\n");
    IntStackFree(&stack);
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

shlDeclareStack(EntriesStack, Entry*)
shlDefineStack(EntriesStack, Entry*)

static int stackFreeCount = 0;

void EntryTrackFree(Entry* e)
{
    stackFreeCount++;
    free(e);
}

void edgeCaseReferenceTypeTest()
{
    EntriesStackOptions options = {0};
    options.defaultValue = NULL;
    options.equalsFn = EntryEquals;
    options.freeFn = EntryTrackFree;

    EntriesStack stack;
    EntriesStackInit(&stack, options);

    for (int i = 0; i < 32; i++)
    {
        Entry *entry = (Entry*)malloc(sizeof(Entry));
        entry->index = i;
        entry->name = "entry";
        EntriesStackPush(&stack, entry);
    }

    stackFreeCount = 0;
    EntriesStackClear(&stack);
    assert(stack.count == 0);
    assert(stackFreeCount == 32);
    assert(EntriesStackPeek(&stack) == NULL);

    EntriesStackFree(&stack);
}

void referenceTypeTest()
{
    float start, end;

    EntriesStackOptions options = {0};
    options.defaultValue = NULL;
    options.equalsFn = EntryEquals;
    options.freeFn = EntryFree;

    EntriesStack stack;
    EntriesStackInit(&stack, options);

    printf("--- Start reference type tests ---\n");

    printf("--- Start test 1: push %d objects ---\n", count);
    start = getTime();
    for(int i = 0; i < count; i++)
    {
        Entry *entry = (Entry*)malloc(sizeof(Entry));
        entry->index = i;
        entry->name = "entry";
        EntriesStackPush(&stack, entry);
        assert(EntriesStackPeek(&stack)->index == entry->index);
    }
    end = getTime();
    printf("Stack count and capacity: (%d, %d)\n", stack.count, stack.capacity);
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
        assert(EntriesStackContains(&stack, entry));
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
        Entry *peek = EntriesStackPeek(&stack);
        Entry *entry = EntriesStackPop(&stack);
        assert(peek->index == entry->index);
        free(entry);
    }
    end = getTime();
    printf("Stack count and capacity: (%d, %d)\n", stack.count, stack.capacity);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 3: pop %d objects ---\n", count / 2);

    printf("--- End reference type tests ---\n");
    EntriesStackFree(&stack);
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
