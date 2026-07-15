#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SHL_FLIC_IMPLEMENTATION
#include "../flic.h"
#include "test_common.h"

static void write_uint16LE(FILE* file, uint16_t value)
{
    fputc(value & 0xFF, file);
    fputc((value >> 8) & 0xFF, file);
}

static void write_uint32LE(FILE* file, uint32_t value)
{
    fputc(value & 0xFF, file);
    fputc((value >> 8) & 0xFF, file);
    fputc((value >> 16) & 0xFF, file);
    fputc((value >> 24) & 0xFF, file);
}

static void write_minimal_flic(const char* path, uint16_t magic, uint16_t width, uint16_t height, uint32_t speed)
{
    FILE* file = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(file);

    write_uint32LE(file, 128);
    write_uint16LE(file, magic);
    write_uint16LE(file, 1);
    write_uint16LE(file, width);
    write_uint16LE(file, height);
    write_uint16LE(file, 8);
    write_uint16LE(file, 0);
    write_uint32LE(file, speed);

    for (int i = 20; i < 128; i++)
        fputc(0, file);

    fclose(file);
}

static void write_minimal_flic_with_offsets(const char* path, uint32_t frame1Offset, uint32_t frame2Offset)
{
    FILE* file = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(file);

    write_uint32LE(file, 128);
    write_uint16LE(file, FLC_MAGIC_NUMBER);
    write_uint16LE(file, 2);
    write_uint16LE(file, 10);
    write_uint16LE(file, 12);
    write_uint16LE(file, 8);
    write_uint16LE(file, 0);
    write_uint32LE(file, 33);

    for (int i = 20; i < 80; i++)
        fputc(0, file);

    write_uint32LE(file, frame1Offset);
    write_uint32LE(file, frame2Offset);

    for (int i = 88; i < 128; i++)
        fputc(0, file);

    fclose(file);
}

static void write_flic_with_frame_and_chunk(const char* path, uint16_t magic, uint16_t width, uint16_t height, uint32_t speed, uint16_t chunkType, const uint8_t* chunkData, uint32_t chunkDataSize)
{
    FILE* file = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(file);

    uint32_t headerSize = 128;
    uint32_t frameSize = 16 + 6 + chunkDataSize;
    uint32_t fileSize = headerSize + frameSize;

    write_uint32LE(file, fileSize);
    write_uint16LE(file, magic);
    write_uint16LE(file, 1);
    write_uint16LE(file, width);
    write_uint16LE(file, height);
    write_uint16LE(file, 8);
    write_uint16LE(file, 0);
    write_uint32LE(file, speed);

    for (int i = 20; i < 128; i++)
        fputc(0, file);

    // Frame header (16 bytes)
    write_uint32LE(file, frameSize);
    write_uint16LE(file, FLI_FRAME_MAGIC_NUMBER);
    write_uint16LE(file, 1); // 1 chunk
    write_uint16LE(file, 42); // delay
    write_uint16LE(file, 0);
    write_uint16LE(file, 0);
    write_uint16LE(file, 0);

    // Chunk header (6 bytes)
    write_uint32LE(file, 6 + chunkDataSize);
    write_uint16LE(file, chunkType);

    // Chunk data
    if (chunkData && chunkDataSize > 0)
    {
        fwrite(chunkData, 1, chunkDataSize, file);
    }

    fclose(file);
}

