/*
    tests/voc_test.c — comprehensive test suite for voc.h

    Covers:
      - mv_read_memory / mv_read_file   (valid, malformed, edge cases)
      - mv_write_memory / mv_write_file (roundtrip, NULL safety)
      - VOC block types: 0x01 (standard), 0x08 (extended), 0x09 (modern)
      - Unknown block types (must be ignored)
      - Version checksum validation
      - ADPCM / non-zero pack rejection
      - mv_resample_pcm                 (identity, fast paths 2x/4x up/down,
                                         fallback path, NULL inputs)
      - mv_free_buffer                  (NULL safety)
      - Allocator balance tracking      (leak detection)

    Build (from repo root, requires Unity):
        gcc -std=c99 -Wall -I. -Itests -Itests/vendor/unity/src \
            tests/voc_test.c tests/vendor/unity/src/unity.c -lm \
            -o build/default/voc_test
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
    if (!ptr && sz > 0) g_alloc_count++;
    if (ptr  && sz == 0) g_alloc_count--;
    return realloc(ptr, sz);
}
static void tracked_free(void* ptr) {
    if (ptr) g_alloc_count--;
    free(ptr);
}

#define MINIVOC_MALLOC(sz)         tracked_malloc(sz)
#define MINIVOC_REALLOC(ptr, size) tracked_realloc(ptr, size)
#define MINIVOC_FREE(ptr)          tracked_free(ptr)

#define MINIVOC_IMPLEMENTATION
#include "../voc.h"
#include "test_common.h"

/* -------------------------------------------------------------------------
   Test file names
   ---------------------------------------------------------------------- */
#define TEST_MONO8_FILE   "voc_test_mono8.voc"
#define TEST_STEREO16_FILE "voc_test_stereo16.voc"

/* =========================================================================
   Helpers
   ======================================================================= */

static void make_audio(mv_audio_buffer* audio,
                       uint32_t channels, uint32_t rate,
                       uint32_t bps, uint32_t sample_count)
{
    audio->channels        = channels;
    audio->sample_rate     = rate;
    audio->bits_per_sample = bps;
    audio->data_length     = sample_count * channels * (bps / 8);
    audio->data = (uint8_t*)MINIVOC_MALLOC(audio->data_length);
    TEST_ASSERT_NOT_NULL(audio->data);
    for (uint32_t i = 0; i < audio->data_length; i++)
        audio->data[i] = (uint8_t)(i % 255);
}

/* Build a minimal VOC blob in memory containing a type-0x09 block.
   mv_write_memory always produces a v1.20 header with a type-0x09 block. */
static uint8_t* build_voc_blob(mv_audio_buffer* audio, size_t* out_size)
{
    return (uint8_t*)mv_write_memory(audio, out_size);
}

/* Craft a raw VOC buffer with a type-0x01 (standard 8-bit) block.
   `rate_divisor` = 256 - (1000000 / sample_rate) */
static uint8_t* build_voc_block1(const uint8_t* pcm, uint32_t pcm_len,
                                  uint8_t rate_divisor, size_t* out_size)
{
    /* Header (26) + block header (4) + divisor+compression (2) + pcm + terminator (1) */
    size_t total = 26 + 4 + 2 + pcm_len + 1;
    uint8_t* buf = (uint8_t*)malloc(total);
    TEST_ASSERT_NOT_NULL(buf);
    memset(buf, 0, total);

    uint8_t* p = buf;
    /* VOC header */
    memcpy(p, "Creative Voice File\x1A", 20); p += 20;
    /* data_offset = 26 */
    p[0] = 26; p[1] = 0; p += 2;
    /* version = 0x0114 */
    p[0] = 0x14; p[1] = 0x01; p += 2;
    /* checksum: ~0x0114 + 0x1234 = 0xFEEB + 0x1234 = 0x111F... */
    /* actually: (~version + 0x1234): ~0x0114 = 0xFEEB, +0x1234 = 0x111F */
    p[0] = 0x1F; p[1] = 0x11; p += 2;

    /* Block type 0x01 */
    *p = 0x01; p++;
    /* 3-byte LE length = pcm_len + 2 */
    uint32_t block_len = pcm_len + 2;
    p[0] = (uint8_t)(block_len & 0xFF);
    p[1] = (uint8_t)((block_len >> 8) & 0xFF);
    p[2] = (uint8_t)((block_len >> 16) & 0xFF);
    p += 3;
    /* rate divisor */
    *p = rate_divisor; p++;
    /* compression = 0 (uncompressed) */
    *p = 0x00; p++;
    /* PCM data */
    memcpy(p, pcm, pcm_len); p += pcm_len;
    /* terminator */
    *p = 0x00;

    *out_size = total;
    return buf;
}

