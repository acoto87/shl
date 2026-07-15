#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
 * Allocator hooks make allocation-failure behavior deterministic and testable.
 * The production build uses malloc/realloc/free unless these macros are set.
 */
static size_t mb_test_allocation_count = 0u;
static size_t mb_test_fail_on_allocation = SIZE_MAX;

static void* mb_test_malloc(size_t size)
{
    mb_test_allocation_count += 1u;
    if (mb_test_allocation_count == mb_test_fail_on_allocation)
        return NULL;
    return malloc(size);
}

static void* mb_test_realloc(void* pointer, size_t size)
{
    mb_test_allocation_count += 1u;
    if (mb_test_allocation_count == mb_test_fail_on_allocation)
        return NULL;
    return realloc(pointer, size);
}

static void mb_test_free(void* pointer)
{
    free(pointer);
}

static void mb_test_fail_next_allocation(void)
{
    mb_test_fail_on_allocation = mb_test_allocation_count + 1u;
}

#define SHL_MEMORY_BUFFER_MALLOC(size) mb_test_malloc(size)
#define SHL_MEMORY_BUFFER_REALLOC(pointer, size) mb_test_realloc((pointer), (size))
#define SHL_MEMORY_BUFFER_FREE(pointer) mb_test_free(pointer)
#define SHL_MEMORY_BUFFER_IMPLEMENTATION
#include "../memory_buffer.h"
#include "test_common.h"

#define MB_TEST_STRESS_COUNT 4096

static const uint8_t MB_TEST_SAMPLE[] = {
    0x10u, 0x20u, 0x30u, 0x40u, 0x50u, 0x60u
};

static void assert_buffer_bytes(
    const memory_buffer_t* buffer,
    const uint8_t* expected,
    size_t count)
{
    TEST_ASSERT_EQUAL_size_t(count, buffer->length);
    if (count > 0u)
    {
        TEST_ASSERT_NOT_NULL(buffer->data);
        TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, buffer->data, count);
    }
}

static uint32_t mb_test_prng(uint32_t* state)
{
    uint32_t x = *state;
    x ^= x << 13u;
    x ^= x >> 17u;
    x ^= x << 5u;
    *state = x;
    return x;
}

/* -------------------------------------------------------------------------- */
/* Lifetime and ownership                                                     */
/* -------------------------------------------------------------------------- */

static void mb_initEmptyCreatesCanonicalEmptyBuffer(void)
{
    memory_buffer_t buffer = {0};

    mb_initEmpty(&buffer);

    TEST_ASSERT_NULL(buffer.data);
    TEST_ASSERT_EQUAL_size_t(0u, buffer.length);
    TEST_ASSERT_EQUAL_size_t(0u, buffer.capacity);
    TEST_ASSERT_EQUAL_size_t(0u, buffer.position);
    TEST_ASSERT_TRUE(mb_isEOF(&buffer));
    TEST_ASSERT_NULL(mb_end(&buffer));
}

static void mb_initWithCapacityReservesWithoutChangingLength(void)
{
    memory_buffer_t buffer = {0};

    TEST_ASSERT_TRUE(mb_initWithCapacity(&buffer, 128u));
    TEST_ASSERT_NOT_NULL(buffer.data);
    TEST_ASSERT_EQUAL_size_t(0u, buffer.length);
    TEST_ASSERT_GREATER_OR_EQUAL_size_t(128u, buffer.capacity);
    TEST_ASSERT_EQUAL_size_t(0u, buffer.position);

    mb_free(&buffer);
}

static void mb_initFromMemoryCopiesTheInput(void)
{
    uint8_t source[] = {1u, 2u, 3u, 4u};
    memory_buffer_t buffer = {0};

    TEST_ASSERT_TRUE(mb_initFromMemory(&buffer, source, sizeof(source)));
    TEST_ASSERT_FALSE(source == buffer.data);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(source, buffer.data, sizeof(source));

    buffer.data[0] = 99u;
    TEST_ASSERT_EQUAL_UINT8(1u, source[0]);

    mb_free(&buffer);
}

static void mb_initFromMemoryAcceptsEmptyNullInput(void)
{
    memory_buffer_t buffer = {0};

    TEST_ASSERT_TRUE(mb_initFromMemory(&buffer, NULL, 0u));
    TEST_ASSERT_TRUE(mb_isEOF(&buffer));

    mb_free(&buffer);
}

static void mb_initFromMemoryRejectsNullNonEmptyInput(void)
{
    memory_buffer_t buffer = {0};

    TEST_ASSERT_FALSE(mb_initFromMemory(&buffer, NULL, 1u));
    TEST_ASSERT_NULL(buffer.data);
    TEST_ASSERT_EQUAL_size_t(0u, buffer.length);
}

static void mb_initFromMemoryAllocationFailureLeavesEmptyBuffer(void)
{
    const uint8_t source[] = {1u, 2u, 3u};
    memory_buffer_t buffer = {0};

    mb_test_fail_next_allocation();
    TEST_ASSERT_FALSE(mb_initFromMemory(&buffer, source, sizeof(source)));
    TEST_ASSERT_NULL(buffer.data);
    TEST_ASSERT_EQUAL_size_t(0u, buffer.length);
    TEST_ASSERT_EQUAL_size_t(0u, buffer.capacity);
    TEST_ASSERT_EQUAL_size_t(0u, buffer.position);
}

static void mb_initTakeOwnershipAndDetachRoundTrip(void)
{
    uint8_t* allocation = (uint8_t*)malloc(16u);
    memory_buffer_t buffer = {0};
    uint8_t* detached;
    size_t length = 0u;
    size_t capacity = 0u;

    TEST_ASSERT_NOT_NULL(allocation);
    allocation[0] = 0xAAu;
    allocation[1] = 0xBBu;

    TEST_ASSERT_TRUE(mb_initTakeOwnership(&buffer, allocation, 2u, 16u));
    TEST_ASSERT_EQUAL_PTR(allocation, buffer.data);

    detached = mb_detach(&buffer, &length, &capacity);
    TEST_ASSERT_EQUAL_PTR(allocation, detached);
    TEST_ASSERT_EQUAL_size_t(2u, length);
    TEST_ASSERT_EQUAL_size_t(16u, capacity);
    TEST_ASSERT_NULL(buffer.data);
    TEST_ASSERT_EQUAL_size_t(0u, buffer.length);

    free(detached);
}

