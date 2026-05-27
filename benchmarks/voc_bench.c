/*
    benchmarks/voc_bench.c — benchmarks for voc.h using ubench.h

    Covers: in-memory parse (block 0x09), serialise, and all resample paths.

    Build (GCC/Clang, from repo root):
        gcc -std=gnu11 -O2 -I. -Ibenchmarks benchmarks/voc_bench.c -o build/bench/voc_bench -lm

    Build (MSVC, from repo root):
        cl /std:c11 /O2 /I. /Ibenchmarks benchmarks/voc_bench.c /Fe:build/bench/voc_bench.exe /link /DEFAULTLIB:libm
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

#define MINIVOC_IMPLEMENTATION
#include "../voc.h"
#include "ubench.h"

/* -------------------------------------------------------------------------
   Constants
   ---------------------------------------------------------------------- */

enum {
    N_SAMPLES_LARGE  = 65536,  /* 64 Ki samples  — parse/write benches   */
    N_SAMPLES_MEDIUM = 16384,  /* 16 Ki samples  — resample benches       */
    N_WARMUP         = 4
};

/* -------------------------------------------------------------------------
   Helpers
   ---------------------------------------------------------------------- */

/*
 * Build a minimal block-0x09 VOC blob:
 *   26 bytes  header
 *    1 byte   block type = 0x09
 *    3 bytes  LE block length = 12 + data_bytes
 *   12 bytes  block-9 header (rate, bits, channels, format, reserved)
 *   N bytes   raw PCM data
 *    1 byte   terminator = 0x00
 */
static uint8_t* make_voc_blob(uint32_t channels, uint32_t rate, uint32_t bps,
                               uint32_t sample_count, size_t* out_size)
{
    uint32_t bytes_per_sample = (bps / 8) * channels;
    uint32_t data_bytes       = sample_count * bytes_per_sample;
    uint32_t block_payload    = 12 + data_bytes;  /* block-9 header + PCM  */
    size_t   total            = 26 + 1 + 3 + block_payload + 1;

    uint8_t* buf = (uint8_t*)malloc(total);
    if (!buf) return NULL;

    uint8_t* p = buf;

    /* VOC file header (26 bytes) */
    memcpy(p, "Creative Voice File\x1A", 20); p += 20;
    uint16_t data_off   = 26;
    uint16_t version    = 0x0114;
    uint16_t checksum   = (uint16_t)(~version + 0x1234u);
    memcpy(p, &data_off,  2); p += 2;
    memcpy(p, &version,   2); p += 2;
    memcpy(p, &checksum,  2); p += 2;

    /* Block type 0x09 */
    *p++ = 0x09;
    p[0] = (uint8_t)(block_payload & 0xFF);
    p[1] = (uint8_t)((block_payload >> 8) & 0xFF);
    p[2] = (uint8_t)((block_payload >> 16) & 0xFF);
    p += 3;

    /* Block-9 header (12 bytes) */
    memcpy(p, &rate, 4); p += 4;
    *p++ = (uint8_t)bps;
    *p++ = (uint8_t)channels;
    uint16_t fmt = 0x0000; memcpy(p, &fmt, 2); p += 2;
    uint32_t pad = 0;      memcpy(p, &pad, 4); p += 4;

    /* PCM data */
    for (uint32_t i = 0; i < data_bytes; i++)
        *p++ = (uint8_t)(i & 0xFF);

    /* Terminator */
    *p = 0x00;

    *out_size = total;
    return buf;
}

static void make_mono8_buffer(mv_audio_buffer* buf, uint32_t rate, uint32_t sample_count)
{
    buf->channels        = 1;
    buf->sample_rate     = rate;
    buf->bits_per_sample = 8;
    buf->data_length     = sample_count;
    buf->data            = (uint8_t*)malloc(sample_count);
    for (uint32_t i = 0; i < sample_count; i++)
        buf->data[i] = (uint8_t)(i & 0xFF);
}

static void make_stereo16_buffer(mv_audio_buffer* buf, uint32_t rate, uint32_t sample_count)
{
    buf->channels        = 2;
    buf->sample_rate     = rate;
    buf->bits_per_sample = 16;
    buf->data_length     = sample_count * 4;
    buf->data            = (uint8_t*)malloc(buf->data_length);
    for (uint32_t i = 0; i < buf->data_length; i++)
        buf->data[i] = (uint8_t)(i & 0xFF);
}

/* =========================================================================
   Fixtures
   ======================================================================= */

struct VocMono8 {
    uint8_t*        blob;
    size_t          blob_size;
    mv_audio_buffer audio;
};

