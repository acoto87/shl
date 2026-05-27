/*
    tests/wav_test.c — comprehensive test suite for wav.h

    Covers:
      - mw_read_memory / mw_read_file   (valid, malformed, edge cases)
      - mw_write_memory / mw_write_file (roundtrip, NULL safety)
      - mw_resample_pcm                 (identity, fast paths 2x/4x up/down,
                                         fallback path, NULL inputs)
      - mw_free_buffer                  (NULL safety)
      - Allocator balance tracking      (leak detection)

    Build (from repo root, requires Unity):
        gcc -std=c99 -Wall -I. -Itests -Itests/vendor/unity/src \
            tests/wav_test.c tests/vendor/unity/src/unity.c -lm \
            -o build/default/wav_test
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* -------------------------------------------------------------------------
   Custom allocator wrappers for leak tracking
   ---------------------------------------------------------------------- */
static int g_alloc_count = 0;

static void* tracked_malloc(size_t sz) {
    if (sz == 0) return NULL;
    g_alloc_count++;
    return malloc(sz);
}
static void* tracked_realloc(void* ptr, size_t sz) {
    /* Count new allocations; do not touch counter for resize-in-place */
    if (!ptr && sz > 0) g_alloc_count++;
    if (ptr  && sz == 0) g_alloc_count--;
    return realloc(ptr, sz);
}
static void tracked_free(void* ptr) {
    if (ptr) g_alloc_count--;
    free(ptr);
}

#define MINIWAVE_MALLOC(sz)         tracked_malloc(sz)
#define MINIWAVE_REALLOC(ptr, size) tracked_realloc(ptr, size)
#define MINIWAVE_FREE(ptr)          tracked_free(ptr)

#define MINIWAVE_IMPLEMENTATION
#include "../wav.h"
#include "test_common.h"

/* -------------------------------------------------------------------------
   Test file names
   ---------------------------------------------------------------------- */
#define TEST_MONO8_FILE    "wav_test_mono8.wav"
#define TEST_STEREO16_FILE "wav_test_stereo16.wav"

/* =========================================================================
   Helpers
   ======================================================================= */

/* Build a heap-allocated audio buffer filled with a predictable ramp */
static void make_audio(mw_audio_buffer* audio,
                       uint32_t channels, uint32_t rate,
                       uint32_t bps, uint32_t sample_count)
{
    audio->channels        = channels;
    audio->sample_rate     = rate;
    audio->bits_per_sample = bps;
    audio->data_length     = sample_count * channels * (bps / 8);
    audio->data            = (uint8_t*)MINIWAVE_MALLOC(audio->data_length);
    TEST_ASSERT_NOT_NULL(audio->data);
    for (uint32_t i = 0; i < audio->data_length; i++)
        audio->data[i] = (uint8_t)(i % 255);
}

/* Build a minimal in-memory WAV blob containing raw PCM `data` */
static uint8_t* build_wav_blob(uint32_t channels, uint32_t rate,
                               uint32_t bps, const uint8_t* pcm,
                               uint32_t pcm_len, size_t* out_size)
{
    mw_audio_buffer audio = {
        channels, rate, bps, pcm_len, (uint8_t*)(uintptr_t)pcm
    };
    return (uint8_t*)mw_write_memory(&audio, out_size);
}

/* =========================================================================
   setUp / tearDown
   ======================================================================= */
void setUp(void)    { /* nothing per-test */ }
void tearDown(void) { /* nothing per-test */ }

/* =========================================================================
   mw_read_memory — happy path
   ======================================================================= */

