#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* memzone.h must be included before set.h so that the #ifdef SHL_MZ_H
   bridge in alloc.h is compiled in. */
#define SHL_MZ_IMPLEMENTATION
#include "../memzone.h"

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
    IntSetInit(&set, shl_heap_alloc(), hashInt, equalsInt);

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

void test_int_set_init_with_missing_callbacks_resets_to_safe_empty_state(void)
{
    IntSet set;
    memset(&set, 0xA5, sizeof(set));

    IntSetInit(&set, shl_heap_alloc(), NULL, equalsInt);

    TEST_ASSERT_EQUAL_INT(0, set.count);
    TEST_ASSERT_EQUAL_INT(0, set.capacity);
    TEST_ASSERT_EQUAL_INT(0, set.loadFactor);
    TEST_ASSERT_EQUAL_INT(0, set.shift);
    TEST_ASSERT_NULL(set.alloc);
    TEST_ASSERT_NULL(set.hashFn);
    TEST_ASSERT_NULL(set.equalsFn);
    TEST_ASSERT_NULL(set.entries);

    TEST_ASSERT_FALSE(IntSetAdd(&set, 1));
    TEST_ASSERT_FALSE(IntSetContains(&set, 1));

    IntSetFree(&set);
    TEST_ASSERT_EQUAL_INT(0, set.count);
    TEST_ASSERT_NULL(set.entries);
}

void test_collision_set_remove_preserves_other_entries(void)
{
    CollisionSet set;
    CollisionSetInit(&set, shl_heap_alloc(), collideInt, equalsInt);

    for (int i = 0; i < 64; i++)
        TEST_ASSERT_TRUE(CollisionSetAdd(&set, i));

    CollisionSetRemove(&set, 0);
    TEST_ASSERT_FALSE(CollisionSetContains(&set, 0));
    for (int i = 1; i < 64; i++)
        TEST_ASSERT_TRUE(CollisionSetContains(&set, i));
    TEST_ASSERT_EQUAL_INT(63, set.count);

    CollisionSetFree(&set);
}

void test_int_set_stress_add_and_remove_halves_count(void)
{
    IntSet set;
    IntSetInit(&set, shl_heap_alloc(), hashInt, equalsInt);

    for (int i = 0; i < SHL_TEST_STRESS_COUNT; i++)
        TEST_ASSERT_TRUE(IntSetAdd(&set, i));

    for (int i = 0; i < SHL_TEST_STRESS_COUNT; i += 2)
    {
        IntSetRemove(&set, i);
        TEST_ASSERT_FALSE(IntSetContains(&set, i));
    }

    TEST_ASSERT_EQUAL_INT(SHL_TEST_STRESS_COUNT / 2, set.count);
    for (int i = 1; i < SHL_TEST_STRESS_COUNT; i += 2)
        TEST_ASSERT_TRUE(IntSetContains(&set, i));

    IntSetFree(&set);
}

/* Remove decrements count; Clear resets it to zero.  The caller (not the
   set) is responsible for freeing any resources owned by items. */
void test_int_set_remove_and_clear_update_count(void)
{
    /* Use collideInt so all keys land in the same bucket, exercising
       the collision chain through Add, Remove, and Clear. */
    CollisionSet set;
    CollisionSetInit(&set, shl_heap_alloc(), collideInt, equalsInt);

    TEST_ASSERT_TRUE(CollisionSetAdd(&set, 1));
    TEST_ASSERT_TRUE(CollisionSetAdd(&set, 10));
    TEST_ASSERT_TRUE(CollisionSetAdd(&set, 100));
    TEST_ASSERT_EQUAL_INT(3, set.count);

    CollisionSetRemove(&set, 10);
    TEST_ASSERT_EQUAL_INT(2, set.count);
    TEST_ASSERT_FALSE(CollisionSetContains(&set, 10));
    TEST_ASSERT_TRUE(CollisionSetContains(&set, 1));
    TEST_ASSERT_TRUE(CollisionSetContains(&set, 100));

    CollisionSetClear(&set);
    TEST_ASSERT_EQUAL_INT(0, set.count);
    TEST_ASSERT_FALSE(CollisionSetContains(&set, 1));
    TEST_ASSERT_FALSE(CollisionSetContains(&set, 100));

    CollisionSetFree(&set);
}