static void mb_initTakeOwnershipRejectsInvalidCombinations(void)
{
    uint8_t byte = 0u;
    memory_buffer_t buffer = {0};

    TEST_ASSERT_FALSE(mb_initTakeOwnership(&buffer, &byte, 2u, 1u));
    TEST_ASSERT_FALSE(mb_initTakeOwnership(&buffer, NULL, 1u, 1u));
    TEST_ASSERT_FALSE(mb_initTakeOwnership(&buffer, &byte, 0u, 0u));
    TEST_ASSERT_TRUE(mb_initTakeOwnership(&buffer, NULL, 0u, 0u));
}

static void mb_freeResetsFieldsAndIsIdempotent(void)
{
    memory_buffer_t buffer = {0};

    TEST_ASSERT_TRUE(mb_initWithCapacity(&buffer, 32u));
    TEST_ASSERT_TRUE(mb_writeString(&buffer, "abc", 3u));

    mb_free(&buffer);
    TEST_ASSERT_NULL(buffer.data);
    TEST_ASSERT_EQUAL_size_t(0u, buffer.length);
    TEST_ASSERT_EQUAL_size_t(0u, buffer.capacity);
    TEST_ASSERT_EQUAL_size_t(0u, buffer.position);

    mb_free(&buffer);
    TEST_ASSERT_NULL(buffer.data);
}

static void mb_dataReturnsIndependentCopy(void)
{
    memory_buffer_t buffer = {0};
    uint8_t* copy;
    size_t length = 999u;

    TEST_ASSERT_TRUE(mb_initFromMemory(&buffer, MB_TEST_SAMPLE, sizeof(MB_TEST_SAMPLE)));
    copy = mb_data(&buffer, &length);

    TEST_ASSERT_NOT_NULL(copy);
    TEST_ASSERT_EQUAL_size_t(sizeof(MB_TEST_SAMPLE), length);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(MB_TEST_SAMPLE, copy, length);

    copy[0] = 0u;
    TEST_ASSERT_EQUAL_UINT8(MB_TEST_SAMPLE[0], buffer.data[0]);

    free(copy);
    mb_free(&buffer);
}

static void mb_dataOnEmptyBufferReturnsNullAndZeroLength(void)
{
    memory_buffer_t buffer = {0};
    size_t length = 99u;

    mb_initEmpty(&buffer);
    TEST_ASSERT_NULL(mb_data(&buffer, &length));
    TEST_ASSERT_EQUAL_size_t(0u, length);
}

static void mb_dataAllocationFailureReportsZeroLength(void)
{
    memory_buffer_t buffer = {0};
    size_t length = 99u;

    TEST_ASSERT_TRUE(mb_initFromMemory(&buffer, MB_TEST_SAMPLE, sizeof(MB_TEST_SAMPLE)));
    mb_test_fail_next_allocation();

    TEST_ASSERT_NULL(mb_data(&buffer, &length));
    TEST_ASSERT_EQUAL_size_t(0u, length);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(MB_TEST_SAMPLE, buffer.data, sizeof(MB_TEST_SAMPLE));

    mb_free(&buffer);
}

/* -------------------------------------------------------------------------- */
/* Capacity, size, and positioning                                             */
/* -------------------------------------------------------------------------- */

static void mb_reservePreservesContentLengthAndPosition(void)
{
    memory_buffer_t buffer = {0};
    size_t old_capacity;

    TEST_ASSERT_TRUE(mb_initFromMemory(&buffer, MB_TEST_SAMPLE, sizeof(MB_TEST_SAMPLE)));
    TEST_ASSERT_TRUE(mb_seek(&buffer, 3u));
    old_capacity = buffer.capacity;

    TEST_ASSERT_TRUE(mb_reserve(&buffer, old_capacity + 100u));
    TEST_ASSERT_GREATER_OR_EQUAL_size_t(old_capacity + 100u, buffer.capacity);
    TEST_ASSERT_EQUAL_size_t(sizeof(MB_TEST_SAMPLE), buffer.length);
    TEST_ASSERT_EQUAL_size_t(3u, buffer.position);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(MB_TEST_SAMPLE, buffer.data, sizeof(MB_TEST_SAMPLE));

    mb_free(&buffer);
}

static void mb_resizeGrowthZeroInitializesNewBytes(void)
{
    static const uint8_t expected[] = {'A', 'B', 0u, 0u, 0u, 0u};
    memory_buffer_t buffer = {0};

    TEST_ASSERT_TRUE(mb_writeString(&buffer, "AB", 2u));
    TEST_ASSERT_TRUE(mb_resize(&buffer, 6u));

    assert_buffer_bytes(&buffer, expected, sizeof(expected));
    TEST_ASSERT_EQUAL_size_t(2u, buffer.position);

    mb_free(&buffer);
}

static void mb_resizeShrinkClampsPosition(void)
{
    memory_buffer_t buffer = {0};

    TEST_ASSERT_TRUE(mb_initFromMemory(&buffer, MB_TEST_SAMPLE, sizeof(MB_TEST_SAMPLE)));
    TEST_ASSERT_TRUE(mb_seek(&buffer, buffer.length));
    TEST_ASSERT_TRUE(mb_resize(&buffer, 2u));

    TEST_ASSERT_EQUAL_size_t(2u, buffer.length);
    TEST_ASSERT_EQUAL_size_t(2u, buffer.position);
    TEST_ASSERT_TRUE(mb_isEOF(&buffer));

    mb_free(&buffer);
}

static void mb_resizeAllocationFailurePreservesState(void)
{
    memory_buffer_t buffer = {0};
    uint8_t* old_data;
    size_t old_length;
    size_t old_capacity;
    size_t old_position;

    TEST_ASSERT_TRUE(mb_initFromMemory(&buffer, MB_TEST_SAMPLE, sizeof(MB_TEST_SAMPLE)));
    TEST_ASSERT_TRUE(mb_seek(&buffer, 2u));

    old_data = buffer.data;
    old_length = buffer.length;
    old_capacity = buffer.capacity;
    old_position = buffer.position;

    mb_test_fail_next_allocation();
    TEST_ASSERT_FALSE(mb_resize(&buffer, old_capacity + 1u));

    TEST_ASSERT_EQUAL_PTR(old_data, buffer.data);
    TEST_ASSERT_EQUAL_size_t(old_length, buffer.length);
    TEST_ASSERT_EQUAL_size_t(old_capacity, buffer.capacity);
    TEST_ASSERT_EQUAL_size_t(old_position, buffer.position);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(MB_TEST_SAMPLE, buffer.data, sizeof(MB_TEST_SAMPLE));

    mb_free(&buffer);
}

