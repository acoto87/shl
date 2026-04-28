#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    int32_t allocCount;
    int32_t freeCount;
    size_t lastAllocSize;
    void* lastFreedPointer;
} AllocatorCapture;

static AllocatorCapture g_allocatorCapture;

static void* testMalloc(size_t size)
{
    g_allocatorCapture.allocCount++;
    g_allocatorCapture.lastAllocSize = size;
    return malloc(size);
}

static void testFree(void* p)
{
    g_allocatorCapture.freeCount++;
    g_allocatorCapture.lastFreedPointer = p;
    free(p);
}

#define SHL_MZ_MALLOC(sz) testMalloc(sz)
#define SHL_MZ_FREE(p) testFree(p)
#define SHL_MZ_PRIVATE_API
#define SHL_MZ_IMPLEMENTATION
#include "../memzone.h"
#include "test_common.h"

typedef enum
{
    ENTITY_TYPE_1,
    ENTITY_TYPE_2,
    ENTITY_TYPE_3,

    ENTITY_TYPE_COUNT
} EntityType;

typedef struct
{
    int32_t id;
    EntityType type;
} Entity;

typedef struct
{
    int32_t count;
    mz_report_t lastReport;
    const void* lastContext;
    const char* lastMessage;
} ReportCapture;

#if defined(SHL_LEAK_CHECK)
#define ENTITY_COUNT 5000
#else
#define ENTITY_COUNT 30000
#endif

#define ARRAY_LEN(xs) (sizeof(xs) / sizeof((xs)[0]))
void setUp(void)
{
    memset(&g_allocatorCapture, 0, sizeof(g_allocatorCapture));
}

void tearDown(void)
{
}

static size_t alignUp(size_t value)
{
    size_t alignment = mz_alignment();
    size_t remainder = value % alignment;
    return remainder == 0 ? value : value + (alignment - remainder);
}

static size_t getHeaderSize(const memzone_t* zone)
{
    return mz_usedSize(zone) - offsetof(memzone_t, blockList);
}

static void captureReport(const memzone_t* zone, mz_report_t report, const void* context, const char* message, void* userData)
{
    (void)zone;
    ReportCapture* capture = (ReportCapture*)userData;
    capture->count++;
    capture->lastReport = report;
    capture->lastContext = context;
    capture->lastMessage = message;
}

static void assertZoneStructuralInvariants(const memzone_t* zone)
{
    const uint8_t* zoneStart = (const uint8_t*)zone;
    const uint8_t* zoneEnd = zoneStart + mz_maxSize(zone);
    const uint8_t* expectedBlockStart = zoneStart + offsetof(memzone_t, blockList);
    const memblock_t* rover = &zone->blockList;

    do
    {
        const uint8_t* blockStart = (const uint8_t*)rover;
        const uint8_t* blockEnd = blockStart + rover->size;

        TEST_ASSERT_EQUAL_PTR(expectedBlockStart, blockStart);
        TEST_ASSERT_TRUE(blockEnd <= zoneEnd);

        if (rover->next != &zone->blockList)
        {
            TEST_ASSERT_EQUAL_PTR(blockEnd, rover->next);
            TEST_ASSERT_FALSE(rover->user == NULL && rover->next->user == NULL);
        }

        expectedBlockStart = blockEnd;
        rover = rover->next;
    } while (rover != &zone->blockList);

    TEST_ASSERT_EQUAL_PTR(zoneEnd, expectedBlockStart);
}

static void assertZoneInvariants(const memzone_t* zone)
{
    bool isValid = mz_validate(zone);
    size_t usableFreeSize = mz_usableFreeSize(zone);
    size_t expectedUsedSize = mz_maxSize(zone) - usableFreeSize;

    TEST_ASSERT_TRUE(isValid);
    TEST_ASSERT_EQUAL_size_t(expectedUsedSize, mz_usedSize(zone));
    assertZoneStructuralInvariants(zone);
}

static memzone_t* createZoneOrFail(size_t size)
{
    memzone_t* zone = mz_init(size);
    TEST_ASSERT_NOT_NULL(zone);
    assertZoneInvariants(zone);
    return zone;
}

static void test_mz_init_uses_allocator_override(void)
{
    memzone_t* zone = mz_init(4096);

    TEST_ASSERT_NOT_NULL(zone);
    TEST_ASSERT_EQUAL_INT32(1, g_allocatorCapture.allocCount);
    TEST_ASSERT_EQUAL_size_t(4096, g_allocatorCapture.lastAllocSize);
    assertZoneInvariants(zone);

    mz_destroy(zone);
    TEST_ASSERT_EQUAL_INT32(1, g_allocatorCapture.freeCount);
    TEST_ASSERT_EQUAL_PTR(zone, g_allocatorCapture.lastFreedPointer);
}

static void test_mz_init_rejects_too_small_zone(void)
{
    TEST_ASSERT_NULL(mz_init(mz_alignment()));
    TEST_ASSERT_EQUAL_INT32(0, g_allocatorCapture.allocCount);
}

