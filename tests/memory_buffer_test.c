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
    memory_buffer_t buf = {0};
    mb_initEmpty(&buf);

    mb_writeString(&buf, BUFFER_TEST_STR, 11);
    for (int32_t i = 0; i < 10; i++)
        mb_writeInt32LE(&buf, i);
    for (int32_t i = 0; i < 10; i++)
        mb_writeInt32BE(&buf, i);
    mb_writeString(&buf, END_BLOCK_STR, 9);

    uint8_t* data = mb_data(&buf, out_length);
    mb_free(&buf);
    return data;
}

/* ------------------------------------------------------------------ */
/* Write tests                                                          */
/* ------------------------------------------------------------------ */

void mb_write100ByteBufferTest(void)
{
    memory_buffer_t buf = {0};
    mb_initEmpty(&buf);

    TEST_ASSERT_TRUE(mb_writeString(&buf, BUFFER_TEST_STR, 11));
    TEST_ASSERT_EQUAL_size_t(11, buf.length);

    for (int32_t i = 0; i < 10; i++)
    {
        TEST_ASSERT_TRUE(mb_writeInt32LE(&buf, i));
        TEST_ASSERT_EQUAL_size_t((size_t)(11 + (i + 1) * 4), buf.length);
    }

    TEST_ASSERT_EQUAL_INT(11 + 10 * 4, mb_position(&buf));

    for (int32_t i = 0; i < 10; i++)
    {
        TEST_ASSERT_TRUE(mb_writeInt32BE(&buf, i));
        TEST_ASSERT_EQUAL_size_t((size_t)(11 + (10 + i + 1) * 4), buf.length);
    }

    TEST_ASSERT_EQUAL_INT(11 + 20 * 4, mb_position(&buf));

    TEST_ASSERT_TRUE(mb_writeString(&buf, END_BLOCK_STR, 9));
    TEST_ASSERT_EQUAL_size_t(100, buf.length);

    size_t length = 0;
    uint8_t* data = mb_data(&buf, &length);
    TEST_ASSERT_EQUAL_size_t(100, length);
    TEST_ASSERT_NOT_NULL(data);
    free(data);

    mb_free(&buf);
    TEST_ASSERT_NULL(buf.data);
}

/* ------------------------------------------------------------------ */
/* Read tests (each builds the 100-byte buffer fresh)                  */
/* ------------------------------------------------------------------ */

void mb_readStringTest(void)
{
    size_t length = 0;
    uint8_t* data = make_test_buffer(&length);

    memory_buffer_t buf = {0};
    mb_initFromMemory(&buf, data, length);
    TEST_ASSERT_EQUAL_size_t(100, buf.length);

    char header[12] = {0};
    TEST_ASSERT_TRUE(mb_readString(&buf, header, 11));
    TEST_ASSERT_EQUAL_STRING(BUFFER_TEST_STR, header);
    TEST_ASSERT_EQUAL_INT(11, mb_position(&buf));

    free(data);
}

void mb_readInt32LETest(void)
{
    size_t length = 0;
    uint8_t* data = make_test_buffer(&length);

    memory_buffer_t buf = {0};
    mb_initFromMemory(&buf, data, length);
    TEST_ASSERT_TRUE(mb_seek(&buf, 11)); /* skip string header */

    for (int32_t i = 0; i < 10; i++)
    {
        int32_t v = -1;
        TEST_ASSERT_TRUE(mb_readInt32LE(&buf, &v));
        TEST_ASSERT_EQUAL_INT32(i, v);
    }

    TEST_ASSERT_EQUAL_INT(11 + 10 * 4, mb_position(&buf));

    free(data);
}

void mb_readInt32BETest(void)
{
    size_t length = 0;
    uint8_t* data = make_test_buffer(&length);

    memory_buffer_t buf = {0};
    mb_initFromMemory(&buf, data, length);
    TEST_ASSERT_TRUE(mb_seek(&buf, 51)); /* skip string + 10 LE ints */

    for (int32_t i = 0; i < 10; i++)
    {
        int32_t v = -1;
        TEST_ASSERT_TRUE(mb_readInt32BE(&buf, &v));
        TEST_ASSERT_EQUAL_INT32(i, v);
    }

    TEST_ASSERT_EQUAL_INT(11 + 20 * 4, mb_position(&buf));

    free(data);
}

/* ------------------------------------------------------------------ */
/* Scan-to test                                                         */
/* ------------------------------------------------------------------ */