static void mb_shrinkToFitReducesCapacity(void)
{
    memory_buffer_t buffer = {0};

    TEST_ASSERT_TRUE(mb_initWithCapacity(&buffer, 256u));
    TEST_ASSERT_TRUE(mb_writeString(&buffer, "hello", 5u));
    TEST_ASSERT_GREATER_THAN_size_t(buffer.length, buffer.capacity);

    TEST_ASSERT_TRUE(mb_shrinkToFit(&buffer));
    TEST_ASSERT_EQUAL_size_t(buffer.length, buffer.capacity);
    TEST_ASSERT_EQUAL_STRING_LEN("hello", (const char*)buffer.data, 5u);

    mb_free(&buffer);
}

static void mb_shrinkToFitEmptyBufferReleasesCapacity(void)
{
    memory_buffer_t buffer = {0};

    TEST_ASSERT_TRUE(mb_initWithCapacity(&buffer, 128u));
    TEST_ASSERT_NOT_NULL(buffer.data);
    TEST_ASSERT_TRUE(mb_shrinkToFit(&buffer));

    TEST_ASSERT_NULL(buffer.data);
    TEST_ASSERT_EQUAL_size_t(0u, buffer.length);
    TEST_ASSERT_EQUAL_size_t(0u, buffer.capacity);
    TEST_ASSERT_EQUAL_size_t(0u, buffer.position);
}

static void mb_clearKeepsCapacityAndAllowsReuse(void)
{
    memory_buffer_t buffer = {0};
    size_t capacity;

    TEST_ASSERT_TRUE(mb_writeString(&buffer, "first", 5u));
    capacity = buffer.capacity;

    mb_clear(&buffer);
    TEST_ASSERT_EQUAL_size_t(0u, buffer.length);
    TEST_ASSERT_EQUAL_size_t(0u, buffer.position);
    TEST_ASSERT_EQUAL_size_t(capacity, buffer.capacity);

    TEST_ASSERT_TRUE(mb_writeString(&buffer, "second", 6u));
    TEST_ASSERT_EQUAL_STRING_LEN("second", (const char*)buffer.data, 6u);

    mb_free(&buffer);
}

static void mb_rewindOnlyChangesPosition(void)
{
    memory_buffer_t buffer = {0};

    TEST_ASSERT_TRUE(mb_initFromMemory(&buffer, MB_TEST_SAMPLE, sizeof(MB_TEST_SAMPLE)));
    TEST_ASSERT_TRUE(mb_seek(&buffer, 4u));
    mb_rewind(&buffer);

    TEST_ASSERT_EQUAL_size_t(0u, buffer.position);
    TEST_ASSERT_EQUAL_size_t(sizeof(MB_TEST_SAMPLE), buffer.length);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(MB_TEST_SAMPLE, buffer.data, sizeof(MB_TEST_SAMPLE));

    mb_free(&buffer);
}

static void mb_seekAcceptsStartMiddleAndEndButRejectsPastEnd(void)
{
    memory_buffer_t buffer = {0};

    TEST_ASSERT_TRUE(mb_initFromMemory(&buffer, MB_TEST_SAMPLE, sizeof(MB_TEST_SAMPLE)));
    TEST_ASSERT_TRUE(mb_seek(&buffer, 0u));
    TEST_ASSERT_TRUE(mb_seek(&buffer, 3u));
    TEST_ASSERT_TRUE(mb_seek(&buffer, buffer.length));
    TEST_ASSERT_TRUE(mb_isEOF(&buffer));

    TEST_ASSERT_FALSE(mb_seek(&buffer, buffer.length + 1u));
    TEST_ASSERT_EQUAL_size_t(buffer.length, buffer.position);

    mb_free(&buffer);
}

static void mb_skipHandlesBothDirectionsTransactionally(void)
{
    memory_buffer_t buffer = {0};

    TEST_ASSERT_TRUE(mb_initFromMemory(&buffer, MB_TEST_SAMPLE, sizeof(MB_TEST_SAMPLE)));
    TEST_ASSERT_TRUE(mb_skip(&buffer, 4));
    TEST_ASSERT_EQUAL_size_t(4u, buffer.position);
    TEST_ASSERT_TRUE(mb_skip(&buffer, -3));
    TEST_ASSERT_EQUAL_size_t(1u, buffer.position);

    TEST_ASSERT_FALSE(mb_skip(&buffer, -2));
    TEST_ASSERT_EQUAL_size_t(1u, buffer.position);
    TEST_ASSERT_FALSE(mb_skip(&buffer, 6));
    TEST_ASSERT_EQUAL_size_t(1u, buffer.position);
    TEST_ASSERT_FALSE(mb_skip(&buffer, PTRDIFF_MIN));
    TEST_ASSERT_EQUAL_size_t(1u, buffer.position);

    mb_free(&buffer);
}

/* -------------------------------------------------------------------------- */
/* Search                                                                      */
/* -------------------------------------------------------------------------- */

static void mb_scanToFindsBinaryPatternAndLeavesCursorAtMatch(void)
{
    const uint8_t input[] = {0u, 1u, 2u, 0u, 1u, 3u};
    const uint8_t pattern[] = {0u, 1u, 3u};
    memory_buffer_t buffer = {0};

    TEST_ASSERT_TRUE(mb_initFromMemory(&buffer, input, sizeof(input)));
    TEST_ASSERT_TRUE(mb_scanTo(&buffer, pattern, sizeof(pattern)));
    TEST_ASSERT_EQUAL_size_t(3u, buffer.position);

    mb_free(&buffer);
}

static void mb_scanToFailurePreservesPosition(void)
{
    const uint8_t pattern[] = {9u, 9u};
    memory_buffer_t buffer = {0};

    TEST_ASSERT_TRUE(mb_initFromMemory(&buffer, MB_TEST_SAMPLE, sizeof(MB_TEST_SAMPLE)));
    TEST_ASSERT_TRUE(mb_seek(&buffer, 2u));
    TEST_ASSERT_FALSE(mb_scanTo(&buffer, pattern, sizeof(pattern)));
    TEST_ASSERT_EQUAL_size_t(2u, buffer.position);

    mb_free(&buffer);
}