void test_ReadMemory_Mono8Bit_Roundtrip(void)
{
    mw_audio_buffer original = {0};
    make_audio(&original, 1, 11025, 8, 100);

    size_t blob_size = 0;
    uint8_t* blob = (uint8_t*)mw_write_memory(&original, &blob_size);
    TEST_ASSERT_NOT_NULL(blob);
    TEST_ASSERT_GREATER_THAN(original.data_length, (uint32_t)blob_size);

    mw_audio_buffer parsed = {0};
    TEST_ASSERT_EQUAL_INT(1, mw_read_memory(blob, blob_size, &parsed));
    TEST_ASSERT_EQUAL_UINT32(original.channels,        parsed.channels);
    TEST_ASSERT_EQUAL_UINT32(original.sample_rate,     parsed.sample_rate);
    TEST_ASSERT_EQUAL_UINT32(original.bits_per_sample, parsed.bits_per_sample);
    TEST_ASSERT_EQUAL_UINT32(original.data_length,     parsed.data_length);
    TEST_ASSERT_EQUAL_MEMORY(original.data, parsed.data, original.data_length);

    mw_free_buffer(&original);
    mw_free_buffer(&parsed);
    MINIWAVE_FREE(blob);
}

void test_ReadMemory_Stereo16Bit_Roundtrip(void)
{
    mw_audio_buffer original = {0};
    make_audio(&original, 2, 44100, 16, 200);

    size_t blob_size = 0;
    uint8_t* blob = (uint8_t*)mw_write_memory(&original, &blob_size);
    TEST_ASSERT_NOT_NULL(blob);

    mw_audio_buffer parsed = {0};
    TEST_ASSERT_EQUAL_INT(1, mw_read_memory(blob, blob_size, &parsed));
    TEST_ASSERT_EQUAL_UINT32(2,     parsed.channels);
    TEST_ASSERT_EQUAL_UINT32(44100, parsed.sample_rate);
    TEST_ASSERT_EQUAL_UINT32(16,    parsed.bits_per_sample);
    TEST_ASSERT_EQUAL_MEMORY(original.data, parsed.data, original.data_length);

    mw_free_buffer(&original);
    mw_free_buffer(&parsed);
    MINIWAVE_FREE(blob);
}

void test_ReadMemory_Mono16Bit_Roundtrip(void)
{
    mw_audio_buffer original = {0};
    make_audio(&original, 1, 22050, 16, 64);

    size_t blob_size = 0;
    uint8_t* blob = (uint8_t*)mw_write_memory(&original, &blob_size);
    TEST_ASSERT_NOT_NULL(blob);

    mw_audio_buffer parsed = {0};
    TEST_ASSERT_EQUAL_INT(1, mw_read_memory(blob, blob_size, &parsed));
    TEST_ASSERT_EQUAL_UINT32(1,     parsed.channels);
    TEST_ASSERT_EQUAL_UINT32(22050, parsed.sample_rate);
    TEST_ASSERT_EQUAL_UINT32(16,    parsed.bits_per_sample);
    TEST_ASSERT_EQUAL_MEMORY(original.data, parsed.data, original.data_length);

    mw_free_buffer(&original);
    mw_free_buffer(&parsed);
    MINIWAVE_FREE(blob);
}