/* Craft a VOC with a type-0x08 (extended attributes) block followed by 0x01 */
static uint8_t* build_voc_block8_then_block1(const uint8_t* pcm, uint32_t pcm_len,
                                              uint16_t div_code, uint8_t mode,
                                              size_t* out_size)
{
    /* Header(26) + block8 header(4)+payload(4) + block1 header(4)+2+pcm + term(1) */
    size_t total = 26 + 4 + 4 + 4 + 2 + pcm_len + 1;
    uint8_t* buf = (uint8_t*)malloc(total);
    TEST_ASSERT_NOT_NULL(buf);
    memset(buf, 0, total);

    uint8_t* p = buf;
    memcpy(p, "Creative Voice File\x1A", 20); p += 20;
    p[0] = 26; p[1] = 0; p += 2;
    p[0] = 0x14; p[1] = 0x01; p += 2;
    p[0] = 0x1F; p[1] = 0x11; p += 2;

    /* Block 0x08 */
    *p = 0x08; p++;
    p[0] = 4; p[1] = 0; p[2] = 0; p += 3; /* length = 4 */
    p[0] = (uint8_t)(div_code & 0xFF);
    p[1] = (uint8_t)((div_code >> 8) & 0xFF);
    p += 2;
    *p = 0x00; p++; /* pack_method = 0 */
    *p = mode;  p++; /* 0=mono, 1=stereo */

    /* Block 0x01 (preceded by 0x08, ext_attributes_set=1) */
    *p = 0x01; p++;
    uint32_t b1_len = pcm_len + 2;
    p[0] = (uint8_t)(b1_len & 0xFF);
    p[1] = (uint8_t)((b1_len >> 8) & 0xFF);
    p[2] = (uint8_t)((b1_len >> 16) & 0xFF);
    p += 3;
    *p = 0x00; p++; /* divisor (ignored when ext_attributes_set) */
    *p = 0x00; p++; /* compression = 0 */
    memcpy(p, pcm, pcm_len); p += pcm_len;
    *p = 0x00; /* terminator */

    *out_size = total;
    return buf;
}

/* =========================================================================
   setUp / tearDown
   ======================================================================= */
void setUp(void)    {}
void tearDown(void) {}

/* =========================================================================
   mv_read_memory — happy path (block 0x09, via write/read roundtrip)
   ======================================================================= */

void test_ReadMemory_Block9_Mono8_Roundtrip(void)
{
    mv_audio_buffer original = {0};
    make_audio(&original, 1, 11025, 8, 100);

    size_t blob_size = 0;
    uint8_t* blob = build_voc_blob(&original, &blob_size);
    TEST_ASSERT_NOT_NULL(blob);

    mv_audio_buffer parsed = {0};
    TEST_ASSERT_EQUAL_INT(1, mv_read_memory(blob, blob_size, &parsed));
    TEST_ASSERT_EQUAL_UINT32(original.channels,        parsed.channels);
    TEST_ASSERT_EQUAL_UINT32(original.sample_rate,     parsed.sample_rate);
    TEST_ASSERT_EQUAL_UINT32(original.bits_per_sample, parsed.bits_per_sample);
    TEST_ASSERT_EQUAL_UINT32(original.data_length,     parsed.data_length);
    TEST_ASSERT_EQUAL_MEMORY(original.data, parsed.data, original.data_length);

    mv_free_buffer(&original);
    mv_free_buffer(&parsed);
    MINIVOC_FREE(blob);
}

void test_ReadMemory_Block9_Stereo16_Roundtrip(void)
{
    mv_audio_buffer original = {0};
    make_audio(&original, 2, 44100, 16, 200);

    size_t blob_size = 0;
    uint8_t* blob = build_voc_blob(&original, &blob_size);
    TEST_ASSERT_NOT_NULL(blob);

    mv_audio_buffer parsed = {0};
    TEST_ASSERT_EQUAL_INT(1, mv_read_memory(blob, blob_size, &parsed));
    TEST_ASSERT_EQUAL_UINT32(2,     parsed.channels);
    TEST_ASSERT_EQUAL_UINT32(44100, parsed.sample_rate);
    TEST_ASSERT_EQUAL_UINT32(16,    parsed.bits_per_sample);
    TEST_ASSERT_EQUAL_MEMORY(original.data, parsed.data, original.data_length);

    mv_free_buffer(&original);
    mv_free_buffer(&parsed);
    MINIVOC_FREE(blob);
}