UBENCH_F_SETUP(VocMono8)
{
    ubench_fixture->blob = make_voc_blob(1, 22050, 8, N_SAMPLES_LARGE,
                                          &ubench_fixture->blob_size);
    make_mono8_buffer(&ubench_fixture->audio, 22050, N_SAMPLES_LARGE);
}

UBENCH_F_TEARDOWN(VocMono8)
{
    free(ubench_fixture->blob);
    mv_free_buffer(&ubench_fixture->audio);
}

struct VocStereo16 {
    uint8_t*        blob;
    size_t          blob_size;
    mv_audio_buffer audio;
};

UBENCH_F_SETUP(VocStereo16)
{
    ubench_fixture->blob = make_voc_blob(2, 44100, 16, N_SAMPLES_LARGE,
                                          &ubench_fixture->blob_size);
    make_stereo16_buffer(&ubench_fixture->audio, 44100, N_SAMPLES_LARGE);
}

UBENCH_F_TEARDOWN(VocStereo16)
{
    free(ubench_fixture->blob);
    mv_free_buffer(&ubench_fixture->audio);
}

struct VocResample {
    mv_audio_buffer mono8_11025;
    mv_audio_buffer mono8_22050;
    mv_audio_buffer mono8_44100;
    mv_audio_buffer stereo16_44100;
};

UBENCH_F_SETUP(VocResample)
{
    make_mono8_buffer(&ubench_fixture->mono8_11025,   11025, N_SAMPLES_MEDIUM);
    make_mono8_buffer(&ubench_fixture->mono8_22050,   22050, N_SAMPLES_MEDIUM);
    make_mono8_buffer(&ubench_fixture->mono8_44100,   44100, N_SAMPLES_MEDIUM);
    make_stereo16_buffer(&ubench_fixture->stereo16_44100, 44100, N_SAMPLES_MEDIUM);
}

UBENCH_F_TEARDOWN(VocResample)
{
    mv_free_buffer(&ubench_fixture->mono8_11025);
    mv_free_buffer(&ubench_fixture->mono8_22050);
    mv_free_buffer(&ubench_fixture->mono8_44100);
    mv_free_buffer(&ubench_fixture->stereo16_44100);
}

/* =========================================================================
   Parse (mv_read_memory)
   ======================================================================= */

UBENCH_EX_F(VocMono8, ReadMemory_Mono8_64Ki)
{
    for (int w = 0; w < N_WARMUP; w++) {
        mv_audio_buffer a = {0};
        mv_read_memory(ubench_fixture->blob, ubench_fixture->blob_size, &a);
        mv_free_buffer(&a);
    }

    UBENCH_DO_BENCHMARK()
    {
        mv_audio_buffer a = {0};
        mv_read_memory(ubench_fixture->blob, ubench_fixture->blob_size, &a);
        mv_free_buffer(&a);
        UBENCH_DO_NOTHING(&a);
    }
}

UBENCH_EX_F(VocStereo16, ReadMemory_Stereo16_64Ki)
{
    for (int w = 0; w < N_WARMUP; w++) {
        mv_audio_buffer a = {0};
        mv_read_memory(ubench_fixture->blob, ubench_fixture->blob_size, &a);
        mv_free_buffer(&a);
    }

    UBENCH_DO_BENCHMARK()
    {
        mv_audio_buffer a = {0};
        mv_read_memory(ubench_fixture->blob, ubench_fixture->blob_size, &a);
        mv_free_buffer(&a);
        UBENCH_DO_NOTHING(&a);
    }
}

/* =========================================================================
   Serialise (mv_write_memory)
   ======================================================================= */

UBENCH_EX_F(VocMono8, WriteMemory_Mono8_64Ki)
{
    for (int w = 0; w < N_WARMUP; w++) {
        size_t sz = 0;
        void* out = mv_write_memory(&ubench_fixture->audio, &sz);
        MINIVOC_FREE(out);
    }

    UBENCH_DO_BENCHMARK()
    {
        size_t sz = 0;
        void* out = mv_write_memory(&ubench_fixture->audio, &sz);
        MINIVOC_FREE(out);
        UBENCH_DO_NOTHING(&out);
    }
}

UBENCH_EX_F(VocStereo16, WriteMemory_Stereo16_64Ki)
{
    for (int w = 0; w < N_WARMUP; w++) {
        size_t sz = 0;
        void* out = mv_write_memory(&ubench_fixture->audio, &sz);
        MINIVOC_FREE(out);
    }

    UBENCH_DO_BENCHMARK()
    {
        size_t sz = 0;
        void* out = mv_write_memory(&ubench_fixture->audio, &sz);
        MINIVOC_FREE(out);
        UBENCH_DO_NOTHING(&out);
    }
}

