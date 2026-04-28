/*
 * Unit tests for the SHL_MZ_AUDIT facility in memzone.h.
 *
 * Strategy: drive the public API, then read the produced log file and assert
 * on its textual content.  This keeps the tests decoupled from internal
 * implementation details while giving full coverage of every logged event,
 * both output formats, the flush/close lifecycle, all three realloc
 * strategies, and the per-zone design (two zones writing to separate files).
 *
 * SHL_MZ_AUDIT_VERBOSE is defined so the block-list feature can also be exercised.
 *
 * Key API contract under test:
 *   mz_initAudit(size, format, filepath) — create a zone with audit enabled;
 *       INIT event is captured immediately.
 *   mz_auditConfigure(zone, format, filepath) — attach / reconfigure audit on
 *       an existing zone; no INIT event is logged.
 *   mz_auditFlush(zone) — flush the zone's log without closing it.
 *   mz_auditClose(zone) — flush and close the zone's log.
 *   mz_destroy(zone)    — automatically closes the log before freeing memory.
 */

#define SHL_MZ_IMPLEMENTATION
#define SHL_MZ_AUDIT_IMPLEMENTATION
#define SHL_MZ_AUDIT_VERBOSE
#include "../memzone_audit.h"
#include "test_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =========================================================================
   Helpers
   ========================================================================= */

#define AUDIT_TMP "audit_test_tmp.log"

static void remove_tmp(void)
{
    remove(AUDIT_TMP);
}

static int file_exists(const char* path)
{
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    fclose(f);
    return 1;
}

/*
 * Returns non-zero if any line in the file contains `needle` as a substring.
 */
static int file_contains(const char* path, const char* needle)
{
    FILE* f = fopen(path, "r");
    char line[512];
    int found = 0;

    if (!f) return 0;

    while (!found && fgets(line, sizeof(line), f))
        if (strstr(line, needle))
            found = 1;

    fclose(f);
    return found;
}

/*
 * Returns the number of lines in `path` that contain `needle`.
 */
static int count_occurrences(const char* path, const char* needle)
{
    FILE* f = fopen(path, "r");
    char line[512];
    int count = 0;

    if (!f) return 0;

    while (fgets(line, sizeof(line), f))
        if (strstr(line, needle))
            count++;

    fclose(f);
    return count;
}

/* =========================================================================
   setUp / tearDown — called by Unity before and after every test
   ========================================================================= */

void setUp(void)
{
    remove_tmp();
}

void tearDown(void)
{
    remove_tmp();
}

/* =========================================================================
   1. Log-file lifecycle
   ========================================================================= */

static void test_audit_creates_log_file(void)
{
    memzone_t* zone = mz_initAudit(4096, SHL_MZ_AUDIT_FORMAT_VERBOSE, AUDIT_TMP);
    mz_destroy(zone); /* logs DESTROY and closes file automatically */

    TEST_ASSERT_TRUE(file_exists(AUDIT_TMP));
}

static void test_audit_flush_makes_data_readable_before_close(void)
{
    memzone_t* zone = mz_initAudit(4096, SHL_MZ_AUDIT_FORMAT_VERBOSE, AUDIT_TMP);

    mz_auditFlush(zone); /* explicit flush without close */

    /* The INIT event must already be on disk after an explicit flush. */
    TEST_ASSERT_TRUE(file_exists(AUDIT_TMP));
    TEST_ASSERT_TRUE(file_contains(AUDIT_TMP, "INIT"));

    mz_destroy(zone);
}

static void test_audit_second_init_audit_truncates_file(void)
{
    /* First session: write verbose content. */
    memzone_t* z1 = mz_initAudit(4096, SHL_MZ_AUDIT_FORMAT_VERBOSE, AUDIT_TMP);
    mz_destroy(z1);

    /* Second session on the same file, compact format — default mode truncates. */
    memzone_t* z2 = mz_initAudit(4096, SHL_MZ_AUDIT_FORMAT_COMPACT, AUDIT_TMP);
    mz_destroy(z2);

    TEST_ASSERT_TRUE(file_contains(AUDIT_TMP, "format   : compact"));
    TEST_ASSERT_FALSE(file_contains(AUDIT_TMP, "format   : verbose"));
}

static void test_audit_close_when_not_open_is_noop(void)
{
    /* mz_init (without audit) gives a zone with auditFp == NULL. */
    memzone_t* zone = mz_init(4096);
    mz_auditClose(zone); /* no-op — nothing to close */
    mz_auditClose(zone); /* calling again must not crash */
    mz_destroy(zone);
    TEST_ASSERT_TRUE(1); /* reaching here means no crash */
}