void test_ReadMemory_SkipsExtraMetadataChunk(void)
{
    /* Build a WAV with a synthetic LIST chunk inserted before the data chunk */
    uint8_t pcm[8] = { 10, 20, 30, 40, 50, 60, 70, 80 };

    /* Manually craft: RIFF header + fmt chunk + LIST chunk + data chunk */
    uint8_t list_chunk[] = {
        'L','I','S','T',
        0x04, 0x00, 0x00, 0x00,   /* 4-byte payload */
        'I','N','F','O'
    };
    uint32_t list_sz = (uint32_t)sizeof(list_chunk);
    uint32_t data_sz = (uint32_t)sizeof(pcm);
    uint32_t fmt_length = 16;
    /* overall_size = 4 (WAVE) + 8+16 (fmt) + list_sz + 8 + data_sz */
    uint32_t overall   = 4 + 8 + fmt_length + list_sz + 8 + data_sz;

    size_t total = 8 + overall;
    uint8_t* buf = (uint8_t*)malloc(total);
    TEST_ASSERT_NOT_NULL(buf);

    uint8_t* p = buf;
    memcpy(p, "RIFF", 4);           p += 4;
    *(uint32_t*)p = overall;        p += 4;
    memcpy(p, "WAVE", 4);           p += 4;
    memcpy(p, "fmt ", 4);           p += 4;
    *(uint32_t*)p = fmt_length;     p += 4;
    *(uint16_t*)p = 1;              p += 2;  /* PCM */
    *(uint16_t*)p = 1;              p += 2;  /* mono */
    *(uint32_t*)p = 11025;          p += 4;  /* sample rate */
    *(uint32_t*)p = 11025;          p += 4;  /* byte rate */
    *(uint16_t*)p = 1;              p += 2;  /* block align */
    *(uint16_t*)p = 8;              p += 2;  /* bits */
    memcpy(p, list_chunk, list_sz); p += list_sz;
    memcpy(p, "data", 4);           p += 4;
    *(uint32_t*)p = data_sz;        p += 4;
    memcpy(p, pcm, data_sz);

    mw_audio_buffer parsed = {0};
    TEST_ASSERT_EQUAL_INT(1, mw_read_memory(buf, total, &parsed));
    TEST_ASSERT_EQUAL_UINT32(data_sz, parsed.data_length);
    TEST_ASSERT_EQUAL_MEMORY(pcm, parsed.data, data_sz);

    mw_free_buffer(&parsed);
    free(buf);
}

/* =========================================================================
   mw_read_memory — failure paths
   ======================================================================= */

void test_ReadMemory_Null_Buffer_Returns0(void)
{
    mw_audio_buffer out = {0};
    TEST_ASSERT_EQUAL_INT(0, mw_read_memory(NULL, 64, &out));
    TEST_ASSERT_NULL(out.data);
}

void test_ReadMemory_Null_OutAudio_Returns0(void)
{
    uint8_t dummy[64] = {0};
    TEST_ASSERT_EQUAL_INT(0, mw_read_memory(dummy, sizeof(dummy), NULL));
}

void test_ReadMemory_TooSmall_Returns0(void)
{
    uint8_t tiny[10] = {0};
    mw_audio_buffer out = {0};
    TEST_ASSERT_EQUAL_INT(0, mw_read_memory(tiny, sizeof(tiny), &out));
    TEST_ASSERT_NULL(out.data);
}

void test_ReadMemory_GarbageHeader_Returns0(void)
{
    uint8_t garbage[128];
    memset(garbage, 0xFF, sizeof(garbage));

    mw_audio_buffer out = {0};
    TEST_ASSERT_EQUAL_INT(0, mw_read_memory(garbage, sizeof(garbage), &out));
    TEST_ASSERT_NULL(out.data);
}

void test_ReadMemory_CorrectRIFFButMissingWAVE_Returns0(void)
{
    uint8_t buf[64];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, "RIFF", 4);
    memcpy(buf + 8, "JUNK", 4); /* wrong form type */

    mw_audio_buffer out = {0};
    TEST_ASSERT_EQUAL_INT(0, mw_read_memory(buf, sizeof(buf), &out));
}

/* =========================================================================
   mw_write_memory — failure paths
   ======================================================================= */

void test_WriteMemory_Null_Audio_ReturnsNull(void)
{
    size_t sz = 0;
    TEST_ASSERT_NULL(mw_write_memory(NULL, &sz));
}

void test_WriteMemory_Null_OutSize_ReturnsNull(void)
{
    mw_audio_buffer audio = {0};
    uint8_t dummy[4] = {0};
    audio.data = dummy;
    audio.data_length = 4;
    TEST_ASSERT_NULL(mw_write_memory(&audio, NULL));
}

void test_WriteMemory_Null_Data_ReturnsNull(void)
{
    mw_audio_buffer audio = { 1, 11025, 8, 100, NULL };
    size_t sz = 0;
    TEST_ASSERT_NULL(mw_write_memory(&audio, &sz));
}

