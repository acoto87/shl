/*
 * memory_buffer.h - acoto87 (acoto87@gmail.com)
 * MIT License
 *
 * Copyright (c) 2018 Alejandro Coto Gutiérrez
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Single-header owning memory buffer with transactional reads, automatic growth
 * on writes, explicit resize/reserve operations, and endian-aware integer I/O.
 *
 * Usage:
 *
 *     #define SHL_MEMORY_BUFFER_IMPLEMENTATION
 *     #include "memory_buffer.h"
 *
 * Define SHL_MEMORY_BUFFER_IMPLEMENTATION in exactly one translation unit.
 */

#ifndef SHL_MEMORY_BUFFER_H
#define SHL_MEMORY_BUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MB_INT24_MIN  (-8388607 - 1)
#define MB_INT24_MAX  8388607
#define MB_UINT24_MAX 16777215u

typedef struct memory_buffer_t
{
    uint8_t* data;
    size_t length;
    size_t capacity;
    size_t position;
} memory_buffer_t;

/* Lightweight accessors. */
static inline uint8_t* mb_end(memory_buffer_t* buffer)
{
    if (buffer == NULL || buffer->data == NULL)
        return NULL;
    return buffer->data + buffer->length;
}

static inline const uint8_t* mb_constEnd(const memory_buffer_t* buffer)
{
    if (buffer == NULL || buffer->data == NULL)
        return NULL;
    return buffer->data + buffer->length;
}

static inline size_t mb_position(const memory_buffer_t* buffer)
{
    return buffer != NULL ? buffer->position : 0u;
}

static inline size_t mb_length(const memory_buffer_t* buffer)
{
    return buffer != NULL ? buffer->length : 0u;
}

static inline size_t mb_capacity(const memory_buffer_t* buffer)
{
    return buffer != NULL ? buffer->capacity : 0u;
}

static inline size_t mb_remaining(const memory_buffer_t* buffer)
{
    if (buffer == NULL || buffer->position > buffer->length)
        return 0u;
    return buffer->length - buffer->position;
}

/* Lifetime and ownership. */
void mb_initEmpty(memory_buffer_t* buffer);
bool mb_initWithCapacity(memory_buffer_t* buffer, size_t initial_capacity);

/* Copies data. A zero-length input may use data == NULL. */
bool mb_initFromMemory(memory_buffer_t* buffer, const void* data, size_t length);

/*
 * Transfers ownership of data to the buffer. The allocation must be compatible
 * with SHL_MEMORY_BUFFER_FREE. On failure, ownership remains with the caller.
 */
bool mb_initTakeOwnership(
    memory_buffer_t* buffer,
    uint8_t* data,
    size_t length,
    size_t capacity);

void mb_free(memory_buffer_t* buffer);

/* Returns a newly allocated copy, or NULL for an empty buffer / allocation failure. */
uint8_t* mb_data(const memory_buffer_t* buffer, size_t* length);

const uint8_t* mb_constData(const memory_buffer_t* buffer);
uint8_t* mb_mutableData(memory_buffer_t* buffer);

/* Transfers the allocation to the caller and resets the buffer. */
uint8_t* mb_detach(memory_buffer_t* buffer, size_t* length, size_t* capacity);

/* Capacity and logical-size management. */
bool mb_reserve(memory_buffer_t* buffer, size_t minimum_capacity);
bool mb_resize(memory_buffer_t* buffer, size_t new_length);
bool mb_shrinkToFit(memory_buffer_t* buffer);
void mb_clear(memory_buffer_t* buffer);
void mb_rewind(memory_buffer_t* buffer);

/* Positioning. mb_seek never grows the buffer. */
bool mb_seek(memory_buffer_t* buffer, size_t position);
bool mb_skip(memory_buffer_t* buffer, ptrdiff_t distance);

/* Search from the current cursor. Failure leaves the cursor unchanged. */
bool mb_scanTo(memory_buffer_t* buffer, const void* data, size_t length);