static void test_mz_alignment_is_power_of_two(void)
{
    size_t alignment = mz_alignment();
    TEST_ASSERT_TRUE(alignment != 0);
    TEST_ASSERT_TRUE((alignment & (alignment - 1)) == 0);
}

static void test_mz_alloc_zero_returns_null(void)
{
    memzone_t* zone = createZoneOrFail(4096);

    TEST_ASSERT_NULL(mz_alloc(zone, 0));
    assertZoneInvariants(zone);

    mz_destroy(zone);
}

static void test_mz_alloc_returns_aligned_pointer_and_aligned_size(void)
{
    memzone_t* zone = createZoneOrFail(4096);
    const size_t sizes[] = { 1, 2, 3, 5, 7, 15, 31, 63 };
    void* allocations[ARRAY_LEN(sizes)] = {0};

    for (size_t i = 0; i < ARRAY_LEN(sizes); i++)
    {
        allocations[i] = mz_alloc(zone, sizes[i]);
        TEST_ASSERT_NOT_NULL(allocations[i]);
        TEST_ASSERT_EQUAL_UINT64(0, (unsigned long long)((uintptr_t)allocations[i] % mz_alignment()));
        TEST_ASSERT_EQUAL_size_t(alignUp(sizes[i]), mz_allocationSize(zone, allocations[i]));
        assertZoneInvariants(zone);
    }

    for (size_t i = 0; i < ARRAY_LEN(sizes); i++)
    {
        mz_free(zone, allocations[i]);
        assertZoneInvariants(zone);
    }

    mz_destroy(zone);
}

static void test_mz_allocAligned_supports_explicit_alignments(void)
{
    memzone_t* zone = createZoneOrFail(8192);
    const size_t alignments[] = { 16, 32, 64 };
    void* allocations[ARRAY_LEN(alignments)] = {0};

    for (size_t i = 0; i < ARRAY_LEN(alignments); i++)
    {
        allocations[i] = mz_allocAligned(zone, 17, alignments[i]);
        TEST_ASSERT_NOT_NULL(allocations[i]);
        TEST_ASSERT_EQUAL_UINT64(0, (unsigned long long)((uintptr_t)allocations[i] % alignments[i]));
        TEST_ASSERT_TRUE(mz_allocationSize(zone, allocations[i]) >= alignUp(17));
        assertZoneInvariants(zone);
    }

    for (size_t i = 0; i < ARRAY_LEN(alignments); i++)
    {
        mz_free(zone, allocations[i]);
        assertZoneInvariants(zone);
    }

    mz_destroy(zone);
}

static void test_mz_allocAligned_rejects_invalid_alignments_and_reports(void)
{
    memzone_t* zone = createZoneOrFail(4096);
    ReportCapture capture = {0};
    mz_setReporter(zone, captureReport, &capture);

    TEST_ASSERT_NULL(mz_allocAligned(zone, 64, 0));
    TEST_ASSERT_EQUAL_INT32(1, capture.count);
    TEST_ASSERT_EQUAL_INT32(MZ_REPORT_ALLOCATION_FAILURE, capture.lastReport);

    TEST_ASSERT_NULL(mz_allocAligned(zone, 64, 3));
    TEST_ASSERT_EQUAL_INT32(2, capture.count);

    TEST_ASSERT_NULL(mz_allocAligned(zone, 64, 24));
    TEST_ASSERT_EQUAL_INT32(3, capture.count);

    TEST_ASSERT_NULL(mz_allocAligned(zone, 64, 128));
    TEST_ASSERT_EQUAL_INT32(4, capture.count);
    TEST_ASSERT_NOT_NULL(capture.lastMessage);
    assertZoneInvariants(zone);

    mz_destroy(zone);
}

static void test_mz_allocationSize_and_contains_report_live_allocations_only(void)
{
    memzone_t* zone = createZoneOrFail(4096);
    void* p = mz_alloc(zone, 33);

    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_TRUE(mz_contains(zone, p));
    TEST_ASSERT_EQUAL_size_t(alignUp(33), mz_allocationSize(zone, p));

    mz_free(zone, p);

    TEST_ASSERT_FALSE(mz_contains(zone, p));
    TEST_ASSERT_EQUAL_size_t(0, mz_allocationSize(zone, p));
    assertZoneInvariants(zone);

    mz_destroy(zone);
}

static void test_mz_accessors_report_zone_sizes(void)
{
    memzone_t* zone = createZoneOrFail(4096);
    size_t usedBefore = mz_usedSize(zone);
    void* p = mz_alloc(zone, 64);

    TEST_ASSERT_EQUAL_size_t(4096, mz_maxSize(zone));
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_TRUE(mz_usedSize(zone) > usedBefore);
    assertZoneInvariants(zone);

    mz_destroy(zone);
}

static void test_mz_free_unknown_pointer_reports_without_mutating_zone(void)
{
    memzone_t* zone = createZoneOrFail(4096);
    ReportCapture capture = {0};
    size_t usedBefore = mz_usedSize(zone);
    int value = 42;
    mz_setReporter(zone, captureReport, &capture);

    mz_free(zone, &value);

    TEST_ASSERT_EQUAL_INT32(1, capture.count);
    TEST_ASSERT_EQUAL_INT32(MZ_REPORT_INVALID_FREE, capture.lastReport);
    TEST_ASSERT_EQUAL_PTR(&value, capture.lastContext);
    TEST_ASSERT_EQUAL_size_t(usedBefore, mz_usedSize(zone));
    assertZoneInvariants(zone);

    mz_destroy(zone);
}

