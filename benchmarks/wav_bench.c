/*
    benchmarks/wav_bench.c — benchmarks for wav.h using ubench.h

    Covers: in-memory parse, serialise, and all resample paths.

    Build (GCC/Clang, from repo root):
        gcc -std=gnu11 -O2 -I. -Ibenchmarks benchmarks/wav_bench.c -o build/bench/wav_bench -lm

    Build (MSVC, from repo root):
        cl /std:c11 /O2 /I. /Ibenchmarks benchmarks/wav_bench.c /Fe:build/bench/wav_bench.exe /link /DEFAULTLIB:libm
*/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ubench.h declares ubench_large_integer only under _MSC_VER but uses it for
   MinGW too.  Provide the typedef via <windows.h> before ubench.h sees it. */
#if (defined(__MINGW32__) || defined(__MINGW64__)) && !defined(_MSC_VER)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
   typedef LARGE_INTEGER ubench_large_integer;
#endif

#define MINIWAVE_IMPLEMENTATION
#include "../wav.h"
#include "ubench.h"

/* -------------------------------------------------------------------------
   Constants
   ---------------------------------------------------------------------- */

enum {
    N_SAMPLES_LARGE  = 65536,  /* 64 Ki samples  — parse/write benches   */
    N_SAMPLES_MEDIUM = 16384,  /* 16 Ki samples  — resample benches       */
    N_WARMUP         = 4,      /* setup iterations before first timing    */
    N_BATCH          = 16      /* ops per timed sample for sub-µs ops     */
};

/* -------------------------------------------------------------------------
   Helpers
   ---------------------------------------------------------------------- */

/* Build a minimal WAV blob in memory (44-byte header + raw PCM). */
static uint8_t* make_wav_blob(uint32_t channels, uint32_t rate, uint32_t bps,
                               uint32_t sample_count, size_t* out_size)
{
    uint32_t bytes_per_sample = (bps / 8) * channels;
    uint32_t data_bytes       = sample_count * bytes_per_sample;
    size_t   total            = 44 + data_bytes;
    uint8_t* buf              = (uint8_t*)malloc(total);
    if (!buf) return NULL;

    /* RIFF/WAVE header (little-endian) */
    memcpy(buf,      "RIFF", 4);
    uint32_t sz = (uint32_t)(total - 8);
    memcpy(buf +  4, &sz,        4);
    memcpy(buf +  8, "WAVE",     4);
    memcpy(buf + 12, "fmt ",     4);
    uint32_t fmt_len = 16; memcpy(buf + 16, &fmt_len,        4);
    uint16_t fmt_tag = 1;  memcpy(buf + 20, &fmt_tag,        2);
    uint16_t ch16    = (uint16_t)channels; memcpy(buf + 22, &ch16,   2);
    memcpy(buf + 24, &rate,      4);
    uint32_t byte_rate  = rate * bytes_per_sample; memcpy(buf + 28, &byte_rate, 4);
    uint16_t blk_align  = (uint16_t)bytes_per_sample; memcpy(buf + 32, &blk_align, 2);
    uint16_t bps16      = (uint16_t)bps; memcpy(buf + 34, &bps16,   2);
    memcpy(buf + 36, "data",     4);
    memcpy(buf + 40, &data_bytes, 4);

    /* Fill PCM data with a repeating sawtooth so the content is realistic */
    for (uint32_t i = 0; i < data_bytes; i++)
        buf[44 + i] = (uint8_t)(i & 0xFF);

    *out_size = total;
    return buf;
}

/* Build an mw_audio_buffer owning N_SAMPLES_MEDIUM 8-bit mono samples. */
static void make_mono8_buffer(mw_audio_buffer* buf, uint32_t rate, uint32_t sample_count)
{
    buf->channels        = 1;
    buf->sample_rate     = rate;
    buf->bits_per_sample = 8;
    buf->data_length     = sample_count;
    buf->data            = (uint8_t*)malloc(sample_count);
    for (uint32_t i = 0; i < sample_count; i++)
        buf->data[i] = (uint8_t)(i & 0xFF);
}

/* Build an mw_audio_buffer owning N_SAMPLES_MEDIUM stereo 16-bit samples. */
static void make_stereo16_buffer(mw_audio_buffer* buf, uint32_t rate, uint32_t sample_count)
{
    buf->channels        = 2;
    buf->sample_rate     = rate;
    buf->bits_per_sample = 16;
    buf->data_length     = sample_count * 4; /* 2 channels × 2 bytes */
    buf->data            = (uint8_t*)malloc(buf->data_length);
    for (uint32_t i = 0; i < buf->data_length; i++)
        buf->data[i] = (uint8_t)(i & 0xFF);
}

/* =========================================================================
   Fixtures
   ======================================================================= */

/* --- WavMono8: pre-built 64 Ki-sample 8-bit mono WAV blob + audio_buffer --- */
struct WavMono8 {
    uint8_t*        blob;
    size_t          blob_size;
    mw_audio_buffer audio;
};

