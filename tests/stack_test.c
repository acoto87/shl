#include <stdlib.h>
#include <string.h>

#include "../stack.h"
#include "test_common.h"

static bool intEquals(const int x, const int y)
{
    return x == y;
}

typedef struct
{
    int index;
    const char* name;
} Entry;

static bool entryEquals(const Entry* left, const Entry* right)
{
    return left->index == right->index && strcmp(left->name, right->name) == 0;
}

static void entryFree(Entry* entry)
{
    free(entry);
}

shlDeclareStack(IntStack, int)
shlDefineStack(IntStack, int)
shlDeclareStack(EntryStack, Entry*)
shlDefineStack(EntryStack, Entry*)

static int g_entryFreeCount = 0;

static void trackedEntryFree(Entry* entry)
{
    g_entryFreeCount++;
    free(entry);
}

static Entry* makeEntry(int index, const char* name)
{
    Entry* entry = (Entry*)malloc(sizeof(Entry));
    TEST_ASSERT_NOT_NULL(entry);
    entry->index = index;
    entry->name = name;
    return entry;
}

void test_int_stack_returns_default_for_empty_stack(void)
{
    IntStack stack;
    IntStackInit(&stack, (IntStackOptions){ .defaultValue = -1, .equalsFn = intEquals });

    TEST_ASSERT_EQUAL_INT(-1, IntStackPeek(&stack));
    TEST_ASSERT_EQUAL_INT(-1, IntStackPop(&stack));

    IntStackFree(&stack);
}

void test_int_stack_push_pop_is_lifo(void)
{
    IntStack stack;
    IntStackInit(&stack, (IntStackOptions){ .defaultValue = -1, .equalsFn = intEquals });

    for (int i = 0; i < 16; i++)
    {
        IntStackPush(&stack, i);
        TEST_ASSERT_EQUAL_INT(i + 1, stack.count);
        TEST_ASSERT_EQUAL_INT(i, IntStackPeek(&stack));
    }

    for (int i = 15; i >= 0; i--)
    {
        TEST_ASSERT_TRUE(IntStackContains(&stack, i));
        TEST_ASSERT_EQUAL_INT(i, IntStackPop(&stack));
    }

    TEST_ASSERT_EQUAL_INT(0, stack.count);
    TEST_ASSERT_EQUAL_INT(-1, IntStackPop(&stack));
    IntStackFree(&stack);
}

void test_int_stack_clear_resets_count(void)
{
    IntStack stack;
    IntStackInit(&stack, (IntStackOptions){ .defaultValue = -1, .equalsFn = intEquals });

    for (int i = 0; i < 32; i++)
    {
        IntStackPush(&stack, i);
    }

    IntStackClear(&stack);

    TEST_ASSERT_EQUAL_INT(0, stack.count);
    TEST_ASSERT_EQUAL_INT(-1, IntStackPeek(&stack));
    IntStackFree(&stack);
}

void test_int_stack_stress_push_pop_cycle(void)
{
    IntStack stack;
    IntStackInit(&stack, (IntStackOptions){ .defaultValue = -1, .equalsFn = intEquals });

    for (int i = 0; i < SHL_TEST_STRESS_COUNT; i++)
    {
        IntStackPush(&stack, i);
    }

    TEST_ASSERT_EQUAL_INT(SHL_TEST_STRESS_COUNT, stack.count);
    TEST_ASSERT_TRUE(stack.capacity >= SHL_TEST_STRESS_COUNT);

    for (int i = SHL_TEST_STRESS_COUNT - 1; i >= SHL_TEST_STRESS_COUNT / 2; i--)
    {
        TEST_ASSERT_EQUAL_INT(i, IntStackPop(&stack));
    }

    TEST_ASSERT_EQUAL_INT(SHL_TEST_STRESS_COUNT / 2, stack.count);
    TEST_ASSERT_EQUAL_INT((SHL_TEST_STRESS_COUNT / 2) - 1, IntStackPeek(&stack));
    IntStackFree(&stack);
}

void test_entry_stack_contains_equivalent_value(void)
{
    EntryStack stack;
    EntryStackInit(&stack, (EntryStackOptions){ .defaultValue = NULL, .equalsFn = entryEquals, .freeFn = entryFree });

    Entry* stored = makeEntry(7, "entry");
    Entry probe = { .index = 7, .name = "entry" };

    EntryStackPush(&stack, stored);

    TEST_ASSERT_TRUE(EntryStackContains(&stack, &probe));
    TEST_ASSERT_EQUAL_PTR(stored, EntryStackPeek(&stack));

    EntryStackFree(&stack);
}

void test_entry_stack_clear_calls_free_function(void)
{
    EntryStack stack;
    EntryStackInit(&stack, (EntryStackOptions){ .defaultValue = NULL, .equalsFn = entryEquals, .freeFn = trackedEntryFree });

    g_entryFreeCount = 0;
    for (int i = 0; i < 24; i++)
    {
        EntryStackPush(&stack, makeEntry(i, "tracked"));
    }

    EntryStackClear(&stack);

    TEST_ASSERT_EQUAL_INT(24, g_entryFreeCount);
    TEST_ASSERT_EQUAL_INT(0, stack.count);
    TEST_ASSERT_NULL(EntryStackPeek(&stack));
    EntryStackFree(&stack);
}

void test_entry_stack_integration_pop_then_clear_releases_remaining_items(void)
{
    EntryStack stack;
    EntryStackInit(&stack, (EntryStackOptions){ .defaultValue = NULL, .equalsFn = entryEquals, .freeFn = trackedEntryFree });

    g_entryFreeCount = 0;
    for (int i = 0; i < SHL_TEST_MEDIUM_COUNT; i++)
    {
        EntryStackPush(&stack, makeEntry(i, "bulk"));
    }

    for (int i = 0; i < SHL_TEST_MEDIUM_COUNT / 2; i++)
    {
        Entry* popped = EntryStackPop(&stack);
        TEST_ASSERT_NOT_NULL(popped);
        trackedEntryFree(popped);
    }

    TEST_ASSERT_EQUAL_INT(SHL_TEST_MEDIUM_COUNT / 2, stack.count);
    EntryStackClear(&stack);
    TEST_ASSERT_EQUAL_INT(SHL_TEST_MEDIUM_COUNT, g_entryFreeCount);
    EntryStackFree(&stack);
}

void setUp(void)
{
    g_entryFreeCount = 0;
}

void tearDown(void)
{
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_int_stack_returns_default_for_empty_stack);
    RUN_TEST(test_int_stack_push_pop_is_lifo);
    RUN_TEST(test_int_stack_clear_resets_count);
    RUN_TEST(test_int_stack_stress_push_pop_cycle);
    RUN_TEST(test_entry_stack_contains_equivalent_value);
    RUN_TEST(test_entry_stack_clear_calls_free_function);
    RUN_TEST(test_entry_stack_integration_pop_then_clear_releases_remaining_items);
    return UNITY_END();
}