static void test_mz_free_coalesces_adjacent_free_blocks(void)
{
    memzone_t* zone = createZoneOrFail(4096);
    void* a = mz_alloc(zone, 64);
    void* b = mz_alloc(zone, 64);
    void* c = mz_alloc(zone, 64);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_NOT_NULL(c);

    mz_free(zone, b);
    TEST_ASSERT_EQUAL_INT32(4, mz_blockCount(zone));
    assertZoneInvariants(zone);

    mz_free(zone, c);
    TEST_ASSERT_EQUAL_INT32(2, mz_blockCount(zone));
    assertZoneInvariants(zone);

    mz_free(zone, a);
    TEST_ASSERT_EQUAL_INT32(1, mz_blockCount(zone));
    assertZoneInvariants(zone);

    mz_destroy(zone);
}

static void test_mz_alloc_does_not_create_undersized_tail_block(void)
{
    memzone_t* zone = createZoneOrFail(4096);
    size_t initialFreeSize = mz_usableFreeSize(zone);
    void* allocation = mz_alloc(zone, initialFreeSize - mz_alignment());

    TEST_ASSERT_NOT_NULL(allocation);
    TEST_ASSERT_EQUAL_INT32(1, mz_blockCount(zone));
    TEST_ASSERT_EQUAL_size_t(initialFreeSize, mz_allocationSize(zone, allocation));
    assertZoneInvariants(zone);

    mz_free(zone, allocation);
    assertZoneInvariants(zone);
    mz_destroy(zone);
}

static void test_mz_reset_restores_single_free_block(void)
{
    memzone_t* zone = createZoneOrFail(4096);
    void* a = mz_alloc(zone, 128);
    void* b = mz_alloc(zone, 256);

    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);

    mz_reset(zone);

    TEST_ASSERT_EQUAL_INT32(1, mz_blockCount(zone));
    TEST_ASSERT_EQUAL_size_t(mz_maxSize(zone) - mz_usedSize(zone), mz_usableFreeSize(zone));
    TEST_ASSERT_FALSE(mz_contains(zone, a));
    TEST_ASSERT_FALSE(mz_contains(zone, b));
    assertZoneInvariants(zone);

    mz_destroy(zone);
}

static void test_mz_fragmentation_is_zero_for_single_free_block(void)
{
    memzone_t* zone = createZoneOrFail(4096);

    TEST_ASSERT_EQUAL_FLOAT(0.0f, mz_fragmentation(zone));

    mz_destroy(zone);
}

static void test_mz_fragmentation_increases_when_free_space_is_split(void)
{
    memzone_t* zone = createZoneOrFail(4096);
    void* a = mz_alloc(zone, 128);
    void* b = mz_alloc(zone, 128);
    void* c = mz_alloc(zone, 128);

    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_NOT_NULL(c);

    mz_free(zone, a);
    mz_free(zone, c);

    TEST_ASSERT_TRUE(mz_fragmentation(zone) > 0.0f);
    assertZoneInvariants(zone);

    mz_destroy(zone);
}

static void test_mz_fragmentation_matches_deterministic_pattern(void)
{
    memzone_t* zone = createZoneOrFail(4096);
    void* a = mz_alloc(zone, 100);
    void* b = mz_alloc(zone, 200);
    void* c = mz_alloc(zone, 300);
    size_t freeA = mz_allocationSize(zone, a);
    size_t mergedTailFree = 0;
    float expected = 0.0f;

    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_NOT_NULL(c);

    mz_free(zone, a);
    mz_free(zone, c);

    mergedTailFree = mz_usableFreeSize(zone) - freeA;
    expected = ((float)freeA / (float)(freeA + mergedTailFree)) * 100.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, expected, mz_fragmentation(zone));
    assertZoneInvariants(zone);

    mz_destroy(zone);
}

static void test_mz_fragmentation_returns_to_zero_after_coalescing(void)
{
    memzone_t* zone = createZoneOrFail(4096);
    void* a = mz_alloc(zone, 128);
    void* b = mz_alloc(zone, 128);
    void* c = mz_alloc(zone, 128);

    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_NOT_NULL(c);

    mz_free(zone, a);
    mz_free(zone, c);
    TEST_ASSERT_TRUE(mz_fragmentation(zone) > 0.0f);

    mz_free(zone, b);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, mz_fragmentation(zone));
    assertZoneInvariants(zone);

    mz_destroy(zone);
}

