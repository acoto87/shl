#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../set.h"
#include "test_common.h"

static uint32_t hashInt(const int x)
{
    return (uint32_t)x;
}

static bool equalsInt(const int a, const int b)
{
    return a == b;
}

static uint32_t collideInt(const int x)
{
    (void)x;
    return 1u;
}

static uint32_t fnv32(const char* data)
{
    uint32_t hash = 0x811c9dc5u;
    while (*data != 0)
    {
        hash = ((uint32_t)(unsigned char)(*data++) ^ hash) * 0x01000193u;
    }

    return hash;
}

static bool equalsStr(const char* left, const char* right)
{
    return strcmp(left, right) == 0;
}

shlDeclareSet(IntSet, int)
shlDefineSet(IntSet, int)
shlDeclareSet(CollisionSet, int)
shlDefineSet(CollisionSet, int)
shlDeclareSet(StringSet, char*)
shlDefineSet(StringSet, char*)
shlDeclareSet(TrackedIntSet, int)
shlDefineSet(TrackedIntSet, int)

static int g_setFreeCount = 0;

static void freeTrackedInt(int value)
{
    g_setFreeCount += value;
}

static char* makeStringFromIndex(int value)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "value-%d", value);
    size_t length = strlen(buffer);
    char* text = (char*)malloc(length + 1u);
    TEST_ASSERT_NOT_NULL(text);
    memcpy(text, buffer, length + 1u);
    return text;
}

static void freeStr(char* str)
{
    free(str);
}

void test_int_set_add_contains_and_rejects_duplicates(void)
{
    IntSet set;
    IntSetInit(&set, (IntSetOptions){ .defaultValue = 0, .hashFn = hashInt, .equalsFn = equalsInt });

    TEST_ASSERT_TRUE(IntSetAdd(&set, 1));
    TEST_ASSERT_TRUE(IntSetAdd(&set, 2));
    TEST_ASSERT_TRUE(IntSetAdd(&set, 3));
    TEST_ASSERT_FALSE(IntSetAdd(&set, 2));
    TEST_ASSERT_EQUAL_INT(3, set.count);
    TEST_ASSERT_TRUE(IntSetContains(&set, 1));
    TEST_ASSERT_TRUE(IntSetContains(&set, 2));
    TEST_ASSERT_FALSE(IntSetContains(&set, 99));

    IntSetFree(&set);
}

void test_collision_set_remove_preserves_other_entries(void)
{
    CollisionSet set;
    CollisionSetInit(&set, (CollisionSetOptions){ .defaultValue = -1, .hashFn = collideInt, .equalsFn = equalsInt });

    for (int i = 0; i < 64; i++)
    {
        TEST_ASSERT_TRUE(CollisionSetAdd(&set, i));
    }

    CollisionSetRemove(&set, 0);
    TEST_ASSERT_FALSE(CollisionSetContains(&set, 0));
    for (int i = 1; i < 64; i++)
    {
        TEST_ASSERT_TRUE(CollisionSetContains(&set, i));
    }
    TEST_ASSERT_EQUAL_INT(63, set.count);

    CollisionSetFree(&set);
}

void test_int_set_stress_add_and_remove_halves_count(void)
{
    IntSet set;
    IntSetInit(&set, (IntSetOptions){ .defaultValue = 0, .hashFn = hashInt, .equalsFn = equalsInt });

    for (int i = 0; i < SHL_TEST_STRESS_COUNT; i++)
    {
        TEST_ASSERT_TRUE(IntSetAdd(&set, i));
    }

    for (int i = 0; i < SHL_TEST_STRESS_COUNT; i += 2)
    {
        IntSetRemove(&set, i);
        TEST_ASSERT_FALSE(IntSetContains(&set, i));
    }

    TEST_ASSERT_EQUAL_INT(SHL_TEST_STRESS_COUNT / 2, set.count);
    for (int i = 1; i < SHL_TEST_STRESS_COUNT; i += 2)
    {
        TEST_ASSERT_TRUE(IntSetContains(&set, i));
    }

    IntSetFree(&set);
}

void test_tracked_set_clear_calls_free_function_for_remaining_items(void)
{
    TrackedIntSet set;
    TrackedIntSetInit(&set, (TrackedIntSetOptions){ .defaultValue = 0, .hashFn = collideInt, .equalsFn = equalsInt, .freeFn = freeTrackedInt });

    TEST_ASSERT_TRUE(TrackedIntSetAdd(&set, 1));
    TEST_ASSERT_TRUE(TrackedIntSetAdd(&set, 10));
    TEST_ASSERT_TRUE(TrackedIntSetAdd(&set, 100));
    TrackedIntSetRemove(&set, 10);

    TEST_ASSERT_EQUAL_INT(10, g_setFreeCount);
    TrackedIntSetClear(&set);

    TEST_ASSERT_EQUAL_INT(111, g_setFreeCount);
    TEST_ASSERT_EQUAL_INT(0, set.count);
    TrackedIntSetFree(&set);
}

void test_string_set_contains_equivalent_key_and_releases_removed_values(void)
{
    StringSet set;
    StringSetInit(&set, (StringSetOptions){ .defaultValue = NULL, .hashFn = fnv32, .equalsFn = equalsStr, .freeFn = freeStr });

    char* alpha = makeStringFromIndex(1);
    char* beta = makeStringFromIndex(2);
    char* gamma = makeStringFromIndex(3);
    char probe[32];

    TEST_ASSERT_TRUE(StringSetAdd(&set, alpha));
    TEST_ASSERT_TRUE(StringSetAdd(&set, beta));
    TEST_ASSERT_TRUE(StringSetAdd(&set, gamma));

    strcpy(probe, beta);
    TEST_ASSERT_TRUE(StringSetContains(&set, probe));

    StringSetRemove(&set, probe);
    TEST_ASSERT_FALSE(StringSetContains(&set, probe));
    TEST_ASSERT_EQUAL_INT(2, set.count);

    StringSetFree(&set);
}

void test_string_set_integration_bulk_unique_insert_then_duplicate_probe(void)
{
    StringSet set;
    StringSetInit(&set, (StringSetOptions){ .defaultValue = NULL, .hashFn = fnv32, .equalsFn = equalsStr, .freeFn = freeStr });

    for (int i = 0; i < SHL_TEST_MEDIUM_COUNT; i++)
    {
        TEST_ASSERT_TRUE(StringSetAdd(&set, makeStringFromIndex(i)));
    }

    for (int i = 0; i < SHL_TEST_MEDIUM_COUNT; i += 5)
    {
        char* duplicate = makeStringFromIndex(i);
        TEST_ASSERT_FALSE(StringSetAdd(&set, duplicate));
        freeStr(duplicate);
    }

    TEST_ASSERT_EQUAL_INT(SHL_TEST_MEDIUM_COUNT, set.count);
    TEST_ASSERT_TRUE(StringSetContains(&set, "value-0"));
    TEST_ASSERT_TRUE(StringSetContains(&set, "value-255"));

    StringSetFree(&set);
}

void setUp(void)
{
    g_setFreeCount = 0;
}

void tearDown(void)
{
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_int_set_add_contains_and_rejects_duplicates);
    RUN_TEST(test_collision_set_remove_preserves_other_entries);
    RUN_TEST(test_int_set_stress_add_and_remove_halves_count);
    RUN_TEST(test_tracked_set_clear_calls_free_function_for_remaining_items);
    RUN_TEST(test_string_set_contains_equivalent_key_and_releases_removed_values);
    RUN_TEST(test_string_set_integration_bulk_unique_insert_then_duplicate_probe);
    return UNITY_END();
}
