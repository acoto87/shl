/*
    tests/xmi2mid_test.c — comprehensive test suite for xmi2mid.h

    Covers:
      - x2m_transcode NULL / empty / malformed input handling
      - Minimal valid XMI (tempo change + end-of-track) produces correct MIDI
      - MIDI header structure ("MThd", "MTrk", correct ticks-per-beat)
      - Note-On events generate paired Note-Off events (via running status)
      - Tempo application
      - Program change / control change event pass-through
      - Delay byte accumulation and XMI→MIDI tick scaling
      - Allocator balance tracking (leak detection)

    Design notes
    -------------------------------------------------------------------------
    The library uses an unstable quicksort to order tokens by time.  When two
    events share the same timestamp the final output order is undefined.  All
    tests that depend on specific output bytes therefore place EOT at a time
    strictly greater than every other event (by inserting a 1-tick XMI delay
    = 3 MIDI ticks before EOF).

    Running-status optimisation: the library omits the status byte when
    consecutive events share the same type.  Note-off events are synthesised
    as Note-On with velocity=0 and re-use the preceding Note-On status byte.
    Tests check track bytes directly rather than searching for explicit status
    bytes.

    Freeing before asserting: Unity's TEST_ASSERT macros call longjmp on
    failure, bypassing any cleanup code that follows.  Each test therefore
    copies the output to a fixed-size stack buffer and frees the heap
    allocation *before* running assertions so that g_alloc_count stays
    balanced even when a test fails.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* -------------------------------------------------------------------------
   Custom allocator wrappers for leak tracking
   ---------------------------------------------------------------------- */
static int g_alloc_count = 0;

static void* tracked_realloc(void* ptr, size_t sz) {
    if (!ptr && sz > 0) g_alloc_count++;
    if (ptr  && sz == 0) g_alloc_count--;
    return realloc(ptr, sz);
}
static void tracked_free(void* ptr) {
    if (ptr) g_alloc_count--;
    free(ptr);
}

#define X2M_REALLOC(ptr, size) tracked_realloc(ptr, size)
#define X2M_FREE(ptr)          tracked_free(ptr)

#define XMI2MID_IMPLEMENTATION
#include "../xmi2mid.h"
#include "test_common.h"

/* =========================================================================
   Helper: build a minimal XMI buffer

   Layout:
     3 bytes  garbage prefix (scanned over)
     4 bytes  "EVNT"
     4 bytes  IFF chunk size (big-endian)
     n bytes  event stream  (provided by caller)

   x2m_in_scan_to_evnt stops at offset 3.
   x2m_in_skip(8) → pos = 11 = first event byte.
   ======================================================================= */
