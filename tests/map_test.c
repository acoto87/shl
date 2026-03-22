#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#include "../map.h"
#include "test_common.h"

#if defined(SHL_LEAK_CHECK)
static const int32_t count = 5000;
#else
static const int32_t count = 100000;
#endif

static float getTime()
{
    return (float)clock() / CLOCKS_PER_SEC;
}

// value type tests
uint32_t hashInt(const int x)
{
    return x;
}

bool equalsInt(const int a, const int b)
{
    return a == b;
}

shlDeclareMap(IntMap, int, int)
shlDefineMap(IntMap, int, int)

static uint32_t collideInt(const int x)
{
    (void)x;
    return 1;
}

static int mapFreeCount = 0;

void freeTrackedInt(int value)
{
    mapFreeCount += value;
}

shlDeclareMap(CollisionMap, int, int)
shlDefineMap(CollisionMap, int, int)

void collisionAndEdgeCaseValueTest()
{
    CollisionMapOptions options = (CollisionMapOptions){0};
    options.hashFn = collideInt;
    options.equalsFn = equalsInt;
    options.defaultValue = -1;

    CollisionMap map;
    CollisionMapInit(&map, options);

    for (int i = 0; i < 64; i++)
        CollisionMapSet(&map, i, i * 10);

    assert(map.count == 64);
    for (int i = 0; i < 64; i++)
        assert(CollisionMapGet(&map, i) == i * 10);

    CollisionMapRemove(&map, 0);
    assert(!CollisionMapContains(&map, 0));
    for (int i = 1; i < 64; i++)
        assert(CollisionMapGet(&map, i) == i * 10);

    CollisionMapRemove(&map, 9999);
    assert(map.count == 63);

    CollisionMapFree(&map);
}

void valueTypeTest()
{
    float start, end;

    IntMapOptions options = (IntMapOptions){0};
    options.hashFn = hashInt;
    options.equalsFn = equalsInt;
    options.defaultValue = 0;

    IntMap map;
    IntMapInit(&map, options);

    printf("--- Start test 1: add %d objects ---\n", count);
    start = getTime();
    for(int i = 0; i < count; i++)
    {
        int key = i;
        IntMapSet(&map, key, key*key);
        int value = IntMapGet(&map, key);
        assert(value == key*key);
    }
    end = getTime();
    printf("Map count and capacity: (%d, %d)\n", map.count, map.capacity);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 1: add %d objects ---\n", count);

    printf("\n");

    printf("--- Start test 2: contains %d objects ---\n", count/2);
    start = getTime();
    for(int i = 0; i < count/2; i++)
    {
        int key = rand() % count;
        assert(IntMapContains(&map, key));
    }
    end = getTime();
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 2: contains %d objects ---\n", count/2);

    printf("\n");

    printf("--- Start test 3: set existing %d objects ---\n", count/2);
    start = getTime();
    for(int i = 0; i < count/2; i++)
    {
        int key = rand() % count;
        IntMapSet(&map, key, key);
        int value = IntMapGet(&map, key);
        assert(key == value);
    }
    end = getTime();
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 3: set existing %d objects ---\n", count/2);

    printf("\n");

    printf("--- Start test 4: remove %d objects ---\n", count/2);
    start = getTime();
    for(int i = 0; i < count/2; i++)
    {
        int key = rand() % map.count;
        IntMapRemove(&map, key);
        assert(!IntMapContains(&map, key));
    }
    end = getTime();
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 4: remove %d objects ---\n", count/2);

    IntMapFree(&map);
}

// reference type tests
#define FNV_PRIME_32 0x01000193
#define FNV_OFFSET_32 0x811c9dc5

uint32_t fnv32(const char* data) 
{
    uint32_t hash = FNV_OFFSET_32;
    while(*data != 0)
        hash = (*data++ ^ hash) * FNV_PRIME_32;

    return hash;
}

bool equalsStr(const char *s1, const char *s2)
{
    return strcmp(s1, s2) == 0;
}

shlDeclareMap(SSMap, char*, char*)
shlDefineMap(SSMap, char*, char*)

char* generateString()
{
    const int stringLength = 50;
    char *s = (char*)calloc(stringLength + 1, sizeof(char));
    
    for(int i = 0; i < 50; i++)
        s[i] = rand() % 27 + 97;
    
    return s;
}

