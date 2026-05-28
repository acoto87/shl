#include <stdlib.h>
#include <string.h>

/* memzone.h must be included before queue.h so that the #ifdef SHL_MZ_H
   bridge in alloc.h is compiled in. */
#define SHL_MZ_IMPLEMENTATION
#include "../memzone.h"

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

void test_int_queue_returns_zero_for_empty_queue(void)
{
    IntQueue queue;
    IntQueueInit(&queue, shl_heap_alloc());

    TEST_ASSERT_EQUAL_INT(0, IntQueuePeek(&queue));
    TEST_ASSERT_EQUAL_INT(0, IntQueuePop(&queue));

    IntQueueFree(&queue);
}

void test_int_queue_preserves_fifo_order(void)
{
    IntQueue queue;
    IntQueueInit(&queue, shl_heap_alloc());

    for (int i = 0; i < 16; i++)
    {
        IntQueuePush(&queue, i);
        TEST_ASSERT_EQUAL_INT(0, IntQueuePeek(&queue));
    }

    for (int i = 0; i < 16; i++)
    {
        TEST_ASSERT_TRUE(IntQueueContains(&queue, i, intEquals));
        TEST_ASSERT_EQUAL_INT(i, IntQueuePop(&queue));
    }

    TEST_ASSERT_EQUAL_INT(0, queue.count);
    TEST_ASSERT_EQUAL_INT(0, IntQueuePop(&queue));
    IntQueueFree(&queue);
}

void test_int_queue_wraparound_keeps_order(void)
{
    IntQueue queue;
    IntQueueInit(&queue, shl_heap_alloc());

    for (int i = 0; i < 6; i++)
        IntQueuePush(&queue, i);

    for (int i = 0; i < 4; i++)
        TEST_ASSERT_EQUAL_INT(i, IntQueuePop(&queue));

    for (int i = 6; i < 18; i++)
        IntQueuePush(&queue, i);

    for (int i = 4; i < 18; i++)
        TEST_ASSERT_EQUAL_INT(i, IntQueuePop(&queue));

    TEST_ASSERT_EQUAL_INT(0, queue.count);
    TEST_ASSERT_EQUAL_INT(0, IntQueuePeek(&queue));
    IntQueueFree(&queue);
}

void test_int_queue_stress_push_pop_mix_keeps_consistent_front(void)
{
    IntQueue queue;
    IntQueueInit(&queue, shl_heap_alloc());

    int nextExpected = 0;
    for (int i = 0; i < SHL_TEST_STRESS_COUNT; i++)
        IntQueuePush(&queue, i);

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
    EntryQueueInit(&queue, shl_heap_alloc());

    Entry* stored = makeEntry(11, "entry");
    Entry probe = { .index = 11, .name = "entry" };

    EntryQueuePush(&queue, stored);
    TEST_ASSERT_TRUE(EntryQueueContains(&queue, &probe, entryEquals));
    TEST_ASSERT_EQUAL_PTR(stored, EntryQueuePeek(&queue));

    trackedEntryFree(EntryQueuePop(&queue));
    EntryQueueFree(&queue);
}

/* Clear resets count/head/tail to zero.  The caller is responsible for
   freeing items before (or instead of) calling Clear. */
void test_entry_queue_clear_resets_state_after_wraparound(void)
{
    EntryQueue queue;
    EntryQueueInit(&queue, shl_heap_alloc());

    /* Push 6, pop 4 to force wraparound, then push 8 more => 10 items. */
    for (int i = 0; i < 6; i++)
        EntryQueuePush(&queue, makeEntry(i, "entry"));

    for (int i = 0; i < 4; i++)
        trackedEntryFree(EntryQueuePop(&queue));

    for (int i = 6; i < 14; i++)
        EntryQueuePush(&queue, makeEntry(i, "entry"));

    TEST_ASSERT_EQUAL_INT(10, queue.count);

    /* Caller pops and frees the remaining items before Clear. */
    while (queue.count > 0)
        trackedEntryFree(EntryQueuePop(&queue));

    EntryQueueClear(&queue);

    TEST_ASSERT_EQUAL_INT(14, g_entryFreeCount);
    TEST_ASSERT_EQUAL_INT(0, queue.count);
    TEST_ASSERT_EQUAL_INT(0, queue.head);
    TEST_ASSERT_EQUAL_INT(0, queue.tail);
    TEST_ASSERT_NULL(EntryQueuePeek(&queue));
    EntryQueueFree(&queue);
}