static void test_mz_validate_reports_corrupted_used_size(void)
{
    memzone_t* zone = createZoneOrFail(4096);
    ReportCapture capture = {0};
    mz_setReporter(zone, captureReport, &capture);

    zone->usedSize = mz_maxSize(zone) + 1;

    TEST_ASSERT_FALSE(mz_validate(zone));
    TEST_ASSERT_EQUAL_INT32(1, capture.count);
    TEST_ASSERT_EQUAL_INT32(MZ_REPORT_VALIDATION_FAILURE, capture.lastReport);
    TEST_ASSERT_EQUAL_PTR(zone, capture.lastContext);
    TEST_ASSERT_NOT_NULL(capture.lastMessage);

    mz_destroy(zone);
}

static void test_mz_validate_reports_gap_between_blocks(void)
{
    memzone_t* zone = createZoneOrFail(4096);
    ReportCapture capture = {0};
    void* p = mz_alloc(zone, 64);
    TEST_ASSERT_NOT_NULL(p);
    mz_setReporter(zone, captureReport, &capture);

    zone->blockList.next = (memblock_t*)((uint8_t*)zone->blockList.next + mz_alignment());

    TEST_ASSERT_FALSE(mz_validate(zone));
    TEST_ASSERT_EQUAL_INT32(1, capture.count);
    TEST_ASSERT_EQUAL_INT32(MZ_REPORT_VALIDATION_FAILURE, capture.lastReport);
    TEST_ASSERT_EQUAL_PTR(&zone->blockList, capture.lastContext);
    TEST_ASSERT_NOT_NULL(capture.lastMessage);

    mz_destroy(zone);
}

static void test_mz_validate_reports_adjacent_free_blocks(void)
{
    memzone_t* zone = createZoneOrFail(4096);
    ReportCapture capture = {0};
    void* p = mz_alloc(zone, 64);
    TEST_ASSERT_NOT_NULL(p);
    mz_setReporter(zone, captureReport, &capture);

    zone->blockList.user = NULL;

    TEST_ASSERT_FALSE(mz_validate(zone));
    TEST_ASSERT_EQUAL_INT32(1, capture.count);
    TEST_ASSERT_EQUAL_INT32(MZ_REPORT_VALIDATION_FAILURE, capture.lastReport);
    TEST_ASSERT_EQUAL_PTR(&zone->blockList, capture.lastContext);
    TEST_ASSERT_NOT_NULL(capture.lastMessage);

    mz_destroy(zone);
}

static void test_mz_large_allocation_stress_path(void)
{
    size_t zoneSize = 2 * 1024 * 1024;
    memzone_t* zone = createZoneOrFail(zoneSize);
    size_t baseUsedSize = mz_usedSize(zone);
    size_t headerSize = getHeaderSize(zone);
    size_t alignedArraySize = alignUp(ENTITY_COUNT * sizeof(Entity*));
    size_t alignedEntitySize = alignUp(sizeof(Entity));
    size_t sizeOfArray = ENTITY_COUNT * sizeof(Entity*);
    Entity** entities = (Entity**)mz_alloc(zone, sizeOfArray);

    TEST_ASSERT_NOT_NULL(entities);
    TEST_ASSERT_EQUAL_UINT64(0, (unsigned long long)((uintptr_t)entities % mz_alignment()));
    TEST_ASSERT_EQUAL_size_t(alignedArraySize, mz_allocationSize(zone, entities));
    TEST_ASSERT_EQUAL_size_t(baseUsedSize + alignedArraySize + headerSize, mz_usedSize(zone));
    assertZoneInvariants(zone);

    for (int32_t i = 0; i < ENTITY_COUNT; i++)
    {
        entities[i] = (Entity*)mz_alloc(zone, sizeof(Entity));
        TEST_ASSERT_NOT_NULL(entities[i]);
        entities[i]->id = i;
        TEST_ASSERT_EQUAL_size_t(alignedEntitySize, mz_allocationSize(zone, entities[i]));
    }
    assertZoneInvariants(zone);

    TEST_ASSERT_EQUAL_INT32(1 + ENTITY_COUNT + 1, mz_blockCount(zone));

    for (int32_t i = 0; i < ENTITY_COUNT; i += 2)
    {
        int32_t blocksBeforeFree = mz_blockCount(zone);
        size_t usedSizeBeforeFree = mz_usedSize(zone);
        size_t allocationSize = mz_allocationSize(zone, entities[i]);

        mz_free(zone, entities[i]);
        entities[i] = NULL;

        int32_t blocksAfterFree = mz_blockCount(zone);
        size_t reclaimedMetadata = (size_t)(blocksBeforeFree - blocksAfterFree) * headerSize;
        TEST_ASSERT_EQUAL_size_t(usedSizeBeforeFree - allocationSize - reclaimedMetadata, mz_usedSize(zone));
    }
    assertZoneInvariants(zone);

    TEST_ASSERT_TRUE(mz_blockCount(zone) <= 1 + ENTITY_COUNT + 1);
    mz_destroy(zone);
}

/* =========================================================================
   mz_alloc / mz_allocAligned zeroing
   ========================================================================= */

static bool allZero(const void* p, size_t size)
{
    const uint8_t* bytes = (const uint8_t*)p;
    for (size_t i = 0; i < size; i++)
    {
        if (bytes[i] != 0) return false;
    }
    return true;
}