/* =========================================================================
   mv_read_memory — block type 0x01 (standard 8-bit)
   ======================================================================= */

void test_ReadMemory_Block1_Standard_ParsesCorrectly(void)
{
    uint8_t pcm[] = { 10, 20, 30, 40, 50 };
    /* 11025 Hz: divisor = 256 - (1000000/11025) ≈ 256 - 90 = 166 */
    uint8_t divisor = (uint8_t)(256 - (1000000 / 11025));
    size_t blob_size = 0;
    uint8_t* blob = build_voc_block1(pcm, sizeof(pcm), divisor, &blob_size);

    mv_audio_buffer parsed = {0};
    TEST_ASSERT_EQUAL_INT(1, mv_read_memory(blob, blob_size, &parsed));
    TEST_ASSERT_EQUAL_UINT32(1, parsed.channels);
    TEST_ASSERT_EQUAL_UINT32(8, parsed.bits_per_sample);
    /* Rate through block-1 formula: 1000000/(256-divisor) */
    uint32_t expected_rate = 1000000 / (256 - divisor);
    TEST_ASSERT_UINT32_WITHIN(10, expected_rate, parsed.sample_rate);
    TEST_ASSERT_EQUAL_UINT32(sizeof(pcm), parsed.data_length);
    TEST_ASSERT_EQUAL_MEMORY(pcm, parsed.data, sizeof(pcm));

    mv_free_buffer(&parsed);
    free(blob);
}

void test_ReadMemory_Block1_ADPCM_Rejected(void)
{
    /* Build a block-0x01 with compression = 0x01 (ADPCM) — must fail */
    size_t total = 26 + 4 + 2 + 4 + 1;
    uint8_t* buf = (uint8_t*)malloc(total);
    TEST_ASSERT_NOT_NULL(buf);
    memset(buf, 0, total);

    uint8_t* p = buf;
    memcpy(p, "Creative Voice File\x1A", 20); p += 20;
    p[0] = 26; p[1] = 0; p += 2;
    p[0] = 0x14; p[1] = 0x01; p += 2;
    p[0] = 0x1F; p[1] = 0x11; p += 2;

    *p = 0x01; p++;
    p[0] = 6; p[1] = 0; p[2] = 0; p += 3; /* length = 6: 2 header + 4 pcm */
    *p = 90; p++;  /* divisor */
    *p = 0x01; p++; /* compression = ADPCM — not supported */
    p[0] = 1; p[1] = 2; p[2] = 3; p[3] = 4; p += 4;
    *p = 0x00;

    mv_audio_buffer parsed = {0};
    TEST_ASSERT_EQUAL_INT(0, mv_read_memory(buf, total, &parsed));
    TEST_ASSERT_NULL(parsed.data);

    free(buf);
}

/* =========================================================================
   mv_read_memory — block type 0x08 (extended attribute)
   ======================================================================= */

void test_ReadMemory_Block8_SetsStereoRate(void)
{
    uint8_t pcm[] = { 1, 2, 3, 4, 5, 6 };
    /* Stereo at ~11025 Hz per channel.
       div_code = 65536 - (256000000 / (rate * 2))
       For rate=11025 stereo: div_code = 65536 - 256000000/(11025*2) ≈ 65536-11609 = 53927 */
    uint16_t div_code = 53927;
    size_t blob_size = 0;
    uint8_t* blob = build_voc_block8_then_block1(pcm, sizeof(pcm), div_code, 1, &blob_size);

    mv_audio_buffer parsed = {0};
    TEST_ASSERT_EQUAL_INT(1, mv_read_memory(blob, blob_size, &parsed));
    TEST_ASSERT_EQUAL_UINT32(2, parsed.channels); /* stereo */
    TEST_ASSERT_EQUAL_UINT32(8, parsed.bits_per_sample);
    TEST_ASSERT_EQUAL_UINT32(sizeof(pcm), parsed.data_length);
    TEST_ASSERT_EQUAL_MEMORY(pcm, parsed.data, sizeof(pcm));

    mv_free_buffer(&parsed);
    free(blob);
}