void test_WriteMemory_SizeEqualsHeaderPlusPayload(void)
{
    mw_audio_buffer audio = {0};
    make_audio(&audio, 1, 11025, 8, 50);

    size_t expected = sizeof(mw_wav_header) + audio.data_length;
    size_t actual = 0;
    uint8_t* blob = (uint8_t*)mw_write_memory(&audio, &actual);
    TEST_ASSERT_NOT_NULL(blob);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)expected, (uint32_t)actual);

    mw_free_buffer(&audio);
    MINIWAVE_FREE(blob);
}

/* =========================================================================
   mw_read_file / mw_write_file
   ======================================================================= */

void test_WriteAndReadFile_Mono8Bit_Roundtrip(void)
{
    mw_audio_buffer original = {0};
    make_audio(&original, 1, 11025, 8, 100);

    TEST_ASSERT_EQUAL_INT(1, mw_write_file(TEST_MONO8_FILE, &original));

    mw_audio_buffer parsed = {0};
    TEST_ASSERT_EQUAL_INT(1, mw_read_file(TEST_MONO8_FILE, &parsed));
    TEST_ASSERT_EQUAL_UINT32(original.channels,        parsed.channels);
    TEST_ASSERT_EQUAL_UINT32(original.sample_rate,     parsed.sample_rate);
    TEST_ASSERT_EQUAL_UINT32(original.bits_per_sample, parsed.bits_per_sample);
    TEST_ASSERT_EQUAL_MEMORY(original.data, parsed.data, original.data_length);

    mw_free_buffer(&original);
    mw_free_buffer(&parsed);
    remove(TEST_MONO8_FILE);
}

void test_WriteAndReadFile_Stereo16Bit_Roundtrip(void)
{
    mw_audio_buffer original = {0};
    make_audio(&original, 2, 44100, 16, 200);

    TEST_ASSERT_EQUAL_INT(1, mw_write_file(TEST_STEREO16_FILE, &original));

    mw_audio_buffer parsed = {0};
    TEST_ASSERT_EQUAL_INT(1, mw_read_file(TEST_STEREO16_FILE, &parsed));
    TEST_ASSERT_EQUAL_UINT32(2,     parsed.channels);
    TEST_ASSERT_EQUAL_UINT32(44100, parsed.sample_rate);
    TEST_ASSERT_EQUAL_UINT32(16,    parsed.bits_per_sample);
    TEST_ASSERT_EQUAL_MEMORY(original.data, parsed.data, original.data_length);

    mw_free_buffer(&original);
    mw_free_buffer(&parsed);
    remove(TEST_STEREO16_FILE);
}

void test_ReadFile_NonExistent_Returns0(void)
{
    mw_audio_buffer out = {0};
    TEST_ASSERT_EQUAL_INT(0, mw_read_file("__no_such_file__.wav", &out));
    TEST_ASSERT_NULL(out.data);
}

/* =========================================================================
   mw_free_buffer — NULL safety
   ======================================================================= */

void test_FreeBuffer_NullAudio_DoesNotCrash(void)
{
    mw_free_buffer(NULL);   /* must not crash */
    TEST_PASS();
}

void test_FreeBuffer_ZeroInitialised_DoesNotCrash(void)
{
    mw_audio_buffer empty = {0};
    mw_free_buffer(&empty);
    TEST_ASSERT_NULL(empty.data);
}

void test_FreeBuffer_SetsDataToNull(void)
{
    mw_audio_buffer audio = {0};
    make_audio(&audio, 1, 11025, 8, 4);
    TEST_ASSERT_NOT_NULL(audio.data);
    mw_free_buffer(&audio);
    TEST_ASSERT_NULL(audio.data);
    TEST_ASSERT_EQUAL_UINT32(0, audio.data_length);
}

/* =========================================================================
   mw_resample_pcm — NULL / zero-rate guard
   ======================================================================= */

void test_Resample_NullSrc_Returns0(void)
{
    mw_audio_buffer dst = {0};
    TEST_ASSERT_EQUAL_INT(0, mw_resample_pcm(NULL, &dst, 44100));
}