/* Raw reads. Failures leave position and destination unchanged. */
bool mb_read(memory_buffer_t* buffer, uint8_t* value);
bool mb_readBytes(memory_buffer_t* buffer, void* values, size_t count);
bool mb_peekBytes(const memory_buffer_t* buffer, void* values, size_t count);
bool mb_readAt(
    const memory_buffer_t* buffer,
    size_t position,
    void* values,
    size_t count);
bool mb_readString(memory_buffer_t* buffer, char* str, size_t count);

/* Raw writes. Failures leave the buffer unchanged. */
bool mb_write(memory_buffer_t* buffer, uint8_t value);
bool mb_writeBytes(memory_buffer_t* buffer, const void* values, size_t count);
bool mb_writeAt(
    memory_buffer_t* buffer,
    size_t position,
    const void* values,
    size_t count);
bool mb_writeString(memory_buffer_t* buffer, const char* str, size_t count);
bool mb_writeZeros(memory_buffer_t* buffer, size_t count);

/* Endian-aware reads. */
bool mb_readInt16LE(memory_buffer_t* buffer, int16_t* value);
bool mb_readInt16BE(memory_buffer_t* buffer, int16_t* value);
bool mb_readUInt16LE(memory_buffer_t* buffer, uint16_t* value);
bool mb_readUInt16BE(memory_buffer_t* buffer, uint16_t* value);

bool mb_readInt24LE(memory_buffer_t* buffer, int32_t* value);
bool mb_readInt24BE(memory_buffer_t* buffer, int32_t* value);
bool mb_readUInt24LE(memory_buffer_t* buffer, uint32_t* value);
bool mb_readUInt24BE(memory_buffer_t* buffer, uint32_t* value);

bool mb_readInt32LE(memory_buffer_t* buffer, int32_t* value);
bool mb_readInt32BE(memory_buffer_t* buffer, int32_t* value);
bool mb_readUInt32LE(memory_buffer_t* buffer, uint32_t* value);
bool mb_readUInt32BE(memory_buffer_t* buffer, uint32_t* value);

bool mb_readInt64LE(memory_buffer_t* buffer, int64_t* value);
bool mb_readInt64BE(memory_buffer_t* buffer, int64_t* value);
bool mb_readUInt64LE(memory_buffer_t* buffer, uint64_t* value);
bool mb_readUInt64BE(memory_buffer_t* buffer, uint64_t* value);

/* Endian-aware writes. Signed 24-bit writes reject values outside the range. */
bool mb_writeInt16LE(memory_buffer_t* buffer, int16_t value);
bool mb_writeInt16BE(memory_buffer_t* buffer, int16_t value);
bool mb_writeUInt16LE(memory_buffer_t* buffer, uint16_t value);
bool mb_writeUInt16BE(memory_buffer_t* buffer, uint16_t value);

bool mb_writeInt24LE(memory_buffer_t* buffer, int32_t value);
bool mb_writeInt24BE(memory_buffer_t* buffer, int32_t value);
bool mb_writeUInt24LE(memory_buffer_t* buffer, uint32_t value);
bool mb_writeUInt24BE(memory_buffer_t* buffer, uint32_t value);

bool mb_writeInt32LE(memory_buffer_t* buffer, int32_t value);
bool mb_writeInt32BE(memory_buffer_t* buffer, int32_t value);
bool mb_writeUInt32LE(memory_buffer_t* buffer, uint32_t value);
bool mb_writeUInt32BE(memory_buffer_t* buffer, uint32_t value);

bool mb_writeInt64LE(memory_buffer_t* buffer, int64_t value);
bool mb_writeInt64BE(memory_buffer_t* buffer, int64_t value);
bool mb_writeUInt64LE(memory_buffer_t* buffer, uint64_t value);
bool mb_writeUInt64BE(memory_buffer_t* buffer, uint64_t value);

bool mb_isEOF(const memory_buffer_t* buffer);

#ifdef __cplusplus
}
#endif

#ifdef SHL_MEMORY_BUFFER_IMPLEMENTATION

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#ifndef SHL_MEMORY_BUFFER_MALLOC
#define SHL_MEMORY_BUFFER_MALLOC(size) malloc(size)
#endif

