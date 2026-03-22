#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <assert.h>

#define SHL_MEMORY_BUFFER_IMPLEMENTATION
#include "../memory_buffer.h"
#include "test_common.h"

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

    char header[12] = {0};
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

    char header[10] = {0};
    assert(mbReadString(&buffer, header, 9));
    assert(strcmp(header, EndBlockStr) == 0);
    assert(mbIsEOF(&buffer));
}

void testSeek(uint8_t* data, size_t length)
{
    MemoryBuffer buffer = {0};
    mbInitFromMemory(&buffer, data, length);

    assert(mbSeek(&buffer, 91));
    char header[10] = {0};
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

void test24BitIO()
{
    MemoryBuffer buffer = {0};
    mbInitEmpty(&buffer);

    assert(mbWriteInt24LE(&buffer, 0x00112233));
    assert(mbWriteInt24BE(&buffer, 0x00445566));
    assert(mbWriteUInt24LE(&buffer, 0x00778899));
    assert(mbWriteUInt24BE(&buffer, 0x00AABBCC));
    assert(buffer.length == 12);

    assert(mbSeek(&buffer, 0));

    int32_t signedValue = 0;
    uint32_t unsignedValue = 0;

    assert(mbReadInt24LE(&buffer, &signedValue));
    assert(signedValue == 0x00112233);
    assert(mbReadInt24BE(&buffer, &signedValue));
    assert(signedValue == 0x00445566);
    assert(mbReadUInt24LE(&buffer, &unsignedValue));
    assert(unsignedValue == 0x00778899);
    assert(mbReadUInt24BE(&buffer, &unsignedValue));
    assert(unsignedValue == 0x00AABBCC);
    assert(mbIsEOF(&buffer));

    mbFree(&buffer);
}

void testSkipBoundaries(uint8_t* data, size_t length)
{
    MemoryBuffer buffer = {0};
    mbInitFromMemory(&buffer, data, length);

    assert(!mbSkip(&buffer, -1));
    assert(mbSkip(&buffer, 11));
    assert(mbPosition(&buffer) == 11);
    assert(!mbSkip(&buffer, -12));
}

void memoryBufferWriteTest(void)
{
    size_t length = 0;
    uint8_t* data = testWriting(&length);
    free(data);
}

void memoryBufferReadTest(void)
{
    size_t length = 0;
    uint8_t* data = testWriting(&length);
    testReading(data, length);
    free(data);
}

void memoryBufferScanToTest(void)
{
    size_t length = 0;
    uint8_t* data = testWriting(&length);
    testScanTo(data, length);
    free(data);
}

void memoryBufferSeekTest(void)
{
    size_t length = 0;
    uint8_t* data = testWriting(&length);
    testSeek(data, length);
    free(data);
}

void memoryBufferSkipBoundaryTest(void)
{
    size_t length = 0;
    uint8_t* data = testWriting(&length);
    testSkipBoundaries(data, length);
    free(data);
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
    RUN_TEST(memoryBufferWriteTest);
    RUN_TEST(memoryBufferReadTest);
    RUN_TEST(memoryBufferScanToTest);
    RUN_TEST(memoryBufferSeekTest);
    RUN_TEST(test24BitIO);
    RUN_TEST(memoryBufferSkipBoundaryTest);
    return UNITY_END();
}