void test_Resample_NullDst_Returns0(void)
{
    mw_audio_buffer src = {0};
    make_audio(&src, 1, 11025, 8, 4);
    TEST_ASSERT_EQUAL_INT(0, mw_resample_pcm(&src, NULL, 44100));
    mw_free_buffer(&src);
}

void test_Resample_ZeroRate_Returns0(void)
{
    mw_audio_buffer src = {0};
    make_audio(&src, 1, 11025, 8, 4);
    mw_audio_buffer dst = {0};
    TEST_ASSERT_EQUAL_INT(0, mw_resample_pcm(&src, &dst, 0));
    mw_free_buffer(&src);
}

void test_Resample_NullData_Returns0(void)
{
    mw_audio_buffer src = { 1, 11025, 8, 100, NULL };
    mw_audio_buffer dst = {0};
    TEST_ASSERT_EQUAL_INT(0, mw_resample_pcm(&src, &dst, 22050));
}

/* =========================================================================
   mw_resample_pcm — identity (same rate)
   ======================================================================= */

void test_Resample_Identity_CopiesExactly(void)
{
    mw_audio_buffer src = {0};
    make_audio(&src, 1, 44100, 8, 64);

    mw_audio_buffer dst = {0};
    TEST_ASSERT_EQUAL_INT(1, mw_resample_pcm(&src, &dst, 44100));
    TEST_ASSERT_EQUAL_UINT32(44100, dst.sample_rate);
    TEST_ASSERT_EQUAL_UINT32(src.data_length, dst.data_length);
    TEST_ASSERT_EQUAL_MEMORY(src.data, dst.data, src.data_length);

    mw_free_buffer(&src);
    mw_free_buffer(&dst);
}

/* =========================================================================
   mw_resample_pcm — fast path: 2x upsample (8-bit mono)
   ======================================================================= */

void test_Resample_FastPath_Upsample2x_Interpolation(void)
{
    mw_audio_buffer src = {0};
    src.channels        = 1;
    src.sample_rate     = 11025;
    src.bits_per_sample = 8;
    src.data_length     = 4;
    src.data = (uint8_t*)MINIWAVE_MALLOC(4);
    src.data[0] = 0;
    src.data[1] = 100;
    src.data[2] = 200;
    src.data[3] = 200;

    mw_audio_buffer dst = {0};
    TEST_ASSERT_EQUAL_INT(1, mw_resample_pcm(&src, &dst, 22050));
    TEST_ASSERT_EQUAL_UINT32(22050, dst.sample_rate);
    TEST_ASSERT_EQUAL_UINT32(8, dst.data_length); /* 4 * 2 */

    /* First pair: a=0, b=100 → 0, 50 */
    TEST_ASSERT_EQUAL_UINT8(0,   dst.data[0]);
    TEST_ASSERT_EQUAL_UINT8(50,  dst.data[1]);
    /* Second pair: a=100, b=200 → 100, 150 */
    TEST_ASSERT_EQUAL_UINT8(100, dst.data[2]);
    TEST_ASSERT_EQUAL_UINT8(150, dst.data[3]);

    mw_free_buffer(&src);
    mw_free_buffer(&dst);
}

/* =========================================================================
   mw_resample_pcm — fast path: 4x upsample (8-bit mono)
   ======================================================================= */

