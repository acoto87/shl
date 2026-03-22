#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#include "../binary_heap.h"
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

int32_t compareInt(const int a, const int b)
{
    return a - b;
}

bool equalsInt(const int a, const int b)
{
    return a == b;
}

shlDeclareBinaryHeap(IntHeap, int)
shlDefineBinaryHeap(IntHeap, int)

void valueTypeTest()
{
    float start, end;
    int min = count;

    IntHeapOptions options = {0};
    options.compareFn = compareInt;
    options.defaultValue = 0;
    options.equalsFn = equalsInt;
    
    IntHeap heap;
    IntHeapInit(&heap, options);

    printf("--- Start test 1: add %d objects ---\n", count);
    start = getTime();
    for(int i = 0; i < count; i++)
    {
        int x = rand() % count;
        if (x < min) min = x;
        IntHeapPush(&heap, x);
        assert(heap.count == (uint32_t)(i + 1));
    }
    end = getTime();
    printf("BinaryHeap count and capacity: (%d, %d)\n", heap.count, heap.capacity);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 1: add %d objects ---\n", count);

    printf("\n");

    printf("--- Start test 2: peek min object ---\n");
    start = getTime();
    int peek = IntHeapPeek(&heap);
    assert(peek == min);
    end = getTime();
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 2:  peek min object ---\n");

    printf("\n");

    printf("--- Start test 3: pop min object ---\n");
    start = getTime();
    int prev = -1;
    while (heap.count > 0)
    {
        int x = IntHeapPop(&heap);
        if (prev >= 0)
            assert(prev <= x);

        prev = x;
    }
    end = getTime();
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 3:  pop min object ---\n");

    IntHeapFree(&heap);
}

void edgeCaseValueTypeTest()
{
    IntHeapOptions options = {0};
    options.compareFn = compareInt;
    options.equalsFn = equalsInt;
    options.defaultValue = -1;

    IntHeap heap;
    IntHeapInit(&heap, options);

    assert(IntHeapPeek(&heap) == -1);
    assert(IntHeapPop(&heap) == -1);

    IntHeapPush(&heap, 10);
    IntHeapPush(&heap, 20);
    IntHeapPush(&heap, 30);
    IntHeapPush(&heap, 40);

    int index = IntHeapIndexOf(&heap, 30);
    assert(index >= 0);
    IntHeapUpdate(&heap, index, 5);
    assert(IntHeapPeek(&heap) == 5);

    index = IntHeapIndexOf(&heap, 10);
    assert(index >= 0);
    IntHeapUpdate(&heap, index, 50);

    int previous = IntHeapPop(&heap);
    while (heap.count > 0)
    {
        int current = IntHeapPop(&heap);
        assert(previous <= current);
        previous = current;
    }

    IntHeapFree(&heap);
}

int32_t compareStrLength(const char* a, const char* b)
{
    return strlen(a) - strlen(b);
}

shlDeclareBinaryHeap(SHeap, const char*)
shlDefineBinaryHeap(SHeap, const char*)

char* generateString()
{
    const int stringLength = 50;
    char *s = (char*)calloc(stringLength + 1, sizeof(char));
    
    for(int i = 0; i < 50; i++)
        s[i] = rand() % 27 + 97;
    
    return s;
}

void freeStr(const char* str)
{
    free((void*)str);
}

void referenceTypeTest()
{
    float start, end;
    int min = INT32_MAX;

    printf("--- Start generating %d tests strings ---\n", count);
    start = getTime();
    char *strings[100000];
    for(int i = 0; i < count; i++)
        strings[i] = generateString();
    end = getTime();
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End generating %d tests strings ---\n", count);

    printf("\n\n");

    SHeapOptions options = {0};
    options.compareFn = compareStrLength;
    options.defaultValue = NULL;
    options.freeFn = NULL;
    
    SHeap heap;
    SHeapInit(&heap, options);

    printf("--- Start test 1: add %d objects ---\n", count);
    start = getTime();
    for(int i = 0; i < count; i++)
    {
        int len = strlen(strings[i]);
        if (len < min) min = len;
        SHeapPush(&heap, strings[i]);
        assert(heap.count == (uint32_t)(i + 1));
    }
    end = getTime();
    printf("BinaryHeap count and capacity: (%d, %d)\n", heap.count, heap.capacity);
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 1: add %d objects ---\n", count);

    printf("\n");

    printf("--- Start test 2: peek min object ---\n");
    start = getTime();
    const char* peek = SHeapPeek(&heap);
        assert(strlen(peek) == (size_t)min);
    end = getTime();
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 2:  peek min object ---\n");

    printf("\n");

    printf("--- Start test 3: pop min object ---\n");
    start = getTime();
    int prev = -1;
    while (heap.count > 0)
    {
        const char* str = SHeapPop(&heap);
        int len = strlen(str);
        if (prev >= 0)
            assert(prev <= len);

        prev = len;
        free((void*)str);
    }
    end = getTime();
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End test 3:  pop min object ---\n");

    SHeapFree(&heap);
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
    RUN_TEST(edgeCaseValueTypeTest);
    RUN_TEST(referenceTypeTest);
    return UNITY_END();
}
