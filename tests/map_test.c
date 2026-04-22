#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../map.h"
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

static uint32_t fnv32(char* data)
{
    uint32_t hash = 0x811c9dc5u;
    while (*data != 0)
    {
        hash = ((uint32_t)(unsigned char)(*data++) ^ hash) * 0x01000193u;
    }

    return hash;
}

static bool equalsStr(char* left, char* right)
{
    return strcmp(left, right) == 0;
}

shlDeclareMap(IntMap, int, int)
shlDefineMap(IntMap, int, int)
shlDeclareMap(CollisionMap, int, int)
shlDefineMap(CollisionMap, int, int)
shlDeclareMap(StringMap, char*, char*)
shlDefineMap(StringMap, char*, char*)
shlDeclareMap(TrackedMap, int, int)
shlDefineMap(TrackedMap, int, int)

static int g_mapFreeCount = 0;

static void freeTrackedInt(int value)
{
    g_mapFreeCount += value;
}

static char* duplicateString(const char* text)
{
    size_t length = strlen(text);
    char* copy = (char*)malloc(length + 1u);
    TEST_ASSERT_NOT_NULL(copy);
    memcpy(copy, text, length + 1u);
    return copy;
}

static char* makeKey(int value)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "key-%d", value);
    return duplicateString(buffer);
}

static char* makeUpperValue(const char* text)
{
    size_t length = strlen(text);
    char* copy = duplicateString(text);
    for (size_t i = 0; i < length; i++)
    {
        copy[i] = (char)toupper((unsigned char)copy[i]);
    }
    return copy;
}

static void freeStr(char* str)
{
    free(str);
}

void test_int_map_set_get_and_update_values(void)
{
    IntMap map;
    IntMapInit(&map, (IntMapOptions){ .defaultValue = -1, .hashFn = hashInt, .equalsFn = equalsInt });

    IntMapSet(&map, 2, 4);
    IntMapSet(&map, 3, 9);
    TEST_ASSERT_EQUAL_INT(4, IntMapGet(&map, 2));
    TEST_ASSERT_EQUAL_INT(9, IntMapGet(&map, 3));

    IntMapSet(&map, 2, 200);
    TEST_ASSERT_EQUAL_INT(200, IntMapGet(&map, 2));
    TEST_ASSERT_TRUE(IntMapContains(&map, 2));
    TEST_ASSERT_EQUAL_INT(2, map.count);

    IntMapFree(&map);
}

void test_collision_map_remove_preserves_other_entries(void)
{
    CollisionMap map;
    CollisionMapInit(&map, (CollisionMapOptions){ .defaultValue = -1, .hashFn = collideInt, .equalsFn = equalsInt });

    for (int i = 0; i < 64; i++)
    {
        CollisionMapSet(&map, i, i * 10);
    }

    CollisionMapRemove(&map, 0);
    TEST_ASSERT_FALSE(CollisionMapContains(&map, 0));
    for (int i = 1; i < 64; i++)
    {
        TEST_ASSERT_EQUAL_INT(i * 10, CollisionMapGet(&map, i));
    }
    TEST_ASSERT_EQUAL_INT(63, map.count);

    CollisionMapFree(&map);
}

void test_int_map_stress_remove_even_keys_leaves_odds(void)
{
    IntMap map;
    IntMapInit(&map, (IntMapOptions){ .defaultValue = -1, .hashFn = hashInt, .equalsFn = equalsInt });

    for (int i = 0; i < SHL_TEST_STRESS_COUNT; i++)
    {
        IntMapSet(&map, i, i * i);
    }

    for (int i = 0; i < SHL_TEST_STRESS_COUNT; i += 2)
    {
        IntMapRemove(&map, i);
        TEST_ASSERT_FALSE(IntMapContains(&map, i));
    }

    TEST_ASSERT_EQUAL_INT(SHL_TEST_STRESS_COUNT / 2, map.count);
    for (int i = 1; i < SHL_TEST_STRESS_COUNT; i += 2)
    {
        TEST_ASSERT_TRUE(IntMapContains(&map, i));
        TEST_ASSERT_EQUAL_INT(i * i, IntMapGet(&map, i));
    }

    IntMapFree(&map);
}