static void mb_scanToEmptyPatternMatchesCurrentPosition(void)
{
    memory_buffer_t buffer = {0};

    TEST_ASSERT_TRUE(mb_initFromMemory(&buffer, MB_TEST_SAMPLE, sizeof(MB_TEST_SAMPLE)));
    TEST_ASSERT_TRUE(mb_seek(&buffer, 4u));
    TEST_ASSERT_TRUE(mb_scanTo(&buffer, NULL, 0u));
    TEST_ASSERT_EQUAL_size_t(4u, buffer.position);

    mb_free(&buffer);
}

static void mb_scanToChecksLastPossibleOffset(void)
{
    const uint8_t pattern[] = {0x50u, 0x60u};
    memory_buffer_t buffer = {0};

    TEST_ASSERT_TRUE(mb_initFromMemory(&buffer, MB_TEST_SAMPLE, sizeof(MB_TEST_SAMPLE)));
    TEST_ASSERT_TRUE(mb_scanTo(&buffer, pattern, sizeof(pattern)));
    TEST_ASSERT_EQUAL_size_t(4u, buffer.position);

    mb_free(&buffer);
}

/* -------------------------------------------------------------------------- */
/* Raw I/O                                                                     */
/* -------------------------------------------------------------------------- */

static void mb_rawWriteAndReadRoundTrip(void)
{
    memory_buffer_t buffer = {0};
    uint8_t output[sizeof(MB_TEST_SAMPLE)] = {0};

    TEST_ASSERT_TRUE(mb_writeBytes(&buffer, MB_TEST_SAMPLE, sizeof(MB_TEST_SAMPLE)));
    TEST_ASSERT_EQUAL_size_t(sizeof(MB_TEST_SAMPLE), buffer.length);
    TEST_ASSERT_EQUAL_size_t(sizeof(MB_TEST_SAMPLE), buffer.position);

    mb_rewind(&buffer);
    TEST_ASSERT_TRUE(mb_readBytes(&buffer, output, sizeof(output)));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(MB_TEST_SAMPLE, output, sizeof(output));
    TEST_ASSERT_TRUE(mb_isEOF(&buffer));

    mb_free(&buffer);
}

static void mb_writeOverwritesWithinLengthAndAppendsAtEnd(void)
{
    static const uint8_t expected[] = {1u, 9u, 8u, 4u, 5u};
    const uint8_t replacement[] = {9u, 8u};
    memory_buffer_t buffer = {0};

    TEST_ASSERT_TRUE(mb_initFromMemory(&buffer, (uint8_t[]){1u, 2u, 3u, 4u}, 4u));
    TEST_ASSERT_TRUE(mb_seek(&buffer, 1u));
    TEST_ASSERT_TRUE(mb_writeBytes(&buffer, replacement, sizeof(replacement)));
    TEST_ASSERT_EQUAL_size_t(4u, buffer.length);
    TEST_ASSERT_TRUE(mb_seek(&buffer, buffer.length));
    TEST_ASSERT_TRUE(mb_write(&buffer, 5u));

    assert_buffer_bytes(&buffer, expected, sizeof(expected));

    mb_free(&buffer);
}

static void mb_internalOverlappingWriteIsSupported(void)
{
    static const uint8_t expected[] = {'a', 'b', 'a', 'b', 'c', 'd'};
    memory_buffer_t buffer = {0};

    /* Copy initialization gives capacity == length, so this write must grow. */
    TEST_ASSERT_TRUE(mb_initFromMemory(&buffer, "abcd", 4u));
    TEST_ASSERT_TRUE(mb_seek(&buffer, 2u));
    TEST_ASSERT_TRUE(mb_writeBytes(&buffer, buffer.data, 4u));

    assert_buffer_bytes(&buffer, expected, sizeof(expected));

    mb_free(&buffer);
}

static void mb_internalGrowthTemporaryAllocationFailurePreservesState(void)
{
    memory_buffer_t buffer = {0};
    uint8_t* old_data;

    TEST_ASSERT_TRUE(mb_initFromMemory(&buffer, "abcd", 4u));
    TEST_ASSERT_TRUE(mb_seek(&buffer, 2u));
    old_data = buffer.data;
    mb_test_fail_next_allocation();

    TEST_ASSERT_FALSE(mb_writeBytes(&buffer, buffer.data, 4u));
    TEST_ASSERT_EQUAL_PTR(old_data, buffer.data);
    TEST_ASSERT_EQUAL_size_t(4u, buffer.length);
    TEST_ASSERT_EQUAL_size_t(4u, buffer.capacity);
    TEST_ASSERT_EQUAL_size_t(2u, buffer.position);
    TEST_ASSERT_EQUAL_STRING_LEN("abcd", (const char*)buffer.data, 4u);

    mb_free(&buffer);
}

static void mb_internalGrowthReserveFailurePreservesState(void)
{
    memory_buffer_t buffer = {0};
    uint8_t* old_data;

    TEST_ASSERT_TRUE(mb_initFromMemory(&buffer, "abcd", 4u));
    TEST_ASSERT_TRUE(mb_seek(&buffer, 2u));
    old_data = buffer.data;

    /* The staging allocation succeeds; the following realloc fails. */
    mb_test_fail_on_allocation = mb_test_allocation_count + 2u;

    TEST_ASSERT_FALSE(mb_writeBytes(&buffer, buffer.data, 4u));
    TEST_ASSERT_EQUAL_PTR(old_data, buffer.data);
    TEST_ASSERT_EQUAL_size_t(4u, buffer.length);
    TEST_ASSERT_EQUAL_size_t(4u, buffer.capacity);
    TEST_ASSERT_EQUAL_size_t(2u, buffer.position);
    TEST_ASSERT_EQUAL_STRING_LEN("abcd", (const char*)buffer.data, 4u);

    mb_free(&buffer);
}

static void mb_zeroLengthReadAndWriteAcceptNullPointers(void)
{
    memory_buffer_t buffer = {0};

    mb_initEmpty(&buffer);
    TEST_ASSERT_TRUE(mb_writeBytes(&buffer, NULL, 0u));
    TEST_ASSERT_TRUE(mb_readBytes(&buffer, NULL, 0u));
    TEST_ASSERT_EQUAL_size_t(0u, buffer.position);
    TEST_ASSERT_EQUAL_size_t(0u, buffer.length);
}

