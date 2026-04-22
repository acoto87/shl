#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#define SHL_WAVE_WRITER_IMPLEMENTATION
#include "../wave_writer.h"
#include "test_common.h"

#define PI 3.14159265359f

static void writeSamples(const char* path, bool stereo)
{
    shlWaveFile waveFile = {0};

    long sampleRate = 44100;
    shl_sample_t buffer[] = { 0, 1000, -1000, 2000 };

    TEST_ASSERT_TRUE(shlWaveInit(&waveFile, sampleRate, path));
    if (stereo)
        shlWaveStereo(&waveFile, true);

    TEST_ASSERT_TRUE(shlWaveWrite(&waveFile, buffer, (long)(sizeof(buffer) / sizeof(buffer[0])), 1));
    TEST_ASSERT_EQUAL_INT(sizeof(buffer) / sizeof(buffer[0]), shlWaveSampleCount(&waveFile));
    TEST_ASSERT_TRUE(shlWaveFlush(&waveFile, true));
    TEST_ASSERT_NULL(waveFile._buffer);
    TEST_ASSERT_NULL(waveFile._file);
}

static uint32_t readU32LE(const uint8_t* data)
{
    return ((uint32_t)data[3] << 24) | ((uint32_t)data[2] << 16) | ((uint32_t)data[1] << 8) | (uint32_t)data[0];
}

void waveWriterCreatesMonoFile(void)
{
    const char* path = "wave_test_mono.wav";
    writeSamples(path, false);

    FILE* file = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(file);

    uint8_t header[SHL_WAVE_HEADER_SIZE];
    TEST_ASSERT_EQUAL_size_t(sizeof(header), fread(header, 1, sizeof(header), file));
    fclose(file);

    TEST_ASSERT_EQUAL_MEMORY("RIFF", header, 4);
    TEST_ASSERT_EQUAL_MEMORY("WAVE", header + 8, 4);
    TEST_ASSERT_EQUAL_MEMORY("fmt ", header + 12, 4);
    TEST_ASSERT_EQUAL_UINT8(1, header[22]);  /* channel count */
    TEST_ASSERT_EQUAL_UINT8(16, header[34]); /* bits per sample */

    remove(path);
}

void waveWriterCreatesStereoHeader(void)
{
    const char* path = "wave_test_stereo.wav";
    writeSamples(path, true);

    FILE* file = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(file);

    uint8_t header[SHL_WAVE_HEADER_SIZE];
    TEST_ASSERT_EQUAL_size_t(sizeof(header), fread(header, 1, sizeof(header), file));
    fclose(file);

    TEST_ASSERT_EQUAL_UINT8(2, header[22]); /* channel count */
    TEST_ASSERT_EQUAL_UINT8(4, header[32]); /* block align (channels * sizeof(short)) */

    remove(path);
}

void waveWriterInitFailsForMissingDirectory(void)
{
    shlWaveFile waveFile = {0};
    TEST_ASSERT_FALSE(shlWaveInit(&waveFile, 44100, "missing-dir/out.wav"));
    TEST_ASSERT_NULL(waveFile._buffer);
}

void waveWriterFlushesHeaderWithExpectedDataSize(void)
{
    const char* path = "wave_test_datasize.wav";
    shlWaveFile waveFile = {0};
    shl_sample_t buffer[] = { 100, 200, 300, 400, 500, 600 };

    TEST_ASSERT_TRUE(shlWaveInit(&waveFile, 22050, path));
    TEST_ASSERT_TRUE(shlWaveWrite(&waveFile, buffer, 6, 1));
    TEST_ASSERT_TRUE(shlWaveFlush(&waveFile, true));

    FILE* file = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(file);
    uint8_t header[SHL_WAVE_HEADER_SIZE];
    TEST_ASSERT_EQUAL_size_t(sizeof(header), fread(header, 1, sizeof(header), file));
    fclose(file);

    TEST_ASSERT_EQUAL_UINT32(6u * sizeof(shl_sample_t), readU32LE(header + 40));
    TEST_ASSERT_EQUAL_UINT32((SHL_WAVE_HEADER_SIZE - 8) + 6u * sizeof(shl_sample_t), readU32LE(header + 4));
    remove(path);
}

void waveWriterStressMultipleWritesAccumulateSampleCount(void)
{
    const char* path = "wave_test_stress.wav";
    shlWaveFile waveFile = {0};
    shl_sample_t buffer[64];

    for (int i = 0; i < 64; i++)
    {
        buffer[i] = (shl_sample_t)(i * 10);
    }

    TEST_ASSERT_TRUE(shlWaveInit(&waveFile, 44100, path));
    for (int i = 0; i < SHL_TEST_MEDIUM_COUNT; i++)
    {
        TEST_ASSERT_TRUE(shlWaveWrite(&waveFile, buffer, 64, 1));
    }

    TEST_ASSERT_EQUAL_INT(SHL_TEST_MEDIUM_COUNT * 64, shlWaveSampleCount(&waveFile));
    TEST_ASSERT_TRUE(shlWaveFlush(&waveFile, true));
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
    RUN_TEST(waveWriterCreatesMonoFile);
    RUN_TEST(waveWriterCreatesStereoHeader);
    RUN_TEST(waveWriterInitFailsForMissingDirectory);
    RUN_TEST(waveWriterFlushesHeaderWithExpectedDataSize);
    RUN_TEST(waveWriterStressMultipleWritesAccumulateSampleCount);
    return UNITY_END();
}