void test_Resample_FastPath_Upsample4x_Interpolation(void)
{
    mw_audio_buffer src = {0};
    src.channels        = 1;
    src.sample_rate     = 11025;
    src.bits_per_sample = 8;
    src.data_length     = 3;
    src.data = (uint8_t*)MINIWAVE_MALLOC(3);
    src.data[0] = 10;
    src.data[1] = 50;   /* diff = 40 */
    src.data[2] = 90;   /* diff = 40 */

    mw_audio_buffer dst = {0};
    TEST_ASSERT_EQUAL_INT(1, mw_resample_pcm(&src, &dst, 44100));
    TEST_ASSERT_EQUAL_UINT32(44100, dst.sample_rate);
    TEST_ASSERT_EQUAL_UINT32(12, dst.data_length); /* 3 * 4 */

    /* Segment [10,50]: 10, 10+(40*1)>>2=20, 10+(40*2)>>2=30, 10+(40*3)>>2=40 */
    TEST_ASSERT_EQUAL_UINT8(10, dst.data[0]);
    TEST_ASSERT_EQUAL_UINT8(20, dst.data[1]);
    TEST_ASSERT_EQUAL_UINT8(30, dst.data[2]);
    TEST_ASSERT_EQUAL_UINT8(40, dst.data[3]);
    /* Segment [50,90] starts with 50 */
    TEST_ASSERT_EQUAL_UINT8(50, dst.data[4]);

    mw_free_buffer(&src);
    mw_free_buffer(&dst);
}

/* =========================================================================
   mw_resample_pcm — fast path: 2x downsample (8-bit mono)
   ======================================================================= */

void test_Resample_FastPath_Downsample2x_Averaging(void)
{
    mw_audio_buffer src = {0};
    src.channels        = 1;
    src.sample_rate     = 22050;
    src.bits_per_sample = 8;
    src.data_length     = 4;
    src.data = (uint8_t*)MINIWAVE_MALLOC(4);
    src.data[0] = 10; src.data[1] = 20; /* avg = 15 */
    src.data[2] = 40; src.data[3] = 60; /* avg = 50 */

    mw_audio_buffer dst = {0};
    TEST_ASSERT_EQUAL_INT(1, mw_resample_pcm(&src, &dst, 11025));
    TEST_ASSERT_EQUAL_UINT32(11025, dst.sample_rate);
    TEST_ASSERT_EQUAL_UINT32(2, dst.data_length);
    TEST_ASSERT_EQUAL_UINT8(15, dst.data[0]);
    TEST_ASSERT_EQUAL_UINT8(50, dst.data[1]);

    mw_free_buffer(&src);
    mw_free_buffer(&dst);
}

/* =========================================================================
   mw_resample_pcm — fast path: 4x downsample (8-bit mono)
   ======================================================================= */

void test_Resample_FastPath_Downsample4x_Averaging(void)
{
    mw_audio_buffer src = {0};
    src.channels        = 1;
    src.sample_rate     = 44100;
    src.bits_per_sample = 8;
    src.data_length     = 8;
    src.data = (uint8_t*)MINIWAVE_MALLOC(8);
    /* group 0: 0+4+8+12 = 24, avg=6  */
    src.data[0] = 0; src.data[1] = 4; src.data[2] = 8; src.data[3] = 12;
    /* group 1: 20+40+60+80 = 200, avg=50 */
    src.data[4] = 20; src.data[5] = 40; src.data[6] = 60; src.data[7] = 80;

    mw_audio_buffer dst = {0};
    TEST_ASSERT_EQUAL_INT(1, mw_resample_pcm(&src, &dst, 11025));
    TEST_ASSERT_EQUAL_UINT32(11025, dst.sample_rate);
    TEST_ASSERT_EQUAL_UINT32(2, dst.data_length);
    TEST_ASSERT_EQUAL_UINT8(6,  dst.data[0]);
    TEST_ASSERT_EQUAL_UINT8(50, dst.data[1]);

    mw_free_buffer(&src);
    mw_free_buffer(&dst);
}

/* =========================================================================
   mw_resample_pcm — fallback path: arbitrary ratio, 8-bit mono
   ======================================================================= */

void test_Resample_Fallback_Mono8_ArbitraryRatio(void)
{
    mw_audio_buffer src = {0};
    make_audio(&src, 1, 11025, 8, 64);

    mw_audio_buffer dst = {0};
    TEST_ASSERT_EQUAL_INT(1, mw_resample_pcm(&src, &dst, 8000));
    TEST_ASSERT_EQUAL_UINT32(8000, dst.sample_rate);
    TEST_ASSERT_EQUAL_UINT32(1,    dst.channels);
    TEST_ASSERT_EQUAL_UINT32(8,    dst.bits_per_sample);
    /* output should be smaller: 64 * (8000/11025) ≈ 46 samples */
    TEST_ASSERT_TRUE(dst.data_length < src.data_length);
    TEST_ASSERT_GREATER_THAN(0u, dst.data_length);

    mw_free_buffer(&src);
    mw_free_buffer(&dst);
}