UBENCH_F_SETUP(WavMono8)
{
    ubench_fixture->blob = make_wav_blob(1, 22050, 8, N_SAMPLES_LARGE,
                                          &ubench_fixture->blob_size);
    make_mono8_buffer(&ubench_fixture->audio, 22050, N_SAMPLES_LARGE);
}

UBENCH_F_TEARDOWN(WavMono8)
{
    free(ubench_fixture->blob);
    mw_free_buffer(&ubench_fixture->audio);
}

/* --- WavStereo16: pre-built 64 Ki-sample stereo 16-bit WAV blob + buffer --- */
struct WavStereo16 {
    uint8_t*        blob;
    size_t          blob_size;
    mw_audio_buffer audio;
};

UBENCH_F_SETUP(WavStereo16)
{
    ubench_fixture->blob = make_wav_blob(2, 44100, 16, N_SAMPLES_LARGE,
                                          &ubench_fixture->blob_size);
    make_stereo16_buffer(&ubench_fixture->audio, 44100, N_SAMPLES_LARGE);
}

UBENCH_F_TEARDOWN(WavStereo16)
{
    free(ubench_fixture->blob);
    mw_free_buffer(&ubench_fixture->audio);
}

/* --- WavResample: 16 Ki-sample source for resample benchmarks --- */
struct WavResample {
    mw_audio_buffer mono8_11025;
    mw_audio_buffer mono8_22050;
    mw_audio_buffer mono8_44100;
    mw_audio_buffer stereo16_44100;
};

UBENCH_F_SETUP(WavResample)
{
    make_mono8_buffer(&ubench_fixture->mono8_11025,   11025, N_SAMPLES_MEDIUM);
    make_mono8_buffer(&ubench_fixture->mono8_22050,   22050, N_SAMPLES_MEDIUM);
    make_mono8_buffer(&ubench_fixture->mono8_44100,   44100, N_SAMPLES_MEDIUM);
    make_stereo16_buffer(&ubench_fixture->stereo16_44100, 44100, N_SAMPLES_MEDIUM);
}

UBENCH_F_TEARDOWN(WavResample)
{
    mw_free_buffer(&ubench_fixture->mono8_11025);
    mw_free_buffer(&ubench_fixture->mono8_22050);
    mw_free_buffer(&ubench_fixture->mono8_44100);
    mw_free_buffer(&ubench_fixture->stereo16_44100);
}

/* =========================================================================
   Parse (mw_read_memory)
   ======================================================================= */

UBENCH_EX_F(WavMono8, ReadMemory_Mono8_64Ki)
{
    /* Warm up */
    for (int w = 0; w < N_WARMUP; w++) {
        mw_audio_buffer a = {0};
        mw_read_memory(ubench_fixture->blob, ubench_fixture->blob_size, &a);
        mw_free_buffer(&a);
    }

    UBENCH_DO_BENCHMARK()
    {
        mw_audio_buffer a = {0};
        mw_read_memory(ubench_fixture->blob, ubench_fixture->blob_size, &a);
        mw_free_buffer(&a);
        UBENCH_DO_NOTHING(&a);
    }
}

UBENCH_EX_F(WavStereo16, ReadMemory_Stereo16_64Ki)
{
    for (int w = 0; w < N_WARMUP; w++) {
        mw_audio_buffer a = {0};
        mw_read_memory(ubench_fixture->blob, ubench_fixture->blob_size, &a);
        mw_free_buffer(&a);
    }

    UBENCH_DO_BENCHMARK()
    {
        mw_audio_buffer a = {0};
        mw_read_memory(ubench_fixture->blob, ubench_fixture->blob_size, &a);
        mw_free_buffer(&a);
        UBENCH_DO_NOTHING(&a);
    }
}

/* =========================================================================
   Serialise (mw_write_memory)
   ======================================================================= */

UBENCH_EX_F(WavMono8, WriteMemory_Mono8_64Ki)
{
    for (int w = 0; w < N_WARMUP; w++) {
        size_t sz = 0;
        void* out = mw_write_memory(&ubench_fixture->audio, &sz);
        MINIWAVE_FREE(out);
    }

    UBENCH_DO_BENCHMARK()
    {
        size_t sz = 0;
        void* out = mw_write_memory(&ubench_fixture->audio, &sz);
        MINIWAVE_FREE(out);
        UBENCH_DO_NOTHING(&out);
    }
}

UBENCH_EX_F(WavStereo16, WriteMemory_Stereo16_64Ki)
{
    for (int w = 0; w < N_WARMUP; w++) {
        size_t sz = 0;
        void* out = mw_write_memory(&ubench_fixture->audio, &sz);
        MINIWAVE_FREE(out);
    }

    UBENCH_DO_BENCHMARK()
    {
        size_t sz = 0;
        void* out = mw_write_memory(&ubench_fixture->audio, &sz);
        MINIWAVE_FREE(out);
        UBENCH_DO_NOTHING(&out);
    }
}

