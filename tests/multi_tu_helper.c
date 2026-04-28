/* =============================================================================
   multi_tu_helper.c

   Declaration-only translation unit.  Includes all five implementation-guarded
   single-header libraries WITHOUT their _IMPLEMENTATION define and exposes
   helper functions that exercise each library's API from this TU.  The actual
   implementations are provided by multi_tu_test.c, which is compiled as a
   separate object and linked together with this file.

   This file therefore proves that every library's symbols are correctly
   exported and that the linker resolves cross-TU calls without duplicate-
   symbol or undefined-symbol errors.
   ============================================================================= */

#include "../memzone.h"
#include "../wstr.h"
#include "../memory_buffer.h"
#include "../wav.h"
#include "../flic.h"

#include <stdbool.h>
#include <stdint.h>

/* -------------------------------------------------------------------------
   memzone helpers
   memzone_t* is always an opaque pointer in the public API, so all calls
   work correctly even though the struct body is not visible here.
   ------------------------------------------------------------------------- */

memzone_t* helper_memzone_create(size_t size)
{
    return mz_init(size);
}

bool helper_memzone_alloc_and_write(memzone_t* zone, int value)
{
    int* p = (int*)mz_alloc(zone, sizeof(int));
    if (!p) return false;
    *p = value;
    return *p == value;
}

void helper_memzone_destroy(memzone_t* zone)
{
    mz_destroy(zone);
}

/* -------------------------------------------------------------------------
   wstr helpers
   StringView and String are fully-defined structs in the public API, so
   stack-allocation and by-value passing work without SHL_WSTR_IMPLEMENTATION.
   ------------------------------------------------------------------------- */

bool helper_wstr_roundtrip(void)
{
    String s = wstr_fromCString("multi_tu");
    bool ok = wstr_view(&s).length == 8 &&
              wsv_equals(wstr_view(&s), wsv_fromCString("multi_tu"));
    wstr_free(s);
    return ok;
}

bool helper_wstr_append(void)
{
    String s = wstr_fromCString("foo");
    bool ok = wstr_appendCString(&s, "bar") &&
              wstr_view(&s).length == 6 &&
              wsv_equals(wstr_view(&s), wsv_fromCString("foobar"));
    wstr_free(s);
    return ok;
}

/* -------------------------------------------------------------------------
   memory_buffer helpers
   memory_buffer_t is an incomplete type in this TU (its struct body is
   inside #ifdef SHL_MEMORY_BUFFER_IMPLEMENTATION).  The implementation TU
   allocates the object on the stack and passes a pointer here; using a
   pointer to an incomplete type is valid in C99.
   ------------------------------------------------------------------------- */

bool helper_mb_write_byte(memory_buffer_t* buf, uint8_t value)
{
    return mb_write(buf, value);
}

bool helper_mb_read_byte(memory_buffer_t* buf, uint8_t* out)
{
    return mb_read(buf, out);
}

bool helper_mb_seek(memory_buffer_t* buf, uint32_t pos)
{
    return mb_seek(buf, pos);
}

/* -------------------------------------------------------------------------
   wav helpers
   wav_file_t is an incomplete type in this TU (its struct body is inside
   #ifdef SHL_WAV_IMPLEMENTATION).  The implementation TU allocates the
   object on the stack and passes a pointer here.
   ------------------------------------------------------------------------- */

bool helper_wav_write_samples(wav_file_t* wf, const wav_sample_t* samples,
                              long count)
{
    return wav_write(wf, samples, count, 1);
}

/* -------------------------------------------------------------------------
   flic helpers
   Flic is a fully-defined struct in the public API section, so it can be
   stack-allocated here without SHL_FLIC_IMPLEMENTATION.
   ------------------------------------------------------------------------- */

bool helper_flic_open_missing(void)
{
    Flic flic;
    /* Attempting to open a file that does not exist must return false. */
    return !flicOpen(&flic, "file_that_does_not_exist_for_multi_tu_test.flc");
}