static void test_mz_alloc_returned_memory_is_zeroed(void)
{
    memzone_t* zone = createZoneOrFail(4096);
    const size_t sizes[] = { 1, 7, 8, 64, 100, 127 };

    for (size_t i = 0; i < ARRAY_LEN(sizes); i++)
    {
        void* p = mz_alloc(zone, sizes[i]);
        TEST_ASSERT_NOT_NULL(p);
        TEST_ASSERT_TRUE(allZero(p, sizes[i]));
        mz_free(zone, p);
        assertZoneInvariants(zone);
    }

    mz_destroy(zone);
}

static void test_mz_allocAligned_returned_memory_is_zeroed(void)
{
    memzone_t* zone = createZoneOrFail(8192);
    const size_t alignments[] = { 16, 32, 64 };

    for (size_t i = 0; i < ARRAY_LEN(alignments); i++)
    {
        void* p = mz_allocAligned(zone, 100, alignments[i]);
        TEST_ASSERT_NOT_NULL(p);
        TEST_ASSERT_TRUE(allZero(p, 100));
        mz_free(zone, p);
        assertZoneInvariants(zone);
    }

    mz_destroy(zone);
}

static void test_mz_alloc_recycled_block_is_zeroed(void)
{
    /* A block re-used after free must still be zeroed on re-allocation. */
    memzone_t* zone = createZoneOrFail(4096);
    void* p = mz_alloc(zone, 64);
    TEST_ASSERT_NOT_NULL(p);

    memset(p, 0xAB, 64);
    mz_free(zone, p);
    assertZoneInvariants(zone);

    void* q = mz_alloc(zone, 64);
    TEST_ASSERT_NOT_NULL(q);
    TEST_ASSERT_TRUE(allZero(q, 64));
    assertZoneInvariants(zone);

    mz_free(zone, q);
    mz_destroy(zone);
}

/* =========================================================================
   mz_realloc
   ========================================================================= */

static void test_mz_realloc_null_zone_returns_null(void)
{
    TEST_ASSERT_NULL(mz_realloc(NULL, NULL, 64));
}

static void test_mz_realloc_null_pointer_behaves_like_alloc(void)
{
    memzone_t* zone = createZoneOrFail(4096);

    void* p = mz_realloc(zone, NULL, 64);

    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_size_t(alignUp(64), mz_allocationSize(zone, p));
    assertZoneInvariants(zone);

    mz_free(zone, p);
    mz_destroy(zone);
}

static void test_mz_realloc_zero_size_frees_and_returns_null(void)
{
    memzone_t* zone = createZoneOrFail(4096);
    void* p = mz_alloc(zone, 64);
    TEST_ASSERT_NOT_NULL(p);

    void* result = mz_realloc(zone, p, 0);

    TEST_ASSERT_NULL(result);
    TEST_ASSERT_FALSE(mz_contains(zone, p));
    assertZoneInvariants(zone);

    mz_destroy(zone);
}

static void test_mz_realloc_invalid_pointer_reports_without_modifying_zone(void)
{
    memzone_t* zone = createZoneOrFail(4096);
    ReportCapture capture = {0};
    int value = 42;
    size_t usedBefore = mz_usedSize(zone);
    mz_setReporter(zone, captureReport, &capture);

    void* result = mz_realloc(zone, &value, 64);

    TEST_ASSERT_NULL(result);
    TEST_ASSERT_EQUAL_INT32(1, capture.count);
    TEST_ASSERT_EQUAL_INT32(MZ_REPORT_INVALID_FREE, capture.lastReport);
    TEST_ASSERT_EQUAL_size_t(usedBefore, mz_usedSize(zone));
    assertZoneInvariants(zone);

    mz_destroy(zone);
}

static void test_mz_realloc_smaller_size_returns_same_pointer(void)
{
    memzone_t* zone = createZoneOrFail(4096);
    void* p = mz_alloc(zone, 64);
    TEST_ASSERT_NOT_NULL(p);

    void* result = mz_realloc(zone, p, 32);

    TEST_ASSERT_EQUAL_PTR(p, result);
    assertZoneInvariants(zone);

    mz_free(zone, p);
    mz_destroy(zone);
}

static void test_mz_realloc_same_size_returns_same_pointer(void)
{
    memzone_t* zone = createZoneOrFail(4096);
    void* p = mz_alloc(zone, 64);
    TEST_ASSERT_NOT_NULL(p);

    void* result = mz_realloc(zone, p, 64);

    TEST_ASSERT_EQUAL_PTR(p, result);
    assertZoneInvariants(zone);

    mz_free(zone, p);
    mz_destroy(zone);
}

static void test_mz_realloc_fits_in_existing_block_capacity(void)
{
    /* alloc(1) → allocSize = alignUp(1); realloc to alignUp(1)-1 rounds back to alignUp(1) → Case 1 */
    memzone_t* zone = createZoneOrFail(4096);
    size_t align = mz_alignment();
    void* p = mz_alloc(zone, 1);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_size_t(align, mz_allocationSize(zone, p));

    /* Request something that aligns to the same alloc size */
    void* result = mz_realloc(zone, p, align - 1);

    TEST_ASSERT_EQUAL_PTR(p, result);
    assertZoneInvariants(zone);

    mz_free(zone, p);
    mz_destroy(zone);
}