#ifndef SHL_MEMORY_BUFFER_REALLOC
#define SHL_MEMORY_BUFFER_REALLOC(pointer, size) realloc((pointer), (size))
#endif

#ifndef SHL_MEMORY_BUFFER_FREE
#define SHL_MEMORY_BUFFER_FREE(pointer) free(pointer)
#endif

#ifndef SHL_MEMORY_BUFFER_INITIAL_CAPACITY
#define SHL_MEMORY_BUFFER_INITIAL_CAPACITY 64u
#endif

static bool mb__isValid(const memory_buffer_t* buffer)
{
    if (buffer == NULL)
        return false;
    if (buffer->length > buffer->capacity)
        return false;
    if (buffer->position > buffer->length)
        return false;
    if (buffer->capacity == 0u)
        return buffer->data == NULL;
    return buffer->data != NULL;
}

static bool mb__canReadAt(
    const memory_buffer_t* buffer,
    size_t position,
    size_t count)
{
    if (!mb__isValid(buffer))
        return false;
    if (position > buffer->length)
        return false;
    return count <= buffer->length - position;
}

static bool mb__addSize(size_t a, size_t b, size_t* result)
{
    if (result == NULL || b > SIZE_MAX - a)
        return false;
    *result = a + b;
    return true;
}

static size_t mb__nextCapacity(size_t current, size_t minimum)
{
    size_t next = current;

    if (next == 0u)
        next = (size_t)SHL_MEMORY_BUFFER_INITIAL_CAPACITY;
    if (next == 0u)
        next = 1u;

    while (next < minimum)
    {
        if (next > SIZE_MAX / 2u)
            return minimum;
        next *= 2u;
    }

    return next;
}

static bool mb__writeAtInternal(
    memory_buffer_t* buffer,
    size_t position,
    const void* values,
    size_t count,
    bool update_cursor)
{
    size_t end_position;
    uint8_t* temporary = NULL;
    const void* source = values;

    if (!mb__isValid(buffer))
        return false;
    if (position > buffer->length)
        return false;
    if (count == 0u)
    {
        if (update_cursor)
            buffer->position = position;
        return true;
    }
    if (values == NULL)
        return false;
    if (!mb__addSize(position, count, &end_position))
        return false;

    /*
     * realloc may invalidate an internal source pointer. When growth is needed
     * and storage already exists, stage the bytes before reserving. This keeps
     * overlapping writes valid without non-portable pointer-range comparisons.
     */
    if (end_position > buffer->capacity && buffer->data != NULL)
    {
        temporary = (uint8_t*)SHL_MEMORY_BUFFER_MALLOC(count);
        if (temporary == NULL)
            return false;
        memcpy(temporary, values, count);
        source = temporary;
    }

    if (!mb_reserve(buffer, end_position))
    {
        if (temporary != NULL)
            SHL_MEMORY_BUFFER_FREE(temporary);
        return false;
    }

    memmove(buffer->data + position, source, count);

    if (temporary != NULL)
        SHL_MEMORY_BUFFER_FREE(temporary);

    if (end_position > buffer->length)
        buffer->length = end_position;
    if (update_cursor)
        buffer->position = end_position;

    return true;
}

static int16_t mb__u16ToI16(uint16_t value)
{
    if (value <= (uint16_t)INT16_MAX)
        return (int16_t)value;
    return (int16_t)(-1 - (int16_t)(UINT16_MAX - value));
}

static int32_t mb__u24ToI32(uint32_t value)
{
    value &= MB_UINT24_MAX;
    if (value <= (uint32_t)MB_INT24_MAX)
        return (int32_t)value;
    return -1 - (int32_t)(MB_UINT24_MAX - value);
}

static int32_t mb__u32ToI32(uint32_t value)
{
    if (value <= (uint32_t)INT32_MAX)
        return (int32_t)value;
    return -1 - (int32_t)(UINT32_MAX - value);
}

static int64_t mb__u64ToI64(uint64_t value)
{
    if (value <= (uint64_t)INT64_MAX)
        return (int64_t)value;
    return -1 - (int64_t)(UINT64_MAX - value);
}