static void write_flic_with_two_frames(const char* path, uint16_t width, uint16_t height, uint16_t chunkType1, const uint8_t* chunkData1, uint32_t chunkDataSize1, uint16_t chunkType2, const uint8_t* chunkData2, uint32_t chunkDataSize2)
{
    FILE* file = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(file);

    uint32_t headerSize = 128;
    uint32_t frame1Size = 16 + 6 + chunkDataSize1;
    uint32_t frame2Size = 16 + 6 + chunkDataSize2;

    uint32_t oframe1 = headerSize;
    uint32_t oframe2 = oframe1 + frame1Size;

    // FLC Main Header
    write_uint32LE(file, oframe2 + frame2Size);
    write_uint16LE(file, FLC_MAGIC_NUMBER);
    write_uint16LE(file, 2); // 2 frames
    write_uint16LE(file, width);
    write_uint16LE(file, height);
    write_uint16LE(file, 8);
    write_uint16LE(file, 0);
    write_uint32LE(file, 33); // speed

    for (int i = 20; i < 80; i++)
        fputc(0, file);

    write_uint32LE(file, oframe1);
    write_uint32LE(file, oframe2);

    for (int i = 88; i < 128; i++)
        fputc(0, file);

    // Frame 1
    write_uint32LE(file, frame1Size);
    write_uint16LE(file, FLI_FRAME_MAGIC_NUMBER);
    write_uint16LE(file, 1); // 1 chunk
    write_uint16LE(file, 15); // delay = 15ms
    write_uint16LE(file, 0);
    write_uint16LE(file, 0);
    write_uint16LE(file, 0);

    // Chunk 1
    write_uint32LE(file, 6 + chunkDataSize1);
    write_uint16LE(file, chunkType1);
    if (chunkData1 && chunkDataSize1 > 0)
        fwrite(chunkData1, 1, chunkDataSize1, file);

    // Frame 2
    write_uint32LE(file, frame2Size);
    write_uint16LE(file, FLI_FRAME_MAGIC_NUMBER);
    write_uint16LE(file, 1); // 1 chunk
    write_uint16LE(file, 25); // delay = 25ms
    write_uint16LE(file, 0);
    write_uint16LE(file, 0);
    write_uint16LE(file, 0);

    // Chunk 2
    write_uint32LE(file, 6 + chunkDataSize2);
    write_uint16LE(file, chunkType2);
    if (chunkData2 && chunkDataSize2 > 0)
        fwrite(chunkData2, 1, chunkDataSize2, file);

    fclose(file);
}

static void write_flic_with_bad_frame_magic(const char* path)
{
    FILE* file = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(file);

    write_uint32LE(file, 128 + 16);
    write_uint16LE(file, FLI_MAGIC_NUMBER);
    write_uint16LE(file, 1);
    write_uint16LE(file, 4);
    write_uint16LE(file, 4);
    write_uint16LE(file, 8);
    write_uint16LE(file, 0);
    write_uint32LE(file, 10);

    for (int i = 20; i < 128; i++)
        fputc(0, file);

    // Frame header with bad magic
    write_uint32LE(file, 16);
    write_uint16LE(file, 0x0000); // bad magic
    write_uint16LE(file, 0);
    write_uint16LE(file, 0);
    write_uint16LE(file, 0);
    write_uint16LE(file, 0);
    write_uint16LE(file, 0);

    fclose(file);
}

static void write_flic_with_unknown_and_black_chunk(const char* path)
{
    FILE* file = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(file);

    uint32_t headerSize = 128;
    // Chunk 1 (Unknown): header (6) + data (4) = 10
    // Chunk 2 (Black): header (6) + data (0) = 6
    uint32_t frameSize = 16 + 10 + 6;

    write_uint32LE(file, headerSize + frameSize);
    write_uint16LE(file, FLI_MAGIC_NUMBER);
    write_uint16LE(file, 1);
    write_uint16LE(file, 4);
    write_uint16LE(file, 4);
    write_uint16LE(file, 8);
    write_uint16LE(file, 0);
    write_uint32LE(file, 10);

    for (int i = 20; i < 128; i++)
        fputc(0, file);

    // Frame header
    write_uint32LE(file, frameSize);
    write_uint16LE(file, FLI_FRAME_MAGIC_NUMBER);
    write_uint16LE(file, 2); // 2 chunks
    write_uint16LE(file, 0);
    write_uint16LE(file, 0);
    write_uint16LE(file, 0);
    write_uint16LE(file, 0);

    // Chunk 1 (Unknown, type 0x9999)
    write_uint32LE(file, 10);
    write_uint16LE(file, 0x9999);
    write_uint32LE(file, 0x12345678);

    // Chunk 2 (Black)
    write_uint32LE(file, 6);
    write_uint16LE(file, FLI_BLACK_CHUNK);

    fclose(file);
}