void test_tracked_map_clear_calls_free_function_for_live_values(void)
{
    TrackedMap map;
    TrackedMapInit(&map, (TrackedMapOptions){ .defaultValue = 0, .hashFn = collideInt, .equalsFn = equalsInt, .freeFn = freeTrackedInt });

    TrackedMapSet(&map, 1, 1);
    TrackedMapSet(&map, 2, 10);
    TrackedMapSet(&map, 3, 100);
    TrackedMapRemove(&map, 2);

    TEST_ASSERT_EQUAL_INT(10, g_mapFreeCount);
    TrackedMapClear(&map);
    TEST_ASSERT_EQUAL_INT(111, g_mapFreeCount);
    TEST_ASSERT_EQUAL_INT(0, map.count);
    TrackedMapFree(&map);
}

void test_string_map_contains_equivalent_keys_and_updates_values(void)
{
    StringMap map;
    StringMapInit(&map, (StringMapOptions){ .defaultValue = NULL, .hashFn = fnv32, .equalsFn = equalsStr, .freeFn = freeStr });

    char* key = makeKey(7);
    char* initial = makeUpperValue(key);
    char* replacement = duplicateString("UPDATED");
    char probe[32];

    strcpy(probe, key);
    StringMapSet(&map, key, initial);
    TEST_ASSERT_TRUE(StringMapContains(&map, probe));
    TEST_ASSERT_EQUAL_STRING(initial, StringMapGet(&map, probe));

    StringMapSet(&map, key, replacement);
    TEST_ASSERT_EQUAL_STRING("UPDATED", StringMapGet(&map, probe));
    TEST_ASSERT_EQUAL_INT(1, map.count);

    StringMapFree(&map);
    free(key);
}

void test_string_map_integration_bulk_insert_update_and_remove(void)
{
    StringMap map;
    StringMapInit(&map, (StringMapOptions){ .defaultValue = NULL, .hashFn = fnv32, .equalsFn = equalsStr, .freeFn = freeStr });

    char** keys = (char**)calloc((size_t)SHL_TEST_MEDIUM_COUNT, sizeof(char*));
    TEST_ASSERT_NOT_NULL(keys);

    for (int i = 0; i < SHL_TEST_MEDIUM_COUNT; i++)
    {
        keys[i] = makeKey(i);
        StringMapSet(&map, keys[i], makeUpperValue(keys[i]));
    }

    for (int i = 0; i < SHL_TEST_MEDIUM_COUNT; i += 3)
    {
        StringMapSet(&map, keys[i], duplicateString("PATCHED"));
        TEST_ASSERT_EQUAL_STRING("PATCHED", StringMapGet(&map, keys[i]));
    }

    for (int i = 0; i < SHL_TEST_MEDIUM_COUNT; i += 2)
    {
        if (i < 32)
        {
            StringMapRemove(&map, keys[i]);
            TEST_ASSERT_FALSE(StringMapContains(&map, keys[i]));
        }
    }

    for (int i = 1; i < 64; i += 2)
    {
        TEST_ASSERT_TRUE(StringMapContains(&map, keys[i]));
    }

    StringMapFree(&map);
    for (int i = 0; i < SHL_TEST_MEDIUM_COUNT; i++)
    {
        free(keys[i]);
    }
    free(keys);
}

void setUp(void)
{
    g_mapFreeCount = 0;
}

void tearDown(void)
{
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_int_map_set_get_and_update_values);
    RUN_TEST(test_collision_map_remove_preserves_other_entries);
    RUN_TEST(test_int_map_stress_remove_even_keys_leaves_odds);
    RUN_TEST(test_tracked_map_clear_calls_free_function_for_live_values);
    RUN_TEST(test_string_map_contains_equivalent_keys_and_updates_values);
    RUN_TEST(test_string_map_integration_bulk_insert_update_and_remove);
    return UNITY_END();
}