/* Caller frees items before Remove; Remove does not call any destructor. */
void test_string_set_remove_and_caller_frees_items(void)
{
    StringSet set;
    StringSetInit(&set, shl_heap_alloc(), fnv32, equalsStr);

    char* alpha = makeStringFromIndex(1);
    char* beta  = makeStringFromIndex(2);
    char* gamma = makeStringFromIndex(3);
    char probe[32];

    TEST_ASSERT_TRUE(StringSetAdd(&set, alpha));
    TEST_ASSERT_TRUE(StringSetAdd(&set, beta));
    TEST_ASSERT_TRUE(StringSetAdd(&set, gamma));

    strcpy(probe, beta);
    TEST_ASSERT_TRUE(StringSetContains(&set, probe));

    /* Remove the entry first; the set stores a copy of the pointer so the
       comparison needs the pointed-to memory to still be valid.  The caller
       frees the owned string after the entry is gone from the set. */
    StringSetRemove(&set, probe);
    freeStr(beta);
    TEST_ASSERT_FALSE(StringSetContains(&set, probe));
    TEST_ASSERT_EQUAL_INT(2, set.count);

    /* Free remaining items before releasing the set. */
    freeStr(alpha);
    freeStr(gamma);
    StringSetFree(&set);
}

void test_string_set_integration_bulk_unique_insert_then_duplicate_probe(void)
{
    StringSet set;
    StringSetInit(&set, shl_heap_alloc(), fnv32, equalsStr);

    for (int i = 0; i < SHL_TEST_MEDIUM_COUNT; i++)
        TEST_ASSERT_TRUE(StringSetAdd(&set, makeStringFromIndex(i)));

    for (int i = 0; i < SHL_TEST_MEDIUM_COUNT; i += 5)
    {
        char* duplicate = makeStringFromIndex(i);
        TEST_ASSERT_FALSE(StringSetAdd(&set, duplicate));
        freeStr(duplicate);
    }

    TEST_ASSERT_EQUAL_INT(SHL_TEST_MEDIUM_COUNT, set.count);
    TEST_ASSERT_TRUE(StringSetContains(&set, "value-0"));
    TEST_ASSERT_TRUE(StringSetContains(&set, "value-255"));

    /* Free all strings still in the set before releasing it. */
    for (int i = 0; i < set.capacity; i++)
    {
        if (set.entries[i].active)
            freeStr(set.entries[i].item);
    }

    StringSetFree(&set);
}

/* Zone allocator: allocations are routed through a memzone_t and the
   entries buffer lives inside the zone. */
void test_int_set_zone_alloc_routes_through_zone(void)
{
    memzone_t* zone = mz_init(1 << 20);
    TEST_ASSERT_NOT_NULL(zone);

    shl_allocator_t alloc = shl_zone_alloc(zone);

    IntSet set;
    IntSetInit(&set, &alloc, hashInt, equalsInt);

    for (int i = 0; i < 20; i++)
        TEST_ASSERT_TRUE(IntSetAdd(&set, i));

    TEST_ASSERT_EQUAL_INT(20, set.count);
    TEST_ASSERT_TRUE(IntSetContains(&set, 0));
    TEST_ASSERT_TRUE(IntSetContains(&set, 19));

    /* The entries buffer must live inside the zone. */
    TEST_ASSERT_TRUE(mz_contains(zone, set.entries));

    IntSetFree(&set);
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
    RUN_TEST(test_int_set_add_contains_and_rejects_duplicates);
    RUN_TEST(test_int_set_init_with_missing_callbacks_resets_to_safe_empty_state);
    RUN_TEST(test_collision_set_remove_preserves_other_entries);
    RUN_TEST(test_int_set_stress_add_and_remove_halves_count);
    RUN_TEST(test_int_set_remove_and_clear_update_count);
    RUN_TEST(test_string_set_remove_and_caller_frees_items);
    RUN_TEST(test_string_set_integration_bulk_unique_insert_then_duplicate_probe);
    RUN_TEST(test_int_set_zone_alloc_routes_through_zone);
    return UNITY_END();
}
