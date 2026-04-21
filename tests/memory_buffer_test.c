#include <stdlib.h>

#define SHL_MEMORY_BUFFER_IMPLEMENTATION
#include "../memory_buffer.h"
#include "test_common.h"

static const char* BUFFER_TEST_STR = "Buffer test"; /* 11 bytes */
static const char* END_BLOCK_STR   = "End block";   /*  9 bytes */

/*
 * Builds the canonical 100-byte test buffer:
 *   [0..10]  "Buffer test"          (11 bytes, string)
 *   [11..50] 0..9 as int32 LE       (10 * 4 = 40 bytes)
 *   [51..90] 0..9 as int32 BE       (10 * 4 = 40 bytes)
 *   [91..99] "End block"            ( 9 bytes, string)
 *
 * Returns a malloc'd copy; caller must free().
 */
static uint8_t* make_test_buffer(size_t* out_length)
{
    MemoryBuffer buf = {0};
    mbInitEmpty(&buf);

    mbWriteString(&buf, BUFFER_TEST_STR, 11);
    for (int32_t i = 0; i < 10; i++)
        mbWriteInt32LE(&buf, i);
    for (int32_t i = 0; i < 10; i++)
        mbWriteInt32BE(&buf, i);
    mbWriteString(&buf, END_BLOCK_STR, 9);

    uint8_t* data = mbGetData(&buf, out_length);
    mbFree(&buf);
    return data;
}

/* ------------------------------------------------------------------ */
/* Write tests                                                          */
/* ------------------------------------------------------------------ */

void mbWrite100ByteBufferTest(void)
{
    MemoryBuffer buf = {0};
    mbInitEmpty(&buf);

    TEST_ASSERT_TRUE(mbWriteString(&buf, BUFFER_TEST_STR, 11));
    TEST_ASSERT_EQUAL_size_t(11, buf.length);

    for (int32_t i = 0; i < 10; i++)
    {
        TEST_ASSERT_TRUE(mbWriteInt32LE(&buf, i));
        TEST_ASSERT_EQUAL_size_t((size_t)(11 + (i + 1) * 4), buf.length);
    }

    TEST_ASSERT_EQUAL_INT(11 + 10 * 4, mbPosition(&buf));

    for (int32_t i = 0; i < 10; i++)
    {
        TEST_ASSERT_TRUE(mbWriteInt32BE(&buf, i));
        TEST_ASSERT_EQUAL_size_t((size_t)(11 + (10 + i + 1) * 4), buf.length);
    }

    TEST_ASSERT_EQUAL_INT(11 + 20 * 4, mbPosition(&buf));

    TEST_ASSERT_TRUE(mbWriteString(&buf, END_BLOCK_STR, 9));
    TEST_ASSERT_EQUAL_size_t(100, buf.length);

    size_t length = 0;
    uint8_t* data = mbGetData(&buf, &length);
    TEST_ASSERT_EQUAL_size_t(100, length);
    TEST_ASSERT_NOT_NULL(data);
    free(data);

    mbFree(&buf);
    TEST_ASSERT_NULL(buf.data);
}

/* ------------------------------------------------------------------ */
/* Read tests (each builds the 100-byte buffer fresh)                  */
/* ------------------------------------------------------------------ */

void mbReadStringTest(void)
{
    size_t length = 0;
    uint8_t* data = make_test_buffer(&length);

    MemoryBuffer buf = {0};
    mbInitFromMemory(&buf, data, length);
    TEST_ASSERT_EQUAL_size_t(100, buf.length);

    char header[12] = {0};
    TEST_ASSERT_TRUE(mbReadString(&buf, header, 11));
    TEST_ASSERT_EQUAL_STRING(BUFFER_TEST_STR, header);
    TEST_ASSERT_EQUAL_INT(11, mbPosition(&buf));

    free(data);
}