/* Renamed existing tests */

void flic_open_fails_for_missing_file(void)
{
    Flic flic;
    TEST_ASSERT_FALSE(flic_open(&flic, "missing-file.fli"));
}

void flic_open_fails_for_invalid_magic(void)
{
    const char* path = "invalid_magic.fli";
    write_minimal_flic(path, 0x0000, 10, 10, 1);

    Flic flic;
    TEST_ASSERT_FALSE(flic_open(&flic, path));
    TEST_ASSERT_NULL(flic.file);

    remove(path);
}

void flic_open_applies_default_dimensions(void)
{
    const char* path = "default_dimensions.fli";
    write_minimal_flic(path, FLI_MAGIC_NUMBER, 0, 0, 0);

    Flic flic;
    TEST_ASSERT_TRUE(flic_open(&flic, path));
    TEST_ASSERT_EQUAL_UINT16(1, flic.frames);
    TEST_ASSERT_EQUAL_UINT16(320, flic.width);
    TEST_ASSERT_EQUAL_UINT16(200, flic.height);
    TEST_ASSERT_EQUAL_UINT16(70, flic.speed);
    flic_close(&flic);

    remove(path);
}

void flic_open_reads_flc_frame_offsets(void)
{
    const char* path = "offsets.flc";
    write_minimal_flic_with_offsets(path, 256, 512);

    Flic flic;
    TEST_ASSERT_TRUE(flic_open(&flic, path));
    TEST_ASSERT_EQUAL_UINT16(2, flic.frames);
    TEST_ASSERT_EQUAL_UINT16(10, flic.width);
    TEST_ASSERT_EQUAL_UINT16(12, flic.height);
    TEST_ASSERT_EQUAL_UINT32(256u, flic.oframe1);
    TEST_ASSERT_EQUAL_UINT32(512u, flic.oframe2);
    flic_close(&flic);

    remove(path);
}

void flic_make_image_applies_changed_pixels_only(void)
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

    flic_makeImage(&flic, &frame, image);

    TEST_ASSERT_EQUAL_UINT8(10, image[0]);
    TEST_ASSERT_EQUAL_UINT8(11, image[1]);
    TEST_ASSERT_EQUAL_UINT8(12, image[2]);
    TEST_ASSERT_EQUAL_UINT8(0xCC, image[3]);
    TEST_ASSERT_EQUAL_UINT8(30, image[6]);
    TEST_ASSERT_EQUAL_UINT8(31, image[7]);
    TEST_ASSERT_EQUAL_UINT8(32, image[8]);
    TEST_ASSERT_EQUAL_UINT8(0xCC, image[9]);
}

/* 18 New Tests */

void flic_open_fails_for_null_args(void)
{
    TEST_ASSERT_FALSE(flic_open(NULL, "somefile.fli"));
    Flic flic;
    TEST_ASSERT_FALSE(flic_open(&flic, NULL));
}

void flic_open_converts_fli_speed_to_milliseconds(void)
{
    const char* path = "speed_test.fli";
    write_minimal_flic(path, FLI_MAGIC_NUMBER, 10, 10, 7);

    Flic flic;
    TEST_ASSERT_TRUE(flic_open(&flic, path));
    TEST_ASSERT_EQUAL_UINT16(100, flic.speed);

    flic_close(&flic);
    remove(path);
}

void flic_read_frame_returns_false_for_bad_magic(void)
{
    const char* path = "bad_frame_magic.fli";
    write_flic_with_bad_frame_magic(path);

    Flic flic;
    TEST_ASSERT_TRUE(flic_open(&flic, path));

    uint16_t pixels[16];
    FlicFrame frame = {
        .pixels = pixels,
        .rowStride = 4,
    };
    TEST_ASSERT_FALSE(flic_readFrame(&flic, &frame));

    flic_close(&flic);
    remove(path);
}