void mb_initEmpty(memory_buffer_t* buffer)
{
    if (buffer == NULL)
        return;

    buffer->data = NULL;
    buffer->length = 0u;
    buffer->capacity = 0u;
    buffer->position = 0u;
}

bool mb_initWithCapacity(memory_buffer_t* buffer, size_t initial_capacity)
{
    if (buffer == NULL)
        return false;

    mb_initEmpty(buffer);
    return mb_reserve(buffer, initial_capacity);
}

bool mb_initFromMemory(memory_buffer_t* buffer, const void* data, size_t length)
{
    uint8_t* copy;

    if (buffer == NULL)
        return false;

    mb_initEmpty(buffer);

    if (length == 0u)
        return true;
    if (data == NULL)
        return false;

    copy = (uint8_t*)SHL_MEMORY_BUFFER_MALLOC(length);
    if (copy == NULL)
        return false;

    memcpy(copy, data, length);
    buffer->data = copy;
    buffer->length = length;
    buffer->capacity = length;
    buffer->position = 0u;
    return true;
}

bool mb_initTakeOwnership(
    memory_buffer_t* buffer,
    uint8_t* data,
    size_t length,
    size_t capacity)
{
    if (buffer == NULL)
        return false;
    if (length > capacity)
        return false;
    if (capacity == 0u && data != NULL)
        return false;
    if (capacity > 0u && data == NULL)
        return false;

    buffer->data = data;
    buffer->length = length;
    buffer->capacity = capacity;
    buffer->position = 0u;
    return true;
}

void mb_free(memory_buffer_t* buffer)
{
    if (buffer == NULL)
        return;

    if (buffer->data != NULL)
        SHL_MEMORY_BUFFER_FREE(buffer->data);

    mb_initEmpty(buffer);
}

uint8_t* mb_data(const memory_buffer_t* buffer, size_t* length)
{
    uint8_t* copy;

    if (length != NULL)
        *length = 0u;

    if (!mb__isValid(buffer))
        return NULL;
    if (buffer->length == 0u)
        return NULL;

    copy = (uint8_t*)SHL_MEMORY_BUFFER_MALLOC(buffer->length);
    if (copy == NULL)
        return NULL;

    memcpy(copy, buffer->data, buffer->length);
    if (length != NULL)
        *length = buffer->length;
    return copy;
}

const uint8_t* mb_constData(const memory_buffer_t* buffer)
{
    return mb__isValid(buffer) ? buffer->data : NULL;
}

uint8_t* mb_mutableData(memory_buffer_t* buffer)
{
    return mb__isValid(buffer) ? buffer->data : NULL;
}

uint8_t* mb_detach(memory_buffer_t* buffer, size_t* length, size_t* capacity)
{
    uint8_t* data;

    if (length != NULL)
        *length = 0u;
    if (capacity != NULL)
        *capacity = 0u;

    if (!mb__isValid(buffer))
        return NULL;

    data = buffer->data;
    if (length != NULL)
        *length = buffer->length;
    if (capacity != NULL)
        *capacity = buffer->capacity;

    mb_initEmpty(buffer);
    return data;
}

bool mb_reserve(memory_buffer_t* buffer, size_t minimum_capacity)
{
    size_t new_capacity;
    uint8_t* new_data;

    if (!mb__isValid(buffer))
        return false;
    if (minimum_capacity <= buffer->capacity)
        return true;

    new_capacity = mb__nextCapacity(buffer->capacity, minimum_capacity);
    if (new_capacity < minimum_capacity)
        return false;

    new_data = (uint8_t*)SHL_MEMORY_BUFFER_REALLOC(buffer->data, new_capacity);
    if (new_data == NULL)
        return false;

    buffer->data = new_data;
    buffer->capacity = new_capacity;
    return true;
}

bool mb_resize(memory_buffer_t* buffer, size_t new_length)
{
    size_t old_length;

    if (!mb__isValid(buffer))
        return false;

    old_length = buffer->length;

    if (new_length > old_length)
    {
        if (!mb_reserve(buffer, new_length))
            return false;
        memset(buffer->data + old_length, 0, new_length - old_length);
    }

    buffer->length = new_length;
    if (buffer->position > new_length)
        buffer->position = new_length;
    return true;
}

