#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SHL_FLIC_IMPLEMENTATION
#include "../flic.h"
#include "test_common.h"

static void writeUInt16LE(FILE* file, uint16_t value)
{
    fputc(value & 0xFF, file);
    fputc((value >> 8) & 0xFF, file);
}

static void writeUInt32LE(FILE* file, uint32_t value)
{
    fputc(value & 0xFF, file);
    fputc((value >> 8) & 0xFF, file);
    fputc((value >> 16) & 0xFF, file);
    fputc((value >> 24) & 0xFF, file);
}

static void writeMinimalFlic(const char* path, uint16_t magic, uint16_t width, uint16_t height, uint32_t speed)
{
    FILE* file = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(file);

    writeUInt32LE(file, 128);
    writeUInt16LE(file, magic);
    writeUInt16LE(file, 1);
    writeUInt16LE(file, width);
    writeUInt16LE(file, height);
    writeUInt16LE(file, 8);
    writeUInt16LE(file, 0);
    writeUInt32LE(file, speed);

    for (int i = 20; i < 128; i++)
        fputc(0, file);

    fclose(file);
}

void flicOpenFailsForMissingFile(void)
{
    Flic flic;
    TEST_ASSERT_FALSE(flicOpen(&flic, "missing-file.fli"));
}

void flicOpenFailsForInvalidMagic(void)
{
    const char* path = "invalid_magic.fli";
    writeMinimalFlic(path, 0x0000, 10, 10, 1);

    Flic flic;
    TEST_ASSERT_FALSE(flicOpen(&flic, path));
    TEST_ASSERT_NULL(flic.file);

    remove(path);
}

void flicOpenAppliesDefaultDimensions(void)
{
    const char* path = "default_dimensions.fli";
    writeMinimalFlic(path, FLI_MAGIC_NUMBER, 0, 0, 0);

    Flic flic;
    TEST_ASSERT_TRUE(flicOpen(&flic, path));
    TEST_ASSERT_EQUAL_UINT16(1, flic.frames);
    TEST_ASSERT_EQUAL_UINT16(320, flic.width);
    TEST_ASSERT_EQUAL_UINT16(200, flic.height);
    TEST_ASSERT_EQUAL_UINT16(70, flic.speed);
    flicClose(&flic);

    remove(path);
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
    RUN_TEST(flicOpenFailsForMissingFile);
    RUN_TEST(flicOpenFailsForInvalidMagic);
    RUN_TEST(flicOpenAppliesDefaultDimensions);
    return UNITY_END();
}