/* =========================================================================
   Resample — fast paths (8-bit mono)
   ======================================================================= */

UBENCH_EX_F(VocResample, Resample_Identity_22050)
{
    for (int w = 0; w < N_WARMUP; w++) {
        mv_audio_buffer dst = {0};
        mv_resample_pcm(&ubench_fixture->mono8_22050, &dst, 22050);
        mv_free_buffer(&dst);
    }
    UBENCH_DO_BENCHMARK()
    {
        mv_audio_buffer dst = {0};
        mv_resample_pcm(&ubench_fixture->mono8_22050, &dst, 22050);
        mv_free_buffer(&dst);
        UBENCH_DO_NOTHING(&dst);
    }
}

UBENCH_EX_F(VocResample, Resample_FastUp2x_22050to44100)
{
    for (int w = 0; w < N_WARMUP; w++) {
        mv_audio_buffer dst = {0};
        mv_resample_pcm(&ubench_fixture->mono8_22050, &dst, 44100);
        mv_free_buffer(&dst);
    }
    UBENCH_DO_BENCHMARK()
    {
        mv_audio_buffer dst = {0};
        mv_resample_pcm(&ubench_fixture->mono8_22050, &dst, 44100);
        mv_free_buffer(&dst);
        UBENCH_DO_NOTHING(&dst);
    }
}

UBENCH_EX_F(VocResample, Resample_FastUp4x_11025to44100)
{
    for (int w = 0; w < N_WARMUP; w++) {
        mv_audio_buffer dst = {0};
        mv_resample_pcm(&ubench_fixture->mono8_11025, &dst, 44100);
        mv_free_buffer(&dst);
    }
    UBENCH_DO_BENCHMARK()
    {
        mv_audio_buffer dst = {0};
        mv_resample_pcm(&ubench_fixture->mono8_11025, &dst, 44100);
        mv_free_buffer(&dst);
        UBENCH_DO_NOTHING(&dst);
    }
}

UBENCH_EX_F(VocResample, Resample_FastDown2x_44100to22050)
{
    for (int w = 0; w < N_WARMUP; w++) {
        mv_audio_buffer dst = {0};
        mv_resample_pcm(&ubench_fixture->mono8_44100, &dst, 22050);
        mv_free_buffer(&dst);
    }
    UBENCH_DO_BENCHMARK()
    {
        mv_audio_buffer dst = {0};
        mv_resample_pcm(&ubench_fixture->mono8_44100, &dst, 22050);
        mv_free_buffer(&dst);
        UBENCH_DO_NOTHING(&dst);
    }
}

UBENCH_EX_F(VocResample, Resample_FastDown4x_44100to11025)
{
    for (int w = 0; w < N_WARMUP; w++) {
        mv_audio_buffer dst = {0};
        mv_resample_pcm(&ubench_fixture->mono8_44100, &dst, 11025);
        mv_free_buffer(&dst);
    }
    UBENCH_DO_BENCHMARK()
    {
        mv_audio_buffer dst = {0};
        mv_resample_pcm(&ubench_fixture->mono8_44100, &dst, 11025);
        mv_free_buffer(&dst);
        UBENCH_DO_NOTHING(&dst);
    }
}

/* General fallback: stereo 16-bit arbitrary ratio */
UBENCH_EX_F(VocResample, Resample_Fallback_Stereo16_44100to32000)
{
    for (int w = 0; w < N_WARMUP; w++) {
        mv_audio_buffer dst = {0};
        mv_resample_pcm(&ubench_fixture->stereo16_44100, &dst, 32000);
        mv_free_buffer(&dst);
    }
    UBENCH_DO_BENCHMARK()
    {
        mv_audio_buffer dst = {0};
        mv_resample_pcm(&ubench_fixture->stereo16_44100, &dst, 32000);
        mv_free_buffer(&dst);
        UBENCH_DO_NOTHING(&dst);
    }
}

/* General fallback: mono 8-bit arbitrary ratio */
UBENCH_EX_F(VocResample, Resample_Fallback_Mono8_22050to16000)
{
    for (int w = 0; w < N_WARMUP; w++) {
        mv_audio_buffer dst = {0};
        mv_resample_pcm(&ubench_fixture->mono8_22050, &dst, 16000);
        mv_free_buffer(&dst);
    }
    UBENCH_DO_BENCHMARK()
    {
        mv_audio_buffer dst = {0};
        mv_resample_pcm(&ubench_fixture->mono8_22050, &dst, 16000);
        mv_free_buffer(&dst);
        UBENCH_DO_NOTHING(&dst);
    }
}

/* =========================================================================
   Entry point
   ======================================================================= */

UBENCH_MAIN()
