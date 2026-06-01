#include <stdlib.h>
#include <string.h>

/* memzone.h must be included before stack.h so that the #ifdef SHL_MZ_H
   bridge in alloc.h is compiled in. */
#define SHL_MZ_IMPLEMENTATION
#include "../memzone.h"

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

void test_int_stack_returns_zero_for_empty_stack(void)
{
    IntStack stack;
    IntStackInit(&stack, shl_heap_alloc());

    TEST_ASSERT_EQUAL_INT(0, IntStackPeek(&stack));
    TEST_ASSERT_EQUAL_INT(0, IntStackPop(&stack));

    IntStackFree(&stack);
}

void test_int_stack_init_with_invalid_allocator_resets_to_safe_empty_state(void)
{
    shl_allocator_t invalidAlloc = { 0 };
    IntStack stack;
    memset(&stack, 0xA5, sizeof(stack));

    IntStackInit(&stack, &invalidAlloc);

    TEST_ASSERT_EQUAL_INT(0, stack.count);
    TEST_ASSERT_EQUAL_INT(0, stack.capacity);
    TEST_ASSERT_NULL(stack.alloc);
    TEST_ASSERT_NULL(stack.items);

    IntStackPush(&stack, 42);
    TEST_ASSERT_EQUAL_INT(0, stack.count);

    IntStackFree(&stack);
    TEST_ASSERT_EQUAL_INT(0, stack.count);
    TEST_ASSERT_NULL(stack.items);
}

void test_int_stack_push_pop_is_lifo(void)
{
    IntStack stack;
    IntStackInit(&stack, shl_heap_alloc());

    for (int i = 0; i < 16; i++)
    {
        IntStackPush(&stack, i);
        TEST_ASSERT_EQUAL_INT(i + 1, stack.count);
        TEST_ASSERT_EQUAL_INT(i, IntStackPeek(&stack));
    }

    for (int i = 15; i >= 0; i--)
    {
        TEST_ASSERT_TRUE(IntStackContains(&stack, i, intEquals));
        TEST_ASSERT_EQUAL_INT(i, IntStackPop(&stack));
    }

    TEST_ASSERT_EQUAL_INT(0, stack.count);
    TEST_ASSERT_EQUAL_INT(0, IntStackPop(&stack));
    IntStackFree(&stack);
}

void test_int_stack_clear_resets_count(void)
{
    IntStack stack;
    IntStackInit(&stack, shl_heap_alloc());

    for (int i = 0; i < 32; i++)
        IntStackPush(&stack, i);

    IntStackClear(&stack);

    TEST_ASSERT_EQUAL_INT(0, stack.count);
    TEST_ASSERT_EQUAL_INT(0, IntStackPeek(&stack));
    IntStackFree(&stack);
}

void test_int_stack_stress_push_pop_cycle(void)
{
    IntStack stack;
    IntStackInit(&stack, shl_heap_alloc());

    for (int i = 0; i < SHL_TEST_STRESS_COUNT; i++)
        IntStackPush(&stack, i);

    TEST_ASSERT_EQUAL_INT(SHL_TEST_STRESS_COUNT, stack.count);
    TEST_ASSERT_TRUE(stack.capacity >= SHL_TEST_STRESS_COUNT);

    for (int i = SHL_TEST_STRESS_COUNT - 1; i >= SHL_TEST_STRESS_COUNT / 2; i--)
        TEST_ASSERT_EQUAL_INT(i, IntStackPop(&stack));

    TEST_ASSERT_EQUAL_INT(SHL_TEST_STRESS_COUNT / 2, stack.count);
    TEST_ASSERT_EQUAL_INT((SHL_TEST_STRESS_COUNT / 2) - 1, IntStackPeek(&stack));
    IntStackFree(&stack);
}

void test_entry_stack_contains_equivalent_value(void)
{
    EntryStack stack;
    EntryStackInit(&stack, shl_heap_alloc());

    Entry* stored = makeEntry(7, "entry");
    Entry probe = { .index = 7, .name = "entry" };

    EntryStackPush(&stack, stored);

    TEST_ASSERT_TRUE(EntryStackContains(&stack, &probe, entryEquals));
    TEST_ASSERT_EQUAL_PTR(stored, EntryStackPeek(&stack));

    trackedEntryFree(EntryStackPop(&stack));
    EntryStackFree(&stack);
}

/* Clear resets count to zero.  The caller is responsible for freeing
   items before calling Clear; the stack itself never calls item destructors. */
