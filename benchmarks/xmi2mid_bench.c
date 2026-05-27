/*
    benchmarks/xmi2mid_bench.c — benchmarks for xmi2mid.h using ubench.h

    Covers: transcoding of minimal, moderate (note-heavy), and large XMI inputs.

    Build (GCC/Clang, from repo root):
        gcc -std=gnu11 -O2 -I. -Ibenchmarks benchmarks/xmi2mid_bench.c -o build/bench/xmi2mid_bench -lm

    Build (MSVC, from repo root):
        cl /std:c11 /O2 /I. /Ibenchmarks benchmarks/xmi2mid_bench.c /Fe:build/bench/xmi2mid_bench.exe
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

#define XMI2MID_IMPLEMENTATION
#include "../xmi2mid.h"
#include "ubench.h"

/* -------------------------------------------------------------------------
   Helpers
   ---------------------------------------------------------------------- */

/*
 * Wrap an event stream in the minimal IFF container expected by x2m_transcode:
 *   3 bytes  garbage (scanned over)
 *   4 bytes  "EVNT"
 *   4 bytes  big-endian chunk size
 *   n bytes  event stream
 */
static uint8_t* build_xmi(const uint8_t* events, size_t event_len, size_t* out_size)
{
    size_t total = 3 + 4 + 4 + event_len;
    uint8_t* buf = (uint8_t*)malloc(total);
    if (!buf) return NULL;

    buf[0] = 0x00; buf[1] = 0x01; buf[2] = 0x02;
    buf[3] = 'E'; buf[4] = 'V'; buf[5] = 'N'; buf[6] = 'T';
    buf[7]  = (uint8_t)((event_len >> 24) & 0xFF);
    buf[8]  = (uint8_t)((event_len >> 16) & 0xFF);
    buf[9]  = (uint8_t)((event_len >> 8)  & 0xFF);
    buf[10] = (uint8_t)(event_len & 0xFF);
    if (event_len > 0)
        memcpy(buf + 11, events, event_len);

    *out_size = total;
    return buf;
}

/*
 * Build an event stream with N note-on/note-off pairs spread across time,
 * followed by a 1-tick delay before EOT.
 *
 * Each note block:
 *   0x01              1-tick delay (3 MIDI ticks)
 *   0x90 note vel dur XMI note-on (ch 0, given note, vel, dur=1 XMI tick)
 *
 * Trailer:
 *   0x01              1-tick delay before EOT
 *   0xFF 0x2F 0x00    End of Track
 *
 * n must be <= 127 so note bytes stay < 0x80.
 */
static uint8_t* build_note_stream(int32_t n, size_t* out_event_len)
{
    /* Each note event: 1 (delay) + 1 (status) + 1 (note) + 1 (vel) + 1 (dur) = 5 bytes */
    size_t event_len = (size_t)n * 5 + 4; /* +4 for delay + EOT */
    uint8_t* buf = (uint8_t*)malloc(event_len);
    if (!buf) return NULL;

    uint8_t* p = buf;
    for (int32_t i = 0; i < n; i++) {
        *p++ = 0x01;          /* 1-tick delay */
        *p++ = 0x90;          /* Note-On ch 0 */
        *p++ = (uint8_t)(i & 0x7F); /* note 0..n-1 */
        *p++ = 0x40;          /* velocity 64 */
        *p++ = 0x04;          /* duration 4 XMI ticks */
    }
    *p++ = 0x01;              /* 1-tick delay before EOT */
    *p++ = 0xFF;
    *p++ = 0x2F;
    *p++ = 0x00;

    *out_event_len = event_len;
    return buf;
}

/* -------------------------------------------------------------------------
   Constants
   ---------------------------------------------------------------------- */

enum {
    N_NOTES_MEDIUM = 64,   /* moderate note-event count  */
    N_NOTES_LARGE  = 500,  /* stress note-event count    */
    N_WARMUP       = 4
};

/* Minimal XMI: tempo + 1-tick delay + EOT */
static const uint8_t k_minimal_events[] = {
    0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20,  /* tempo 500000 µs at t=0 */
    0x01,                                  /* 1-tick delay            */
    0xFF, 0x2F, 0x00                       /* EOT at t=3              */
};

