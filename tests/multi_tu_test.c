/* =============================================================================
   multi_tu_test.c

   Multi-translation-unit test.  This file provides the _IMPLEMENTATION for
   all five single-header libraries AND contains the Unity test runner.  It
   is compiled together with multi_tu_helper.c, which includes the same
   headers WITHOUT any _IMPLEMENTATION define.

   Together the two files verify that:
     1. Symbols are exported exactly once (no ODR / duplicate-symbol errors).
     2. Calls made from the non-implementation TU (helper.c) resolve correctly
        at link time (no undefined-symbol errors).
     3. Runtime behaviour is correct when functions are called across TU
        boundaries.
   ============================================================================= */

/* Provide the implementation for all five libraries in this translation unit. */
#define SHL_MZ_IMPLEMENTATION
#include "../memzone.h"

#define SHL_WSTR_IMPLEMENTATION
#include "../wstr.h"

#define SHL_MEMORY_BUFFER_IMPLEMENTATION
#include "../memory_buffer.h"

#define SHL_WAV_IMPLEMENTATION
#include "../wav.h"

#define SHL_FLIC_IMPLEMENTATION
#include "../flic.h"

#include "test_common.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* -------------------------------------------------------------------------
   Forward-declarations of helpers defined in multi_tu_helper.c.
   The linker resolves these calls to the symbols compiled in that TU.
   ------------------------------------------------------------------------- */

memzone_t* helper_memzone_create(size_t size);
bool       helper_memzone_alloc_and_write(memzone_t* zone, int value);
void       helper_memzone_destroy(memzone_t* zone);

bool helper_wstr_roundtrip(void);
bool helper_wstr_append(void);

bool helper_mb_write_byte(memory_buffer_t* buf, uint8_t value);
bool helper_mb_read_byte(memory_buffer_t* buf, uint8_t* out);
bool helper_mb_seek(memory_buffer_t* buf, uint32_t pos);

bool helper_wav_write_samples(wav_file_t* wf, const wav_sample_t* samples,
                              long count);

bool helper_flic_open_missing(void);

/* -------------------------------------------------------------------------
   Unity boilerplate
   ------------------------------------------------------------------------- */

void setUp(void)    {}
void tearDown(void) {}

/* =========================================================================
   memzone multi-TU tests
   ========================================================================= */

static void test_multi_tu_memzone_create_and_destroy(void)
{
    /* Zone created by the helper (non-implementation TU). */
    memzone_t* zone = helper_memzone_create(4096);
    TEST_ASSERT_NOT_NULL(zone);
    helper_memzone_destroy(zone);
}

static void test_multi_tu_memzone_alloc_in_helper(void)
{
    /* Zone created and allocation performed in the helper TU. */
    memzone_t* zone = helper_memzone_create(4096);
    TEST_ASSERT_NOT_NULL(zone);
    TEST_ASSERT_TRUE(helper_memzone_alloc_and_write(zone, 99));
    helper_memzone_destroy(zone);
}

static void test_multi_tu_memzone_alloc_in_test_tu(void)
{
    /* Zone created in helper TU, allocation performed here. */
    memzone_t* zone = helper_memzone_create(4096);
    TEST_ASSERT_NOT_NULL(zone);
    int* p = (int*)mz_alloc(zone, sizeof(int));
    TEST_ASSERT_NOT_NULL(p);
    *p = 1234;
    TEST_ASSERT_EQUAL_INT(1234, *p);
    helper_memzone_destroy(zone);
}

/* =========================================================================
   wstr multi-TU tests
   ========================================================================= */

static void test_multi_tu_wstr_roundtrip(void)
{
    /* String created, used, and freed entirely inside the helper TU. */
    TEST_ASSERT_TRUE(helper_wstr_roundtrip());
}

static void test_multi_tu_wstr_append(void)
{
    /* append path exercised inside the helper TU. */
    TEST_ASSERT_TRUE(helper_wstr_append());
}

static void test_multi_tu_wstr_in_test_tu(void)
{
    /* String created here, confirming the implementation is visible. */
    String s = wstr_fromCString("hello");
    TEST_ASSERT_EQUAL_size_t(5, wstr_view(&s).length);
    wstr_free(s);
}

/* =========================================================================
   memory_buffer multi-TU tests
   memory_buffer_t is a complete type here (after _IMPLEMENTATION).
   The helper TU operates on it through a pointer, which is the canonical
   multi-TU usage pattern for opaque/pimpl types.
   ========================================================================= */

