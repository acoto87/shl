#include <stdlib.h>
#include <string.h>

/* memzone.h must be included before list.h so that the #ifdef SHL_MZ_H
   bridge in shl_internal.h is compiled in. */
#define SHL_MZ_IMPLEMENTATION
#include "../memzone.h"

#include "../list.h"
#include "test_common.h"

static bool intEquals(const int x, const int y)
{
    return x == y;
}

static int32_t intCompare(const int x, const int y, void* userdata)
{
    (void)userdata;
    return x - y;
}

static int32_t intCompareDescending(const int x, const int y, void* userdata)
{
    (void)userdata;
    return y - x;
}

/* userdata points to an int field offset within a struct; used to sort
   flat int arrays by a chosen "column" index. Here we use it simply as
   a multiplier (+1 / -1) to flip sort direction at runtime. */
static int32_t intCompareWithDirection(const int x, const int y, void* userdata)
{
    int direction = *(int*)userdata; /* +1 = ascending, -1 = descending */
    return direction * (x - y);
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

static int32_t entryCompare(const Entry* left, const Entry* right, void* userdata)
{
    (void)userdata;
    if (left->index == right->index)
    {
        return strcmp(left->name, right->name);
    }

    return left->index - right->index;
}

static void entryFree(Entry* entry)
{
    free(entry);
}

shlDeclareList(IntList, int)
shlDefineList(IntList, int)
shlDeclareList(EntryList, Entry*)
shlDefineList(EntryList, Entry*)

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

void test_shl_heap_alloc_returns_valid_allocator(void)
{
    shl_allocator_t* alloc = shl_heap_alloc();
    TEST_ASSERT_NOT_NULL(alloc);
    TEST_ASSERT_NOT_NULL(alloc->mallocFn);
    TEST_ASSERT_NOT_NULL(alloc->reallocFn);
    TEST_ASSERT_NOT_NULL(alloc->freeFn);

    /* Returns the same stable pointer on every call. */
    TEST_ASSERT_EQUAL_PTR(alloc, shl_heap_alloc());

    IntList list;
    IntListInit(&list, alloc);
    IntListAdd(&list, 42);
    TEST_ASSERT_EQUAL_INT(42, IntListGet(&list, 0));
    IntListFree(&list);
}

void test_int_list_get_returns_zero_when_out_of_range(void)
{
    IntList list;
    IntListInit(&list, shl_heap_alloc());

    TEST_ASSERT_EQUAL_INT(0, IntListGet(&list, 0));
    TEST_ASSERT_EQUAL_INT(0, IntListGet(&list, 42));

    IntListFree(&list);
}

void test_int_list_init_with_invalid_allocator_resets_to_safe_empty_state(void)
{
    shl_allocator_t invalidAlloc = { 0 };
    IntList list;
    memset(&list, 0xA5, sizeof(list));

    IntListInit(&list, &invalidAlloc);

    TEST_ASSERT_EQUAL_INT(0, list.count);
    TEST_ASSERT_EQUAL_INT(0, list.capacity);
    TEST_ASSERT_NULL(list.alloc);
    TEST_ASSERT_NULL(list.items);

    IntListAdd(&list, 42);
    TEST_ASSERT_EQUAL_INT(0, list.count);

    IntListFree(&list);
    TEST_ASSERT_EQUAL_INT(0, list.count);
    TEST_ASSERT_NULL(list.items);
}

void test_int_list_insert_remove_and_contains_work_together(void)
{
    IntList list;
    IntListInit(&list, shl_heap_alloc());

    IntListAdd(&list, 2);
    IntListAdd(&list, 4);
    IntListInsert(&list, 0, 1);
    IntListInsert(&list, 2, 3);

    TEST_ASSERT_EQUAL_INT(4, list.count);
    TEST_ASSERT_EQUAL_INT(1, IntListGet(&list, 0));
    TEST_ASSERT_EQUAL_INT(2, IntListGet(&list, 1));
    TEST_ASSERT_EQUAL_INT(3, IntListGet(&list, 2));
    TEST_ASSERT_EQUAL_INT(4, IntListGet(&list, 3));
    TEST_ASSERT_TRUE(IntListContains(&list, 3, intEquals));

    IntListRemove(&list, 2, intEquals);
    IntListRemoveAt(&list, 0);

    TEST_ASSERT_EQUAL_INT(2, list.count);
    TEST_ASSERT_EQUAL_INT(3, IntListGet(&list, 0));
    TEST_ASSERT_EQUAL_INT(4, IntListGet(&list, 1));
    IntListFree(&list);
}

void test_int_list_range_operations_copy_and_reverse(void)
{
    const int values[] = { 1, 2, 3, 4, 5 };
    const int range[] = { 9, 8, 7 };
    int copy[10] = {0};
    IntList list;
    IntListInit(&list, shl_heap_alloc());

    IntListAddRange(&list, 5, (int*)values);
    IntListInsertRange(&list, 2, 3, (int*)range);
    TEST_ASSERT_EQUAL_INT(8, list.count);
    TEST_ASSERT_EQUAL_INT(9, IntListGet(&list, 2));
    TEST_ASSERT_EQUAL_INT(7, IntListGet(&list, 4));

    IntListRemoveAtRange(&list, 2, 3);
    TEST_ASSERT_EQUAL_INT(5, list.count);

    IntListReverse(&list);
    TEST_ASSERT_EQUAL_INT(5, IntListGet(&list, 0));
    TEST_ASSERT_EQUAL_INT(1, IntListGet(&list, 4));

    IntListCopyTo(&list, copy, 2);
    TEST_ASSERT_EQUAL_INT(5, copy[2]);
    TEST_ASSERT_EQUAL_INT(1, copy[6]);

    int* array = IntListToArray(&list);
    TEST_ASSERT_NOT_NULL(array);
    TEST_ASSERT_EQUAL_INT(5, array[0]);
    TEST_ASSERT_EQUAL_INT(1, array[4]);
    free(array);
    IntListFree(&list);
}

void test_int_list_sort_orders_values_ascending(void)
{
    const int values[] = { 9, 1, 5, 3, 7, 2 };
    IntList list;
    IntListInit(&list, shl_heap_alloc());

    IntListAddRange(&list, 6, (int*)values);
    IntListSort(&list, intCompare, NULL);

    for (int i = 1; i < list.count; i++)
    {
        TEST_ASSERT_TRUE(list.items[i - 1] <= list.items[i]);
    }

    IntListFree(&list);
}

void test_int_list_stress_insert_range_and_remove_range(void)
{
    IntList list;
    IntListInit(&list, shl_heap_alloc());

    for (int i = 0; i < SHL_TEST_STRESS_COUNT; i++)
    {
        IntListAdd(&list, i);
    }

    int* range = (int*)malloc((size_t)SHL_TEST_MEDIUM_COUNT * sizeof(int));
    TEST_ASSERT_NOT_NULL(range);
    for (int i = 0; i < SHL_TEST_MEDIUM_COUNT; i++)
    {
        range[i] = -i;
    }

    IntListInsertRange(&list, 32, SHL_TEST_MEDIUM_COUNT, range);
    TEST_ASSERT_EQUAL_INT(SHL_TEST_STRESS_COUNT + SHL_TEST_MEDIUM_COUNT, list.count);
    TEST_ASSERT_EQUAL_INT(0, IntListGet(&list, 32));
    TEST_ASSERT_EQUAL_INT(-(SHL_TEST_MEDIUM_COUNT - 1), IntListGet(&list, 32 + SHL_TEST_MEDIUM_COUNT - 1));

    IntListRemoveAtRange(&list, 32, SHL_TEST_MEDIUM_COUNT);
    TEST_ASSERT_EQUAL_INT(SHL_TEST_STRESS_COUNT, list.count);
    TEST_ASSERT_EQUAL_INT(32, IntListGet(&list, 32));

    free(range);
    IntListFree(&list);
}

void test_int_list_sort_descending_via_compare_fn(void)
{
    const int values[] = { 3, 1, 4, 1, 5, 9, 2, 6 };
    IntList list;
    IntListInit(&list, shl_heap_alloc());

    IntListAddRange(&list, 8, (int*)values);
    IntListSort(&list, intCompareDescending, NULL);

    for (int i = 1; i < list.count; i++)
    {
        TEST_ASSERT_TRUE(list.items[i - 1] >= list.items[i]);
    }

    IntListFree(&list);
}

void test_int_list_sort_direction_controlled_by_userdata(void)
{
    const int values[] = { 9, 1, 5, 3, 7, 2 };
    IntList asc, desc;
    IntListInit(&asc,  shl_heap_alloc());
    IntListInit(&desc, shl_heap_alloc());

    IntListAddRange(&asc,  6, (int*)values);
    IntListAddRange(&desc, 6, (int*)values);

    int ascending  =  1;
    int descending = -1;
    IntListSort(&asc,  intCompareWithDirection, &ascending);
    IntListSort(&desc, intCompareWithDirection, &descending);

    for (int i = 1; i < asc.count; i++)
    {
        TEST_ASSERT_TRUE(asc.items[i - 1]  <= asc.items[i]);
        TEST_ASSERT_TRUE(desc.items[i - 1] >= desc.items[i]);
    }

    /* The two lists should be mirror images of each other. */
    for (int i = 0; i < asc.count; i++)
    {
        TEST_ASSERT_EQUAL_INT(asc.items[i], desc.items[asc.count - 1 - i]);
    }

    IntListFree(&asc);
    IntListFree(&desc);
}

/* Set replaces the item at the given index.  The caller is responsible for
   freeing the old item — the list does not manage item lifecycle. */
void test_entry_list_set_replaces_item_at_index(void)
{
    EntryList list;
    EntryListInit(&list, shl_heap_alloc());

    Entry* original = makeEntry(1, "one");
    EntryListAdd(&list, original);
    EntryListAdd(&list, makeEntry(2, "two"));

    EntryListSet(&list, 0, makeEntry(3, "three"));

    TEST_ASSERT_EQUAL_INT(2, list.count);
    TEST_ASSERT_EQUAL_INT(3, list.items[0]->index);

    /* Caller frees the item that was replaced and all items still in the list. */
    trackedEntryFree(original);
    for (int i = 0; i < list.count; i++)
        trackedEntryFree(list.items[i]);

    EntryListFree(&list);
    TEST_ASSERT_EQUAL_INT(3, g_entryFreeCount);
}

/* RemoveAtRange and Clear update count.  The caller frees items before
   removing them; the list itself never calls item destructors. */
void test_entry_list_remove_range_and_clear_update_count(void)
{
    EntryList list;
    EntryListInit(&list, shl_heap_alloc());

    for (int i = 0; i < 10; i++)
        EntryListAdd(&list, makeEntry(i, "entry"));

    /* Caller frees items [3..6] before removing the range. */
    for (int i = 3; i < 7; i++)
        trackedEntryFree(list.items[i]);

    EntryListRemoveAtRange(&list, 3, 4);
    TEST_ASSERT_EQUAL_INT(4, g_entryFreeCount);
    TEST_ASSERT_EQUAL_INT(6, list.count);

    /* Caller frees remaining items before clearing. */
    for (int i = 0; i < list.count; i++)
        trackedEntryFree(list.items[i]);

    EntryListClear(&list);
    TEST_ASSERT_EQUAL_INT(10, g_entryFreeCount);
    TEST_ASSERT_EQUAL_INT(0, list.count);
    EntryListFree(&list);
}

void test_entry_list_integration_sorts_remaining_entries_after_mutations(void)
{
    EntryList list;
    EntryListInit(&list, shl_heap_alloc());

    EntryListAdd(&list, makeEntry(5, "e"));
    EntryListAdd(&list, makeEntry(2, "b"));
    EntryListInsert(&list, 1, makeEntry(4, "d"));
    EntryListInsert(&list, 0, makeEntry(1, "a"));
    EntryListAdd(&list, makeEntry(3, "c"));

    /* Caller locates, frees, then removes the item. */
    Entry needle = { .index = 4, .name = "d" };
    int32_t idx = EntryListIndexOf(&list, &needle, entryEquals);
    TEST_ASSERT_TRUE(idx >= 0);
    entryFree(list.items[idx]);
    EntryListRemoveAt(&list, idx);

    TEST_ASSERT_EQUAL_INT(4, list.count);
    EntryListSort(&list, entryCompare, NULL);

    for (int i = 1; i < list.count; i++)
    {
        TEST_ASSERT_TRUE(entryCompare(list.items[i - 1], list.items[i], NULL) <= 0);
    }

    TEST_ASSERT_EQUAL_INT(1, list.items[0]->index);
    TEST_ASSERT_EQUAL_INT(5, list.items[3]->index);

    /* Caller frees all remaining items before Free. */
    for (int i = 0; i < list.count; i++)
        entryFree(list.items[i]);
    EntryListFree(&list);
}

/* InitFixed binds a caller-owned buffer; alloc is NULL (no heap involvement).
   Items beyond capacity are silently dropped.  Free is a safe no-op. */
void test_int_list_init_fixed_uses_stack_buffer(void)
{
    int buffer[4];
    IntList list;
    IntListInitFixed(&list, buffer, 4);

    TEST_ASSERT_NULL(list.alloc);
    TEST_ASSERT_EQUAL_INT(4, list.capacity);

    IntListAdd(&list, 10);
    IntListAdd(&list, 20);
    IntListAdd(&list, 30);
    IntListAdd(&list, 40);
    TEST_ASSERT_EQUAL_INT(4, list.count);

    /* Items beyond capacity are silently dropped when alloc == NULL. */
    IntListAdd(&list, 99);
    TEST_ASSERT_EQUAL_INT(4, list.count);
    TEST_ASSERT_EQUAL_INT(10, IntListGet(&list, 0));
    TEST_ASSERT_EQUAL_INT(40, IntListGet(&list, 3));

    /* InsertRange into a full fixed list is also a no-op. */
    int extra[] = { 55, 66 };
    IntListInsertRange(&list, 1, 2, extra);
    TEST_ASSERT_EQUAL_INT(4, list.count);

    /* Free is a safe no-op: count is reset, buffer is untouched. */
    IntListFree(&list);
    TEST_ASSERT_EQUAL_INT(0, list.count);
    TEST_ASSERT_EQUAL_INT(40, buffer[3]);
}

/* Zone allocator: allocations are routed through a memzone_t and the items
   buffer lives inside the zone. */
void test_int_list_zone_alloc_routes_through_zone(void)
{
    memzone_t* zone = mz_init(1 << 20);
    TEST_ASSERT_NOT_NULL(zone);

    shl_allocator_t alloc = shl_zone_alloc(zone);

    IntList list;
    IntListInit(&list, &alloc);

    for (int i = 0; i < 20; i++)
        IntListAdd(&list, i * 10);

    TEST_ASSERT_EQUAL_INT(20, list.count);
    TEST_ASSERT_EQUAL_INT(0,   IntListGet(&list, 0));
    TEST_ASSERT_EQUAL_INT(190, IntListGet(&list, 19));

    /* The items buffer must be a live allocation inside the zone. */
    TEST_ASSERT_TRUE(mz_contains(zone, list.items));

    IntListFree(&list);
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
    RUN_TEST(test_shl_heap_alloc_returns_valid_allocator);
    RUN_TEST(test_int_list_get_returns_zero_when_out_of_range);
    RUN_TEST(test_int_list_init_with_invalid_allocator_resets_to_safe_empty_state);
    RUN_TEST(test_int_list_insert_remove_and_contains_work_together);
    RUN_TEST(test_int_list_range_operations_copy_and_reverse);
    RUN_TEST(test_int_list_sort_orders_values_ascending);
    RUN_TEST(test_int_list_sort_descending_via_compare_fn);
    RUN_TEST(test_int_list_sort_direction_controlled_by_userdata);
    RUN_TEST(test_int_list_stress_insert_range_and_remove_range);
    RUN_TEST(test_entry_list_set_replaces_item_at_index);
    RUN_TEST(test_entry_list_remove_range_and_clear_update_count);
    RUN_TEST(test_entry_list_integration_sorts_remaining_entries_after_mutations);
    RUN_TEST(test_int_list_init_fixed_uses_stack_buffer);
    RUN_TEST(test_int_list_zone_alloc_routes_through_zone);
    return UNITY_END();
}