void mb_ScanToTest(void)
{
    size_t length = 0;
    uint8_t* data = make_test_buffer(&length);

    memory_buffer_t buf = {0};
    mb_initFromMemory(&buf, data, length);

    TEST_ASSERT_TRUE(mb_scanTo(&buf, END_BLOCK_STR, 9));

    char found[10] = {0};
    TEST_ASSERT_TRUE(mb_readString(&buf, found, 9));
    TEST_ASSERT_EQUAL_STRING(END_BLOCK_STR, found);
    TEST_ASSERT_TRUE(mb_isEOF(&buf));

    free(data);
}

/* ------------------------------------------------------------------ */
/* Seek tests                                                           */
/* ------------------------------------------------------------------ */

void mb_SeekAndReadTest(void)
{
    size_t length = 0;
    uint8_t* data = make_test_buffer(&length);

    memory_buffer_t buf = {0};
    mb_initFromMemory(&buf, data, length);

    TEST_ASSERT_TRUE(mb_seek(&buf, 91)); /* "End block" starts at offset 91 */

    char found[10] = {0};
    TEST_ASSERT_TRUE(mb_readString(&buf, found, 9));
    TEST_ASSERT_EQUAL_STRING(END_BLOCK_STR, found);
    TEST_ASSERT_TRUE(mb_isEOF(&buf));

    free(data);
}

void mb_SeekWriteReadBackTest(void)
{
    size_t length = 0;
    uint8_t* data = make_test_buffer(&length);

    memory_buffer_t buf = {0};
    mb_initFromMemory(&buf, data, length);

    /* Overwrite the int at offset 7 (second LE int32, value 1) with 22 */
    TEST_ASSERT_TRUE(mb_seek(&buf, 7));
    TEST_ASSERT_TRUE(mb_writeInt32LE(&buf, 22));

    /* Seek back and read the same bytes as big-endian:
     * 22 in LE = 0x16 0x00 0x00 0x00; read as BE = 0x16000000 = 369098752 */
    TEST_ASSERT_TRUE(mb_seek(&buf, 7));
    int32_t v = 0;
    TEST_ASSERT_TRUE(mb_readInt32BE(&buf, &v));
    TEST_ASSERT_EQUAL_INT32(369098752, v);

    free(data);
}

/* ------------------------------------------------------------------ */
/* 24-bit roundtrip tests (one per format)                             */
/* ------------------------------------------------------------------ */

void mb_readWriteInt24LETest(void)
{
    memory_buffer_t buf = {0};
    mb_initEmpty(&buf);

    TEST_ASSERT_TRUE(mb_writeInt24LE(&buf, 0x00112233));
    TEST_ASSERT_TRUE(mb_seek(&buf, 0));

    int32_t v = 0;
    TEST_ASSERT_TRUE(mb_readInt24LE(&buf, &v));
    TEST_ASSERT_EQUAL_INT32(0x00112233, v);
    TEST_ASSERT_TRUE(mb_isEOF(&buf));

    mb_free(&buf);
}

void mb_readWriteInt24BETest(void)
{
    memory_buffer_t buf = {0};
    mb_initEmpty(&buf);

    TEST_ASSERT_TRUE(mb_writeInt24BE(&buf, 0x00445566));
    TEST_ASSERT_TRUE(mb_seek(&buf, 0));

    int32_t v = 0;
    TEST_ASSERT_TRUE(mb_readInt24BE(&buf, &v));
    TEST_ASSERT_EQUAL_INT32(0x00445566, v);
    TEST_ASSERT_TRUE(mb_isEOF(&buf));

    mb_free(&buf);
}

void mb_readWriteUInt24LETest(void)
{
    memory_buffer_t buf = {0};
    mb_initEmpty(&buf);

    TEST_ASSERT_TRUE(mb_writeUInt24LE(&buf, 0x00778899u));
    TEST_ASSERT_TRUE(mb_seek(&buf, 0));

    uint32_t v = 0;
    TEST_ASSERT_TRUE(mb_readUInt24LE(&buf, &v));
    TEST_ASSERT_EQUAL_UINT32(0x00778899u, v);
    TEST_ASSERT_TRUE(mb_isEOF(&buf));

    mb_free(&buf);
}

void mb_readWriteUInt24BETest(void)
{
    memory_buffer_t buf = {0};
    mb_initEmpty(&buf);

    TEST_ASSERT_TRUE(mb_writeUInt24BE(&buf, 0x00AABBCCu));
    TEST_ASSERT_TRUE(mb_seek(&buf, 0));

    uint32_t v = 0;
    TEST_ASSERT_TRUE(mb_readUInt24BE(&buf, &v));
    TEST_ASSERT_EQUAL_UINT32(0x00AABBCCu, v);
    TEST_ASSERT_TRUE(mb_isEOF(&buf));

    mb_free(&buf);
}