void test_ReadMemory_Block8_NonZeroPack_Rejected(void)
{
    size_t total = 26 + 4 + 4 + 1;
    uint8_t* buf = (uint8_t*)malloc(total);
    TEST_ASSERT_NOT_NULL(buf);
    memset(buf, 0, total);

    uint8_t* p = buf;
    memcpy(p, "Creative Voice File\x1A", 20); p += 20;
    p[0] = 26; p[1] = 0; p += 2;
    p[0] = 0x14; p[1] = 0x01; p += 2;
    p[0] = 0x1F; p[1] = 0x11; p += 2;

    *p = 0x08; p++;
    p[0] = 4; p[1] = 0; p[2] = 0; p += 3;
    p[0] = 0; p[1] = 0; p += 2; /* div_code */
    *p = 0x01; p++; /* pack_method = 1 — not supported */
    *p = 0x00; p++; /* mono */
    *p = 0x00; /* terminator */

    mv_audio_buffer parsed = {0};
    TEST_ASSERT_EQUAL_INT(0, mv_read_memory(buf, total, &parsed));
    TEST_ASSERT_NULL(parsed.data);

    free(buf);
}

/* =========================================================================
   mv_read_memory — unknown block types are skipped
   ======================================================================= */

void test_ReadMemory_UnknownBlockType_Skipped(void)
{
    /* Insert an unknown type-0x04 block before a valid type-0x09 block */
    mv_audio_buffer audio = {0};
    make_audio(&audio, 1, 11025, 8, 8);

    size_t voc_size = 0;
    uint8_t* voc = build_voc_blob(&audio, &voc_size);
    TEST_ASSERT_NOT_NULL(voc);

    /* Patch: after the header (26 bytes) inject an 0x04 block of 2 bytes before the 0x09 */
    size_t unknown_block_size = 1 + 3 + 2; /* type + 3-byte len + 2 payload */
    size_t new_size = voc_size + unknown_block_size;
    uint8_t* patched = (uint8_t*)malloc(new_size);
    TEST_ASSERT_NOT_NULL(patched);

    /* Copy header */
    memcpy(patched, voc, 26);
    /* Insert unknown block (type 0x04) */
    uint8_t* ins = patched + 26;
    *ins = 0x04; ins++;         /* unknown type */
    ins[0] = 2; ins[1] = 0; ins[2] = 0; ins += 3; /* length = 2 */
    ins[0] = 0xAA; ins[1] = 0xBB; ins += 2;
    /* Copy rest of original VOC (from offset 26 onwards) */
    memcpy(ins, voc + 26, voc_size - 26);

    mv_audio_buffer parsed = {0};
    TEST_ASSERT_EQUAL_INT(1, mv_read_memory(patched, new_size, &parsed));
    TEST_ASSERT_EQUAL_UINT32(audio.data_length, parsed.data_length);
    TEST_ASSERT_EQUAL_MEMORY(audio.data, parsed.data, audio.data_length);

    mv_free_buffer(&audio);
    mv_free_buffer(&parsed);
    MINIVOC_FREE(voc);
    free(patched);
}

/* =========================================================================
   mv_read_memory — failure paths
   ======================================================================= */

void test_ReadMemory_Null_Buffer_Returns0(void)
{
    mv_audio_buffer out = {0};
    TEST_ASSERT_EQUAL_INT(0, mv_read_memory(NULL, 64, &out));
    TEST_ASSERT_NULL(out.data);
}

void test_ReadMemory_Null_OutAudio_Returns0(void)
{
    uint8_t dummy[64] = {0};
    TEST_ASSERT_EQUAL_INT(0, mv_read_memory(dummy, sizeof(dummy), NULL));
}

void test_ReadMemory_TooSmall_Returns0(void)
{
    uint8_t tiny[10] = {0};
    mv_audio_buffer out = {0};
    TEST_ASSERT_EQUAL_INT(0, mv_read_memory(tiny, sizeof(tiny), &out));
    TEST_ASSERT_NULL(out.data);
}

void test_ReadMemory_GarbageHeader_Returns0(void)
{
    uint8_t garbage[128];
    memset(garbage, 0xFF, sizeof(garbage));
    mv_audio_buffer out = {0};
    TEST_ASSERT_EQUAL_INT(0, mv_read_memory(garbage, sizeof(garbage), &out));
    TEST_ASSERT_NULL(out.data);
}