static void test_mz_realloc_merges_adjacent_empty_block_in_place(void)
{
    memzone_t* zone = createZoneOrFail(4096);
    void* p = mz_alloc(zone, 64);
    void* q = mz_alloc(zone, 64);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_NOT_NULL(q);

    mz_free(zone, q);
    assertZoneInvariants(zone);

    /* p's next is now the merged free block; grow p beyond its current 64 bytes */
    void* result = mz_realloc(zone, p, 128);

    TEST_ASSERT_EQUAL_PTR(p, result);
    TEST_ASSERT_TRUE(mz_contains(zone, p));
    TEST_ASSERT_TRUE(mz_allocationSize(zone, p) >= alignUp(128));
    assertZoneInvariants(zone);

    mz_free(zone, p);
    assertZoneInvariants(zone);
    mz_destroy(zone);
}

static void test_mz_realloc_in_place_growth_preserves_remaining_tail_free_block(void)
{
    memzone_t* zone = createZoneOrFail(4096);
    size_t originalAllocSize = 0;
    size_t freeBefore = 0;
    size_t expectedFreeAfter = 0;
    void* p = mz_alloc(zone, 64);
    void* q = mz_alloc(zone, 64);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_NOT_NULL(q);

    originalAllocSize = mz_allocationSize(zone, p);

    mz_free(zone, q);
    freeBefore = mz_usableFreeSize(zone);
    expectedFreeAfter = freeBefore - (alignUp(128) - originalAllocSize);
    assertZoneInvariants(zone);

    TEST_ASSERT_EQUAL_PTR(p, mz_realloc(zone, p, 128));
    TEST_ASSERT_EQUAL_INT32(2, mz_blockCount(zone));
    TEST_ASSERT_EQUAL_size_t(expectedFreeAfter, mz_usableFreeSize(zone));

    void* r = mz_alloc(zone, 64);
    TEST_ASSERT_NOT_NULL(r);
    assertZoneInvariants(zone);

    mz_free(zone, r);
    mz_free(zone, p);
    assertZoneInvariants(zone);
    mz_destroy(zone);
}

static void test_mz_realloc_in_place_growth_preserves_tail_after_aligned_alloc(void)
{
    memzone_t* zone = createZoneOrFail(4096);
    size_t originalAllocSize = 0;
    size_t freeBefore = 0;
    size_t expectedFreeAfter = 0;
    void* p = mz_allocAligned(zone, 64, 64);
    void* q = mz_alloc(zone, 64);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_NOT_NULL(q);

    originalAllocSize = mz_allocationSize(zone, p);

    mz_free(zone, q);
    freeBefore = mz_usableFreeSize(zone);
    expectedFreeAfter = freeBefore - (alignUp(128) - originalAllocSize);
    assertZoneInvariants(zone);

    TEST_ASSERT_EQUAL_PTR(p, mz_realloc(zone, p, 128));
    TEST_ASSERT_EQUAL_UINT64(0, (unsigned long long)((uintptr_t)p % 64));
    TEST_ASSERT_EQUAL_INT32(2, mz_blockCount(zone));
    TEST_ASSERT_EQUAL_size_t(expectedFreeAfter, mz_usableFreeSize(zone));

    void* r = mz_alloc(zone, 64);
    TEST_ASSERT_NOT_NULL(r);
    assertZoneInvariants(zone);

    mz_free(zone, r);
    mz_free(zone, p);
    assertZoneInvariants(zone);
    mz_destroy(zone);
}

static void test_mz_realloc_adjacent_block_too_small_falls_back_to_new_alloc(void)
{
    memzone_t* zone = createZoneOrFail(4096);
    void* p = mz_alloc(zone, 64);
    void* q = mz_alloc(zone, 1);   /* small free block after p */
    void* r = mz_alloc(zone, 64);  /* pins q so it can't coalesce right */
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_NOT_NULL(q);
    TEST_ASSERT_NOT_NULL(r);

    mz_free(zone, q);
    assertZoneInvariants(zone);

    /* combined capacity = p.allocSize + q.blockSize; request more than that */
    size_t pAllocSize = mz_allocationSize(zone, p);
    size_t qAllocSize = alignUp(1);
    size_t qBlockSize = mz__headerSize() + qAllocSize;
    size_t combinedCapacity = pAllocSize + qBlockSize;
    size_t requestSize = combinedCapacity + 1;  /* alignUp rounds up past combined */

    void* result = mz_realloc(zone, p, requestSize);

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_TRUE(result != p);
    TEST_ASSERT_FALSE(mz_contains(zone, p));
    TEST_ASSERT_TRUE(mz_contains(zone, result));
    assertZoneInvariants(zone);

    mz_free(zone, result);
    mz_free(zone, r);
    assertZoneInvariants(zone);
    mz_destroy(zone);
}

