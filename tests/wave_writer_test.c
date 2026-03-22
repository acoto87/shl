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

void waveWriterCreatesMonoFile(void)
{
    const char* path = "wave_test_mono.wav";
    writeSamples(path, false);

    FILE* file = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(file);

    uint8_t header[SHL_WAVE_HEADER_SIZE];
    TEST_ASSERT_EQUAL_UINT32(sizeof(header), fread(header, 1, sizeof(header), file));
    fclose(file);

    TEST_ASSERT_EQUAL_MEMORY("RIFF", header, 4);
    TEST_ASSERT_EQUAL_MEMORY("WAVE", header + 8, 4);
    TEST_ASSERT_EQUAL_MEMORY("fmt ", header + 12, 4);
    TEST_ASSERT_EQUAL_UINT8(1, header[22]);
    TEST_ASSERT_EQUAL_UINT8(16, header[34]);

    remove(path);
}

void waveWriterCreatesStereoHeader(void)
{
    const char* path = "wave_test_stereo.wav";
    writeSamples(path, true);

    FILE* file = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(file);

    uint8_t header[SHL_WAVE_HEADER_SIZE];
    TEST_ASSERT_EQUAL_UINT32(sizeof(header), fread(header, 1, sizeof(header), file));
    fclose(file);

    TEST_ASSERT_EQUAL_UINT8(2, header[22]);
    TEST_ASSERT_EQUAL_UINT8(4, header[32]);

    remove(path);
}

void waveWriterInitFailsForMissingDirectory(void)
{
    shlWaveFile waveFile = {0};
    TEST_ASSERT_FALSE(shlWaveInit(&waveFile, 44100, "missing-dir/out.wav"));
    TEST_ASSERT_NULL(waveFile._buffer);
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
    return UNITY_END();
}