void test_ReadMemory_InvalidVersionCheck_Returns0(void)
{
    /* Build a minimal VOC header but corrupt the version checksum */
    uint8_t buf[64];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, "Creative Voice File\x1A", 20);
    buf[20] = 26; buf[21] = 0;   /* data_offset */
    buf[22] = 0x14; buf[23] = 0x01; /* version = 0x0114 */
    buf[24] = 0xDE; buf[25] = 0xAD; /* wrong checksum */

    mv_audio_buffer out = {0};
    TEST_ASSERT_EQUAL_INT(0, mv_read_memory(buf, sizeof(buf), &out));
    TEST_ASSERT_NULL(out.data);
}

void test_ReadMemory_EmptyBlocks_Returns0(void)
{
    /* VOC header + only a terminator byte: no audio data */
    uint8_t buf[27];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, "Creative Voice File\x1A", 20);
    buf[20] = 26; buf[21] = 0;
    buf[22] = 0x14; buf[23] = 0x01;
    buf[24] = 0x1F; buf[25] = 0x11;
    buf[26] = 0x00; /* terminator */

    mv_audio_buffer out = {0};
    TEST_ASSERT_EQUAL_INT(0, mv_read_memory(buf, sizeof(buf), &out));
    TEST_ASSERT_NULL(out.data);
}

/* =========================================================================
   mv_write_memory — failure paths
   ======================================================================= */

void test_WriteMemory_Null_Audio_ReturnsNull(void)
{
    size_t sz = 0;
    TEST_ASSERT_NULL(mv_write_memory(NULL, &sz));
}

void test_WriteMemory_Null_OutSize_ReturnsNull(void)
{
    mv_audio_buffer audio = {0};
    uint8_t dummy[4] = {0};
    audio.data = dummy;
    audio.data_length = 4;
    TEST_ASSERT_NULL(mv_write_memory(&audio, NULL));
}

void test_WriteMemory_Null_Data_ReturnsNull(void)
{
    mv_audio_buffer audio = { 1, 11025, 8, 100, NULL };
    size_t sz = 0;
    TEST_ASSERT_NULL(mv_write_memory(&audio, &sz));
}

void test_WriteMemory_SizeIsCorrect(void)
{
    mv_audio_buffer audio = {0};
    make_audio(&audio, 1, 11025, 8, 50);

    /* Expected: 26 (header) + 1+3+12 (block 9 type+len+payload) + data + 1 (terminator) */
    size_t expected = 26 + 16 + audio.data_length + 1;
    size_t actual = 0;
    uint8_t* blob = (uint8_t*)mv_write_memory(&audio, &actual);
    TEST_ASSERT_NOT_NULL(blob);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)expected, (uint32_t)actual);

    mv_free_buffer(&audio);
    MINIVOC_FREE(blob);
}

/* =========================================================================
   mv_read_file / mv_write_file
   ======================================================================= */

void test_WriteAndReadFile_Mono8Bit_Roundtrip(void)
{
    mv_audio_buffer original = {0};
    make_audio(&original, 1, 11025, 8, 100);

    TEST_ASSERT_EQUAL_INT(1, mv_write_file(TEST_MONO8_FILE, &original));

    mv_audio_buffer parsed = {0};
    TEST_ASSERT_EQUAL_INT(1, mv_read_file(TEST_MONO8_FILE, &parsed));
    TEST_ASSERT_EQUAL_UINT32(original.channels,        parsed.channels);
    TEST_ASSERT_EQUAL_UINT32(original.sample_rate,     parsed.sample_rate);
    TEST_ASSERT_EQUAL_UINT32(original.bits_per_sample, parsed.bits_per_sample);
    TEST_ASSERT_EQUAL_MEMORY(original.data, parsed.data, original.data_length);

    mv_free_buffer(&original);
    mv_free_buffer(&parsed);
    remove(TEST_MONO8_FILE);
}

void test_WriteAndReadFile_Stereo16Bit_Roundtrip(void)
{
    mv_audio_buffer original = {0};
    make_audio(&original, 2, 44100, 16, 200);

    TEST_ASSERT_EQUAL_INT(1, mv_write_file(TEST_STEREO16_FILE, &original));

    mv_audio_buffer parsed = {0};
    TEST_ASSERT_EQUAL_INT(1, mv_read_file(TEST_STEREO16_FILE, &parsed));
    TEST_ASSERT_EQUAL_UINT32(2,     parsed.channels);
    TEST_ASSERT_EQUAL_UINT32(44100, parsed.sample_rate);
    TEST_ASSERT_EQUAL_UINT32(16,    parsed.bits_per_sample);
    TEST_ASSERT_EQUAL_MEMORY(original.data, parsed.data, original.data_length);

    mv_free_buffer(&original);
    mv_free_buffer(&parsed);
    remove(TEST_STEREO16_FILE);
}