static void mb_nonEmptyReadAndWriteRejectNullPointers(void)
{
    memory_buffer_t buffer = {0};

    TEST_ASSERT_FALSE(mb_writeBytes(&buffer, NULL, 1u));
    TEST_ASSERT_EQUAL_size_t(0u, buffer.position);
    TEST_ASSERT_EQUAL_size_t(0u, buffer.length);

    TEST_ASSERT_TRUE(mb_write(&buffer, 1u));
    mb_rewind(&buffer);
    TEST_ASSERT_FALSE(mb_readBytes(&buffer, NULL, 1u));
    TEST_ASSERT_EQUAL_size_t(0u, buffer.position);

    mb_free(&buffer);
}

static void mb_shortReadDoesNotAdvanceOrModifyDestination(void)
{
    const uint8_t input[] = {1u, 2u, 3u};
    uint8_t output[] = {0xAAu, 0xBBu, 0xCCu, 0xDDu};
    static const uint8_t unchanged[] = {0xAAu, 0xBBu, 0xCCu, 0xDDu};
    memory_buffer_t buffer = {0};

    TEST_ASSERT_TRUE(mb_initFromMemory(&buffer, input, sizeof(input)));
    TEST_ASSERT_FALSE(mb_readBytes(&buffer, output, sizeof(output)));
    TEST_ASSERT_EQUAL_size_t(0u, buffer.position);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(unchanged, output, sizeof(output));

    mb_free(&buffer);
}

static void mb_peekAndReadAtDoNotMoveCursor(void)
{
    uint8_t output[2] = {0};
    memory_buffer_t buffer = {0};

    TEST_ASSERT_TRUE(mb_initFromMemory(&buffer, MB_TEST_SAMPLE, sizeof(MB_TEST_SAMPLE)));
    TEST_ASSERT_TRUE(mb_seek(&buffer, 2u));

    TEST_ASSERT_TRUE(mb_peekBytes(&buffer, output, sizeof(output)));
    TEST_ASSERT_EQUAL_UINT8(0x30u, output[0]);
    TEST_ASSERT_EQUAL_UINT8(0x40u, output[1]);
    TEST_ASSERT_EQUAL_size_t(2u, buffer.position);

    TEST_ASSERT_TRUE(mb_readAt(&buffer, 4u, output, sizeof(output)));
    TEST_ASSERT_EQUAL_UINT8(0x50u, output[0]);
    TEST_ASSERT_EQUAL_UINT8(0x60u, output[1]);
    TEST_ASSERT_EQUAL_size_t(2u, buffer.position);

    mb_free(&buffer);
}

static void mb_writeAtDoesNotMoveCursor(void)
{
    static const uint8_t expected[] = {0x10u, 0xAAu, 0xBBu, 0x40u, 0x50u, 0x60u};
    const uint8_t replacement[] = {0xAAu, 0xBBu};
    memory_buffer_t buffer = {0};

    TEST_ASSERT_TRUE(mb_initFromMemory(&buffer, MB_TEST_SAMPLE, sizeof(MB_TEST_SAMPLE)));
    TEST_ASSERT_TRUE(mb_seek(&buffer, 5u));
    TEST_ASSERT_TRUE(mb_writeAt(&buffer, 1u, replacement, sizeof(replacement)));

    TEST_ASSERT_EQUAL_size_t(5u, buffer.position);
    assert_buffer_bytes(&buffer, expected, sizeof(expected));

    mb_free(&buffer);
}

static void mb_writeZerosAppendsAndOverwrites(void)
{
    static const uint8_t expected[] = {'A', 0u, 0u, 0u};
    memory_buffer_t buffer = {0};

    TEST_ASSERT_TRUE(mb_writeString(&buffer, "AB", 2u));
    TEST_ASSERT_TRUE(mb_seek(&buffer, 1u));
    TEST_ASSERT_TRUE(mb_writeZeros(&buffer, 3u));

    assert_buffer_bytes(&buffer, expected, sizeof(expected));

    mb_free(&buffer);
}

static void mb_writeAllocationFailurePreservesState(void)
{
    memory_buffer_t buffer = {0};

    mb_test_fail_next_allocation();
    TEST_ASSERT_FALSE(mb_writeString(&buffer, "x", 1u));
    TEST_ASSERT_NULL(buffer.data);
    TEST_ASSERT_EQUAL_size_t(0u, buffer.length);
    TEST_ASSERT_EQUAL_size_t(0u, buffer.capacity);
    TEST_ASSERT_EQUAL_size_t(0u, buffer.position);
}

static void mb_writeRejectsSizeOverflowBeforeTouchingMemory(void)
{
    uint8_t dummy = 0u;
    uint8_t value = 1u;
    memory_buffer_t artificial = {
        &dummy,
        SIZE_MAX,
        SIZE_MAX,
        SIZE_MAX
    };

    TEST_ASSERT_FALSE(mb_writeBytes(&artificial, &value, 1u));
    TEST_ASSERT_EQUAL_size_t(SIZE_MAX, artificial.position);
    TEST_ASSERT_EQUAL_size_t(SIZE_MAX, artificial.length);
}

/* -------------------------------------------------------------------------- */
/* Integer encoding                                                            */
/* -------------------------------------------------------------------------- */

static void mb_uint16RoundTripsBothByteOrders(void)
{
    static const uint16_t values[] = {0u, 1u, 0x7FFFu, 0x8000u, 0xFFFFu};
    size_t i;
    memory_buffer_t buffer = {0};

    for (i = 0u; i < sizeof(values) / sizeof(values[0]); ++i)
    {
        uint16_t le = 0u;
        uint16_t be = 0u;
        mb_clear(&buffer);

        TEST_ASSERT_TRUE(mb_writeUInt16LE(&buffer, values[i]));
        TEST_ASSERT_TRUE(mb_writeUInt16BE(&buffer, values[i]));
        mb_rewind(&buffer);
        TEST_ASSERT_TRUE(mb_readUInt16LE(&buffer, &le));
        TEST_ASSERT_TRUE(mb_readUInt16BE(&buffer, &be));
        TEST_ASSERT_EQUAL_UINT16(values[i], le);
        TEST_ASSERT_EQUAL_UINT16(values[i], be);
    }

    mb_free(&buffer);
}