static void test_mz_realloc_no_adjacent_empty_block_allocates_new(void)
{
    memzone_t* zone = createZoneOrFail(4096);
    void* p = mz_alloc(zone, 64);
    void* r = mz_alloc(zone, 64);  /* allocated block immediately after p */
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_NOT_NULL(r);

    /* p.next = r (allocated) → Case 2 condition fails → Case 3 */
    void* result = mz_realloc(zone, p, 128);

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_TRUE(result != p);
    TEST_ASSERT_FALSE(mz_contains(zone, p));
    TEST_ASSERT_TRUE(mz_contains(zone, result));
    assertZoneInvariants(zone);

    mz_free(zone, result);
    mz_free(zone, r);
    assertZoneInvariants(zone);
    mz_destroy(zone);
}

static void test_mz_realloc_case1_preserves_data(void)
{
    memzone_t* zone = createZoneOrFail(4096);
    char* p = (char*)mz_alloc(zone, 64);
    TEST_ASSERT_NOT_NULL(p);
    memcpy(p, "hello", 6);

    char* result = (char*)mz_realloc(zone, p, 32);  /* shrink → Case 1 */

    TEST_ASSERT_EQUAL_PTR(p, result);
    TEST_ASSERT_EQUAL_STRING("hello", result);
    assertZoneInvariants(zone);

    mz_free(zone, p);
    mz_destroy(zone);
}

static void test_mz_realloc_case3_data_preserved_in_new_location(void)
{
    memzone_t* zone = createZoneOrFail(4096);
    char* p = (char*)mz_alloc(zone, 64);
    void* r = mz_alloc(zone, 64);  /* pin next block so Case 2 cannot apply */
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_NOT_NULL(r);
    memcpy(p, "hello", 6);

    char* result = (char*)mz_realloc(zone, p, 128);  /* Case 3 */

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_TRUE((void*)result != (void*)p);
    TEST_ASSERT_EQUAL_STRING("hello", result);
    assertZoneInvariants(zone);

    mz_free(zone, result);
    mz_free(zone, r);
    mz_destroy(zone);
}

static void test_mz_realloc_in_place_split_one_alignment_unit_growth(void)
{
    /*
     * Regression: when mz_realloc Case 2 splits the adjacent free block with
     * extraNeeded = mz_alignment(), newBlock is placed one alignment unit into
     * the free block's memory.  Because the struct layout is
     *   [size @ 0][user @ align][next @ 2*align][prev @ 3*align]
     * the write "newBlock->user = NULL" lands exactly on next->next (at +2*align),
     * zeroing it before it is read.  The subsequent "newBlock->next->prev = newBlock"
     * then dereferences NULL, crashing at memzone.h line 654.
     *
     * Trigger: alloc A (8*align), alloc B (2*align as payload), free B,
     * realloc A to 9*align.  B_free.size = headerSize + 2*align, so
     * remainingSize = headerSize + align — the minimum that qualifies for a
     * split — ensuring the overlap write always fires.
     */
    size_t align = mz_alignment();
    memzone_t* zone = createZoneOrFail(4096);
    void* a = mz_alloc(zone, align * 8);
    void* b = mz_alloc(zone, align * 2);  /* B_free.size = headerSize + 2*align */
    void* c = mz_alloc(zone, align * 8);  /* pin: prevents b coalescing with tail */
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_NOT_NULL(c);

    mz_free(zone, b);
    assertZoneInvariants(zone);

    /* extraNeeded = align: newBlock->user clobbers next->next without the fix */
    void* result = mz_realloc(zone, a, align * 9);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_PTR(a, result);
    assertZoneInvariants(zone);

    mz_free(zone, result);
    mz_free(zone, c);
    assertZoneInvariants(zone);
    mz_destroy(zone);
}

static void test_mz_realloc_in_place_split_two_alignment_units_growth(void)
{
    /*
     * Regression: same root cause as the one-unit case above, but with
     * extraNeeded = 2*mz_alignment().  Here newBlock lands two alignment units
     * into the free block, so "newBlock->size = remainingSize" (at offset 0
     * within newBlock, i.e. +2*align from next) overwrites next->next, storing
     * a large integer that is later dereferenced as a pointer — undefined
     * behaviour / crash at line 654.
     */
    size_t align = mz_alignment();
    memzone_t* zone = createZoneOrFail(4096);
    void* a = mz_alloc(zone, align * 8);
    void* b = mz_alloc(zone, align * 3);  /* B_free.size = headerSize + 3*align */
    void* c = mz_alloc(zone, align * 8);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_NOT_NULL(c);

    mz_free(zone, b);
    assertZoneInvariants(zone);

    /* extraNeeded = 2*align: newBlock->size clobbers next->next without the fix */
    void* result = mz_realloc(zone, a, align * 10);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_PTR(a, result);
    assertZoneInvariants(zone);

    mz_free(zone, result);
    mz_free(zone, c);
    assertZoneInvariants(zone);
    mz_destroy(zone);
}