void test_ReadFile_NonExistent_Returns0(void)
{
    mv_audio_buffer out = {0};
    TEST_ASSERT_EQUAL_INT(0, mv_read_file("__no_such_file__.voc", &out));
    TEST_ASSERT_NULL(out.data);
}

/* =========================================================================
   mv_free_buffer — NULL safety
   ======================================================================= */

void test_FreeBuffer_NullAudio_DoesNotCrash(void)
{
    mv_free_buffer(NULL);
    TEST_PASS();
}

void test_FreeBuffer_ZeroInitialised_DoesNotCrash(void)
{
    mv_audio_buffer empty = {0};
    mv_free_buffer(&empty);
    TEST_ASSERT_NULL(empty.data);
}

void test_FreeBuffer_SetsDataToNull(void)
{
    mv_audio_buffer audio = {0};
    make_audio(&audio, 1, 11025, 8, 4);
    TEST_ASSERT_NOT_NULL(audio.data);
    mv_free_buffer(&audio);
    TEST_ASSERT_NULL(audio.data);
    TEST_ASSERT_EQUAL_UINT32(0, audio.data_length);
}

/* =========================================================================
   mv_resample_pcm — guard conditions
   ======================================================================= */

void test_Resample_NullSrc_Returns0(void)
{
    mv_audio_buffer dst = {0};
    TEST_ASSERT_EQUAL_INT(0, mv_resample_pcm(NULL, &dst, 44100));
}

void test_Resample_NullDst_Returns0(void)
{
    mv_audio_buffer src = {0};
    make_audio(&src, 1, 11025, 8, 4);
    TEST_ASSERT_EQUAL_INT(0, mv_resample_pcm(&src, NULL, 44100));
    mv_free_buffer(&src);
}

void test_Resample_ZeroRate_Returns0(void)
{
    mv_audio_buffer src = {0};
    make_audio(&src, 1, 11025, 8, 4);
    mv_audio_buffer dst = {0};
    TEST_ASSERT_EQUAL_INT(0, mv_resample_pcm(&src, &dst, 0));
    mv_free_buffer(&src);
}

void test_Resample_NullData_Returns0(void)
{
    mv_audio_buffer src = { 1, 11025, 8, 100, NULL };
    mv_audio_buffer dst = {0};
    TEST_ASSERT_EQUAL_INT(0, mv_resample_pcm(&src, &dst, 22050));
}

/* =========================================================================
   mv_resample_pcm — identity
   ======================================================================= */

void test_Resample_Identity_CopiesExactly(void)
{
    mv_audio_buffer src = {0};
    make_audio(&src, 1, 44100, 8, 64);

    mv_audio_buffer dst = {0};
    TEST_ASSERT_EQUAL_INT(1, mv_resample_pcm(&src, &dst, 44100));
    TEST_ASSERT_EQUAL_UINT32(44100, dst.sample_rate);
    TEST_ASSERT_EQUAL_UINT32(src.data_length, dst.data_length);
    TEST_ASSERT_EQUAL_MEMORY(src.data, dst.data, src.data_length);

    mv_free_buffer(&src);
    mv_free_buffer(&dst);
}

/* =========================================================================
   mv_resample_pcm — fast path: 2x upsample
   ======================================================================= */

void test_Resample_FastPath_Upsample2x_Interpolation(void)
{
    mv_audio_buffer src = {0};
    src.channels = 1; src.sample_rate = 11025;
    src.bits_per_sample = 8; src.data_length = 4;
    src.data = (uint8_t*)MINIVOC_MALLOC(4);
    src.data[0] = 0; src.data[1] = 100; src.data[2] = 200; src.data[3] = 200;

    mv_audio_buffer dst = {0};
    TEST_ASSERT_EQUAL_INT(1, mv_resample_pcm(&src, &dst, 22050));
    TEST_ASSERT_EQUAL_UINT32(22050, dst.sample_rate);
    TEST_ASSERT_EQUAL_UINT32(8, dst.data_length);

    TEST_ASSERT_EQUAL_UINT8(0,   dst.data[0]);
    TEST_ASSERT_EQUAL_UINT8(50,  dst.data[1]);
    TEST_ASSERT_EQUAL_UINT8(100, dst.data[2]);
    TEST_ASSERT_EQUAL_UINT8(150, dst.data[3]);

    mv_free_buffer(&src);
    mv_free_buffer(&dst);
}