static void mb_int16RoundTripsBothByteOrders(void)
{
    static const int16_t values[] = {INT16_MIN, -1, 0, 1, INT16_MAX};
    size_t i;
    memory_buffer_t buffer = {0};

    for (i = 0u; i < sizeof(values) / sizeof(values[0]); ++i)
    {
        int16_t le = 0;
        int16_t be = 0;
        mb_clear(&buffer);

        TEST_ASSERT_TRUE(mb_writeInt16LE(&buffer, values[i]));
        TEST_ASSERT_TRUE(mb_writeInt16BE(&buffer, values[i]));
        mb_rewind(&buffer);
        TEST_ASSERT_TRUE(mb_readInt16LE(&buffer, &le));
        TEST_ASSERT_TRUE(mb_readInt16BE(&buffer, &be));
        TEST_ASSERT_EQUAL_INT16(values[i], le);
        TEST_ASSERT_EQUAL_INT16(values[i], be);
    }

    mb_free(&buffer);
}

static void mb_uint24RoundTripsBoundariesAndRejectsOverflow(void)
{
    static const uint32_t values[] = {0u, 1u, 0x7FFFFFu, 0x800000u, MB_UINT24_MAX};
    size_t i;
    memory_buffer_t buffer = {0};

    for (i = 0u; i < sizeof(values) / sizeof(values[0]); ++i)
    {
        uint32_t le = 0u;
        uint32_t be = 0u;
        mb_clear(&buffer);

        TEST_ASSERT_TRUE(mb_writeUInt24LE(&buffer, values[i]));
        TEST_ASSERT_TRUE(mb_writeUInt24BE(&buffer, values[i]));
        mb_rewind(&buffer);
        TEST_ASSERT_TRUE(mb_readUInt24LE(&buffer, &le));
        TEST_ASSERT_TRUE(mb_readUInt24BE(&buffer, &be));
        TEST_ASSERT_EQUAL_UINT32(values[i], le);
        TEST_ASSERT_EQUAL_UINT32(values[i], be);
    }

    mb_clear(&buffer);
    TEST_ASSERT_FALSE(mb_writeUInt24LE(&buffer, MB_UINT24_MAX + 1u));
    TEST_ASSERT_FALSE(mb_writeUInt24BE(&buffer, MB_UINT24_MAX + 1u));
    TEST_ASSERT_EQUAL_size_t(0u, buffer.length);

    mb_free(&buffer);
}

static void mb_int24RoundTripsNegativeBoundariesAndRejectsOverflow(void)
{
    static const int32_t values[] = {MB_INT24_MIN, -1, 0, 1, MB_INT24_MAX};
    size_t i;
    memory_buffer_t buffer = {0};

    for (i = 0u; i < sizeof(values) / sizeof(values[0]); ++i)
    {
        int32_t le = 0;
        int32_t be = 0;
        mb_clear(&buffer);

        TEST_ASSERT_TRUE(mb_writeInt24LE(&buffer, values[i]));
        TEST_ASSERT_TRUE(mb_writeInt24BE(&buffer, values[i]));
        mb_rewind(&buffer);
        TEST_ASSERT_TRUE(mb_readInt24LE(&buffer, &le));
        TEST_ASSERT_TRUE(mb_readInt24BE(&buffer, &be));
        TEST_ASSERT_EQUAL_INT32(values[i], le);
        TEST_ASSERT_EQUAL_INT32(values[i], be);
    }

    mb_clear(&buffer);
    TEST_ASSERT_FALSE(mb_writeInt24LE(&buffer, MB_INT24_MIN - 1));
    TEST_ASSERT_FALSE(mb_writeInt24BE(&buffer, MB_INT24_MAX + 1));
    TEST_ASSERT_EQUAL_size_t(0u, buffer.length);

    mb_free(&buffer);
}

static void mb_uint32RoundTripsHighBitValuesWithoutUndefinedBehavior(void)
{
    static const uint32_t values[] = {
        0u, 1u, 0x7FFFFFFFu, 0x80000000u, 0x89ABCDEFu, UINT32_MAX
    };
    size_t i;
    memory_buffer_t buffer = {0};

    for (i = 0u; i < sizeof(values) / sizeof(values[0]); ++i)
    {
        uint32_t le = 0u;
        uint32_t be = 0u;
        mb_clear(&buffer);

        TEST_ASSERT_TRUE(mb_writeUInt32LE(&buffer, values[i]));
        TEST_ASSERT_TRUE(mb_writeUInt32BE(&buffer, values[i]));
        mb_rewind(&buffer);
        TEST_ASSERT_TRUE(mb_readUInt32LE(&buffer, &le));
        TEST_ASSERT_TRUE(mb_readUInt32BE(&buffer, &be));
        TEST_ASSERT_EQUAL_UINT32(values[i], le);
        TEST_ASSERT_EQUAL_UINT32(values[i], be);
    }

    mb_free(&buffer);
}

static void mb_int32RoundTripsBothByteOrders(void)
{
    static const int32_t values[] = {INT32_MIN, -1, 0, 1, INT32_MAX};
    size_t i;
    memory_buffer_t buffer = {0};

    for (i = 0u; i < sizeof(values) / sizeof(values[0]); ++i)
    {
        int32_t le = 0;
        int32_t be = 0;
        mb_clear(&buffer);

        TEST_ASSERT_TRUE(mb_writeInt32LE(&buffer, values[i]));
        TEST_ASSERT_TRUE(mb_writeInt32BE(&buffer, values[i]));
        mb_rewind(&buffer);
        TEST_ASSERT_TRUE(mb_readInt32LE(&buffer, &le));
        TEST_ASSERT_TRUE(mb_readInt32BE(&buffer, &be));
        TEST_ASSERT_EQUAL_INT32(values[i], le);
        TEST_ASSERT_EQUAL_INT32(values[i], be);
    }

    mb_free(&buffer);
}

static void mb_uint64RoundTripsBothByteOrders(void)
{
    static const uint64_t values[] = {
        UINT64_C(0),
        UINT64_C(1),
        UINT64_C(0x7FFFFFFFFFFFFFFF),
        UINT64_C(0x8000000000000000),
        UINT64_C(0x89ABCDEF01234567),
        UINT64_MAX
    };
    size_t i;
    memory_buffer_t buffer = {0};

    for (i = 0u; i < sizeof(values) / sizeof(values[0]); ++i)
    {
        uint64_t le = 0u;
        uint64_t be = 0u;
        mb_clear(&buffer);

        TEST_ASSERT_TRUE(mb_writeUInt64LE(&buffer, values[i]));
        TEST_ASSERT_TRUE(mb_writeUInt64BE(&buffer, values[i]));
        mb_rewind(&buffer);
        TEST_ASSERT_TRUE(mb_readUInt64LE(&buffer, &le));
        TEST_ASSERT_TRUE(mb_readUInt64BE(&buffer, &be));
        TEST_ASSERT_EQUAL_UINT64(values[i], le);
        TEST_ASSERT_EQUAL_UINT64(values[i], be);
    }

    mb_free(&buffer);
}