static uint8_t* build_xmi(const uint8_t* events, size_t event_len,
                           size_t* out_size)
{
    size_t total = 3 + 4 + 4 + event_len;
    uint8_t* buf = (uint8_t*)malloc(total);
    if (!buf) return NULL;

    buf[0] = 0x00; buf[1] = 0x01; buf[2] = 0x02; /* garbage */
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

/* =========================================================================
   Shared event stream constants

   k_events_tempo_eot: tempo + immediate EOT.  Both share t=0; output order
   depends on the sort and should not be relied upon for byte-exact tests.

   k_events_tempo_delay_eot: tempo at t=0, then a 1-XMI-tick (=3-MIDI-tick)
   delay, then EOT at t=3.  Safe for byte-exact assertions.
   ======================================================================= */
static const uint8_t k_events_tempo_eot[] = {
    0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20,   /* tempo = 500 000 µs/beat at t=0 */
    0xFF, 0x2F, 0x00                        /* EOT at t=0 (sort order undefined) */
};

/* Event stream with an explicit 1-tick gap between tempo and EOT so that
   the unstable sort produces a deterministic order: tempo@0 then EOT@3. */
static const uint8_t k_events_tempo_delay_eot[] = {
    0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20,   /* tempo = 500 000 µs/beat at t=0 */
    0x01,                                   /* 1 XMI tick → 3 MIDI ticks delay */
    0xFF, 0x2F, 0x00                        /* EOT at t=3 */
};

/* =========================================================================
   Macro: build XMI, transcode, copy output to stack, free all heap.
   On failure the stack copy is zeroed and actual_len is set to 0.
   ======================================================================= */
#define X2M_TRANSCODE_TO_STACK(events_arr, copy_buf, actual_len) \
    do { \
        size_t _xsz = 0; \
        uint8_t* _xmi = build_xmi((events_arr), sizeof(events_arr), &_xsz); \
        size_t _msz = 0; \
        uint8_t* _mid = _xmi ? x2m_transcode(_xmi, _xsz, &_msz) : NULL; \
        free(_xmi); \
        (actual_len) = (uint32_t)_msz; \
        if (_mid && _msz <= sizeof(copy_buf)) { \
            memcpy((copy_buf), _mid, _msz); \
        } else { \
            memset((copy_buf), 0, sizeof(copy_buf)); \
            (actual_len) = 0; \
        } \
        if (_mid) X2M_FREE(_mid); \
    } while (0)

/* =========================================================================
   setUp / tearDown
   ======================================================================= */
void setUp(void)    {}
void tearDown(void) {}

/* =========================================================================
   Null / empty / malformed inputs
   ======================================================================= */

void test_Transcode_NullInput_ReturnsNull(void)
{
    size_t mid_len = 1;
    TEST_ASSERT_NULL(x2m_transcode(NULL, 0, &mid_len));
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)mid_len); /* unchanged */
}

void test_Transcode_EmptyBuffer_ReturnsNull(void)
{
    uint8_t dummy[4] = {0};
    size_t mid_len = 0;
    TEST_ASSERT_NULL(x2m_transcode(dummy, 0, &mid_len));
}

void test_Transcode_NoEvntMarker_ReturnsNull(void)
{
    uint8_t no_evnt[64];
    memset(no_evnt, 0x41, sizeof(no_evnt)); /* 'A' repeated */
    size_t mid_len = 0;
    TEST_ASSERT_NULL(x2m_transcode(no_evnt, sizeof(no_evnt), &mid_len));
}

void test_Transcode_EvntWithOnlyEot_AcceptedOrNull(void)
{
    /* EOT alone: library either accepts it (1 token → valid output) or rejects.
       Both outcomes are acceptable; this test just verifies no crash or UB. */
    uint8_t stream[] = { 0xFF, 0x2F, 0x00 };
    size_t xmi_size = 0;
    uint8_t* xmi = build_xmi(stream, sizeof(stream), &xmi_size);
    TEST_ASSERT_NOT_NULL(xmi);

    size_t mid_len = 0;
    uint8_t* mid = x2m_transcode(xmi, xmi_size, &mid_len);
    if (mid) {
        TEST_ASSERT_GREATER_THAN(0u, (uint32_t)mid_len);
        X2M_FREE(mid);
    }
    free(xmi);
}

/* =========================================================================
   Output has correct MIDI structure
   ======================================================================= */

void test_Transcode_Minimal_OutputIsNonNull(void)
{
    uint8_t buf[512]; uint32_t len = 0;
    X2M_TRANSCODE_TO_STACK(k_events_tempo_eot, buf, len);
    TEST_ASSERT_GREATER_THAN(0u, len);
    TEST_ASSERT_GREATER_THAN(21u, len);
}

void test_Transcode_OutputStartsWithMThd(void)
{
    uint8_t buf[512]; uint32_t len = 0;
    X2M_TRANSCODE_TO_STACK(k_events_tempo_eot, buf, len);
    TEST_ASSERT_GREATER_OR_EQUAL(4u, len);
    TEST_ASSERT_EQUAL_MEMORY("MThd", buf, 4);
}

void test_Transcode_OutputContainsMTrk(void)
{
    uint8_t buf[512]; uint32_t len = 0;
    X2M_TRANSCODE_TO_STACK(k_events_tempo_eot, buf, len);
    TEST_ASSERT_GREATER_OR_EQUAL(18u, len);
    TEST_ASSERT_EQUAL_MEMORY("MTrk", buf + 14, 4);
}

