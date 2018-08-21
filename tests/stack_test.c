#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>

#include "../stack.h"

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

shlDeclareStack(IntStack, int)
shlDefineStack(IntStack, int, intEquals, 0)

void valueTypeTest()
{
    float start, end;

    IntStack stack;
    IntStackInit(&stack);

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

shlDeclareStack(EntriesStack, Entry*)
shlDefineStack(EntriesStack, Entry*, EntryEquals, NULL)

void referenceTypeTest()
{
    float start, end;

    EntriesStack stack;
    EntriesStackInit(&stack);

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