/* =========================================================================
   mw_resample_pcm — fallback path: 16-bit stereo
   ======================================================================= */

void test_Resample_Fallback_Stereo16_Downsample(void)
{
    mw_audio_buffer src = {0};
    make_audio(&src, 2, 44100, 16, 100);

    mw_audio_buffer dst = {0};
    TEST_ASSERT_EQUAL_INT(1, mw_resample_pcm(&src, &dst, 32000));
    TEST_ASSERT_EQUAL_UINT32(32000, dst.sample_rate);
    TEST_ASSERT_EQUAL_UINT32(2,     dst.channels);
    TEST_ASSERT_EQUAL_UINT32(16,    dst.bits_per_sample);
    TEST_ASSERT_TRUE(dst.data_length < src.data_length);

    mw_free_buffer(&src);
    mw_free_buffer(&dst);
}

void test_Resample_Fallback_Mono16_Upsample(void)
{
    mw_audio_buffer src = {0};
    make_audio(&src, 1, 22050, 16, 32);

    mw_audio_buffer dst = {0};
    TEST_ASSERT_EQUAL_INT(1, mw_resample_pcm(&src, &dst, 44100));
    TEST_ASSERT_EQUAL_UINT32(44100, dst.sample_rate);
    TEST_ASSERT_EQUAL_UINT32(1,     dst.channels);
    TEST_ASSERT_EQUAL_UINT32(16,    dst.bits_per_sample);
    TEST_ASSERT_TRUE(dst.data_length > src.data_length);

    mw_free_buffer(&src);
    mw_free_buffer(&dst);
}

/* =========================================================================
   mw_resample_pcm — output metadata preserved
   ======================================================================= */

void test_Resample_PreservesChannelsAndBits(void)
{
    mw_audio_buffer src = {0};
    make_audio(&src, 2, 44100, 16, 64);

    mw_audio_buffer dst = {0};
    mw_resample_pcm(&src, &dst, 22050);
    TEST_ASSERT_EQUAL_UINT32(src.channels,        dst.channels);
    TEST_ASSERT_EQUAL_UINT32(src.bits_per_sample, dst.bits_per_sample);
    TEST_ASSERT_EQUAL_UINT32(22050,               dst.sample_rate);

    mw_free_buffer(&src);
    mw_free_buffer(&dst);
}

/* =========================================================================
   Header field validation (write-memory round-trip)
   ======================================================================= */

void test_WriteMemory_HeaderFieldsAreCorrect(void)
{
    mw_audio_buffer audio = {0};
    make_audio(&audio, 2, 44100, 16, 10);  /* 10 stereo 16-bit samples */

    size_t sz = 0;
    const mw_wav_header* hdr = (const mw_wav_header*)mw_write_memory(&audio, &sz);
    TEST_ASSERT_NOT_NULL(hdr);

    TEST_ASSERT_EQUAL_MEMORY("RIFF", hdr->riff_marker, 4);
    TEST_ASSERT_EQUAL_MEMORY("WAVE", hdr->wave_marker, 4);
    TEST_ASSERT_EQUAL_MEMORY("fmt ", hdr->fmt_marker,  4);
    TEST_ASSERT_EQUAL_MEMORY("data", hdr->data_marker, 4);
    TEST_ASSERT_EQUAL_UINT32(1,      hdr->audio_format);
    TEST_ASSERT_EQUAL_UINT32(2,      hdr->channels);
    TEST_ASSERT_EQUAL_UINT32(44100,  hdr->sample_rate);
    TEST_ASSERT_EQUAL_UINT32(16,     hdr->bits_per_sample);
    TEST_ASSERT_EQUAL_UINT32(4,      hdr->block_align);        /* 2ch * 2bytes */
    TEST_ASSERT_EQUAL_UINT32(44100*4, hdr->byte_rate);
    TEST_ASSERT_EQUAL_UINT32(audio.data_length, hdr->data_size);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)(sz - 8), hdr->overall_size);

    mw_free_buffer(&audio);
    MINIWAVE_FREE((void*)hdr);
}