/* ------------------------------------------------------------------ */
/* Skip edge-case tests (one per case)                                 */
/* ------------------------------------------------------------------ */

void mb_SkipNegativeTest(void)
{
    size_t length = 0;
    uint8_t* data = make_test_buffer(&length);

    memory_buffer_t buf = {0};
    mb_initFromMemory(&buf, data, length);

    /* At position 0 a negative skip must fail */
    TEST_ASSERT_FALSE(mb_skip(&buf, -1));

    free(data);
}

void mb_SkipForwardTest(void)
{
    size_t length = 0;
    uint8_t* data = make_test_buffer(&length);

    memory_buffer_t buf = {0};
    mb_initFromMemory(&buf, data, length);

    TEST_ASSERT_TRUE(mb_skip(&buf, 11));
    TEST_ASSERT_EQUAL_INT(11, mb_position(&buf));

    free(data);
}

void mb_SkipUnderflowTest(void)
{
    size_t length = 0;
    uint8_t* data = make_test_buffer(&length);

    memory_buffer_t buf = {0};
    mb_initFromMemory(&buf, data, length);

    TEST_ASSERT_TRUE(mb_skip(&buf, 11));          /* advance to position 11 */
    TEST_ASSERT_FALSE(mb_skip(&buf, -12));        /* -12 would go to -1: must fail */

    free(data);
}

void mb_readWriteIntegrationRoundTripTest(void)
{
    memory_buffer_t buf = {0};
    mb_initEmpty(&buf);

    TEST_ASSERT_TRUE(mb_writeUInt16LE(&buf, 0x1234u));
    TEST_ASSERT_TRUE(mb_writeUInt32BE(&buf, 0x89ABCDEFu));
    TEST_ASSERT_TRUE(mb_writeString(&buf, "OK", 2));
    TEST_ASSERT_TRUE(mb_seek(&buf, 0));

    uint16_t u16 = 0;
    uint32_t u32 = 0;
    char text[3] = {0};
    TEST_ASSERT_TRUE(mb_readUInt16LE(&buf, &u16));
    TEST_ASSERT_TRUE(mb_readUInt32BE(&buf, &u32));
    TEST_ASSERT_TRUE(mb_readString(&buf, text, 2));
    TEST_ASSERT_EQUAL_UINT16(0x1234u, u16);
    TEST_ASSERT_EQUAL_UINT32(0x89ABCDEFu, u32);
    TEST_ASSERT_EQUAL_STRING("OK", text);

    mb_free(&buf);
}

void mb_StressSequentialInt32RoundTripTest(void)
{
    memory_buffer_t buf = {0};
    mb_initEmpty(&buf);

    for (int32_t i = 0; i < SHL_TEST_STRESS_COUNT; i++)
    {
        TEST_ASSERT_TRUE(mb_writeInt32LE(&buf, i));
    }

    TEST_ASSERT_TRUE(mb_seek(&buf, 0));
    for (int32_t i = 0; i < SHL_TEST_STRESS_COUNT; i++)
    {
        int32_t value = -1;
        TEST_ASSERT_TRUE(mb_readInt32LE(&buf, &value));
        TEST_ASSERT_EQUAL_INT32(i, value);
    }
    TEST_ASSERT_TRUE(mb_isEOF(&buf));

    mb_free(&buf);
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
    RUN_TEST(mb_write100ByteBufferTest);
    RUN_TEST(mb_readStringTest);
    RUN_TEST(mb_readInt32LETest);
    RUN_TEST(mb_readInt32BETest);
    RUN_TEST(mb_ScanToTest);
    RUN_TEST(mb_SeekAndReadTest);
    RUN_TEST(mb_SeekWriteReadBackTest);
    RUN_TEST(mb_readWriteInt24LETest);
    RUN_TEST(mb_readWriteInt24BETest);
    RUN_TEST(mb_readWriteUInt24LETest);
    RUN_TEST(mb_readWriteUInt24BETest);
    RUN_TEST(mb_SkipNegativeTest);
    RUN_TEST(mb_SkipForwardTest);
    RUN_TEST(mb_SkipUnderflowTest);
    RUN_TEST(mb_readWriteIntegrationRoundTripTest);
    RUN_TEST(mb_StressSequentialInt32RoundTripTest);
    return UNITY_END();
}