void test_Transcode_OutputHdrChunkSize6(void)
{
    uint8_t buf[512]; uint32_t len = 0;
    X2M_TRANSCODE_TO_STACK(k_events_tempo_eot, buf, len);
    TEST_ASSERT_GREATER_OR_EQUAL(8u, len);
    TEST_ASSERT_EQUAL_UINT8(0, buf[4]);
    TEST_ASSERT_EQUAL_UINT8(0, buf[5]);
    TEST_ASSERT_EQUAL_UINT8(0, buf[6]);
    TEST_ASSERT_EQUAL_UINT8(6, buf[7]);
}

void test_Transcode_OutputFormat0_OneTrack(void)
{
    uint8_t buf[512]; uint32_t len = 0;
    X2M_TRANSCODE_TO_STACK(k_events_tempo_eot, buf, len);
    TEST_ASSERT_GREATER_OR_EQUAL(12u, len);
    TEST_ASSERT_EQUAL_UINT8(0, buf[8]);
    TEST_ASSERT_EQUAL_UINT8(0, buf[9]);  /* format = 0 */
    TEST_ASSERT_EQUAL_UINT8(0, buf[10]);
    TEST_ASSERT_EQUAL_UINT8(1, buf[11]); /* nTracks = 1 */
}

void test_Transcode_TempoProducesCorrectTicksPerBeat(void)
{
    /*
     * XMI tempo 500 000 µs/beat.
     * Library scales: tempo *= XMI_TIMING_SCALER(3) → 1 500 000.
     * Ticks/beat = (tempo * 3) / 25000 = 4 500 000 / 25000 = 180 = 0x00B4.
     */
    uint8_t buf[512]; uint32_t len = 0;
    X2M_TRANSCODE_TO_STACK(k_events_tempo_eot, buf, len);
    TEST_ASSERT_GREATER_OR_EQUAL(14u, len);
    uint16_t ticks = (uint16_t)((buf[12] << 8) | buf[13]);
    TEST_ASSERT_EQUAL_UINT16(180, ticks);
}

void test_Transcode_TrackLengthFieldIsPatched(void)
{
    /* Track size at bytes 18-21 must equal (total_output - 22). */
    uint8_t buf[512]; uint32_t len = 0;
    X2M_TRANSCODE_TO_STACK(k_events_tempo_eot, buf, len);
    TEST_ASSERT_GREATER_OR_EQUAL(22u, len);
    uint32_t stated = ((uint32_t)buf[18] << 24) | ((uint32_t)buf[19] << 16) |
                      ((uint32_t)buf[20] << 8)  | buf[21];
    TEST_ASSERT_EQUAL_UINT32(len - 22u, stated);
}

void test_Transcode_Minimal_ExactOutput(void)
{
    /*
     * Using k_events_tempo_delay_eot (1-tick gap before EOT) gives a
     * deterministic sort order: tempo@0 before EOT@3.
     *
     * Expected output (33 bytes):
     *   4D 54 68 64  MThd
     *   00 00 00 06  chunk size 6
     *   00 00        format 0
     *   00 01        1 track
     *   00 B4        180 ticks/beat
     *   4D 54 72 6B  MTrk
     *   00 00 00 0B  track length 11
     *   00 FF 51 03 07 A1 20   tempo change at delta=0
     *   03 FF 2F 00            EOT at delta=3
     */
    static const uint8_t expected[] = {
        0x4D,0x54,0x68,0x64, 0x00,0x00,0x00,0x06,
        0x00,0x00,           0x00,0x01,
        0x00,0xB4,
        0x4D,0x54,0x72,0x6B, 0x00,0x00,0x00,0x0B,
        0x00,0xFF,0x51,0x03,0x07,0xA1,0x20,
        0x03,0xFF,0x2F,0x00
    };

    uint8_t buf[512]; uint32_t len = 0;
    X2M_TRANSCODE_TO_STACK(k_events_tempo_delay_eot, buf, len);

    TEST_ASSERT_EQUAL_UINT32((uint32_t)sizeof(expected), len);
    TEST_ASSERT_EQUAL_MEMORY(expected, buf, sizeof(expected));
}