void mbReadInt32LETest(void)
{
    size_t length = 0;
    uint8_t* data = make_test_buffer(&length);

    MemoryBuffer buf = {0};
    mbInitFromMemory(&buf, data, length);
    TEST_ASSERT_TRUE(mbSeek(&buf, 11)); /* skip string header */

    for (int32_t i = 0; i < 10; i++)
    {
        int32_t v = -1;
        TEST_ASSERT_TRUE(mbReadInt32LE(&buf, &v));
        TEST_ASSERT_EQUAL_INT32(i, v);
    }

    TEST_ASSERT_EQUAL_INT(11 + 10 * 4, mbPosition(&buf));

    free(data);
}

void mbReadInt32BETest(void)
{
    size_t length = 0;
    uint8_t* data = make_test_buffer(&length);

    MemoryBuffer buf = {0};
    mbInitFromMemory(&buf, data, length);
    TEST_ASSERT_TRUE(mbSeek(&buf, 51)); /* skip string + 10 LE ints */

    for (int32_t i = 0; i < 10; i++)
    {
        int32_t v = -1;
        TEST_ASSERT_TRUE(mbReadInt32BE(&buf, &v));
        TEST_ASSERT_EQUAL_INT32(i, v);
    }

    TEST_ASSERT_EQUAL_INT(11 + 20 * 4, mbPosition(&buf));

    free(data);
}

/* ------------------------------------------------------------------ */
/* Scan-to test                                                         */
/* ------------------------------------------------------------------ */

void mbScanToTest(void)
{
    size_t length = 0;
    uint8_t* data = make_test_buffer(&length);

    MemoryBuffer buf = {0};
    mbInitFromMemory(&buf, data, length);

    TEST_ASSERT_TRUE(mbScanTo(&buf, END_BLOCK_STR, 9));

    char found[10] = {0};
    TEST_ASSERT_TRUE(mbReadString(&buf, found, 9));
    TEST_ASSERT_EQUAL_STRING(END_BLOCK_STR, found);
    TEST_ASSERT_TRUE(mbIsEOF(&buf));

    free(data);
}

/* ------------------------------------------------------------------ */
/* Seek tests                                                           */
/* ------------------------------------------------------------------ */

void mbSeekAndReadTest(void)
{
    size_t length = 0;
    uint8_t* data = make_test_buffer(&length);

    MemoryBuffer buf = {0};
    mbInitFromMemory(&buf, data, length);

    TEST_ASSERT_TRUE(mbSeek(&buf, 91)); /* "End block" starts at offset 91 */

    char found[10] = {0};
    TEST_ASSERT_TRUE(mbReadString(&buf, found, 9));
    TEST_ASSERT_EQUAL_STRING(END_BLOCK_STR, found);
    TEST_ASSERT_TRUE(mbIsEOF(&buf));

    free(data);
}

void mbSeekWriteReadBackTest(void)
{
    size_t length = 0;
    uint8_t* data = make_test_buffer(&length);

    MemoryBuffer buf = {0};
    mbInitFromMemory(&buf, data, length);

    /* Overwrite the int at offset 7 (second LE int32, value 1) with 22 */
    TEST_ASSERT_TRUE(mbSeek(&buf, 7));
    TEST_ASSERT_TRUE(mbWriteInt32LE(&buf, 22));

    /* Seek back and read the same bytes as big-endian:
     * 22 in LE = 0x16 0x00 0x00 0x00; read as BE = 0x16000000 = 369098752 */
    TEST_ASSERT_TRUE(mbSeek(&buf, 7));
    int32_t v = 0;
    TEST_ASSERT_TRUE(mbReadInt32BE(&buf, &v));
    TEST_ASSERT_EQUAL_INT32(369098752, v);

    free(data);
}

/* ------------------------------------------------------------------ */
/* 24-bit roundtrip tests (one per format)                             */
/* ------------------------------------------------------------------ */

void mbReadWriteInt24LETest(void)
{
    MemoryBuffer buf = {0};
    mbInitEmpty(&buf);

    TEST_ASSERT_TRUE(mbWriteInt24LE(&buf, 0x00112233));
    TEST_ASSERT_TRUE(mbSeek(&buf, 0));

    int32_t v = 0;
    TEST_ASSERT_TRUE(mbReadInt24LE(&buf, &v));
    TEST_ASSERT_EQUAL_INT32(0x00112233, v);
    TEST_ASSERT_TRUE(mbIsEOF(&buf));

    mbFree(&buf);
}

