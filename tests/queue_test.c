#include <stdlib.h>
#include <string.h>

#include "../queue.h"
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

shlDeclareQueue(IntQueue, int)
shlDefineQueue(IntQueue, int)
shlDeclareQueue(EntryQueue, Entry*)
shlDefineQueue(EntryQueue, Entry*)

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

void test_int_queue_returns_default_for_empty_queue(void)
{
    IntQueue queue;
    IntQueueInit(&queue, (IntQueueOptions){ .defaultValue = -1, .equalsFn = intEquals });

    TEST_ASSERT_EQUAL_INT(-1, IntQueuePeek(&queue));
    TEST_ASSERT_EQUAL_INT(-1, IntQueuePop(&queue));

    IntQueueFree(&queue);
}

void test_int_queue_preserves_fifo_order(void)
{
    IntQueue queue;
    IntQueueInit(&queue, (IntQueueOptions){ .defaultValue = -1, .equalsFn = intEquals });

    for (int i = 0; i < 16; i++)
    {
        IntQueuePush(&queue, i);
        TEST_ASSERT_EQUAL_INT(0, IntQueuePeek(&queue));
    }

    for (int i = 0; i < 16; i++)
    {
        TEST_ASSERT_TRUE(IntQueueContains(&queue, i));
        TEST_ASSERT_EQUAL_INT(i, IntQueuePop(&queue));
    }

    TEST_ASSERT_EQUAL_INT(0, queue.count);
    TEST_ASSERT_EQUAL_INT(-1, IntQueuePop(&queue));
    IntQueueFree(&queue);
}

void test_int_queue_wraparound_keeps_order(void)
{
    IntQueue queue;
    IntQueueInit(&queue, (IntQueueOptions){ .defaultValue = -1, .equalsFn = intEquals });

    for (int i = 0; i < 6; i++)
    {
        IntQueuePush(&queue, i);
    }

    for (int i = 0; i < 4; i++)
    {
        TEST_ASSERT_EQUAL_INT(i, IntQueuePop(&queue));
    }

    for (int i = 6; i < 18; i++)
    {
        IntQueuePush(&queue, i);
    }

    for (int i = 4; i < 18; i++)
    {
        TEST_ASSERT_EQUAL_INT(i, IntQueuePop(&queue));
    }

    TEST_ASSERT_EQUAL_INT(0, queue.count);
    TEST_ASSERT_EQUAL_INT(-1, IntQueuePeek(&queue));
    IntQueueFree(&queue);
}

void test_int_queue_stress_push_pop_mix_keeps_consistent_front(void)
{
    IntQueue queue;
    IntQueueInit(&queue, (IntQueueOptions){ .defaultValue = -1, .equalsFn = intEquals });

    int nextExpected = 0;
    for (int i = 0; i < SHL_TEST_STRESS_COUNT; i++)
    {
        IntQueuePush(&queue, i);
    }

    for (int i = 0; i < SHL_TEST_STRESS_COUNT / 2; i++)
    {
        TEST_ASSERT_EQUAL_INT(nextExpected++, IntQueuePop(&queue));
        IntQueuePush(&queue, SHL_TEST_STRESS_COUNT + i);
        TEST_ASSERT_EQUAL_INT(nextExpected, IntQueuePeek(&queue));
    }

    TEST_ASSERT_EQUAL_INT(SHL_TEST_STRESS_COUNT, queue.count);
    IntQueueFree(&queue);
}

void test_entry_queue_contains_equivalent_value(void)
{
    EntryQueue queue;
    EntryQueueInit(&queue, (EntryQueueOptions){ .defaultValue = NULL, .equalsFn = entryEquals, .freeFn = entryFree });

    Entry* stored = makeEntry(11, "entry");
    Entry probe = { .index = 11, .name = "entry" };

    EntryQueuePush(&queue, stored);
    TEST_ASSERT_TRUE(EntryQueueContains(&queue, &probe));
    TEST_ASSERT_EQUAL_PTR(stored, EntryQueuePeek(&queue));

    EntryQueueFree(&queue);
}

void test_entry_queue_clear_calls_free_function_after_wraparound(void)
{
    EntryQueue queue;
    EntryQueueInit(&queue, (EntryQueueOptions){ .defaultValue = NULL, .equalsFn = entryEquals, .freeFn = trackedEntryFree });

    for (int i = 0; i < 6; i++)
    {
        EntryQueuePush(&queue, makeEntry(i, "entry"));
    }
    for (int i = 0; i < 4; i++)
    {
        trackedEntryFree(EntryQueuePop(&queue));
    }
    for (int i = 6; i < 14; i++)
    {
        EntryQueuePush(&queue, makeEntry(i, "entry"));
    }

    EntryQueueClear(&queue);

    TEST_ASSERT_EQUAL_INT(14, g_entryFreeCount);
    TEST_ASSERT_EQUAL_INT(0, queue.count);
    TEST_ASSERT_EQUAL_INT(0, queue.head);
    TEST_ASSERT_EQUAL_INT(0, queue.tail);
    TEST_ASSERT_NULL(EntryQueuePeek(&queue));
    EntryQueueFree(&queue);
}

void test_entry_queue_integration_pop_half_then_clear_releases_all_items(void)
{
    EntryQueue queue;
    EntryQueueInit(&queue, (EntryQueueOptions){ .defaultValue = NULL, .equalsFn = entryEquals, .freeFn = trackedEntryFree });

    for (int i = 0; i < SHL_TEST_MEDIUM_COUNT; i++)
    {
        EntryQueuePush(&queue, makeEntry(i, "bulk"));
    }

    for (int i = 0; i < SHL_TEST_MEDIUM_COUNT / 2; i++)
    {
        Entry* entry = EntryQueuePop(&queue);
        TEST_ASSERT_EQUAL_INT(i, entry->index);
        trackedEntryFree(entry);
    }

    TEST_ASSERT_EQUAL_INT(SHL_TEST_MEDIUM_COUNT / 2, queue.count);
    EntryQueueClear(&queue);
    TEST_ASSERT_EQUAL_INT(SHL_TEST_MEDIUM_COUNT, g_entryFreeCount);
    EntryQueueFree(&queue);
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
    RUN_TEST(test_int_queue_returns_default_for_empty_queue);
    RUN_TEST(test_int_queue_preserves_fifo_order);
    RUN_TEST(test_int_queue_wraparound_keeps_order);
    RUN_TEST(test_int_queue_stress_push_pop_mix_keeps_consistent_front);
    RUN_TEST(test_entry_queue_contains_equivalent_value);
    RUN_TEST(test_entry_queue_clear_calls_free_function_after_wraparound);
    RUN_TEST(test_entry_queue_integration_pop_half_then_clear_releases_all_items);
    return UNITY_END();
}