static void test_audit_explicit_close_then_destroy_is_safe(void)
{
    /* mz_auditClose then mz_destroy must not double-close or crash. */
    memzone_t* zone = mz_initAudit(4096, SHL_MZ_AUDIT_FORMAT_VERBOSE, AUDIT_TMP);
    mz_auditClose(zone);  /* explicitly closes the log */
    mz_destroy(zone);     /* destroy wrapper sees auditFp==NULL — no second close */
    TEST_ASSERT_TRUE(file_exists(AUDIT_TMP));
}

/* =========================================================================
   2. Log-file header
   ========================================================================= */

static void test_audit_verbose_header_says_verbose(void)
{
    memzone_t* zone = mz_initAudit(4096, SHL_MZ_AUDIT_FORMAT_VERBOSE, AUDIT_TMP);
    mz_destroy(zone);

    TEST_ASSERT_TRUE(file_contains(AUDIT_TMP, "format   : verbose"));
}

static void test_audit_compact_header_says_compact(void)
{
    memzone_t* zone = mz_initAudit(4096, SHL_MZ_AUDIT_FORMAT_COMPACT, AUDIT_TMP);
    mz_destroy(zone);

    TEST_ASSERT_TRUE(file_contains(AUDIT_TMP, "format   : compact"));
}

static void test_audit_header_says_truncate_by_default(void)
{
    memzone_t* zone = mz_initAudit(4096, SHL_MZ_AUDIT_FORMAT_VERBOSE, AUDIT_TMP);
    mz_destroy(zone);

    TEST_ASSERT_TRUE(file_contains(AUDIT_TMP, "mode     : truncate"));
}

/* =========================================================================
   3. All seven operation types are logged (verbose)
   ========================================================================= */

static void test_audit_verbose_logs_init_event(void)
{
    memzone_t* zone = mz_initAudit(4096, SHL_MZ_AUDIT_FORMAT_VERBOSE, AUDIT_TMP);
    mz_destroy(zone);

    TEST_ASSERT_TRUE(file_contains(AUDIT_TMP, "INIT"));
}

static void test_audit_verbose_logs_destroy_event(void)
{
    memzone_t* zone = mz_initAudit(4096, SHL_MZ_AUDIT_FORMAT_VERBOSE, AUDIT_TMP);
    mz_destroy(zone);

    TEST_ASSERT_TRUE(file_contains(AUDIT_TMP, "DESTROY"));
}

static void test_audit_verbose_logs_alloc_event(void)
{
    memzone_t* zone = mz_initAudit(4096, SHL_MZ_AUDIT_FORMAT_VERBOSE, AUDIT_TMP);
    void* p = mz_alloc(zone, 64);
    mz_free(zone, p);
    mz_destroy(zone);

    TEST_ASSERT_TRUE(file_contains(AUDIT_TMP, "ALLOC"));
}

static void test_audit_verbose_logs_free_event(void)
{
    memzone_t* zone = mz_initAudit(4096, SHL_MZ_AUDIT_FORMAT_VERBOSE, AUDIT_TMP);
    void* p = mz_alloc(zone, 64);
    mz_free(zone, p);
    mz_destroy(zone);

    TEST_ASSERT_TRUE(file_contains(AUDIT_TMP, "FREE"));
}

static void test_audit_verbose_logs_alloc_aligned_event(void)
{
    memzone_t* zone = mz_initAudit(4096, SHL_MZ_AUDIT_FORMAT_VERBOSE, AUDIT_TMP);
    void* p = mz_allocAligned(zone, 64, 32);
    mz_free(zone, p);
    mz_destroy(zone);

    TEST_ASSERT_TRUE(file_contains(AUDIT_TMP, "ALLOC_ALIGNED"));
}

static void test_audit_verbose_logs_reset_event(void)
{
    memzone_t* zone = mz_initAudit(4096, SHL_MZ_AUDIT_FORMAT_VERBOSE, AUDIT_TMP);
    mz_alloc(zone, 64);
    mz_reset(zone);
    mz_destroy(zone);

    TEST_ASSERT_TRUE(file_contains(AUDIT_TMP, "RESET"));
}