bool mb_shrinkToFit(memory_buffer_t* buffer)
{
    uint8_t* new_data;

    if (!mb__isValid(buffer))
        return false;
    if (buffer->capacity == buffer->length)
        return true;

    if (buffer->length == 0u)
    {
        if (buffer->data != NULL)
            SHL_MEMORY_BUFFER_FREE(buffer->data);
        buffer->data = NULL;
        buffer->capacity = 0u;
        return true;
    }

    new_data = (uint8_t*)SHL_MEMORY_BUFFER_REALLOC(buffer->data, buffer->length);
    if (new_data == NULL)
        return false;

    buffer->data = new_data;
    buffer->capacity = buffer->length;
    return true;
}

void mb_clear(memory_buffer_t* buffer)
{
    if (!mb__isValid(buffer))
        return;
    buffer->length = 0u;
    buffer->position = 0u;
}

void mb_rewind(memory_buffer_t* buffer)
{
    if (!mb__isValid(buffer))
        return;
    buffer->position = 0u;
}

bool mb_seek(memory_buffer_t* buffer, size_t position)
{
    if (!mb__isValid(buffer))
        return false;
    if (position > buffer->length)
        return false;

    buffer->position = position;
    return true;
}

bool mb_skip(memory_buffer_t* buffer, ptrdiff_t distance)
{
    size_t magnitude;

    if (!mb__isValid(buffer))
        return false;

    if (distance < 0)
    {
        /* Avoid negating PTRDIFF_MIN. */
        magnitude = (size_t)(-(distance + 1));
        magnitude += 1u;
        if (magnitude > buffer->position)
            return false;
        buffer->position -= magnitude;
        return true;
    }

    magnitude = (size_t)distance;
    if (magnitude > buffer->length - buffer->position)
        return false;

    buffer->position += magnitude;
    return true;
}

bool mb_scanTo(memory_buffer_t* buffer, const void* data, size_t length)
{
    size_t candidate;
    size_t last;

    if (!mb__isValid(buffer))
        return false;
    if (length == 0u)
        return true;
    if (data == NULL)
        return false;
    if (length > buffer->length - buffer->position)
        return false;

    last = buffer->length - length;
    for (candidate = buffer->position; candidate <= last; ++candidate)
    {
        if (memcmp(buffer->data + candidate, data, length) == 0)
        {
            buffer->position = candidate;
            return true;
        }
    }

    return false;
}

bool mb_read(memory_buffer_t* buffer, uint8_t* value)
{
    return mb_readBytes(buffer, value, 1u);
}

bool mb_readBytes(memory_buffer_t* buffer, void* values, size_t count)
{
    if (!mb__isValid(buffer))
        return false;
    if (count == 0u)
        return true;
    if (values == NULL)
        return false;
    if (count > buffer->length - buffer->position)
        return false;

    memcpy(values, buffer->data + buffer->position, count);
    buffer->position += count;
    return true;
}

bool mb_peekBytes(const memory_buffer_t* buffer, void* values, size_t count)
{
    if (!mb__isValid(buffer))
        return false;
    return mb_readAt(buffer, buffer->position, values, count);
}

bool mb_readAt(
    const memory_buffer_t* buffer,
    size_t position,
    void* values,
    size_t count)
{
    if (!mb__canReadAt(buffer, position, count))
        return false;
    if (count == 0u)
        return true;
    if (values == NULL)
        return false;

    memcpy(values, buffer->data + position, count);
    return true;
}

bool mb_readString(memory_buffer_t* buffer, char* str, size_t count)
{
    return mb_readBytes(buffer, str, count);
}

bool mb_write(memory_buffer_t* buffer, uint8_t value)
{
    return mb_writeBytes(buffer, &value, 1u);
}

bool mb_writeBytes(memory_buffer_t* buffer, const void* values, size_t count)
{
    if (!mb__isValid(buffer))
        return false;
    return mb__writeAtInternal(buffer, buffer->position, values, count, true);
}