void test_entry_queue_pop_half_then_clear_updates_count(void)
{
    EntryQueue queue;
    EntryQueueInit(&queue, shl_heap_alloc());

    for (int i = 0; i < SHL_TEST_MEDIUM_COUNT; i++)
        EntryQueuePush(&queue, makeEntry(i, "bulk"));

    for (int i = 0; i < SHL_TEST_MEDIUM_COUNT / 2; i++)
    {
        Entry* entry = EntryQueuePop(&queue);
        TEST_ASSERT_EQUAL_INT(i, entry->index);
        trackedEntryFree(entry);
    }

    TEST_ASSERT_EQUAL_INT(SHL_TEST_MEDIUM_COUNT / 2, queue.count);

    /* Caller drains remaining items before Clear. */
    while (queue.count > 0)
        trackedEntryFree(EntryQueuePop(&queue));

    EntryQueueClear(&queue);
    TEST_ASSERT_EQUAL_INT(SHL_TEST_MEDIUM_COUNT, g_entryFreeCount);
    EntryQueueFree(&queue);
}

/* InitFixed binds a caller-owned buffer; alloc is NULL (no heap involvement).
   Items beyond capacity are silently dropped.  Free is a safe no-op. */
void test_int_queue_init_fixed_drops_when_full(void)
{
    int buffer[4];
    IntQueue queue;
    IntQueueInitFixed(&queue, buffer, 4);

    TEST_ASSERT_NULL(queue.alloc);
    TEST_ASSERT_EQUAL_INT(4, queue.capacity);

    IntQueuePush(&queue, 10);
    IntQueuePush(&queue, 20);
    IntQueuePush(&queue, 30);
    IntQueuePush(&queue, 40);
    TEST_ASSERT_EQUAL_INT(4, queue.count);

    /* Items beyond capacity are silently dropped when alloc == NULL. */
    IntQueuePush(&queue, 99);
    TEST_ASSERT_EQUAL_INT(4, queue.count);
    TEST_ASSERT_EQUAL_INT(10, IntQueuePeek(&queue));

    /* Free is a safe no-op: count is reset, buffer is untouched. */
    IntQueueFree(&queue);
    TEST_ASSERT_EQUAL_INT(0, queue.count);
    TEST_ASSERT_EQUAL_INT(10, buffer[0]);
}

/* Zone allocator: allocations are routed through a memzone_t and the items
   buffer lives inside the zone. */
void test_int_queue_zone_alloc_routes_through_zone(void)
{
    memzone_t* zone = mz_init(1 << 20);
    TEST_ASSERT_NOT_NULL(zone);

    shl_allocator_t alloc = shl_zone_alloc(zone);

    IntQueue queue;
    IntQueueInit(&queue, &alloc);

    for (int i = 0; i < 20; i++)
        IntQueuePush(&queue, i * 10);

    TEST_ASSERT_EQUAL_INT(20, queue.count);
    TEST_ASSERT_EQUAL_INT(0,   IntQueuePeek(&queue));
    TEST_ASSERT_EQUAL_INT(0,   IntQueuePop(&queue));
    TEST_ASSERT_EQUAL_INT(10,  IntQueuePeek(&queue));

    /* The items buffer must live inside the zone. */
    TEST_ASSERT_TRUE(mz_contains(zone, queue.items));

    IntQueueFree(&queue);
    mz_destroy(zone);
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
    RUN_TEST(test_int_queue_returns_zero_for_empty_queue);
    RUN_TEST(test_int_queue_preserves_fifo_order);
    RUN_TEST(test_int_queue_wraparound_keeps_order);
    RUN_TEST(test_int_queue_stress_push_pop_mix_keeps_consistent_front);
    RUN_TEST(test_entry_queue_contains_equivalent_value);
    RUN_TEST(test_entry_queue_clear_resets_state_after_wraparound);
    RUN_TEST(test_entry_queue_pop_half_then_clear_updates_count);
    RUN_TEST(test_int_queue_init_fixed_drops_when_full);
    RUN_TEST(test_int_queue_zone_alloc_routes_through_zone);
    return UNITY_END();
}
