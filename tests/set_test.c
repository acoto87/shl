
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#include "../set.h"
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

shlDeclareSet(IntSet, int)
shlDefineSet(IntSet, int)

static uint32_t collideInt(const int x)
{
    (void)x;
    return 1;
}

static int setFreeCount = 0;

void freeTrackedInt(int value)
{
    setFreeCount += value;
}

shlDeclareSet(CollisionSet, int)
shlDefineSet(CollisionSet, int)

void collisionAndEdgeCaseValueTest()
{
    CollisionSetOptions options = (CollisionSetOptions){0};
    options.hashFn = collideInt;
    options.equalsFn = equalsInt;
    options.defaultValue = -1;

    CollisionSet set;
    CollisionSetInit(&set, options);

    for (int i = 0; i < 64; i++)
        assert(CollisionSetAdd(&set, i));

    CollisionSetRemove(&set, 0);
    assert(!CollisionSetContains(&set, 0));
    for (int i = 1; i < 64; i++)
        assert(CollisionSetContains(&set, i));

    CollisionSetRemove(&set, 9999);
    assert(set.count == 63);

    CollisionSetFree(&set);
}

void valueTypeTest()
{
    float start, end;

    IntSetOptions options = (IntSetOptions){0};
    options.hashFn = hashInt;
    options.equalsFn = equalsInt;
    options.defaultValue = 0;

    IntSet set;
    IntSetInit(&set, options);

    printf("--- Start test 1: add %d objects ---\n", count);
    start = getTime();
    for(int i = 0; i < count; i++)
    {
        int x = i;
        assert(IntSetAdd(&set, x));
        assert(set.count == i + 1);
    }
    end = getTime();
    printf("Set count and capacity: (%d, %d)\n", set.count, set.capacity);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 1: add %d objects ---\n", count);

    printf("\n");

    printf("--- Start test 2: contains %d objects ---\n", count/2);
    start = getTime();
    for(int i = 0; i < count/2; i++)
    {
        int x = rand() % count;
        assert(IntSetContains(&set, x));
    }
    end = getTime();
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 2: contains %d objects ---\n", count/2);

    printf("\n");

    printf("--- Start test 3: add duplicated %d objects ---\n", count/2);
    start = getTime();
    int32_t prevCount = set.count;
    for(int i = 0; i < count/2; i++)
    {
        int x = rand() % set.count;
        assert(!IntSetAdd(&set, x));
        assert(set.count == prevCount);
    }
    end = getTime();
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 3: add duplicated %d objects ---\n", count/2);

    printf("\n");

    printf("--- Start test 4: remove %d objects ---\n", count/2);
    start = getTime();
    for(int i = 0; i < count/2; i++)
    {
        int x = rand() % set.count;
        IntSetRemove(&set, x);
        assert(!IntSetContains(&set, x));
    }
    end = getTime();
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 4: remove %d objects ---\n", count/2);

    IntSetFree(&set);
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

shlDeclareSet(SSet, char*)
shlDefineSet(SSet, char*)

char* generateString()
{
    const int stringLength = 50;
    char *s = (char*)calloc(stringLength + 1, sizeof(char));
    
    for(int i = 0; i < 50; i++)
        s[i] = rand() % 27 + 97;
    
    return s;
}

void freeStr(char* str)
{
    free((void*)str);
}

shlDeclareSet(TrackedSet, int)
shlDefineSet(TrackedSet, int)

void clearEdgeCaseTest()
{
    TrackedSetOptions options = (TrackedSetOptions){0};
    options.hashFn = collideInt;
    options.equalsFn = equalsInt;
    options.defaultValue = 0;
    options.freeFn = freeTrackedInt;

    TrackedSet set;
    TrackedSetInit(&set, options);

    assert(TrackedSetAdd(&set, 1));
    assert(TrackedSetAdd(&set, 10));
    assert(TrackedSetAdd(&set, 100));
    TrackedSetRemove(&set, 10);

    setFreeCount = 0;
    TrackedSetClear(&set);
    assert(set.count == 0);
    assert(setFreeCount == 101);
    assert(!TrackedSetContains(&set, 1));
    assert(!TrackedSetContains(&set, 100));

    TrackedSetFree(&set);
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

    SSetOptions options = (SSetOptions){0};
    options.hashFn = fnv32;
    options.equalsFn = equalsStr;
    options.defaultValue = NULL;
    options.freeFn = freeStr;

    SSet set;
    SSetInit(&set, options);

    printf("--- Start test 1: add %d strings ---\n", count);
    start = getTime();
    for(int i = 0; i < count; i++)
    {
        char *str = strings[i];
        assert(SSetAdd(&set, str));
        assert(set.count == i + 1);
    }
    end = getTime();
    printf("Set count and capacity: (%d, %d)\n", set.count, set.capacity);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 1: add %d strings ---\n", count);

    printf("\n");

    printf("--- Start test 2: contains %d objects ---\n", count/2);
    start = getTime();
    for(int i = 0; i < count/2; i++)
    {
        int index = rand() % count;
        assert(SSetContains(&set, strings[index]));
    }
    end = getTime();
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 2: contains %d objects ---\n", count/2);

    printf("\n");

    printf("--- Start test 3: add duplicated %d objects ---\n", count/2);
    start = getTime();
    int32_t prevCount = set.count;
    for(int i = 0; i < count/2; i++)
    {
        int index = rand() % set.count;
        assert(!SSetAdd(&set, strings[index]));
        assert(set.count == prevCount);
    }
    end = getTime();
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 3: add duplicated %d objects ---\n", count/2);

    printf("\n");

    printf("--- Start test 4: remove %d objects ---\n", count/2);
    start = getTime();
    for(int i = 0; i < count/2; i++)
    {
        char* key = strings[i];
        char* probe = (char*)calloc(strlen(key) + 1, sizeof(char));
        strcpy(probe, key);
        SSetRemove(&set, key);
        assert(!SSetContains(&set, probe));
        strings[i] = NULL;
        free(probe);
    }
    end = getTime();
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 4: remove %d objects ---\n", count/2);

    SSetFree(&set);
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