/* =========================================================================
   Allocator balance (no leaks)
   ======================================================================= */

void test_AllocatorBalance_NoLeaks(void)
{
    TEST_ASSERT_EQUAL_INT(0, g_alloc_count);
}

/* =========================================================================
   Main
   ======================================================================= */

int main(void)
{
    UNITY_BEGIN();

    /* Read memory — happy path */
    RUN_TEST(test_ReadMemory_Mono8Bit_Roundtrip);
    RUN_TEST(test_ReadMemory_Stereo16Bit_Roundtrip);
    RUN_TEST(test_ReadMemory_Mono16Bit_Roundtrip);
    RUN_TEST(test_ReadMemory_SkipsExtraMetadataChunk);

    /* Read memory — failure paths */
    RUN_TEST(test_ReadMemory_Null_Buffer_Returns0);
    RUN_TEST(test_ReadMemory_Null_OutAudio_Returns0);
    RUN_TEST(test_ReadMemory_TooSmall_Returns0);
    RUN_TEST(test_ReadMemory_GarbageHeader_Returns0);
    RUN_TEST(test_ReadMemory_CorrectRIFFButMissingWAVE_Returns0);

    /* Write memory — failure paths */
    RUN_TEST(test_WriteMemory_Null_Audio_ReturnsNull);
    RUN_TEST(test_WriteMemory_Null_OutSize_ReturnsNull);
    RUN_TEST(test_WriteMemory_Null_Data_ReturnsNull);
    RUN_TEST(test_WriteMemory_SizeEqualsHeaderPlusPayload);

    /* File I/O */
    RUN_TEST(test_WriteAndReadFile_Mono8Bit_Roundtrip);
    RUN_TEST(test_WriteAndReadFile_Stereo16Bit_Roundtrip);
    RUN_TEST(test_ReadFile_NonExistent_Returns0);

    /* Free buffer */
    RUN_TEST(test_FreeBuffer_NullAudio_DoesNotCrash);
    RUN_TEST(test_FreeBuffer_ZeroInitialised_DoesNotCrash);
    RUN_TEST(test_FreeBuffer_SetsDataToNull);

    /* Resample — guard conditions */
    RUN_TEST(test_Resample_NullSrc_Returns0);
    RUN_TEST(test_Resample_NullDst_Returns0);
    RUN_TEST(test_Resample_ZeroRate_Returns0);
    RUN_TEST(test_Resample_NullData_Returns0);

    /* Resample — identity */
    RUN_TEST(test_Resample_Identity_CopiesExactly);

    /* Resample — fast paths */
    RUN_TEST(test_Resample_FastPath_Upsample2x_Interpolation);
    RUN_TEST(test_Resample_FastPath_Upsample4x_Interpolation);
    RUN_TEST(test_Resample_FastPath_Downsample2x_Averaging);
    RUN_TEST(test_Resample_FastPath_Downsample4x_Averaging);

    /* Resample — fallback paths */
    RUN_TEST(test_Resample_Fallback_Mono8_ArbitraryRatio);
    RUN_TEST(test_Resample_Fallback_Stereo16_Downsample);
    RUN_TEST(test_Resample_Fallback_Mono16_Upsample);
    RUN_TEST(test_Resample_PreservesChannelsAndBits);

    /* Header fields */
    RUN_TEST(test_WriteMemory_HeaderFieldsAreCorrect);

    /* Leak check — must be last */
    RUN_TEST(test_AllocatorBalance_NoLeaks);

    return UNITY_END();
}