static void test_mz_realloc_zone_invariants_held_throughout(void)
{
    memzone_t* zone = createZoneOrFail(4096);
    void* a = mz_alloc(zone, 32);
    void* b = mz_alloc(zone, 32);
    void* c = mz_alloc(zone, 32);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_NOT_NULL(c);
    assertZoneInvariants(zone);

    /* Case 1: shrink a */
    void* a2 = mz_realloc(zone, a, 16);
    TEST_ASSERT_EQUAL_PTR(a, a2);
    assertZoneInvariants(zone);

    /* Case 3: grow a beyond b (b is allocated, no free next block) */
    void* a3 = mz_realloc(zone, a2, 64);
    TEST_ASSERT_NOT_NULL(a3);
    assertZoneInvariants(zone);

    mz_free(zone, a3);
    mz_free(zone, b);
    mz_free(zone, c);
    assertZoneInvariants(zone);
    mz_destroy(zone);
}

static void test_mz_realloc_fails_gracefully_when_oom(void)
{
    memzone_t* zone = createZoneOrFail(4096);
    ReportCapture capture = {0};
    mz_setReporter(zone, captureReport, &capture);

    /* Fill the zone completely */
    size_t initialFreeSize = mz_usableFreeSize(zone);
    void* p = mz_alloc(zone, initialFreeSize);
    TEST_ASSERT_NOT_NULL(p);
    assertZoneInvariants(zone);

    /* Now the zone is full; attempt a fresh allocation via realloc(NULL, ...) */
    void* q = mz_realloc(zone, NULL, 1);
    TEST_ASSERT_NULL(q);
    TEST_ASSERT_EQUAL_INT32(1, capture.count);
    TEST_ASSERT_EQUAL_INT32(MZ_REPORT_ALLOCATION_FAILURE, capture.lastReport);
    assertZoneInvariants(zone);

    mz_free(zone, p);
    mz_destroy(zone);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_mz_init_uses_allocator_override);
    RUN_TEST(test_mz_init_rejects_too_small_zone);
    RUN_TEST(test_mz_alignment_is_power_of_two);
    RUN_TEST(test_mz_alloc_zero_returns_null);
    RUN_TEST(test_mz_alloc_returns_aligned_pointer_and_aligned_size);
    RUN_TEST(test_mz_allocAligned_supports_explicit_alignments);
    RUN_TEST(test_mz_allocAligned_rejects_invalid_alignments_and_reports);
    RUN_TEST(test_mz_allocationSize_and_contains_report_live_allocations_only);
    RUN_TEST(test_mz_accessors_report_zone_sizes);
    RUN_TEST(test_mz_free_unknown_pointer_reports_without_mutating_zone);
    RUN_TEST(test_mz_free_coalesces_adjacent_free_blocks);
    RUN_TEST(test_mz_alloc_does_not_create_undersized_tail_block);
    RUN_TEST(test_mz_alloc_returned_memory_is_zeroed);
    RUN_TEST(test_mz_allocAligned_returned_memory_is_zeroed);
    RUN_TEST(test_mz_alloc_recycled_block_is_zeroed);
    RUN_TEST(test_mz_reset_restores_single_free_block);
    RUN_TEST(test_mz_fragmentation_is_zero_for_single_free_block);
    RUN_TEST(test_mz_fragmentation_increases_when_free_space_is_split);
    RUN_TEST(test_mz_fragmentation_matches_deterministic_pattern);
    RUN_TEST(test_mz_fragmentation_returns_to_zero_after_coalescing);
    RUN_TEST(test_mz_validate_reports_corrupted_used_size);
    RUN_TEST(test_mz_validate_reports_gap_between_blocks);
    RUN_TEST(test_mz_validate_reports_adjacent_free_blocks);
    RUN_TEST(test_mz_large_allocation_stress_path);
    RUN_TEST(test_mz_realloc_null_zone_returns_null);
    RUN_TEST(test_mz_realloc_null_pointer_behaves_like_alloc);
    RUN_TEST(test_mz_realloc_zero_size_frees_and_returns_null);
    RUN_TEST(test_mz_realloc_invalid_pointer_reports_without_modifying_zone);
    RUN_TEST(test_mz_realloc_smaller_size_returns_same_pointer);
    RUN_TEST(test_mz_realloc_same_size_returns_same_pointer);
    RUN_TEST(test_mz_realloc_fits_in_existing_block_capacity);
    RUN_TEST(test_mz_realloc_merges_adjacent_empty_block_in_place);
    RUN_TEST(test_mz_realloc_in_place_growth_preserves_remaining_tail_free_block);
    RUN_TEST(test_mz_realloc_in_place_growth_preserves_tail_after_aligned_alloc);
    RUN_TEST(test_mz_realloc_adjacent_block_too_small_falls_back_to_new_alloc);
    RUN_TEST(test_mz_realloc_no_adjacent_empty_block_allocates_new);
    RUN_TEST(test_mz_realloc_case1_preserves_data);
    RUN_TEST(test_mz_realloc_case3_data_preserved_in_new_location);
    RUN_TEST(test_mz_realloc_in_place_split_one_alignment_unit_growth);
    RUN_TEST(test_mz_realloc_in_place_split_two_alignment_units_growth);
    RUN_TEST(test_mz_realloc_zone_invariants_held_throughout);
    RUN_TEST(test_mz_realloc_fails_gracefully_when_oom);
    return UNITY_END();
}
