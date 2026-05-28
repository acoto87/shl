#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "../binary_heap.h"
#include "test_common.h"

static int32_t compareInt(const int a, const int b)
{
    return a - b;
}

static bool equalsInt(const int a, const int b)
{
    return a == b;
}

static int32_t compareStringLength(const char* left, const char* right)
{
    return (int32_t)(strlen(left) - strlen(right));
}

shlDeclareBinaryHeap(IntHeap, int)
shlDefineBinaryHeap(IntHeap, int)
shlDeclareBinaryHeap(StringHeap, const char*)
shlDefineBinaryHeap(StringHeap, const char*)

static char* makeSizedString(int length)
{
    char* text = (char*)malloc((size_t)length + 1u);
    TEST_ASSERT_NOT_NULL(text);
    memset(text, 'a', (size_t)length);
    text[length] = 0;
    return text;
}

void test_heap_returns_zero_for_empty_heap(void)
{
    IntHeap heap;
    IntHeapInit(&heap, shl_heap_alloc(), compareInt);

    TEST_ASSERT_EQUAL_INT(0, IntHeapPeek(&heap));
    TEST_ASSERT_EQUAL_INT(0, IntHeapPop(&heap));

    IntHeapFree(&heap);
}

void test_heap_peek_tracks_minimum_value(void)
{
    const int values[] = { 7, 3, 9, 1, 5 };
    IntHeap heap;
    IntHeapInit(&heap, shl_heap_alloc(), compareInt);

    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++)
        IntHeapPush(&heap, values[i]);

    TEST_ASSERT_EQUAL_INT(1, IntHeapPeek(&heap));
    TEST_ASSERT_TRUE(IntHeapContains(&heap, 7, equalsInt));
    TEST_ASSERT_TRUE(IntHeapIndexOf(&heap, 9, equalsInt) >= 0);
    IntHeapFree(&heap);
}

void test_heap_pop_returns_sorted_values(void)
{
    const int values[] = { 5, 2, 8, 1, 4, 3 };
    IntHeap heap;
    IntHeapInit(&heap, shl_heap_alloc(), compareInt);

    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++)
        IntHeapPush(&heap, values[i]);

    int previous = IntHeapPop(&heap);
    while (heap.count > 0)
    {
        int current = IntHeapPop(&heap);
        TEST_ASSERT_TRUE(previous <= current);
        previous = current;
    }

    IntHeapFree(&heap);
}

void test_heap_update_reorders_entry_both_directions(void)
{
    IntHeap heap;
    IntHeapInit(&heap, shl_heap_alloc(), compareInt);

    IntHeapPush(&heap, 10);
    IntHeapPush(&heap, 20);
    IntHeapPush(&heap, 30);
    IntHeapPush(&heap, 40);

    int index = IntHeapIndexOf(&heap, 30, equalsInt);
    TEST_ASSERT_TRUE(index >= 0);
    IntHeapUpdate(&heap, index, 5);
    TEST_ASSERT_EQUAL_INT(5, IntHeapPeek(&heap));

    index = IntHeapIndexOf(&heap, 10, equalsInt);
    TEST_ASSERT_TRUE(index >= 0);
    IntHeapUpdate(&heap, index, 50);

    int previous = IntHeapPop(&heap);
    while (heap.count > 0)
    {
        int current = IntHeapPop(&heap);
        TEST_ASSERT_TRUE(previous <= current);
        previous = current;
    }

    IntHeapFree(&heap);
}

void test_heap_stress_preserves_sorted_pop_sequence(void)
{
    IntHeap heap;
    IntHeapInit(&heap, shl_heap_alloc(), compareInt);

    int expectedMin = INT_MAX;
    for (int i = 0; i < SHL_TEST_STRESS_COUNT; i++)
    {
        int value = (i * 73) % SHL_TEST_STRESS_COUNT;
        if (value < expectedMin)
            expectedMin = value;
        IntHeapPush(&heap, value);
    }

    TEST_ASSERT_EQUAL_INT(expectedMin, IntHeapPeek(&heap));

    int previous = IntHeapPop(&heap);
    while (heap.count > 0)
    {
        int current = IntHeapPop(&heap);
        TEST_ASSERT_TRUE(previous <= current);
        previous = current;
    }

    IntHeapFree(&heap);
}

void test_string_heap_orders_by_string_length(void)
{
    const int lengths[] = { 8, 3, 5, 1, 6 };
    StringHeap heap;
    StringHeapInit(&heap, shl_heap_alloc(), compareStringLength);

    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++)
        StringHeapPush(&heap, makeSizedString(lengths[i]));

    TEST_ASSERT_EQUAL_size_t(1u, strlen(StringHeapPeek(&heap)));

    const char* first = StringHeapPop(&heap);
    size_t previous = strlen(first);
    free((void*)first);
    while (heap.count > 0)
    {
        const char* text = StringHeapPop(&heap);
        size_t current = strlen(text);
        TEST_ASSERT_TRUE(previous <= current);
        previous = current;
        free((void*)text);
    }

    StringHeapFree(&heap);
}

/* Clear resets count to zero.  The caller is responsible for freeing items
   before calling Clear; the heap itself never calls item destructors. */
void test_heap_clear_resets_count(void)
{
    IntHeap heap;
    IntHeapInit(&heap, shl_heap_alloc(), compareInt);

    for (int i = 0; i < 16; i++)
        IntHeapPush(&heap, i);

    TEST_ASSERT_EQUAL_INT(16, heap.count);
    IntHeapClear(&heap);
    TEST_ASSERT_EQUAL_INT(0, heap.count);
    TEST_ASSERT_EQUAL_INT(0, IntHeapPeek(&heap));

    IntHeapFree(&heap);
}

void setUp(void)
{
}

void tearDown(void)
{
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_heap_returns_zero_for_empty_heap);
    RUN_TEST(test_heap_peek_tracks_minimum_value);
    RUN_TEST(test_heap_pop_returns_sorted_values);
    RUN_TEST(test_heap_update_reorders_entry_both_directions);
    RUN_TEST(test_heap_stress_preserves_sorted_pop_sequence);
    RUN_TEST(test_string_heap_orders_by_string_length);
    RUN_TEST(test_heap_clear_resets_count);
    return UNITY_END();
}