bool mb_writeAt(
    memory_buffer_t* buffer,
    size_t position,
    const void* values,
    size_t count)
{
    return mb__writeAtInternal(buffer, position, values, count, false);
}

bool mb_writeString(memory_buffer_t* buffer, const char* str, size_t count)
{
    return mb_writeBytes(buffer, str, count);
}

bool mb_writeZeros(memory_buffer_t* buffer, size_t count)
{
    size_t end_position;

    if (!mb__isValid(buffer))
        return false;
    if (count == 0u)
        return true;
    if (!mb__addSize(buffer->position, count, &end_position))
        return false;
    if (!mb_reserve(buffer, end_position))
        return false;

    memset(buffer->data + buffer->position, 0, count);
    buffer->position = end_position;
    if (end_position > buffer->length)
        buffer->length = end_position;
    return true;
}

bool mb_readUInt16LE(memory_buffer_t* buffer, uint16_t* value)
{
    uint8_t bytes[2];
    uint16_t decoded;

    if (value == NULL || !mb_readBytes(buffer, bytes, sizeof(bytes)))
        return false;

    decoded = (uint16_t)((uint16_t)bytes[0] |
                         ((uint16_t)bytes[1] << 8u));
    *value = decoded;
    return true;
}

bool mb_readUInt16BE(memory_buffer_t* buffer, uint16_t* value)
{
    uint8_t bytes[2];
    uint16_t decoded;

    if (value == NULL || !mb_readBytes(buffer, bytes, sizeof(bytes)))
        return false;

    decoded = (uint16_t)(((uint16_t)bytes[0] << 8u) |
                         (uint16_t)bytes[1]);
    *value = decoded;
    return true;
}

bool mb_readInt16LE(memory_buffer_t* buffer, int16_t* value)
{
    uint16_t decoded;
    if (value == NULL || !mb_readUInt16LE(buffer, &decoded))
        return false;
    *value = mb__u16ToI16(decoded);
    return true;
}

bool mb_readInt16BE(memory_buffer_t* buffer, int16_t* value)
{
    uint16_t decoded;
    if (value == NULL || !mb_readUInt16BE(buffer, &decoded))
        return false;
    *value = mb__u16ToI16(decoded);
    return true;
}

bool mb_readUInt24LE(memory_buffer_t* buffer, uint32_t* value)
{
    uint8_t bytes[3];
    uint32_t decoded;

    if (value == NULL || !mb_readBytes(buffer, bytes, sizeof(bytes)))
        return false;

    decoded = (uint32_t)bytes[0] |
              ((uint32_t)bytes[1] << 8u) |
              ((uint32_t)bytes[2] << 16u);
    *value = decoded;
    return true;
}

bool mb_readUInt24BE(memory_buffer_t* buffer, uint32_t* value)
{
    uint8_t bytes[3];
    uint32_t decoded;

    if (value == NULL || !mb_readBytes(buffer, bytes, sizeof(bytes)))
        return false;

    decoded = ((uint32_t)bytes[0] << 16u) |
              ((uint32_t)bytes[1] << 8u) |
              (uint32_t)bytes[2];
    *value = decoded;
    return true;
}

bool mb_readInt24LE(memory_buffer_t* buffer, int32_t* value)
{
    uint32_t decoded;
    if (value == NULL || !mb_readUInt24LE(buffer, &decoded))
        return false;
    *value = mb__u24ToI32(decoded);
    return true;
}

bool mb_readInt24BE(memory_buffer_t* buffer, int32_t* value)
{
    uint32_t decoded;
    if (value == NULL || !mb_readUInt24BE(buffer, &decoded))
        return false;
    *value = mb__u24ToI32(decoded);
    return true;
}

bool mb_readUInt32LE(memory_buffer_t* buffer, uint32_t* value)
{
    uint8_t bytes[4];
    uint32_t decoded;

    if (value == NULL || !mb_readBytes(buffer, bytes, sizeof(bytes)))
        return false;

    decoded = (uint32_t)bytes[0] |
              ((uint32_t)bytes[1] << 8u) |
              ((uint32_t)bytes[2] << 16u) |
              ((uint32_t)bytes[3] << 24u);
    *value = decoded;
    return true;
}