void flic_read_frame_decodes_black_chunk(void)
{
    const char* path = "black_chunk.fli";
    write_flic_with_frame_and_chunk(path, FLI_MAGIC_NUMBER, 4, 4, 10, FLI_BLACK_CHUNK, NULL, 0);

    Flic flic;
    TEST_ASSERT_TRUE(flic_open(&flic, path));

    uint16_t pixels[16];
    FlicFrame frame = {
        .pixels = pixels,
        .rowStride = 4,
    };
    memset(pixels, 0, sizeof(pixels));

    TEST_ASSERT_TRUE(flic_readFrame(&flic, &frame));
    for (int i = 0; i < 16; i++)
    {
        TEST_ASSERT_EQUAL_UINT16(FLI_PXL_CHANGE, pixels[i]);
    }

    flic_close(&flic);
    remove(path);
}

void flic_read_frame_decodes_flic_copy_chunk(void)
{
    const char* path = "copy_chunk.fli";
    uint8_t data[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    write_flic_with_frame_and_chunk(path, FLI_MAGIC_NUMBER, 4, 4, 10, FLI_COPY_CHUNK, data, sizeof(data));

    Flic flic;
    TEST_ASSERT_TRUE(flic_open(&flic, path));

    uint16_t pixels[16];
    FlicFrame frame = {
        .pixels = pixels,
        .rowStride = 4,
    };
    memset(pixels, 0, sizeof(pixels));

    TEST_ASSERT_TRUE(flic_readFrame(&flic, &frame));
    for (int i = 0; i < 16; i++)
    {
        TEST_ASSERT_EQUAL_UINT16(i | FLI_PXL_CHANGE, pixels[i]);
    }

    flic_close(&flic);
    remove(path);
}

void flic_read_frame_decodes_byte_run_replicate(void)
{
    const char* path = "brun_replicate.fli";
    uint8_t data[] = {
        0, 4, 7,
        0, 4, 8,
        0, 4, 9,
        0, 4, 10
    };
    write_flic_with_frame_and_chunk(path, FLI_MAGIC_NUMBER, 4, 4, 10, FLI_BRUN_CHUNK, data, sizeof(data));

    Flic flic;
    TEST_ASSERT_TRUE(flic_open(&flic, path));

    uint16_t pixels[16];
    FlicFrame frame = {
        .pixels = pixels,
        .rowStride = 4,
    };
    memset(pixels, 0, sizeof(pixels));

    TEST_ASSERT_TRUE(flic_readFrame(&flic, &frame));
    for (int i = 0; i < 4; i++) TEST_ASSERT_EQUAL_UINT16(7 | FLI_PXL_CHANGE, pixels[i]);
    for (int i = 4; i < 8; i++) TEST_ASSERT_EQUAL_UINT16(8 | FLI_PXL_CHANGE, pixels[i]);
    for (int i = 8; i < 12; i++) TEST_ASSERT_EQUAL_UINT16(9 | FLI_PXL_CHANGE, pixels[i]);
    for (int i = 12; i < 16; i++) TEST_ASSERT_EQUAL_UINT16(10 | FLI_PXL_CHANGE, pixels[i]);

    flic_close(&flic);
    remove(path);
}

void flic_read_frame_decodes_byte_run_literal(void)
{
    const char* path = "brun_literal.fli";
    uint8_t data[] = {
        0, 0xFC, 1, 2, 3, 4,
        0, 0xFC, 5, 6, 7, 8,
        0, 0xFC, 9, 10, 11, 12,
        0, 0xFC, 13, 14, 15, 16
    };
    write_flic_with_frame_and_chunk(path, FLI_MAGIC_NUMBER, 4, 4, 10, FLI_BRUN_CHUNK, data, sizeof(data));

    Flic flic;
    TEST_ASSERT_TRUE(flic_open(&flic, path));

    uint16_t pixels[16];
    FlicFrame frame = {
        .pixels = pixels,
        .rowStride = 4,
    };
    memset(pixels, 0, sizeof(pixels));

    TEST_ASSERT_TRUE(flic_readFrame(&flic, &frame));
    for (int i = 0; i < 16; i++)
    {
        TEST_ASSERT_EQUAL_UINT16((i + 1) | FLI_PXL_CHANGE, pixels[i]);
    }

    flic_close(&flic);
    remove(path);
}

void flic_read_frame_updates_color256_palette(void)
{
    const char* path = "color256.fli";
    uint8_t data[] = {
        1, 0,
        2,
        3,
        10, 11, 12,
        20, 21, 22,
        30, 31, 32
    };
    write_flic_with_frame_and_chunk(path, FLI_MAGIC_NUMBER, 4, 4, 10, FLI_COLOR_256_CHUNK, data, sizeof(data));

    Flic flic;
    TEST_ASSERT_TRUE(flic_open(&flic, path));

    uint16_t pixels[16];
    FlicFrame frame = {
        .pixels = pixels,
        .rowStride = 4,
    };
    memset(frame.colors, 0, sizeof(frame.colors));

    TEST_ASSERT_TRUE(flic_readFrame(&flic, &frame));
    TEST_ASSERT_EQUAL_UINT8(0, frame.colors[0]);
    TEST_ASSERT_EQUAL_UINT8(10, frame.colors[2 * 3 + 0]);
    TEST_ASSERT_EQUAL_UINT8(11, frame.colors[2 * 3 + 1]);
    TEST_ASSERT_EQUAL_UINT8(12, frame.colors[2 * 3 + 2]);
    TEST_ASSERT_EQUAL_UINT8(20, frame.colors[3 * 3 + 0]);
    TEST_ASSERT_EQUAL_UINT8(21, frame.colors[3 * 3 + 1]);
    TEST_ASSERT_EQUAL_UINT8(22, frame.colors[3 * 3 + 2]);
    TEST_ASSERT_EQUAL_UINT8(30, frame.colors[4 * 3 + 0]);
    TEST_ASSERT_EQUAL_UINT8(31, frame.colors[4 * 3 + 1]);
    TEST_ASSERT_EQUAL_UINT8(32, frame.colors[4 * 3 + 2]);

    flic_close(&flic);
    remove(path);
}

void flic_read_frame_updates_color64_palette(void)
{
    const char* path = "color64.fli";
    uint8_t data[] = {
        1, 0,
        0,
        1,
        63, 31, 0
    };
    write_flic_with_frame_and_chunk(path, FLI_MAGIC_NUMBER, 4, 4, 10, FLI_COLOR_64_CHUNK, data, sizeof(data));

    Flic flic;
    TEST_ASSERT_TRUE(flic_open(&flic, path));

    uint16_t pixels[16];
    FlicFrame frame = {
        .pixels = pixels,
        .rowStride = 4,
    };
    memset(frame.colors, 0, sizeof(frame.colors));

    TEST_ASSERT_TRUE(flic_readFrame(&flic, &frame));
    TEST_ASSERT_EQUAL_UINT8(255, frame.colors[0]);
    TEST_ASSERT_EQUAL_UINT8(125, frame.colors[1]);
    TEST_ASSERT_EQUAL_UINT8(0, frame.colors[2]);

    flic_close(&flic);
    remove(path);
}

void flic_read_frame_decodes_delta_fli(void)
{
    const char* path = "delta_fli.fli";
    uint8_t data[] = {
        1, 0,
        2, 0,
        // Row 1
        1,
        1,
        2,
        100, 101,
        // Row 2
        1,
        0,
        0xFE,
        102
    };
    write_flic_with_frame_and_chunk(path, FLI_MAGIC_NUMBER, 4, 4, 10, FLI_LC_CHUNK, data, sizeof(data));

    Flic flic;
    TEST_ASSERT_TRUE(flic_open(&flic, path));

    uint16_t pixels[16];
    FlicFrame frame = {
        .pixels = pixels,
        .rowStride = 4,
    };
    memset(pixels, 0, sizeof(pixels));

    TEST_ASSERT_TRUE(flic_readFrame(&flic, &frame));
    for (int i = 0; i < 4; i++) TEST_ASSERT_EQUAL_UINT16(0, pixels[i]);
    TEST_ASSERT_EQUAL_UINT16(0, pixels[4]);
    TEST_ASSERT_EQUAL_UINT16(100 | FLI_PXL_CHANGE, pixels[5]);
    TEST_ASSERT_EQUAL_UINT16(101 | FLI_PXL_CHANGE, pixels[6]);
    TEST_ASSERT_EQUAL_UINT16(0, pixels[7]);
    TEST_ASSERT_EQUAL_UINT16(102 | FLI_PXL_CHANGE, pixels[8]);
    TEST_ASSERT_EQUAL_UINT16(102 | FLI_PXL_CHANGE, pixels[9]);
    TEST_ASSERT_EQUAL_UINT16(0, pixels[10]);
    TEST_ASSERT_EQUAL_UINT16(0, pixels[11]);
    for (int i = 12; i < 16; i++) TEST_ASSERT_EQUAL_UINT16(0, pixels[i]);

    flic_close(&flic);
    remove(path);
}

void flic_read_frame_decodes_delta_flc(void)
{
    const char* path = "delta_flc.fli";
    uint8_t data[] = {
        2, 0,
        // Row 0
        1, 0,
        0,
        1,
        200, 201,
        // Row 1
        1, 0,
        0,
        0xFE,
        202, 203
    };
    write_flic_with_frame_and_chunk(path, FLI_MAGIC_NUMBER, 4, 4, 10, FLI_DELTA_CHUNK, data, sizeof(data));

    Flic flic;
    TEST_ASSERT_TRUE(flic_open(&flic, path));

    uint16_t pixels[16];
    FlicFrame frame = {
        .pixels = pixels,
        .rowStride = 4,
    };
    memset(pixels, 0, sizeof(pixels));

    TEST_ASSERT_TRUE(flic_readFrame(&flic, &frame));
    TEST_ASSERT_EQUAL_UINT16(200 | FLI_PXL_CHANGE, pixels[0]);
    TEST_ASSERT_EQUAL_UINT16(201 | FLI_PXL_CHANGE, pixels[1]);
    TEST_ASSERT_EQUAL_UINT16(0, pixels[2]);
    TEST_ASSERT_EQUAL_UINT16(0, pixels[3]);
    TEST_ASSERT_EQUAL_UINT16(202 | FLI_PXL_CHANGE, pixels[4]);
    TEST_ASSERT_EQUAL_UINT16(203 | FLI_PXL_CHANGE, pixels[5]);
    TEST_ASSERT_EQUAL_UINT16(202 | FLI_PXL_CHANGE, pixels[6]);
    TEST_ASSERT_EQUAL_UINT16(203 | FLI_PXL_CHANGE, pixels[7]);

    flic_close(&flic);
    remove(path);
}

void flic_read_frame_sequential(void)
{
    const char* path = "sequential.flc";
    uint8_t frame1Data[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    uint8_t frame2Data[16] = {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2};

    write_flic_with_two_frames(path, 4, 4, FLI_COPY_CHUNK, frame1Data, 16, FLI_COPY_CHUNK, frame2Data, 16);

    Flic flic;
    TEST_ASSERT_TRUE(flic_open(&flic, path));
    TEST_ASSERT_EQUAL_UINT16(2, flic.frames);

    uint16_t pixels[16];
    FlicFrame frame = {
        .pixels = pixels,
        .rowStride = 4,
    };

    memset(pixels, 0, sizeof(pixels));
    TEST_ASSERT_TRUE(flic_readFrame(&flic, &frame));
    TEST_ASSERT_EQUAL_UINT16(15, flic.lastFrameDelay);
    for (int i = 0; i < 16; i++)
    {
        TEST_ASSERT_EQUAL_UINT16(1 | FLI_PXL_CHANGE, pixels[i]);
    }

    memset(pixels, 0, sizeof(pixels));
    TEST_ASSERT_TRUE(flic_readFrame(&flic, &frame));
    TEST_ASSERT_EQUAL_UINT16(25, flic.lastFrameDelay);
    for (int i = 0; i < 16; i++)
    {
        TEST_ASSERT_EQUAL_UINT16(2 | FLI_PXL_CHANGE, pixels[i]);
    }

    flic_close(&flic);
    remove(path);
}

void flic_make_image_handles_null_args(void)
{
    Flic flic = { .width = 4, .height = 4 };
    uint16_t pixels[16] = {0};
    FlicFrame frame = {
        .pixels = pixels,
        .rowStride = 4,
    };
    uint8_t image[48] = {0};

    flic_makeImage(NULL, &frame, image);
    flic_makeImage(&flic, NULL, image);
    flic_makeImage(&flic, &frame, NULL);

    FlicFrame frameNullPixels = {
        .pixels = NULL,
        .rowStride = 4,
    };
    flic_makeImage(&flic, &frameNullPixels, image);
}

void flic_make_image_respects_row_stride(void)
{
    Flic flic = { .width = 2, .height = 2 };
    uint16_t pixels[] = {
        FLI_PXL_CHANGE | 5, 0, 99, 99,
        FLI_PXL_CHANGE | 6, 0, 99, 99
    };
    FlicFrame frame = {
        .pixels = pixels,
        .rowStride = 4,
    };
    memset(frame.colors, 0, sizeof(frame.colors));
    frame.colors[5 * 3 + 0] = 50;
    frame.colors[5 * 3 + 1] = 51;
    frame.colors[5 * 3 + 2] = 52;
    frame.colors[6 * 3 + 0] = 60;
    frame.colors[6 * 3 + 1] = 61;
    frame.colors[6 * 3 + 2] = 62;

    uint8_t image[12];
    memset(image, 0xCC, sizeof(image));

    flic_makeImage(&flic, &frame, image);

    TEST_ASSERT_EQUAL_UINT8(50, image[0]);
    TEST_ASSERT_EQUAL_UINT8(51, image[1]);
    TEST_ASSERT_EQUAL_UINT8(52, image[2]);

    TEST_ASSERT_EQUAL_UINT8(0xCC, image[3]);
    TEST_ASSERT_EQUAL_UINT8(0xCC, image[4]);
    TEST_ASSERT_EQUAL_UINT8(0xCC, image[5]);

    TEST_ASSERT_EQUAL_UINT8(60, image[6]);
    TEST_ASSERT_EQUAL_UINT8(61, image[7]);
    TEST_ASSERT_EQUAL_UINT8(62, image[8]);

    TEST_ASSERT_EQUAL_UINT8(0xCC, image[9]);
    TEST_ASSERT_EQUAL_UINT8(0xCC, image[10]);
    TEST_ASSERT_EQUAL_UINT8(0xCC, image[11]);
}

void flic_make_image_skips_unchanged_pixels(void)
{
    Flic flic = { .width = 2, .height = 1 };
    uint16_t pixels[] = {
        FLI_PXL_CHANGE | 1,
        2
    };
    FlicFrame frame = {
        .pixels = pixels,
        .rowStride = 2,
    };
    memset(frame.colors, 0, sizeof(frame.colors));
    frame.colors[1 * 3 + 0] = 10;
    frame.colors[1 * 3 + 1] = 11;
    frame.colors[1 * 3 + 2] = 12;

    uint8_t image[6];
    memset(image, 0xAA, sizeof(image));

    flic_makeImage(&flic, &frame, image);

    TEST_ASSERT_EQUAL_UINT8(10, image[0]);
    TEST_ASSERT_EQUAL_UINT8(11, image[1]);
    TEST_ASSERT_EQUAL_UINT8(12, image[2]);

    TEST_ASSERT_EQUAL_UINT8(0xAA, image[3]);
    TEST_ASSERT_EQUAL_UINT8(0xAA, image[4]);
    TEST_ASSERT_EQUAL_UINT8(0xAA, image[5]);
}

void flic_close_handles_null(void)
{
    flic_close(NULL);
}

void flic_rewind_resets_playback(void)
{
    const char* path = "rewind.flc";
    uint8_t frame1Data[16] = {5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5};
    uint8_t frame2Data[16] = {6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6};

    write_flic_with_two_frames(path, 4, 4, FLI_COPY_CHUNK, frame1Data, 16, FLI_COPY_CHUNK, frame2Data, 16);

    Flic flic;
    TEST_ASSERT_TRUE(flic_open(&flic, path));

    uint16_t pixels[16];
    FlicFrame frame = {
        .pixels = pixels,
        .rowStride = 4,
    };

    TEST_ASSERT_TRUE(flic_readFrame(&flic, &frame));
    TEST_ASSERT_EQUAL_UINT32(1, flic.currentFrame);
    TEST_ASSERT_EQUAL_UINT16(5 | FLI_PXL_CHANGE, pixels[0]);

    flic_rewind(&flic);
    TEST_ASSERT_EQUAL_UINT32(0, flic.currentFrame);

    memset(pixels, 0, sizeof(pixels));
    TEST_ASSERT_TRUE(flic_readFrame(&flic, &frame));
    TEST_ASSERT_EQUAL_UINT16(5 | FLI_PXL_CHANGE, pixels[0]);

    flic_close(&flic);
    remove(path);
}

void flic_read_frame_skips_unknown_chunk_type(void)
{
    const char* path = "unknown_chunk.fli";
    write_flic_with_unknown_and_black_chunk(path);

    Flic flic;
    TEST_ASSERT_TRUE(flic_open(&flic, path));

    uint16_t pixels[16];
    FlicFrame frame = {
        .pixels = pixels,
        .rowStride = 4,
    };
    memset(pixels, 0, sizeof(pixels));

    TEST_ASSERT_TRUE(flic_readFrame(&flic, &frame));
    for (int i = 0; i < 16; i++)
    {
        TEST_ASSERT_EQUAL_UINT16(FLI_PXL_CHANGE, pixels[i]);
    }

    flic_close(&flic);
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
    RUN_TEST(flic_open_fails_for_missing_file);
    RUN_TEST(flic_open_fails_for_invalid_magic);
    RUN_TEST(flic_open_applies_default_dimensions);
    RUN_TEST(flic_open_reads_flc_frame_offsets);
    RUN_TEST(flic_make_image_applies_changed_pixels_only);
    RUN_TEST(flic_open_fails_for_null_args);
    RUN_TEST(flic_open_converts_fli_speed_to_milliseconds);
    RUN_TEST(flic_read_frame_returns_false_for_bad_magic);
    RUN_TEST(flic_read_frame_decodes_black_chunk);
    RUN_TEST(flic_read_frame_decodes_flic_copy_chunk);
    RUN_TEST(flic_read_frame_decodes_byte_run_replicate);
    RUN_TEST(flic_read_frame_decodes_byte_run_literal);
    RUN_TEST(flic_read_frame_updates_color256_palette);
    RUN_TEST(flic_read_frame_updates_color64_palette);
    RUN_TEST(flic_read_frame_decodes_delta_fli);
    RUN_TEST(flic_read_frame_decodes_delta_flc);
    RUN_TEST(flic_read_frame_sequential);
    RUN_TEST(flic_make_image_handles_null_args);
    RUN_TEST(flic_make_image_respects_row_stride);
    RUN_TEST(flic_make_image_skips_unchanged_pixels);
    RUN_TEST(flic_close_handles_null);
    RUN_TEST(flic_rewind_resets_playback);
    RUN_TEST(flic_read_frame_skips_unknown_chunk_type);
    return UNITY_END();
}