/* =========================================================================
   Fixtures
   ======================================================================= */

struct XmiMinimal {
    uint8_t* xmi;
    size_t   xmi_size;
};

UBENCH_F_SETUP(XmiMinimal)
{
    ubench_fixture->xmi = build_xmi(k_minimal_events, sizeof(k_minimal_events),
                                     &ubench_fixture->xmi_size);
}

UBENCH_F_TEARDOWN(XmiMinimal)
{
    free(ubench_fixture->xmi);
}

/* --- N_NOTES_MEDIUM note events --- */
struct XmiMedium {
    uint8_t* xmi;
    size_t   xmi_size;
};

UBENCH_F_SETUP(XmiMedium)
{
    size_t event_len = 0;
    uint8_t* events = build_note_stream(N_NOTES_MEDIUM, &event_len);
    ubench_fixture->xmi = build_xmi(events, event_len, &ubench_fixture->xmi_size);
    free(events);
}

UBENCH_F_TEARDOWN(XmiMedium)
{
    free(ubench_fixture->xmi);
}

/* --- N_NOTES_LARGE note events --- */
struct XmiLarge {
    uint8_t* xmi;
    size_t   xmi_size;
};

UBENCH_F_SETUP(XmiLarge)
{
    size_t event_len = 0;
    uint8_t* events = build_note_stream(N_NOTES_LARGE, &event_len);
    ubench_fixture->xmi = build_xmi(events, event_len, &ubench_fixture->xmi_size);
    free(events);
}

UBENCH_F_TEARDOWN(XmiLarge)
{
    free(ubench_fixture->xmi);
}

/* =========================================================================
   Benchmarks
   ======================================================================= */

/* Minimal input: tempo + EOT (tests the baseline pipeline overhead) */
UBENCH_EX_F(XmiMinimal, Transcode_Minimal)
{
    for (int w = 0; w < N_WARMUP; w++) {
        size_t mid_len = 0;
        uint8_t* mid = x2m_transcode(ubench_fixture->xmi, ubench_fixture->xmi_size, &mid_len);
        if (mid) X2M_FREE(mid);
    }

    UBENCH_DO_BENCHMARK()
    {
        size_t mid_len = 0;
        uint8_t* mid = x2m_transcode(ubench_fixture->xmi, ubench_fixture->xmi_size, &mid_len);
        if (mid) X2M_FREE(mid);
        UBENCH_DO_NOTHING(&mid);
    }
}

/* Medium: 64 note events — tests sort + note-off synthesis at realistic scale */
UBENCH_EX_F(XmiMedium, Transcode_64Notes)
{
    for (int w = 0; w < N_WARMUP; w++) {
        size_t mid_len = 0;
        uint8_t* mid = x2m_transcode(ubench_fixture->xmi, ubench_fixture->xmi_size, &mid_len);
        if (mid) X2M_FREE(mid);
    }

    UBENCH_DO_BENCHMARK()
    {
        size_t mid_len = 0;
        uint8_t* mid = x2m_transcode(ubench_fixture->xmi, ubench_fixture->xmi_size, &mid_len);
        if (mid) X2M_FREE(mid);
        UBENCH_DO_NOTHING(&mid);
    }
}

/* Large: 500 note events — stresses token list growth and O(n log n) sort */
UBENCH_EX_F(XmiLarge, Transcode_500Notes)
{
    for (int w = 0; w < N_WARMUP; w++) {
        size_t mid_len = 0;
        uint8_t* mid = x2m_transcode(ubench_fixture->xmi, ubench_fixture->xmi_size, &mid_len);
        if (mid) X2M_FREE(mid);
    }

    UBENCH_DO_BENCHMARK()
    {
        size_t mid_len = 0;
        uint8_t* mid = x2m_transcode(ubench_fixture->xmi, ubench_fixture->xmi_size, &mid_len);
        if (mid) X2M_FREE(mid);
        UBENCH_DO_NOTHING(&mid);
    }
}

/* =========================================================================
   Entry point
   ======================================================================= */

UBENCH_MAIN()