/* =========================================================================
   Resample — fast paths (8-bit mono only)
   ======================================================================= */

/* Identity copy: 22050 → 22050 */
UBENCH_EX_F(WavResample, Resample_Identity_22050)
{
    for (int w = 0; w < N_WARMUP; w++) {
        mw_audio_buffer dst = {0};
        mw_resample_pcm(&ubench_fixture->mono8_22050, &dst, 22050);
        mw_free_buffer(&dst);
    }

    UBENCH_DO_BENCHMARK()
    {
        mw_audio_buffer dst = {0};
        mw_resample_pcm(&ubench_fixture->mono8_22050, &dst, 22050);
        mw_free_buffer(&dst);
        UBENCH_DO_NOTHING(&dst);
    }
}

/* Fast-path 2× upsample: 22050 → 44100 */
UBENCH_EX_F(WavResample, Resample_FastUp2x_22050to44100)
{
    for (int w = 0; w < N_WARMUP; w++) {
        mw_audio_buffer dst = {0};
        mw_resample_pcm(&ubench_fixture->mono8_22050, &dst, 44100);
        mw_free_buffer(&dst);
    }

    UBENCH_DO_BENCHMARK()
    {
        mw_audio_buffer dst = {0};
        mw_resample_pcm(&ubench_fixture->mono8_22050, &dst, 44100);
        mw_free_buffer(&dst);
        UBENCH_DO_NOTHING(&dst);
    }
}

/* Fast-path 4× upsample: 11025 → 44100 */
UBENCH_EX_F(WavResample, Resample_FastUp4x_11025to44100)
{
    for (int w = 0; w < N_WARMUP; w++) {
        mw_audio_buffer dst = {0};
        mw_resample_pcm(&ubench_fixture->mono8_11025, &dst, 44100);
        mw_free_buffer(&dst);
    }

    UBENCH_DO_BENCHMARK()
    {
        mw_audio_buffer dst = {0};
        mw_resample_pcm(&ubench_fixture->mono8_11025, &dst, 44100);
        mw_free_buffer(&dst);
        UBENCH_DO_NOTHING(&dst);
    }
}

/* Fast-path 2× downsample: 44100 → 22050 */
UBENCH_EX_F(WavResample, Resample_FastDown2x_44100to22050)
{
    for (int w = 0; w < N_WARMUP; w++) {
        mw_audio_buffer dst = {0};
        mw_resample_pcm(&ubench_fixture->mono8_44100, &dst, 22050);
        mw_free_buffer(&dst);
    }

    UBENCH_DO_BENCHMARK()
    {
        mw_audio_buffer dst = {0};
        mw_resample_pcm(&ubench_fixture->mono8_44100, &dst, 22050);
        mw_free_buffer(&dst);
        UBENCH_DO_NOTHING(&dst);
    }
}

/* Fast-path 4× downsample: 44100 → 11025 */
UBENCH_EX_F(WavResample, Resample_FastDown4x_44100to11025)
{
    for (int w = 0; w < N_WARMUP; w++) {
        mw_audio_buffer dst = {0};
        mw_resample_pcm(&ubench_fixture->mono8_44100, &dst, 11025);
        mw_free_buffer(&dst);
    }

    UBENCH_DO_BENCHMARK()
    {
        mw_audio_buffer dst = {0};
        mw_resample_pcm(&ubench_fixture->mono8_44100, &dst, 11025);
        mw_free_buffer(&dst);
        UBENCH_DO_NOTHING(&dst);
    }
}

/* General fallback: stereo 16-bit 44100 → 32000 (arbitrary ratio) */
UBENCH_EX_F(WavResample, Resample_Fallback_Stereo16_44100to32000)
{
    for (int w = 0; w < N_WARMUP; w++) {
        mw_audio_buffer dst = {0};
        mw_resample_pcm(&ubench_fixture->stereo16_44100, &dst, 32000);
        mw_free_buffer(&dst);
    }

    UBENCH_DO_BENCHMARK()
    {
        mw_audio_buffer dst = {0};
        mw_resample_pcm(&ubench_fixture->stereo16_44100, &dst, 32000);
        mw_free_buffer(&dst);
        UBENCH_DO_NOTHING(&dst);
    }
}

/* General fallback: mono 8-bit 22050 → 16000 (arbitrary ratio) */
UBENCH_EX_F(WavResample, Resample_Fallback_Mono8_22050to16000)
{
    for (int w = 0; w < N_WARMUP; w++) {
        mw_audio_buffer dst = {0};
        mw_resample_pcm(&ubench_fixture->mono8_22050, &dst, 16000);
        mw_free_buffer(&dst);
    }

    UBENCH_DO_BENCHMARK()
    {
        mw_audio_buffer dst = {0};
        mw_resample_pcm(&ubench_fixture->mono8_22050, &dst, 16000);
        mw_free_buffer(&dst);
        UBENCH_DO_NOTHING(&dst);
    }
}

/* =========================================================================
   Entry point
   ======================================================================= */

UBENCH_MAIN()