static void mb_int64RoundTripsBothByteOrders(void)
{
    static const int64_t values[] = {INT64_MIN, -1, 0, 1, INT64_MAX};
    size_t i;
    memory_buffer_t buffer = {0};

    for (i = 0u; i < sizeof(values) / sizeof(values[0]); ++i)
    {
        int64_t le = 0;
        int64_t be = 0;
        mb_clear(&buffer);

        TEST_ASSERT_TRUE(mb_writeInt64LE(&buffer, values[i]));
        TEST_ASSERT_TRUE(mb_writeInt64BE(&buffer, values[i]));
        mb_rewind(&buffer);
        TEST_ASSERT_TRUE(mb_readInt64LE(&buffer, &le));
        TEST_ASSERT_TRUE(mb_readInt64BE(&buffer, &be));
        TEST_ASSERT_EQUAL_INT64(values[i], le);
        TEST_ASSERT_EQUAL_INT64(values[i], be);
    }

    mb_free(&buffer);
}

static void mb_integerWritersProduceExactByteOrder(void)
{
    static const uint8_t expected[] = {
        0x34u, 0x12u,
        0x12u, 0x34u,
        0xEFu, 0xCDu, 0xABu, 0x89u,
        0x89u, 0xABu, 0xCDu, 0xEFu
    };
    memory_buffer_t buffer = {0};

    TEST_ASSERT_TRUE(mb_writeUInt16LE(&buffer, 0x1234u));
    TEST_ASSERT_TRUE(mb_writeUInt16BE(&buffer, 0x1234u));
    TEST_ASSERT_TRUE(mb_writeUInt32LE(&buffer, 0x89ABCDEFu));
    TEST_ASSERT_TRUE(mb_writeUInt32BE(&buffer, 0x89ABCDEFu));

    assert_buffer_bytes(&buffer, expected, sizeof(expected));
    mb_free(&buffer);
}

static void mb_typedShortReadIsTransactional(void)
{
    const uint8_t bytes[] = {0x11u, 0x22u, 0x33u};
    memory_buffer_t buffer = {0};
    uint32_t value = 0xDEADBEEFu;

    TEST_ASSERT_TRUE(mb_initFromMemory(&buffer, bytes, sizeof(bytes)));
    TEST_ASSERT_FALSE(mb_readUInt32LE(&buffer, &value));
    TEST_ASSERT_EQUAL_size_t(0u, buffer.position);
    TEST_ASSERT_EQUAL_UINT32(0xDEADBEEFu, value);

    mb_free(&buffer);
}

static void mb_typedReadRejectsNullOutputWithoutMoving(void)
{
    const uint8_t bytes[] = {0u, 0u, 0u, 1u};
    memory_buffer_t buffer = {0};

    TEST_ASSERT_TRUE(mb_initFromMemory(&buffer, bytes, sizeof(bytes)));
    TEST_ASSERT_FALSE(mb_readUInt32BE(&buffer, NULL));
    TEST_ASSERT_EQUAL_size_t(0u, buffer.position);

    mb_free(&buffer);
}

/* -------------------------------------------------------------------------- */
/* Integrated and stress scenarios                                             */
/* -------------------------------------------------------------------------- */

static void mb_mixedFormatIntegrationRoundTrip(void)
{
    memory_buffer_t buffer = {0};
    int16_t i16 = 0;
    int32_t i24 = 0;
    uint32_t u32 = 0u;
    int64_t i64 = 0;
    char text[3] = {0};

    TEST_ASSERT_TRUE(mb_writeInt16LE(&buffer, INT16_MIN));
    TEST_ASSERT_TRUE(mb_writeInt24BE(&buffer, -123456));
    TEST_ASSERT_TRUE(mb_writeUInt32BE(&buffer, 0x89ABCDEFu));
    TEST_ASSERT_TRUE(mb_writeInt64LE(&buffer, INT64_MIN));
    TEST_ASSERT_TRUE(mb_writeString(&buffer, "OK", 2u));

    mb_rewind(&buffer);
    TEST_ASSERT_TRUE(mb_readInt16LE(&buffer, &i16));
    TEST_ASSERT_TRUE(mb_readInt24BE(&buffer, &i24));
    TEST_ASSERT_TRUE(mb_readUInt32BE(&buffer, &u32));
    TEST_ASSERT_TRUE(mb_readInt64LE(&buffer, &i64));
    TEST_ASSERT_TRUE(mb_readString(&buffer, text, 2u));

    TEST_ASSERT_EQUAL_INT16(INT16_MIN, i16);
    TEST_ASSERT_EQUAL_INT32(-123456, i24);
    TEST_ASSERT_EQUAL_UINT32(0x89ABCDEFu, u32);
    TEST_ASSERT_EQUAL_INT64(INT64_MIN, i64);
    TEST_ASSERT_EQUAL_STRING("OK", text);
    TEST_ASSERT_TRUE(mb_isEOF(&buffer));

    mb_free(&buffer);
}

static void mb_stressSequentialUInt32RoundTrip(void)
{
    memory_buffer_t buffer = {0};
    uint32_t state = UINT32_C(0xC0FFEE01);
    uint32_t expected[MB_TEST_STRESS_COUNT];
    size_t i;

    for (i = 0u; i < MB_TEST_STRESS_COUNT; ++i)
    {
        expected[i] = mb_test_prng(&state);
        TEST_ASSERT_TRUE(mb_writeUInt32LE(&buffer, expected[i]));
    }

    TEST_ASSERT_EQUAL_size_t(MB_TEST_STRESS_COUNT * sizeof(uint32_t), buffer.length);
    mb_rewind(&buffer);

    for (i = 0u; i < MB_TEST_STRESS_COUNT; ++i)
    {
        uint32_t value = 0u;
        TEST_ASSERT_TRUE(mb_readUInt32LE(&buffer, &value));
        TEST_ASSERT_EQUAL_UINT32(expected[i], value);
    }

    TEST_ASSERT_TRUE(mb_isEOF(&buffer));
    mb_free(&buffer);
}