void mbReadWriteInt24BETest(void)
{
    MemoryBuffer buf = {0};
    mbInitEmpty(&buf);

    TEST_ASSERT_TRUE(mbWriteInt24BE(&buf, 0x00445566));
    TEST_ASSERT_TRUE(mbSeek(&buf, 0));

    int32_t v = 0;
    TEST_ASSERT_TRUE(mbReadInt24BE(&buf, &v));
    TEST_ASSERT_EQUAL_INT32(0x00445566, v);
    TEST_ASSERT_TRUE(mbIsEOF(&buf));

    mbFree(&buf);
}

void mbReadWriteUInt24LETest(void)
{
    MemoryBuffer buf = {0};
    mbInitEmpty(&buf);

    TEST_ASSERT_TRUE(mbWriteUInt24LE(&buf, 0x00778899u));
    TEST_ASSERT_TRUE(mbSeek(&buf, 0));

    uint32_t v = 0;
    TEST_ASSERT_TRUE(mbReadUInt24LE(&buf, &v));
    TEST_ASSERT_EQUAL_UINT32(0x00778899u, v);
    TEST_ASSERT_TRUE(mbIsEOF(&buf));

    mbFree(&buf);
}

void mbReadWriteUInt24BETest(void)
{
    MemoryBuffer buf = {0};
    mbInitEmpty(&buf);

    TEST_ASSERT_TRUE(mbWriteUInt24BE(&buf, 0x00AABBCCu));
    TEST_ASSERT_TRUE(mbSeek(&buf, 0));

    uint32_t v = 0;
    TEST_ASSERT_TRUE(mbReadUInt24BE(&buf, &v));
    TEST_ASSERT_EQUAL_UINT32(0x00AABBCCu, v);
    TEST_ASSERT_TRUE(mbIsEOF(&buf));

    mbFree(&buf);
}

/* ------------------------------------------------------------------ */
/* Skip edge-case tests (one per case)                                 */
/* ------------------------------------------------------------------ */

void mbSkipNegativeTest(void)
{
    size_t length = 0;
    uint8_t* data = make_test_buffer(&length);

    MemoryBuffer buf = {0};
    mbInitFromMemory(&buf, data, length);

    /* At position 0 a negative skip must fail */
    TEST_ASSERT_FALSE(mbSkip(&buf, -1));

    free(data);
}

void mbSkipForwardTest(void)
{
    size_t length = 0;
    uint8_t* data = make_test_buffer(&length);

    MemoryBuffer buf = {0};
    mbInitFromMemory(&buf, data, length);

    TEST_ASSERT_TRUE(mbSkip(&buf, 11));
    TEST_ASSERT_EQUAL_INT(11, mbPosition(&buf));

    free(data);
}

void mbSkipUnderflowTest(void)
{
    size_t length = 0;
    uint8_t* data = make_test_buffer(&length);

    MemoryBuffer buf = {0};
    mbInitFromMemory(&buf, data, length);

    TEST_ASSERT_TRUE(mbSkip(&buf, 11));          /* advance to position 11 */
    TEST_ASSERT_FALSE(mbSkip(&buf, -12));        /* -12 would go to -1: must fail */

    free(data);
}

/* ------------------------------------------------------------------ */

void setUp(void)
{
}

void tearDown(void)
{
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(mbWrite100ByteBufferTest);
    RUN_TEST(mbReadStringTest);
    RUN_TEST(mbReadInt32LETest);
    RUN_TEST(mbReadInt32BETest);
    RUN_TEST(mbScanToTest);
    RUN_TEST(mbSeekAndReadTest);
    RUN_TEST(mbSeekWriteReadBackTest);
    RUN_TEST(mbReadWriteInt24LETest);
    RUN_TEST(mbReadWriteInt24BETest);
    RUN_TEST(mbReadWriteUInt24LETest);
    RUN_TEST(mbReadWriteUInt24BETest);
    RUN_TEST(mbSkipNegativeTest);
    RUN_TEST(mbSkipForwardTest);
    RUN_TEST(mbSkipUnderflowTest);
    return UNITY_END();
}
