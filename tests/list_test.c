#include <stdlib.h>
#include <string.h>

#include "../list.h"
#include "test_common.h"

static bool intEquals(const int x, const int y)
{
    return x == y;
}

static int32_t intCompare(const int x, const int y)
{
    return x - y;
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

static int32_t entryCompare(const Entry* left, const Entry* right)
{
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

void test_int_list_get_returns_default_when_out_of_range(void)
{
    IntList list;
    IntListInit(&list, (IntListOptions){ .defaultValue = -1, .equalsFn = intEquals });

    TEST_ASSERT_EQUAL_INT(-1, IntListGet(&list, 0));
    TEST_ASSERT_EQUAL_INT(-1, IntListGet(&list, 42));

    IntListFree(&list);
}

void test_int_list_insert_remove_and_contains_work_together(void)
{
    IntList list;
    IntListInit(&list, (IntListOptions){ .defaultValue = -1, .equalsFn = intEquals });

    IntListAdd(&list, 2);
    IntListAdd(&list, 4);
    IntListInsert(&list, 0, 1);
    IntListInsert(&list, 2, 3);

    TEST_ASSERT_EQUAL_INT(4, list.count);
    TEST_ASSERT_EQUAL_INT(1, IntListGet(&list, 0));
    TEST_ASSERT_EQUAL_INT(2, IntListGet(&list, 1));
    TEST_ASSERT_EQUAL_INT(3, IntListGet(&list, 2));
    TEST_ASSERT_EQUAL_INT(4, IntListGet(&list, 3));
    TEST_ASSERT_TRUE(IntListContains(&list, 3));

    IntListRemove(&list, 2);
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
    IntListInit(&list, (IntListOptions){ .defaultValue = -1, .equalsFn = intEquals });

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
    IntListInit(&list, (IntListOptions){ .defaultValue = -1, .equalsFn = intEquals });

    IntListAddRange(&list, 6, (int*)values);
    IntListSort(&list, intCompare);

    for (int i = 1; i < list.count; i++)
    {
        TEST_ASSERT_TRUE(list.items[i - 1] <= list.items[i]);
    }

    IntListFree(&list);
}

void test_int_list_stress_insert_range_and_remove_range(void)
{
    IntList list;
    IntListInit(&list, (IntListOptions){ .defaultValue = -1, .equalsFn = intEquals });

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

void test_entry_list_set_releases_replaced_item(void)
{
    EntryList list;
    EntryListInit(&list, (EntryListOptions){ .defaultValue = NULL, .equalsFn = entryEquals, .freeFn = trackedEntryFree });

    EntryListAdd(&list, makeEntry(1, "one"));
    EntryListAdd(&list, makeEntry(2, "two"));
    EntryListSet(&list, 0, makeEntry(3, "three"));

    TEST_ASSERT_EQUAL_INT(1, g_entryFreeCount);
    TEST_ASSERT_EQUAL_INT(3, list.items[0]->index);
    EntryListFree(&list);
}

void test_entry_list_remove_range_and_clear_call_free_function(void)
{
    EntryList list;
    EntryListInit(&list, (EntryListOptions){ .defaultValue = NULL, .equalsFn = entryEquals, .freeFn = trackedEntryFree });

    for (int i = 0; i < 10; i++)
    {
        EntryListAdd(&list, makeEntry(i, "entry"));
    }

    EntryListRemoveAtRange(&list, 3, 4);
    TEST_ASSERT_EQUAL_INT(4, g_entryFreeCount);
    TEST_ASSERT_EQUAL_INT(6, list.count);

    EntryListClear(&list);
    TEST_ASSERT_EQUAL_INT(10, g_entryFreeCount);
    TEST_ASSERT_EQUAL_INT(0, list.count);
    EntryListFree(&list);
}

void test_entry_list_integration_sorts_remaining_entries_after_mutations(void)
{
    EntryList list;
    EntryListInit(&list, (EntryListOptions){ .defaultValue = NULL, .equalsFn = entryEquals, .freeFn = entryFree });

    EntryListAdd(&list, makeEntry(5, "e"));
    EntryListAdd(&list, makeEntry(2, "b"));
    EntryListInsert(&list, 1, makeEntry(4, "d"));
    EntryListInsert(&list, 0, makeEntry(1, "a"));
    EntryListAdd(&list, makeEntry(3, "c"));
    EntryListRemove(&list, &(Entry){ .index = 4, .name = "d" });

    TEST_ASSERT_EQUAL_INT(4, list.count);
    EntryListSort(&list, entryCompare);

    for (int i = 1; i < list.count; i++)
    {
        TEST_ASSERT_TRUE(entryCompare(list.items[i - 1], list.items[i]) <= 0);
    }

    TEST_ASSERT_EQUAL_INT(1, list.items[0]->index);
    TEST_ASSERT_EQUAL_INT(5, list.items[3]->index);
    EntryListFree(&list);
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
    RUN_TEST(test_int_list_get_returns_default_when_out_of_range);
    RUN_TEST(test_int_list_insert_remove_and_contains_work_together);
    RUN_TEST(test_int_list_range_operations_copy_and_reverse);
    RUN_TEST(test_int_list_sort_orders_values_ascending);
    RUN_TEST(test_int_list_stress_insert_range_and_remove_range);
    RUN_TEST(test_entry_list_set_releases_replaced_item);
    RUN_TEST(test_entry_list_remove_range_and_clear_call_free_function);
    RUN_TEST(test_entry_list_integration_sorts_remaining_entries_after_mutations);
    return UNITY_END();
}
