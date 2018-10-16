#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../binary_heap.h"

static const int32_t count = 100000;

static float getTime()
{
    return (float)clock() / CLOCKS_PER_SEC;
}

int32_t compareInt(const int a, const int b)
{
    return a - b;
}

shlDeclareBinaryHeap(IntHeap, int)
shlDefineBinaryHeap(IntHeap, int)

void valueTypeTest()
{
    IntHeap heap;

    IntHeapOptions options = {0};
    options.compareFn = compareInt;
    options.defaultValue = 0;
    
    IntHeapInit(&heap, options);

    int min = count;
    for(int i = 0; i < count; i++)
    {
        int x = rand() % count;
        if (x < min) min = x;
        IntHeapPush(&heap, x);
    }
    
    int peek = IntHeapPeek(&heap);
    assert(peek == min);

    int prev = -1;
    while (heap.count > 0)
    {
        int x = IntHeapPop(&heap);
        if (prev >= 0)
            assert(prev <= x);

        prev = x;
    }

    IntHeapFree(&heap);
}

int main(int argc, char **argv)
{
    /* initialize random seed: */
    srand(time(NULL));

    valueTypeTest();

    printf("\n\n");

    // referenceTypeTest();

    return 0;
}