static void mb_modelBasedOverwriteAndResizeStress(void)
{
    enum { MODEL_CAPACITY = 2048, ITERATIONS = 3000 };
    uint8_t model[MODEL_CAPACITY] = {0};
    size_t model_length = 0u;
    size_t model_position = 0u;
    uint32_t state = UINT32_C(0x12345678);
    memory_buffer_t buffer = {0};
    int iteration;

    for (iteration = 0; iteration < ITERATIONS; ++iteration)
    {
        uint32_t random = mb_test_prng(&state);
        unsigned operation = (unsigned)(random % 4u);

        if (operation == 0u && model_position < MODEL_CAPACITY)
        {
            uint8_t value = (uint8_t)(random >> 8u);
            TEST_ASSERT_TRUE(mb_write(&buffer, value));
            model[model_position] = value;
            model_position += 1u;
            if (model_position > model_length)
                model_length = model_position;
        }
        else if (operation == 1u)
        {
            size_t target = model_length == 0u
                ? 0u
                : (size_t)(random % (uint32_t)(model_length + 1u));
            TEST_ASSERT_TRUE(mb_seek(&buffer, target));
            model_position = target;
        }
        else if (operation == 2u)
        {
            size_t target = (size_t)(random % (MODEL_CAPACITY + 1u));
            TEST_ASSERT_TRUE(mb_resize(&buffer, target));
            if (target > model_length)
                memset(model + model_length, 0, target - model_length);
            model_length = target;
            if (model_position > model_length)
                model_position = model_length;
        }
        else
        {
            mb_rewind(&buffer);
            model_position = 0u;
        }

        TEST_ASSERT_EQUAL_size_t(model_length, buffer.length);
        TEST_ASSERT_EQUAL_size_t(model_position, buffer.position);
        if (model_length > 0u)
            TEST_ASSERT_EQUAL_UINT8_ARRAY(model, buffer.data, model_length);
    }

    mb_free(&buffer);
}

/* -------------------------------------------------------------------------- */

void setUp(void)
{
    mb_test_allocation_count = 0u;
    mb_test_fail_on_allocation = SIZE_MAX;
}

void tearDown(void)
{
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(mb_initEmptyCreatesCanonicalEmptyBuffer);
    RUN_TEST(mb_initWithCapacityReservesWithoutChangingLength);
    RUN_TEST(mb_initFromMemoryCopiesTheInput);
    RUN_TEST(mb_initFromMemoryAcceptsEmptyNullInput);
    RUN_TEST(mb_initFromMemoryRejectsNullNonEmptyInput);
    RUN_TEST(mb_initFromMemoryAllocationFailureLeavesEmptyBuffer);
    RUN_TEST(mb_initTakeOwnershipAndDetachRoundTrip);
    RUN_TEST(mb_initTakeOwnershipRejectsInvalidCombinations);
    RUN_TEST(mb_freeResetsFieldsAndIsIdempotent);
    RUN_TEST(mb_dataReturnsIndependentCopy);
    RUN_TEST(mb_dataOnEmptyBufferReturnsNullAndZeroLength);
    RUN_TEST(mb_dataAllocationFailureReportsZeroLength);

    RUN_TEST(mb_reservePreservesContentLengthAndPosition);
    RUN_TEST(mb_resizeGrowthZeroInitializesNewBytes);
    RUN_TEST(mb_resizeShrinkClampsPosition);
    RUN_TEST(mb_resizeAllocationFailurePreservesState);
    RUN_TEST(mb_shrinkToFitReducesCapacity);
    RUN_TEST(mb_shrinkToFitEmptyBufferReleasesCapacity);
    RUN_TEST(mb_clearKeepsCapacityAndAllowsReuse);
    RUN_TEST(mb_rewindOnlyChangesPosition);
    RUN_TEST(mb_seekAcceptsStartMiddleAndEndButRejectsPastEnd);
    RUN_TEST(mb_skipHandlesBothDirectionsTransactionally);

    RUN_TEST(mb_scanToFindsBinaryPatternAndLeavesCursorAtMatch);
    RUN_TEST(mb_scanToFailurePreservesPosition);
    RUN_TEST(mb_scanToEmptyPatternMatchesCurrentPosition);
    RUN_TEST(mb_scanToChecksLastPossibleOffset);

    RUN_TEST(mb_rawWriteAndReadRoundTrip);
    RUN_TEST(mb_writeOverwritesWithinLengthAndAppendsAtEnd);
    RUN_TEST(mb_internalOverlappingWriteIsSupported);
    RUN_TEST(mb_internalGrowthTemporaryAllocationFailurePreservesState);
    RUN_TEST(mb_internalGrowthReserveFailurePreservesState);
    RUN_TEST(mb_zeroLengthReadAndWriteAcceptNullPointers);
    RUN_TEST(mb_nonEmptyReadAndWriteRejectNullPointers);
    RUN_TEST(mb_shortReadDoesNotAdvanceOrModifyDestination);
    RUN_TEST(mb_peekAndReadAtDoNotMoveCursor);
    RUN_TEST(mb_writeAtDoesNotMoveCursor);
    RUN_TEST(mb_writeZerosAppendsAndOverwrites);
    RUN_TEST(mb_writeAllocationFailurePreservesState);
    RUN_TEST(mb_writeRejectsSizeOverflowBeforeTouchingMemory);

    RUN_TEST(mb_uint16RoundTripsBothByteOrders);
    RUN_TEST(mb_int16RoundTripsBothByteOrders);
    RUN_TEST(mb_uint24RoundTripsBoundariesAndRejectsOverflow);
    RUN_TEST(mb_int24RoundTripsNegativeBoundariesAndRejectsOverflow);
    RUN_TEST(mb_uint32RoundTripsHighBitValuesWithoutUndefinedBehavior);
    RUN_TEST(mb_int32RoundTripsBothByteOrders);
    RUN_TEST(mb_uint64RoundTripsBothByteOrders);
    RUN_TEST(mb_int64RoundTripsBothByteOrders);
    RUN_TEST(mb_integerWritersProduceExactByteOrder);
    RUN_TEST(mb_typedShortReadIsTransactional);
    RUN_TEST(mb_typedReadRejectsNullOutputWithoutMoving);

    RUN_TEST(mb_mixedFormatIntegrationRoundTrip);
    RUN_TEST(mb_stressSequentialUInt32RoundTrip);
    RUN_TEST(mb_modelBasedOverwriteAndResizeStress);

    return UNITY_END();
}