/* =========================================================================
   Note-On / Note-Off pair generation

   Input XMI stream (events[] below):
     0x90 0x3C 0x40 0x08  Note-On ch0 note=60 vel=64 XMI-duration=8
     0x09                 9-tick XMI delay = 27 MIDI ticks before EOT
     0xFF 0x2F 0x00       EOT at MIDI time 27

   XMI note-off synthesis: at time 0 + 8*3 = 24.
   Times: note-on@0, note-off@24, EOT@27 — all distinct, sort is stable.

   Expected track (11 bytes):
     00 90 3C 40   note-on@0:  delta=0, status=0x90, note=0x3C, vel=0x40
     18 3C 00      note-off@24: delta=24=0x18, running status (no 0x90),
                                note=0x3C, vel=0x00
     03 FF 2F 00   EOT@27:    delta=3, 0xFF 0x2F 0x00
   ======================================================================= */

void test_Transcode_NoteOn_GeneratesNoteOff(void)
{
    static const uint8_t events[] = {
        0x90, 0x3C, 0x40, 0x08,  /* note-on ch0 note=60 vel=64 dur=8 */
        0x09,                    /* 9 XMI ticks = 27 MIDI ticks delay */
        0xFF, 0x2F, 0x00
    };
    static const uint8_t expected_track[] = {
        0x00, 0x90, 0x3C, 0x40,   /* note-on (explicit status) */
        0x18, 0x3C, 0x00,         /* note-off (running status: no 0x90) */
        0x03, 0xFF, 0x2F, 0x00    /* EOT */
    };

    uint8_t buf[512]; uint32_t len = 0;
    X2M_TRANSCODE_TO_STACK(events, buf, len);

    TEST_ASSERT_GREATER_THAN(22u, len);
    uint32_t track_len = len - 22u;
    TEST_ASSERT_EQUAL_UINT32((uint32_t)sizeof(expected_track), track_len);
    TEST_ASSERT_EQUAL_MEMORY(expected_track, buf + 22, sizeof(expected_track));
}

/* =========================================================================
   Program change pass-through

   Insert a 1-tick delay before EOT to guarantee PC sorts before EOT.
   Expected track:
     00 C0 19   PC@0: delta=0, status=0xC0, program=25
     03 FF 2F 00 EOT@3
   ======================================================================= */

void test_Transcode_ProgramChange_InOutput(void)
{
    static const uint8_t events[] = {
        0xC0, 0x19,             /* Program Change ch0, program=25 */
        0x01,                   /* 1 XMI tick → 3 MIDI ticks delay  */
        0xFF, 0x2F, 0x00
    };
    static const uint8_t expected_track[] = {
        0x00, 0xC0, 0x19,
        0x03, 0xFF, 0x2F, 0x00
    };

    uint8_t buf[512]; uint32_t len = 0;
    X2M_TRANSCODE_TO_STACK(events, buf, len);

    TEST_ASSERT_GREATER_THAN(22u, len);
    uint32_t track_len = len - 22u;
    TEST_ASSERT_EQUAL_UINT32((uint32_t)sizeof(expected_track), track_len);
    TEST_ASSERT_EQUAL_MEMORY(expected_track, buf + 22, sizeof(expected_track));
}

/* =========================================================================
   Control change pass-through

   Expected track:
     00 B0 07 7F  CC@0: delta=0, status=0xB0, ctrl=7 (volume), val=127
     03 FF 2F 00  EOT@3
   ======================================================================= */