static void test_multi_tu_mb_write_and_read(void)
{
    memory_buffer_t buf;
    mb_initEmpty(&buf);

    /* Write performed in helper TU. */
    TEST_ASSERT_TRUE(helper_mb_write_byte(&buf, 0xAB));

    /* Seek back via helper TU. */
    TEST_ASSERT_TRUE(helper_mb_seek(&buf, 0));

    /* Read back via helper TU. */
    uint8_t v = 0;
    TEST_ASSERT_TRUE(helper_mb_read_byte(&buf, &v));
    TEST_ASSERT_EQUAL_HEX8(0xAB, v);

    mb_free(&buf);
}

static void test_multi_tu_mb_multiple_writes(void)
{
    memory_buffer_t buf;
    mb_initEmpty(&buf);

    for (uint8_t i = 0; i < 8; ++i)
        TEST_ASSERT_TRUE(helper_mb_write_byte(&buf, i));

    TEST_ASSERT_TRUE(helper_mb_seek(&buf, 0));

    for (uint8_t i = 0; i < 8; ++i)
    {
        uint8_t v = 0xFF;
        TEST_ASSERT_TRUE(helper_mb_read_byte(&buf, &v));
        TEST_ASSERT_EQUAL_UINT8(i, v);
    }

    mb_free(&buf);
}

/* =========================================================================
   wav multi-TU tests
   wav_file_t is a complete type here (after _IMPLEMENTATION).  The struct
   is allocated on the stack in this TU and a pointer is passed to the
   helper TU for the write call, exercising the cross-TU boundary.
   ========================================================================= */

static void test_multi_tu_wav_init_write_flush(void)
{
    const char* path = "multi_tu_wav_test.wav";
    wav_file_t wf;
    TEST_ASSERT_TRUE(wav_init(&wf, 44100, path));

    /* Write performed in helper TU. */
    wav_sample_t samples[8] = { 0, 1000, 2000, -1000, -2000, 500, -500, 0 };
    TEST_ASSERT_TRUE(helper_wav_write_samples(&wf, samples, 8));

    TEST_ASSERT_TRUE(wav_flush(&wf, true));
    remove(path);
}

static void test_multi_tu_wav_stereo_flag(void)
{
    const char* path = "multi_tu_wav_stereo_test.wav";
    wav_file_t wf;
    TEST_ASSERT_TRUE(wav_init(&wf, 22050, path));
    wav_stereo(&wf, true);
    TEST_ASSERT_EQUAL_INT(0, (int)wav_sampleCount(&wf));

    wav_sample_t stereo[4] = { 100, -100, 200, -200 };
    TEST_ASSERT_TRUE(helper_wav_write_samples(&wf, stereo, 4));

    TEST_ASSERT_TRUE(wav_flush(&wf, true));
    remove(path);
}

/* =========================================================================
   flic multi-TU tests
   Flic is a fully-defined struct in the public API, so the helper can
   stack-allocate it without the implementation define.
   ========================================================================= */

static void test_multi_tu_flic_open_missing(void)
{
    /* Opening a nonexistent file must return false in both TUs. */
    TEST_ASSERT_TRUE(helper_flic_open_missing());
}

static void test_multi_tu_flic_open_missing_in_test_tu(void)
{
    /* Same check performed directly in the implementation TU. */
    Flic flic;
    TEST_ASSERT_FALSE(flicOpen(&flic, "no_such_file_for_multi_tu.flc"));
}

/* =========================================================================
   Runner
   ========================================================================= */

int main(void)
{
    UNITY_BEGIN();

    /* memzone */
    RUN_TEST(test_multi_tu_memzone_create_and_destroy);
    RUN_TEST(test_multi_tu_memzone_alloc_in_helper);
    RUN_TEST(test_multi_tu_memzone_alloc_in_test_tu);

    /* wstr */
    RUN_TEST(test_multi_tu_wstr_roundtrip);
    RUN_TEST(test_multi_tu_wstr_append);
    RUN_TEST(test_multi_tu_wstr_in_test_tu);

    /* memory_buffer */
    RUN_TEST(test_multi_tu_mb_write_and_read);
    RUN_TEST(test_multi_tu_mb_multiple_writes);

    /* wav */
    RUN_TEST(test_multi_tu_wav_init_write_flush);
    RUN_TEST(test_multi_tu_wav_stereo_flag);

    /* flic */
    RUN_TEST(test_multi_tu_flic_open_missing);
    RUN_TEST(test_multi_tu_flic_open_missing_in_test_tu);

    return UNITY_END();
}