static void test_audit_verbose_logs_realloc_event(void)
{
    memzone_t* zone = mz_initAudit(4096, SHL_MZ_AUDIT_FORMAT_VERBOSE, AUDIT_TMP);
    void* p = mz_alloc(zone, 64);
    p = mz_realloc(zone, p, 32);
    mz_free(zone, p);
    mz_destroy(zone);

    TEST_ASSERT_TRUE(file_contains(AUDIT_TMP, "REALLOC"));
}

/* =========================================================================
   4. Event count and sequence numbers
   ========================================================================= */

static void test_audit_verbose_event_count_matches_operations(void)
{
    memzone_t* zone = mz_initAudit(4096, SHL_MZ_AUDIT_FORMAT_VERBOSE, AUDIT_TMP);
    void* p = mz_alloc(zone, 64);       /* ALLOC   */
    mz_free(zone, p);                    /* FREE    */
    mz_destroy(zone);                    /* DESTROY */

    /* "EVENT #" appears exactly once per event block header:
       INIT, ALLOC, FREE, DESTROY = 4 */
    TEST_ASSERT_EQUAL_INT(4, count_occurrences(AUDIT_TMP, "EVENT #"));
}

static void test_audit_compact_event_count_matches_operations(void)
{
    memzone_t* zone = mz_initAudit(4096, SHL_MZ_AUDIT_FORMAT_COMPACT, AUDIT_TMP);
    void* p = mz_alloc(zone, 64);       /* ALLOC   */
    mz_free(zone, p);                    /* FREE    */
    mz_destroy(zone);                    /* DESTROY */

    /*
     * In compact format each event is a single line that starts with "#NNNN".
     * Comment lines start with "# " so they don't match event keyword searches.
     */
    TEST_ASSERT_EQUAL_INT(1, count_occurrences(AUDIT_TMP, " INIT "));
    TEST_ASSERT_EQUAL_INT(1, count_occurrences(AUDIT_TMP, " ALLOC "));
    TEST_ASSERT_EQUAL_INT(1, count_occurrences(AUDIT_TMP, " FREE "));
    TEST_ASSERT_EQUAL_INT(1, count_occurrences(AUDIT_TMP, " DESTROY "));
}

/* =========================================================================
   5. Event content: call site, result markers, zone state
   ========================================================================= */

static void test_audit_verbose_event_contains_source_file(void)
{
    memzone_t* zone = mz_initAudit(4096, SHL_MZ_AUDIT_FORMAT_VERBOSE, AUDIT_TMP);
    mz_destroy(zone);

    /* __FILE__ embeds "memzone_audit_test.c" at the mz_initAudit call site. */
    TEST_ASSERT_TRUE(file_contains(AUDIT_TMP, "memzone_audit_test.c"));
}

static void test_audit_verbose_successful_alloc_logs_ok(void)
{
    memzone_t* zone = mz_initAudit(4096, SHL_MZ_AUDIT_FORMAT_VERBOSE, AUDIT_TMP);
    void* p = mz_alloc(zone, 64);
    mz_free(zone, p);
    mz_destroy(zone);

    TEST_ASSERT_TRUE(file_contains(AUDIT_TMP, "[OK]"));
}

static void test_audit_verbose_failed_alloc_logs_fail(void)
{
    memzone_t* zone = mz_initAudit(4096, SHL_MZ_AUDIT_FORMAT_VERBOSE, AUDIT_TMP);
    mz_alloc(zone, 99999999); /* intentionally too large */
    mz_destroy(zone);

    TEST_ASSERT_TRUE(file_contains(AUDIT_TMP, "[FAIL]"));
}

static void test_audit_destroy_reports_leaked_allocation_count(void)
{
    memzone_t* zone = mz_initAudit(4096, SHL_MZ_AUDIT_FORMAT_VERBOSE, AUDIT_TMP);
    mz_alloc(zone, 64); /* deliberately not freed — leaked allocation */
    mz_destroy(zone);

    TEST_ASSERT_TRUE(file_contains(AUDIT_TMP, "Live allocs: 1"));
}

static void test_audit_reset_logs_zone_state_before_and_after(void)
{
    memzone_t* zone = mz_initAudit(4096, SHL_MZ_AUDIT_FORMAT_VERBOSE, AUDIT_TMP);
    mz_alloc(zone, 64);
    mz_reset(zone);
    mz_destroy(zone);

    TEST_ASSERT_TRUE(file_contains(AUDIT_TMP, "Zone before"));
    TEST_ASSERT_TRUE(file_contains(AUDIT_TMP, "Zone after"));
}