/* =========================================================================
   mv_resample_pcm — fast path: 4x upsample
   ======================================================================= */

void test_Resample_FastPath_Upsample4x_Interpolation(void)
{
    mv_audio_buffer src = {0};
    src.channels = 1; src.sample_rate = 11025;
    src.bits_per_sample = 8; src.data_length = 3;
    src.data = (uint8_t*)MINIVOC_MALLOC(3);
    src.data[0] = 10; src.data[1] = 50; src.data[2] = 90;

    mv_audio_buffer dst = {0};
    TEST_ASSERT_EQUAL_INT(1, mv_resample_pcm(&src, &dst, 44100));
    TEST_ASSERT_EQUAL_UINT32(44100, dst.sample_rate);
    TEST_ASSERT_EQUAL_UINT32(12, dst.data_length);
    TEST_ASSERT_EQUAL_UINT8(10, dst.data[0]);
    TEST_ASSERT_EQUAL_UINT8(20, dst.data[1]);
    TEST_ASSERT_EQUAL_UINT8(30, dst.data[2]);
    TEST_ASSERT_EQUAL_UINT8(40, dst.data[3]);
    TEST_ASSERT_EQUAL_UINT8(50, dst.data[4]);

    mv_free_buffer(&src);
    mv_free_buffer(&dst);
}

/* =========================================================================
   mv_resample_pcm — fast path: 2x downsample
   ======================================================================= */

void test_Resample_FastPath_Downsample2x_Averaging(void)
{
    mv_audio_buffer src = {0};
    src.channels = 1; src.sample_rate = 22050;
    src.bits_per_sample = 8; src.data_length = 4;
    src.data = (uint8_t*)MINIVOC_MALLOC(4);
    src.data[0] = 10; src.data[1] = 20;
    src.data[2] = 40; src.data[3] = 60;

    mv_audio_buffer dst = {0};
    TEST_ASSERT_EQUAL_INT(1, mv_resample_pcm(&src, &dst, 11025));
    TEST_ASSERT_EQUAL_UINT32(11025, dst.sample_rate);
    TEST_ASSERT_EQUAL_UINT32(2, dst.data_length);
    TEST_ASSERT_EQUAL_UINT8(15, dst.data[0]);
    TEST_ASSERT_EQUAL_UINT8(50, dst.data[1]);

    mv_free_buffer(&src);
    mv_free_buffer(&dst);
}

/* =========================================================================
   mv_resample_pcm — fast path: 4x downsample
   ======================================================================= */

void test_Resample_FastPath_Downsample4x_Averaging(void)
{
    mv_audio_buffer src = {0};
    src.channels = 1; src.sample_rate = 44100;
    src.bits_per_sample = 8; src.data_length = 8;
    src.data = (uint8_t*)MINIVOC_MALLOC(8);
    src.data[0] = 0;  src.data[1] = 4;  src.data[2] = 8;  src.data[3] = 12;
    src.data[4] = 20; src.data[5] = 40; src.data[6] = 60; src.data[7] = 80;

    mv_audio_buffer dst = {0};
    TEST_ASSERT_EQUAL_INT(1, mv_resample_pcm(&src, &dst, 11025));
    TEST_ASSERT_EQUAL_UINT32(11025, dst.sample_rate);
    TEST_ASSERT_EQUAL_UINT32(2, dst.data_length);
    TEST_ASSERT_EQUAL_UINT8(6,  dst.data[0]);
    TEST_ASSERT_EQUAL_UINT8(50, dst.data[1]);

    mv_free_buffer(&src);
    mv_free_buffer(&dst);
}

/* =========================================================================
   mv_resample_pcm — fallback paths
   ======================================================================= */

void test_Resample_Fallback_Stereo16_Downsample(void)
{
    mv_audio_buffer src = {0};
    make_audio(&src, 2, 44100, 16, 100);

    mv_audio_buffer dst = {0};
    TEST_ASSERT_EQUAL_INT(1, mv_resample_pcm(&src, &dst, 32000));
    TEST_ASSERT_EQUAL_UINT32(32000, dst.sample_rate);
    TEST_ASSERT_EQUAL_UINT32(2,     dst.channels);
    TEST_ASSERT_EQUAL_UINT32(16,    dst.bits_per_sample);
    TEST_ASSERT_TRUE(dst.data_length < src.data_length);

    mv_free_buffer(&src);
    mv_free_buffer(&dst);
}

