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

static void writeMinimalFlicWithOffsets(const char* path, uint32_t frame1Offset, uint32_t frame2Offset)
{
    FILE* file = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(file);

    writeUInt32LE(file, 128);
    writeUInt16LE(file, FLC_MAGIC_NUMBER);
    writeUInt16LE(file, 2);
    writeUInt16LE(file, 10);
    writeUInt16LE(file, 12);
    writeUInt16LE(file, 8);
    writeUInt16LE(file, 0);
    writeUInt32LE(file, 33);

    for (int i = 20; i < 80; i++)
        fputc(0, file);

    writeUInt32LE(file, frame1Offset);
    writeUInt32LE(file, frame2Offset);

    for (int i = 88; i < 128; i++)
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

void flicOpenReadsFlcFrameOffsets(void)
{
    const char* path = "offsets.flc";
    writeMinimalFlicWithOffsets(path, 256, 512);

    Flic flic;
    TEST_ASSERT_TRUE(flicOpen(&flic, path));
    TEST_ASSERT_EQUAL_UINT16(2, flic.frames);
    TEST_ASSERT_EQUAL_UINT16(10, flic.width);
    TEST_ASSERT_EQUAL_UINT16(12, flic.height);
    TEST_ASSERT_EQUAL_UINT32(256u, flic.oframe1);
    TEST_ASSERT_EQUAL_UINT32(512u, flic.oframe2);
    flicClose(&flic);

    remove(path);
}

void flicMakeImageAppliesChangedPixelsOnly(void)
{
    Flic flic = { .width = 2, .height = 2 };
    uint16_t pixels[] = {
        FLI_PXL_CHANGE | 1, 2,
        FLI_PXL_CHANGE | 3, 4
    };
    uint8_t colors[FLI_COLORS_SIZE] = {0};
    uint8_t image[12];
    FlicFrame frame = {
        .pixels = pixels,
        .rowStride = 2,
    };

    memset(image, 0xCC, sizeof(image));
    memcpy(frame.colors, colors, sizeof(colors));
    frame.colors[1 * 3 + 0] = 10;
    frame.colors[1 * 3 + 1] = 11;
    frame.colors[1 * 3 + 2] = 12;
    frame.colors[3 * 3 + 0] = 30;
    frame.colors[3 * 3 + 1] = 31;
    frame.colors[3 * 3 + 2] = 32;

    flicMakeImage(&flic, &frame, image);

    TEST_ASSERT_EQUAL_UINT8(10, image[0]);
    TEST_ASSERT_EQUAL_UINT8(11, image[1]);
    TEST_ASSERT_EQUAL_UINT8(12, image[2]);
    TEST_ASSERT_EQUAL_UINT8(0xCC, image[3]);
    TEST_ASSERT_EQUAL_UINT8(30, image[6]);
    TEST_ASSERT_EQUAL_UINT8(31, image[7]);
    TEST_ASSERT_EQUAL_UINT8(32, image[8]);
    TEST_ASSERT_EQUAL_UINT8(0xCC, image[9]);
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
    RUN_TEST(flicOpenReadsFlcFrameOffsets);
    RUN_TEST(flicMakeImageAppliesChangedPixelsOnly);
    return UNITY_END();
}