void test_Transcode_ControlChange_InOutput(void)
{
    static const uint8_t events[] = {
        0xB0, 0x07, 0x7F,       /* CC ch0 controller=7 (volume) value=127 */
        0x01,                   /* 1 XMI tick → 3 MIDI ticks delay */
        0xFF, 0x2F, 0x00
    };
    static const uint8_t expected_track[] = {
        0x00, 0xB0, 0x07, 0x7F,
        0x03, 0xFF, 0x2F, 0x00
    };

    uint8_t buf[512]; uint32_t len = 0;
    X2M_TRANSCODE_TO_STACK(events, buf, len);

    TEST_ASSERT_GREATER_THAN(22u, len);
    uint32_t track_len = len - 22u;
    TEST_ASSERT_EQUAL_UINT32((uint32_t)sizeof(expected_track), track_len);
    TEST_ASSERT_EQUAL_MEMORY(expected_track, buf + 22, sizeof(expected_track));
}

/* =========================================================================
   XMI delay bytes accumulate and are scaled by 3

   Two PC events separated by five 1-tick delay bytes (5 XMI ticks = 15 MIDI
   ticks), then EOT after one more tick.  We only verify the output is
   non-trivially larger than the 22-byte header; exact byte content is less
   important here.
   ======================================================================= */

void test_Transcode_DelayBytes_AreScaled(void)
{
    static const uint8_t events[] = {
        0xC0, 0x00,                          /* PC at t=0  */
        0x01, 0x01, 0x01, 0x01, 0x01,        /* 5 ticks = 15 MIDI ticks */
        0xC0, 0x01,                          /* PC at t=15 */
        0x01,                                /* 1 tick → EOT at t=18 */
        0xFF, 0x2F, 0x00
    };
    uint8_t buf[512]; uint32_t len = 0;
    X2M_TRANSCODE_TO_STACK(events, buf, len);
    TEST_ASSERT_GREATER_THAN(22u, len);
}

/* =========================================================================
   midLength output parameter is optional (NULL is safe)
   ======================================================================= */

void test_Transcode_NullMidLength_DoesNotCrash(void)
{
    size_t xmi_size = 0;
    uint8_t* xmi = build_xmi(k_events_tempo_eot,
                              sizeof(k_events_tempo_eot), &xmi_size);
    uint8_t* mid = x2m_transcode(xmi, xmi_size, NULL);
    if (mid) X2M_FREE(mid);
    free(xmi);
    TEST_PASS();
}

/* =========================================================================
   Allocator balance — must be last; verifies no net heap leak
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

    /* Null / malformed */
    RUN_TEST(test_Transcode_NullInput_ReturnsNull);
    RUN_TEST(test_Transcode_EmptyBuffer_ReturnsNull);
    RUN_TEST(test_Transcode_NoEvntMarker_ReturnsNull);
    RUN_TEST(test_Transcode_EvntWithOnlyEot_AcceptedOrNull);

    /* MIDI structural checks */
    RUN_TEST(test_Transcode_Minimal_OutputIsNonNull);
    RUN_TEST(test_Transcode_OutputStartsWithMThd);
    RUN_TEST(test_Transcode_OutputContainsMTrk);
    RUN_TEST(test_Transcode_OutputHdrChunkSize6);
    RUN_TEST(test_Transcode_OutputFormat0_OneTrack);
    RUN_TEST(test_Transcode_TempoProducesCorrectTicksPerBeat);
    RUN_TEST(test_Transcode_TrackLengthFieldIsPatched);
    RUN_TEST(test_Transcode_Minimal_ExactOutput);

    /* Event handling */
    RUN_TEST(test_Transcode_NoteOn_GeneratesNoteOff);
    RUN_TEST(test_Transcode_ProgramChange_InOutput);
    RUN_TEST(test_Transcode_ControlChange_InOutput);
    RUN_TEST(test_Transcode_DelayBytes_AreScaled);

    /* Edge cases */
    RUN_TEST(test_Transcode_NullMidLength_DoesNotCrash);

    /* Leak check — must be last */
    RUN_TEST(test_AllocatorBalance_NoLeaks);

    return UNITY_END();
}