void test_Resample_Fallback_Mono8_ArbitraryRatio(void)
{
    mv_audio_buffer src = {0};
    make_audio(&src, 1, 11025, 8, 64);

    mv_audio_buffer dst = {0};
    TEST_ASSERT_EQUAL_INT(1, mv_resample_pcm(&src, &dst, 8000));
    TEST_ASSERT_EQUAL_UINT32(8000, dst.sample_rate);
    TEST_ASSERT_TRUE(dst.data_length < src.data_length);
    TEST_ASSERT_GREATER_THAN(0u, dst.data_length);

    mv_free_buffer(&src);
    mv_free_buffer(&dst);
}

void test_Resample_Fallback_Mono16_Upsample(void)
{
    mv_audio_buffer src = {0};
    make_audio(&src, 1, 22050, 16, 32);

    mv_audio_buffer dst = {0};
    TEST_ASSERT_EQUAL_INT(1, mv_resample_pcm(&src, &dst, 44100));
    TEST_ASSERT_EQUAL_UINT32(44100, dst.sample_rate);
    TEST_ASSERT_TRUE(dst.data_length > src.data_length);

    mv_free_buffer(&src);
    mv_free_buffer(&dst);
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

    /* Block 0x09 roundtrips */
    RUN_TEST(test_ReadMemory_Block9_Mono8_Roundtrip);
    RUN_TEST(test_ReadMemory_Block9_Stereo16_Roundtrip);

    /* Block 0x01 */
    RUN_TEST(test_ReadMemory_Block1_Standard_ParsesCorrectly);
    RUN_TEST(test_ReadMemory_Block1_ADPCM_Rejected);

    /* Block 0x08 */
    RUN_TEST(test_ReadMemory_Block8_SetsStereoRate);
    RUN_TEST(test_ReadMemory_Block8_NonZeroPack_Rejected);

    /* Unknown blocks */
    RUN_TEST(test_ReadMemory_UnknownBlockType_Skipped);

    /* Failure paths */
    RUN_TEST(test_ReadMemory_Null_Buffer_Returns0);
    RUN_TEST(test_ReadMemory_Null_OutAudio_Returns0);
    RUN_TEST(test_ReadMemory_TooSmall_Returns0);
    RUN_TEST(test_ReadMemory_GarbageHeader_Returns0);
    RUN_TEST(test_ReadMemory_InvalidVersionCheck_Returns0);
    RUN_TEST(test_ReadMemory_EmptyBlocks_Returns0);

    /* Write memory */
    RUN_TEST(test_WriteMemory_Null_Audio_ReturnsNull);
    RUN_TEST(test_WriteMemory_Null_OutSize_ReturnsNull);
    RUN_TEST(test_WriteMemory_Null_Data_ReturnsNull);
    RUN_TEST(test_WriteMemory_SizeIsCorrect);

    /* File I/O */
    RUN_TEST(test_WriteAndReadFile_Mono8Bit_Roundtrip);
    RUN_TEST(test_WriteAndReadFile_Stereo16Bit_Roundtrip);
    RUN_TEST(test_ReadFile_NonExistent_Returns0);

    /* Free buffer */
    RUN_TEST(test_FreeBuffer_NullAudio_DoesNotCrash);
    RUN_TEST(test_FreeBuffer_ZeroInitialised_DoesNotCrash);
    RUN_TEST(test_FreeBuffer_SetsDataToNull);

    /* Resample guards */
    RUN_TEST(test_Resample_NullSrc_Returns0);
    RUN_TEST(test_Resample_NullDst_Returns0);
    RUN_TEST(test_Resample_ZeroRate_Returns0);
    RUN_TEST(test_Resample_NullData_Returns0);

    /* Resample identity */
    RUN_TEST(test_Resample_Identity_CopiesExactly);

    /* Resample fast paths */
    RUN_TEST(test_Resample_FastPath_Upsample2x_Interpolation);
    RUN_TEST(test_Resample_FastPath_Upsample4x_Interpolation);
    RUN_TEST(test_Resample_FastPath_Downsample2x_Averaging);
    RUN_TEST(test_Resample_FastPath_Downsample4x_Averaging);

    /* Resample fallback */
    RUN_TEST(test_Resample_Fallback_Stereo16_Downsample);
    RUN_TEST(test_Resample_Fallback_Mono8_ArbitraryRatio);
    RUN_TEST(test_Resample_Fallback_Mono16_Upsample);

    /* Leak check — must be last */
    RUN_TEST(test_AllocatorBalance_NoLeaks);

    return UNITY_END();
}
