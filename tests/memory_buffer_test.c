#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <assert.h>

#define SHL_MEMORY_BUFFER_IMPLEMENTATION
#include "../memory_buffer.h"

const char* BufferTestStr = "Buffer test";
const char* EndBlockStr = "End block";

static float getTime()
{
    return (float)clock() / CLOCKS_PER_SEC;
}

uint8_t* testWriting(size_t* length)
{
    MemoryBuffer buffer = {0};
    mbInitEmpty(&buffer);

    assert(mbWriteString(&buffer, BufferTestStr, 11));
    assert(buffer.length == 11);
    
    for (int32_t i = 0; i < 10; i++)
    {
        assert(mbWriteInt32LE(&buffer, i));
        assert(buffer.length == 11 + (i + 1) * sizeof(int32_t));
    }

    assert(mbPosition(&buffer) == 11 + 10 * sizeof(int32_t));
    
    for (int32_t i = 0; i < 10; i++)
    {
        assert(mbWriteInt32BE(&buffer, i));
        assert(buffer.length == 11 + (10 + i + 1) * sizeof(int32_t));
    }

    assert(mbPosition(&buffer) == 11 + 20 * sizeof(int32_t));

    assert(mbWriteString(&buffer, EndBlockStr, 9));
    assert(buffer.length == 100);

    uint8_t* data = mbGetData(&buffer, length);
    mbFree(&buffer);
    assert(!buffer.data);
    return data;
}

void testReading(uint8_t* data, size_t length)
{
    MemoryBuffer buffer = {0};
    mbInitFromMemory(&buffer, data, length);

    assert(buffer.length == length);

    char header[12];
    assert(mbReadString(&buffer, header, 11));
    assert(strcmp(header, BufferTestStr) == 0);
    assert(mbPosition(&buffer) == 11);

    for (int32_t i = 0; i < 10; i++)
    {
        int32_t v;
        assert(mbReadInt32LE(&buffer, &v));
        assert(v == i);
    }

    assert(mbPosition(&buffer) == 11 + 10 * sizeof(int32_t));

    for (int32_t i = 0; i < 10; i++)
    {
        int32_t v;
        assert(mbReadInt32BE(&buffer, &v));
        assert(v == i);
    }

    assert(mbPosition(&buffer) == 11 + 20 * sizeof(int32_t));
}

void testScanTo(uint8_t* data, size_t length)
{
    MemoryBuffer buffer = {0};
    mbInitFromMemory(&buffer, data, length);

    assert(mbScanTo(&buffer, "End block", 9));

    char header[10];
    assert(mbReadString(&buffer, header, 9));
    assert(strcmp(header, EndBlockStr) == 0);
    assert(mbIsEOF(&buffer));
}

void testSeek(uint8_t* data, size_t length)
{
    MemoryBuffer buffer = {0};
    mbInitFromMemory(&buffer, data, length);

    assert(mbSeek(&buffer, 91));
    char header[10];
    assert(mbReadString(&buffer, header, 9));
    assert(strcmp(header, EndBlockStr) == 0);
    assert(mbIsEOF(&buffer));

    assert(mbSeek(&buffer, 7));
    assert(mbWriteInt32LE(&buffer, 22));
    assert(mbSeek(&buffer, 7));
    int32_t v;
    assert(mbReadInt32BE(&buffer, &v));
    assert(v == 369098752);
}

int main(int argc, char **argv)
{
    float start, end;

    printf("--- Start writing test ---\n");
    start = getTime();
    size_t length;
    uint8_t* data = testWriting(&length);
    end = getTime();
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End writing test ---\n");

    printf("\n");

    printf("--- Start reading test ---\n");
    start = getTime();
    testReading(data, length);
    end = getTime();
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End reading test ---\n");

    printf("\n");

    printf("--- Start scanTo test ---\n");
    start = getTime();
    testScanTo(data, length);
    end = getTime();
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End scanTo test ---\n");

    printf("\n");

    printf("--- Start seek test ---\n");
    start = getTime();
    testSeek(data, length);
    end = getTime();
    printf("Time: %.2f seconds\n", end - start);
    printf("--- End seek test ---\n");

    return 0;
}