char* toUpperString(const char* str)
{
    int len = strlen(str);
    char *upper = (char*)calloc(len + 1, sizeof(char));
    
    for(int i = 0; i < len; i++)
        upper[i] = toupper(str[i]);
    
    return upper;
}

char* reverseString(const char* str)
{
    int len = strlen(str);
    char *reverse = (char*)calloc(len + 1, sizeof(char));
    
    for(int i = 0; i < len; i++)
        reverse[len - i - 1] = str[i];
    
    return reverse;
}

void freeStr(char* str)
{
    free((void*)str);
}

shlDeclareMap(TrackedMap, int, int)
shlDefineMap(TrackedMap, int, int)

void clearEdgeCaseTest()
{
    TrackedMapOptions options = (TrackedMapOptions){0};
    options.hashFn = collideInt;
    options.equalsFn = equalsInt;
    options.defaultValue = 0;
    options.freeFn = freeTrackedInt;

    TrackedMap map;
    TrackedMapInit(&map, options);

    TrackedMapSet(&map, 1, 1);
    TrackedMapSet(&map, 2, 10);
    TrackedMapSet(&map, 3, 100);
    TrackedMapRemove(&map, 2);

    mapFreeCount = 0;
    TrackedMapClear(&map);
    assert(map.count == 0);
    assert(mapFreeCount == 101);
    assert(!TrackedMapContains(&map, 1));
    assert(!TrackedMapContains(&map, 3));

    TrackedMapFree(&map);
}

void referenceTypeTest()
{
    float start, end;

    printf("--- Start generating %d tests strings ---\n", count);
    start = getTime();
    char *strings[100000];
    for(int i = 0; i < count; i++)
        strings[i] = generateString();
    end = getTime();
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End generating %d tests strings ---\n", count);

    printf("\n\n");

    SSMapOptions options = (SSMapOptions){0};
    options.hashFn = fnv32;
    options.equalsFn = equalsStr;
    options.defaultValue = NULL;
    options.freeFn = freeStr;

    SSMap map;
    SSMapInit(&map, options);

    printf("--- Start test 1: add %d strings ---\n", count);
    start = getTime();
    for(int i = 0; i < count; i++)
    {
        char *key = strings[i];
        char *upper = toUpperString(key);
        SSMapSet(&map, key, upper);
        char *value = SSMapGet(&map, key);
        assert(!strcmp(value, upper));
    }
    end = getTime();
    printf("Map count and capacity: (%d, %d)\n", map.count, map.capacity);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 1: add %d strings ---\n", count);

    printf("\n");

    printf("--- Start test 2: contains %d objects ---\n", count/2);
    start = getTime();
    for(int i = 0; i < count/2; i++)
    {
        int index = rand() % count;
        assert(SSMapContains(&map, strings[index]));
    }
    end = getTime();
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 2: contains %d objects ---\n", count/2);

    printf("\n");

    printf("--- Start test 3: set existing %d objects ---\n", count/2);
    start = getTime();
    for(int i = 0; i < count/2; i++)
    {
        int index = rand() % count;
        char* reverse = reverseString(strings[index]);
        SSMapSet(&map, strings[index], reverse);
        char *value = SSMapGet(&map, strings[index]);
        assert(equalsStr(reverse, value));
    }
    end = getTime();
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 3: set existing %d objects ---\n", count/2);

    printf("\n");

    printf("--- Start test 4: remove %d objects ---\n", count/2);
    start = getTime();
    for(int i = 0; i < count/2; i++)
    {
        int index = rand() % map.count;
        SSMapRemove(&map, strings[index]);
        assert(!SSMapContains(&map, strings[index]));
    }
    end = getTime();
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 4: remove %d objects ---\n", count/2);

    SSMapFree(&map);
    for(int i = 0; i < count; i++)
        free(strings[i]);
}

void setUp(void)
{
}

void tearDown(void)
{
}

int main(void)
{
    /* initialize random seed: */
    srand(time(NULL));

    UNITY_BEGIN();
    RUN_TEST(valueTypeTest);
    RUN_TEST(collisionAndEdgeCaseValueTest);
    RUN_TEST(referenceTypeTest);
    RUN_TEST(clearEdgeCaseTest);
    return UNITY_END();
}
