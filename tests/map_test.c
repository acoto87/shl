#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>

#include "../map.h"

static const int32_t count = 100000;

static float getTime()
{
    return (float)clock() / CLOCKS_PER_SEC;
}

// value type tests
int hashInt(const int x)
{
    return x;
}

bool equalsInt(const int a, const int b)
{
    return a == b;
}

shlDeclareMap(IntMap, int, int)
shlDefineMap(IntMap, int, int)

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
    options.freeFn = free;

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
}

int main(int argc, char **argv)
{
    /* initialize random seed: */
    srand(time(NULL));

    valueTypeTest();

    printf("\n\n");

    referenceTypeTest();

    return 0;
}