void test_entry_stack_pop_and_clear_update_count(void)
{
    EntryStack stack;
    EntryStackInit(&stack, shl_heap_alloc());

    for (int i = 0; i < 24; i++)
        EntryStackPush(&stack, makeEntry(i, "tracked"));

    /* Caller pops and frees all items before Clear. */
    while (stack.count > 0)
        trackedEntryFree(EntryStackPop(&stack));

    EntryStackClear(&stack);

    TEST_ASSERT_EQUAL_INT(24, g_entryFreeCount);
    TEST_ASSERT_EQUAL_INT(0, stack.count);
    TEST_ASSERT_NULL(EntryStackPeek(&stack));
    EntryStackFree(&stack);
}

void test_entry_stack_pop_half_then_clear_updates_count(void)
{
    EntryStack stack;
    EntryStackInit(&stack, shl_heap_alloc());

    for (int i = 0; i < SHL_TEST_MEDIUM_COUNT; i++)
        EntryStackPush(&stack, makeEntry(i, "bulk"));

    for (int i = 0; i < SHL_TEST_MEDIUM_COUNT / 2; i++)
    {
        Entry* popped = EntryStackPop(&stack);
        TEST_ASSERT_NOT_NULL(popped);
        trackedEntryFree(popped);
    }

    TEST_ASSERT_EQUAL_INT(SHL_TEST_MEDIUM_COUNT / 2, stack.count);

    /* Caller drains the remaining items before Clear. */
    while (stack.count > 0)
        trackedEntryFree(EntryStackPop(&stack));

    EntryStackClear(&stack);
    TEST_ASSERT_EQUAL_INT(SHL_TEST_MEDIUM_COUNT, g_entryFreeCount);
    EntryStackFree(&stack);
}

/* InitFixed binds a caller-owned buffer; alloc is NULL (no heap involvement).
   Items beyond capacity are silently dropped.  Free is a safe no-op. */
void test_int_stack_init_fixed_drops_when_full(void)
{
    int buffer[4];
    IntStack stack;
    IntStackInitFixed(&stack, buffer, 4);

    TEST_ASSERT_NULL(stack.alloc);
    TEST_ASSERT_EQUAL_INT(4, stack.capacity);

    IntStackPush(&stack, 10);
    IntStackPush(&stack, 20);
    IntStackPush(&stack, 30);
    IntStackPush(&stack, 40);
    TEST_ASSERT_EQUAL_INT(4, stack.count);

    /* Items beyond capacity are silently dropped when alloc == NULL. */
    IntStackPush(&stack, 99);
    TEST_ASSERT_EQUAL_INT(4, stack.count);
    TEST_ASSERT_EQUAL_INT(40, IntStackPeek(&stack));

    /* Free is a safe no-op: count is reset, buffer is untouched. */
    IntStackFree(&stack);
    TEST_ASSERT_EQUAL_INT(0, stack.count);
    TEST_ASSERT_EQUAL_INT(40, buffer[3]);
}

/* Zone allocator: allocations are routed through a memzone_t and the items
   buffer lives inside the zone. */
void test_int_stack_zone_alloc_routes_through_zone(void)
{
    memzone_t* zone = mz_init(1 << 20);
    TEST_ASSERT_NOT_NULL(zone);

    shl_allocator_t alloc = shl_zone_alloc(zone);

    IntStack stack;
    IntStackInit(&stack, &alloc);

    for (int i = 0; i < 20; i++)
        IntStackPush(&stack, i * 10);

    TEST_ASSERT_EQUAL_INT(20, stack.count);
    TEST_ASSERT_EQUAL_INT(190, IntStackPeek(&stack));
    TEST_ASSERT_EQUAL_INT(190, IntStackPop(&stack));
    TEST_ASSERT_EQUAL_INT(180, IntStackPeek(&stack));

    /* The items buffer must live inside the zone. */
    TEST_ASSERT_TRUE(mz_contains(zone, stack.items));

    IntStackFree(&stack);
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
    RUN_TEST(test_int_stack_returns_zero_for_empty_stack);
    RUN_TEST(test_int_stack_init_with_invalid_allocator_resets_to_safe_empty_state);
    RUN_TEST(test_int_stack_push_pop_is_lifo);
    RUN_TEST(test_int_stack_clear_resets_count);
    RUN_TEST(test_int_stack_stress_push_pop_cycle);
    RUN_TEST(test_entry_stack_contains_equivalent_value);
    RUN_TEST(test_entry_stack_pop_and_clear_update_count);
    RUN_TEST(test_entry_stack_pop_half_then_clear_updates_count);
    RUN_TEST(test_int_stack_init_fixed_drops_when_full);
    RUN_TEST(test_int_stack_zone_alloc_routes_through_zone);
    return UNITY_END();
}