/* =========================================================================
   6. Realloc strategy identification
   ========================================================================= */

static void test_audit_realloc_noop_strategy_logged(void)
{
    /* Shrink → request fits within existing block → "no-op" */
    memzone_t* zone = mz_initAudit(4096, SHL_MZ_AUDIT_FORMAT_VERBOSE, AUDIT_TMP);
    void* p = mz_alloc(zone, 64);
    mz_realloc(zone, p, 32);
    mz_destroy(zone);

    TEST_ASSERT_TRUE(file_contains(AUDIT_TMP, "no-op"));
}

static void test_audit_realloc_in_place_strategy_logged(void)
{
    /* Grow into adjacent free block → "in-place expand" */
    memzone_t* zone = mz_initAudit(4096, SHL_MZ_AUDIT_FORMAT_VERBOSE, AUDIT_TMP);
    void* p = mz_alloc(zone, 64);
    void* q = mz_alloc(zone, 64);
    mz_free(zone, q);               /* free the block immediately after p */
    mz_realloc(zone, p, 128);       /* p can absorb q's space in-place   */
    mz_destroy(zone);

    TEST_ASSERT_TRUE(file_contains(AUDIT_TMP, "in-place expand"));
}

static void test_audit_realloc_new_alloc_strategy_logged(void)
{
    /* Next block is live → must allocate elsewhere → "alloc+copy+free" */
    memzone_t* zone = mz_initAudit(4096, SHL_MZ_AUDIT_FORMAT_VERBOSE, AUDIT_TMP);
    void* p = mz_alloc(zone, 64);
    void* r = mz_alloc(zone, 64); /* pins the block right after p */
    mz_realloc(zone, p, 128);
    mz_free(zone, r);
    mz_destroy(zone);

    TEST_ASSERT_TRUE(file_contains(AUDIT_TMP, "alloc + copy + free"));
}

/* =========================================================================
   7. SHL_MZ_AUDIT_VERBOSE block list
   ========================================================================= */

static void test_audit_block_list_present_when_verbose_flag_defined(void)
{
    /* SHL_MZ_AUDIT_VERBOSE is #define'd at the top of this translation unit. */
    memzone_t* zone = mz_initAudit(4096, SHL_MZ_AUDIT_FORMAT_VERBOSE, AUDIT_TMP);
    mz_alloc(zone, 64);
    mz_destroy(zone);

    TEST_ASSERT_TRUE(file_contains(AUDIT_TMP, "Block list"));
}

/* =========================================================================
   8. Compact format fields
   ========================================================================= */

static void test_audit_compact_contains_zone_field(void)
{
    memzone_t* zone = mz_initAudit(4096, SHL_MZ_AUDIT_FORMAT_COMPACT, AUDIT_TMP);
    mz_destroy(zone);

    TEST_ASSERT_TRUE(file_contains(AUDIT_TMP, "zone="));
}

static void test_audit_compact_alloc_contains_ptr_field(void)
{
    memzone_t* zone = mz_initAudit(4096, SHL_MZ_AUDIT_FORMAT_COMPACT, AUDIT_TMP);
    void* p = mz_alloc(zone, 64);
    mz_free(zone, p);
    mz_destroy(zone);

    TEST_ASSERT_TRUE(file_contains(AUDIT_TMP, "ptr="));
}

static void test_audit_compact_contains_site_field(void)
{
    memzone_t* zone = mz_initAudit(4096, SHL_MZ_AUDIT_FORMAT_COMPACT, AUDIT_TMP);
    mz_destroy(zone);

    TEST_ASSERT_TRUE(file_contains(AUDIT_TMP, "site="));
}

/* =========================================================================
   9. mz_auditConfigure — attach audit to an existing zone
   ========================================================================= */

static void test_audit_configure_on_existing_zone_logs_ops(void)
{
    /* Create without audit, then attach it. */
    memzone_t* zone = mz_init(4096);
    mz_auditConfigure(zone, SHL_MZ_AUDIT_FORMAT_VERBOSE, AUDIT_TMP);

    void* p = mz_alloc(zone, 64); /* should be logged */
    mz_free(zone, p);
    mz_destroy(zone); /* logs DESTROY + closes file */

    TEST_ASSERT_TRUE(file_contains(AUDIT_TMP, "ALLOC"));
    TEST_ASSERT_TRUE(file_contains(AUDIT_TMP, "FREE"));
    /* No INIT because audit was attached after creation. */
    TEST_ASSERT_FALSE(file_contains(AUDIT_TMP, "INIT"));
}

