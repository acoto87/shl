#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* memzone.h must be included before map.h so that the #ifdef SHL_MZ_H
   bridge in alloc.h is compiled in. */
#define SHL_MZ_IMPLEMENTATION
#include "../memzone.h"

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

void test_int_map_set_get_and_update_values(void)
{
    IntMap map;
    IntMapInit(&map, shl_heap_alloc(), hashInt, equalsInt);

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

void test_int_map_init_with_missing_callbacks_resets_to_safe_empty_state(void)
{
    IntMap map;
    memset(&map, 0xA5, sizeof(map));

    IntMapInit(&map, shl_heap_alloc(), NULL, equalsInt);

    TEST_ASSERT_EQUAL_INT(0, map.count);
    TEST_ASSERT_EQUAL_INT(0, map.capacity);
    TEST_ASSERT_EQUAL_INT(0, map.loadFactor);
    TEST_ASSERT_EQUAL_INT(0, map.shift);
    TEST_ASSERT_NULL(map.alloc);
    TEST_ASSERT_NULL(map.hashFn);
    TEST_ASSERT_NULL(map.equalsFn);
    TEST_ASSERT_NULL(map.entries);

    IntMapSet(&map, 1, 2);
    TEST_ASSERT_EQUAL_INT(0, map.count);
    TEST_ASSERT_FALSE(IntMapContains(&map, 1));
    TEST_ASSERT_EQUAL_INT(0, IntMapGet(&map, 1));

    IntMapFree(&map);
    TEST_ASSERT_EQUAL_INT(0, map.count);
    TEST_ASSERT_NULL(map.entries);
}

void test_int_map_get_returns_zero_when_key_absent(void)
{
    IntMap map;
    IntMapInit(&map, shl_heap_alloc(), hashInt, equalsInt);

    TEST_ASSERT_EQUAL_INT(0, IntMapGet(&map, 99));
    IntMapSet(&map, 1, 42);
    TEST_ASSERT_EQUAL_INT(0, IntMapGet(&map, 2));

    IntMapFree(&map);
}

void test_collision_map_remove_preserves_other_entries(void)
{
    CollisionMap map;
    CollisionMapInit(&map, shl_heap_alloc(), collideInt, equalsInt);

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
    IntMapInit(&map, shl_heap_alloc(), hashInt, equalsInt);

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

/* Remove decrements count; Clear resets it to zero.  The caller (not the
   map) is responsible for freeing any resources owned by values. */
void test_int_map_remove_and_clear_update_count(void)
{
    /* Use collideInt so all keys land in the same bucket, exercising the
       collision chain through Set, Remove, and Clear. */
    IntMap map;
    IntMapInit(&map, shl_heap_alloc(), collideInt, equalsInt);

    IntMapSet(&map, 1, 1);
    IntMapSet(&map, 2, 10);
    IntMapSet(&map, 3, 100);
    TEST_ASSERT_EQUAL_INT(3, map.count);

    IntMapRemove(&map, 2);
    TEST_ASSERT_EQUAL_INT(2, map.count);
    TEST_ASSERT_FALSE(IntMapContains(&map, 2));
    TEST_ASSERT_TRUE(IntMapContains(&map, 1));
    TEST_ASSERT_TRUE(IntMapContains(&map, 3));

    IntMapClear(&map);
    TEST_ASSERT_EQUAL_INT(0, map.count);
    TEST_ASSERT_FALSE(IntMapContains(&map, 1));
    TEST_ASSERT_FALSE(IntMapContains(&map, 3));
    IntMapFree(&map);
}

void test_string_map_contains_equivalent_keys_and_updates_values(void)
{
    StringMap map;
    StringMapInit(&map, shl_heap_alloc(), fnv32, equalsStr);

    char* key = makeKey(7);
    char* initial = makeUpperValue(key);
    char* replacement = duplicateString("UPDATED");
    char probe[32];

    strcpy(probe, key);
    StringMapSet(&map, key, initial);
    TEST_ASSERT_TRUE(StringMapContains(&map, probe));
    TEST_ASSERT_EQUAL_STRING(initial, StringMapGet(&map, probe));

    /* Caller frees the old value before replacing it. */
    free(StringMapGet(&map, probe));
    StringMapSet(&map, key, replacement);
    TEST_ASSERT_EQUAL_STRING("UPDATED", StringMapGet(&map, probe));
    TEST_ASSERT_EQUAL_INT(1, map.count);

    /* Caller frees remaining value before releasing the map. */
    free(StringMapGet(&map, key));
    StringMapFree(&map);
    free(key);
}

void test_string_map_integration_bulk_insert_update_and_remove(void)
{
    StringMap map;
    StringMapInit(&map, shl_heap_alloc(), fnv32, equalsStr);

    char** keys = (char**)calloc((size_t)SHL_TEST_MEDIUM_COUNT, sizeof(char*));
    TEST_ASSERT_NOT_NULL(keys);

    for (int i = 0; i < SHL_TEST_MEDIUM_COUNT; i++)
    {
        keys[i] = makeKey(i);
        StringMapSet(&map, keys[i], makeUpperValue(keys[i]));
    }

    for (int i = 0; i < SHL_TEST_MEDIUM_COUNT; i += 3)
    {
        /* Caller frees old value before replacing it. */
        free(StringMapGet(&map, keys[i]));
        StringMapSet(&map, keys[i], duplicateString("PATCHED"));
        TEST_ASSERT_EQUAL_STRING("PATCHED", StringMapGet(&map, keys[i]));
    }

    for (int i = 0; i < SHL_TEST_MEDIUM_COUNT; i += 2)
    {
        if (i < 32)
        {
            /* Caller frees the value before removing the entry. */
            free(StringMapGet(&map, keys[i]));
            StringMapRemove(&map, keys[i]);
            TEST_ASSERT_FALSE(StringMapContains(&map, keys[i]));
        }
    }

    for (int i = 1; i < 64; i += 2)
    {
        TEST_ASSERT_TRUE(StringMapContains(&map, keys[i]));
    }

    /* Free remaining live values (Get returns NULL for absent keys). */
    for (int i = 0; i < SHL_TEST_MEDIUM_COUNT; i++)
    {
        char* val = StringMapGet(&map, keys[i]);
        if (val)
            free(val);
    }

    StringMapFree(&map);
    for (int i = 0; i < SHL_TEST_MEDIUM_COUNT; i++)
        free(keys[i]);
    free(keys);
}

/* Zone allocator: allocations are routed through a memzone_t and the
   entries buffer lives inside the zone. */
void test_int_map_zone_alloc_routes_through_zone(void)
{
    memzone_t* zone = mz_init(1 << 20);
    TEST_ASSERT_NOT_NULL(zone);

    shl_allocator_t alloc = shl_zone_alloc(zone);

    IntMap map;
    IntMapInit(&map, &alloc, hashInt, equalsInt);

    for (int i = 0; i < 20; i++)
        IntMapSet(&map, i, i * i);

    TEST_ASSERT_EQUAL_INT(20, map.count);
    TEST_ASSERT_EQUAL_INT(0,   IntMapGet(&map, 0));
    TEST_ASSERT_EQUAL_INT(361, IntMapGet(&map, 19));

    /* The entries buffer must live inside the zone. */
    TEST_ASSERT_TRUE(mz_contains(zone, map.entries));

    IntMapFree(&map);
    mz_destroy(zone);
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
    RUN_TEST(test_int_map_set_get_and_update_values);
    RUN_TEST(test_int_map_init_with_missing_callbacks_resets_to_safe_empty_state);
    RUN_TEST(test_int_map_get_returns_zero_when_key_absent);
    RUN_TEST(test_collision_map_remove_preserves_other_entries);
    RUN_TEST(test_int_map_stress_remove_even_keys_leaves_odds);
    RUN_TEST(test_int_map_remove_and_clear_update_count);
    RUN_TEST(test_string_map_contains_equivalent_keys_and_updates_values);
    RUN_TEST(test_string_map_integration_bulk_insert_update_and_remove);
    RUN_TEST(test_int_map_zone_alloc_routes_through_zone);
    return UNITY_END();
}