bool mb_readUInt32BE(memory_buffer_t* buffer, uint32_t* value)
{
    uint8_t bytes[4];
    uint32_t decoded;

    if (value == NULL || !mb_readBytes(buffer, bytes, sizeof(bytes)))
        return false;

    decoded = ((uint32_t)bytes[0] << 24u) |
              ((uint32_t)bytes[1] << 16u) |
              ((uint32_t)bytes[2] << 8u) |
              (uint32_t)bytes[3];
    *value = decoded;
    return true;
}

bool mb_readInt32LE(memory_buffer_t* buffer, int32_t* value)
{
    uint32_t decoded;
    if (value == NULL || !mb_readUInt32LE(buffer, &decoded))
        return false;
    *value = mb__u32ToI32(decoded);
    return true;
}

bool mb_readInt32BE(memory_buffer_t* buffer, int32_t* value)
{
    uint32_t decoded;
    if (value == NULL || !mb_readUInt32BE(buffer, &decoded))
        return false;
    *value = mb__u32ToI32(decoded);
    return true;
}

bool mb_readUInt64LE(memory_buffer_t* buffer, uint64_t* value)
{
    uint8_t bytes[8];
    uint64_t decoded;

    if (value == NULL || !mb_readBytes(buffer, bytes, sizeof(bytes)))
        return false;

    decoded = (uint64_t)bytes[0] |
              ((uint64_t)bytes[1] << 8u) |
              ((uint64_t)bytes[2] << 16u) |
              ((uint64_t)bytes[3] << 24u) |
              ((uint64_t)bytes[4] << 32u) |
              ((uint64_t)bytes[5] << 40u) |
              ((uint64_t)bytes[6] << 48u) |
              ((uint64_t)bytes[7] << 56u);
    *value = decoded;
    return true;
}

bool mb_readUInt64BE(memory_buffer_t* buffer, uint64_t* value)
{
    uint8_t bytes[8];
    uint64_t decoded;

    if (value == NULL || !mb_readBytes(buffer, bytes, sizeof(bytes)))
        return false;

    decoded = ((uint64_t)bytes[0] << 56u) |
              ((uint64_t)bytes[1] << 48u) |
              ((uint64_t)bytes[2] << 40u) |
              ((uint64_t)bytes[3] << 32u) |
              ((uint64_t)bytes[4] << 24u) |
              ((uint64_t)bytes[5] << 16u) |
              ((uint64_t)bytes[6] << 8u) |
              (uint64_t)bytes[7];
    *value = decoded;
    return true;
}

bool mb_readInt64LE(memory_buffer_t* buffer, int64_t* value)
{
    uint64_t decoded;
    if (value == NULL || !mb_readUInt64LE(buffer, &decoded))
        return false;
    *value = mb__u64ToI64(decoded);
    return true;
}

bool mb_readInt64BE(memory_buffer_t* buffer, int64_t* value)
{
    uint64_t decoded;
    if (value == NULL || !mb_readUInt64BE(buffer, &decoded))
        return false;
    *value = mb__u64ToI64(decoded);
    return true;
}

bool mb_writeUInt16LE(memory_buffer_t* buffer, uint16_t value)
{
    const uint8_t bytes[2] = {
        (uint8_t)value,
        (uint8_t)(value >> 8u)
    };
    return mb_writeBytes(buffer, bytes, sizeof(bytes));
}

bool mb_writeUInt16BE(memory_buffer_t* buffer, uint16_t value)
{
    const uint8_t bytes[2] = {
        (uint8_t)(value >> 8u),
        (uint8_t)value
    };
    return mb_writeBytes(buffer, bytes, sizeof(bytes));
}

bool mb_writeInt16LE(memory_buffer_t* buffer, int16_t value)
{
    return mb_writeUInt16LE(buffer, (uint16_t)value);
}

bool mb_writeInt16BE(memory_buffer_t* buffer, int16_t value)
{
    return mb_writeUInt16BE(buffer, (uint16_t)value);
}