/* =========================================================================
   10. Multi-zone: each zone writes to its own log file
   ========================================================================= */

static void test_audit_two_zones_log_to_separate_files(void)
{
    const char* LOG_A = "audit_test_zone_a.log";
    const char* LOG_B = "audit_test_zone_b.log";

    remove(LOG_A);
    remove(LOG_B);

    memzone_t* za = mz_initAudit(4096, SHL_MZ_AUDIT_FORMAT_COMPACT, LOG_A);
    memzone_t* zb = mz_initAudit(8192, SHL_MZ_AUDIT_FORMAT_COMPACT, LOG_B);

    mz_alloc(za, 32);  /* logged only in LOG_A */
    mz_alloc(zb, 64);  /* logged only in LOG_B */

    mz_destroy(za);
    mz_destroy(zb);

    TEST_ASSERT_TRUE(file_exists(LOG_A));
    TEST_ASSERT_TRUE(file_exists(LOG_B));

    /* Both logs have their own INIT entries. */
    TEST_ASSERT_EQUAL_INT(1, count_occurrences(LOG_A, " INIT "));
    TEST_ASSERT_EQUAL_INT(1, count_occurrences(LOG_B, " INIT "));

    /* Each file has its own DESTROY entry and no cross-contamination. */
    TEST_ASSERT_EQUAL_INT(1, count_occurrences(LOG_A, " DESTROY "));
    TEST_ASSERT_EQUAL_INT(1, count_occurrences(LOG_B, " DESTROY "));

    remove(LOG_A);
    remove(LOG_B);
}

/* =========================================================================
   Test runner
   ========================================================================= */

int main(void)
{
    UNITY_BEGIN();

    /* Lifecycle */
    RUN_TEST(test_audit_creates_log_file);
    RUN_TEST(test_audit_flush_makes_data_readable_before_close);
    RUN_TEST(test_audit_second_init_audit_truncates_file);
    RUN_TEST(test_audit_close_when_not_open_is_noop);
    RUN_TEST(test_audit_explicit_close_then_destroy_is_safe);

    /* Header */
    RUN_TEST(test_audit_verbose_header_says_verbose);
    RUN_TEST(test_audit_compact_header_says_compact);
    RUN_TEST(test_audit_header_says_truncate_by_default);

    /* All seven operations logged */
    RUN_TEST(test_audit_verbose_logs_init_event);
    RUN_TEST(test_audit_verbose_logs_destroy_event);
    RUN_TEST(test_audit_verbose_logs_alloc_event);
    RUN_TEST(test_audit_verbose_logs_free_event);
    RUN_TEST(test_audit_verbose_logs_alloc_aligned_event);
    RUN_TEST(test_audit_verbose_logs_reset_event);
    RUN_TEST(test_audit_verbose_logs_realloc_event);

    /* Event count and sequencing */
    RUN_TEST(test_audit_verbose_event_count_matches_operations);
    RUN_TEST(test_audit_compact_event_count_matches_operations);

    /* Event content */
    RUN_TEST(test_audit_verbose_event_contains_source_file);
    RUN_TEST(test_audit_verbose_successful_alloc_logs_ok);
    RUN_TEST(test_audit_verbose_failed_alloc_logs_fail);
    RUN_TEST(test_audit_destroy_reports_leaked_allocation_count);
    RUN_TEST(test_audit_reset_logs_zone_state_before_and_after);

    /* Realloc strategies */
    RUN_TEST(test_audit_realloc_noop_strategy_logged);
    RUN_TEST(test_audit_realloc_in_place_strategy_logged);
    RUN_TEST(test_audit_realloc_new_alloc_strategy_logged);

    /* Block list (SHL_MZ_AUDIT_VERBOSE) */
    RUN_TEST(test_audit_block_list_present_when_verbose_flag_defined);

    /* Compact format fields */
    RUN_TEST(test_audit_compact_contains_zone_field);
    RUN_TEST(test_audit_compact_alloc_contains_ptr_field);
    RUN_TEST(test_audit_compact_contains_site_field);

    /* mz_auditConfigure on existing zone */
    RUN_TEST(test_audit_configure_on_existing_zone_logs_ops);

    /* Multi-zone: separate log files */
    RUN_TEST(test_audit_two_zones_log_to_separate_files);

    return UNITY_END();
}