bool mb_writeUInt24LE(memory_buffer_t* buffer, uint32_t value)
{
    uint8_t bytes[3];

    if (value > MB_UINT24_MAX)
        return false;

    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8u);
    bytes[2] = (uint8_t)(value >> 16u);
    return mb_writeBytes(buffer, bytes, sizeof(bytes));
}

bool mb_writeUInt24BE(memory_buffer_t* buffer, uint32_t value)
{
    uint8_t bytes[3];

    if (value > MB_UINT24_MAX)
        return false;

    bytes[0] = (uint8_t)(value >> 16u);
    bytes[1] = (uint8_t)(value >> 8u);
    bytes[2] = (uint8_t)value;
    return mb_writeBytes(buffer, bytes, sizeof(bytes));
}

bool mb_writeInt24LE(memory_buffer_t* buffer, int32_t value)
{
    if (value < MB_INT24_MIN || value > MB_INT24_MAX)
        return false;
    return mb_writeUInt24LE(buffer, (uint32_t)value & MB_UINT24_MAX);
}

bool mb_writeInt24BE(memory_buffer_t* buffer, int32_t value)
{
    if (value < MB_INT24_MIN || value > MB_INT24_MAX)
        return false;
    return mb_writeUInt24BE(buffer, (uint32_t)value & MB_UINT24_MAX);
}

bool mb_writeUInt32LE(memory_buffer_t* buffer, uint32_t value)
{
    const uint8_t bytes[4] = {
        (uint8_t)value,
        (uint8_t)(value >> 8u),
        (uint8_t)(value >> 16u),
        (uint8_t)(value >> 24u)
    };
    return mb_writeBytes(buffer, bytes, sizeof(bytes));
}

bool mb_writeUInt32BE(memory_buffer_t* buffer, uint32_t value)
{
    const uint8_t bytes[4] = {
        (uint8_t)(value >> 24u),
        (uint8_t)(value >> 16u),
        (uint8_t)(value >> 8u),
        (uint8_t)value
    };
    return mb_writeBytes(buffer, bytes, sizeof(bytes));
}

bool mb_writeInt32LE(memory_buffer_t* buffer, int32_t value)
{
    return mb_writeUInt32LE(buffer, (uint32_t)value);
}

bool mb_writeInt32BE(memory_buffer_t* buffer, int32_t value)
{
    return mb_writeUInt32BE(buffer, (uint32_t)value);
}

bool mb_writeUInt64LE(memory_buffer_t* buffer, uint64_t value)
{
    const uint8_t bytes[8] = {
        (uint8_t)value,
        (uint8_t)(value >> 8u),
        (uint8_t)(value >> 16u),
        (uint8_t)(value >> 24u),
        (uint8_t)(value >> 32u),
        (uint8_t)(value >> 40u),
        (uint8_t)(value >> 48u),
        (uint8_t)(value >> 56u)
    };
    return mb_writeBytes(buffer, bytes, sizeof(bytes));
}

bool mb_writeUInt64BE(memory_buffer_t* buffer, uint64_t value)
{
    const uint8_t bytes[8] = {
        (uint8_t)(value >> 56u),
        (uint8_t)(value >> 48u),
        (uint8_t)(value >> 40u),
        (uint8_t)(value >> 32u),
        (uint8_t)(value >> 24u),
        (uint8_t)(value >> 16u),
        (uint8_t)(value >> 8u),
        (uint8_t)value
    };
    return mb_writeBytes(buffer, bytes, sizeof(bytes));
}

bool mb_writeInt64LE(memory_buffer_t* buffer, int64_t value)
{
    return mb_writeUInt64LE(buffer, (uint64_t)value);
}

bool mb_writeInt64BE(memory_buffer_t* buffer, int64_t value)
{
    return mb_writeUInt64BE(buffer, (uint64_t)value);
}

bool mb_isEOF(const memory_buffer_t* buffer)
{
    return mb__isValid(buffer) && buffer->position == buffer->length;
}

#endif /* SHL_MEMORY_BUFFER_IMPLEMENTATION */
#endif /* SHL_MEMORY_BUFFER_H */
