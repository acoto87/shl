/*
    queue.h - acoto87 (acoto87@gmail.com)

    MIT License

    Copyright (c) 2018 Alejandro Coto Gutiérrez

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.

    Single-header macro library to declare and define strongly typed queues
    implemented as dynamically resized circular buffers.

    USAGE
    Declare a queue type with shlDeclareQueue(name, type), then define it once
    with shlDefineQueue(name, type) in a C source file.

    ALLOCATION
    Pass an shl_allocator_t* to Init to control where the items buffer lives.
    Pass NULL for the default system heap. Include alloc.h for shl_heap_alloc()
    and include both alloc.h and memzone.h for shl_zone_alloc(zone), in either
    order. Define SHL_ALLOC_IMPLEMENTATION in one C file to emit those helpers.

    Define SHL_QUEUE_INITIAL_CAPACITY before including this header to override
    the default initial capacity of 8.

    For a fixed-capacity queue with no heap involvement at all, use InitFixed
    and supply a caller-owned buffer.  Free is a safe no-op on fixed queues.

    ITEM OWNERSHIP
    The queue stores values by copy and does not manage the lifecycle of
    items.  The caller is responsible for freeing any resources owned by
    items before calling Clear or Free.

    SEARCH
    Contains requires an explicit equality function at the call site rather
    than storing one in the queue struct.

    OUT-OF-RANGE READS
    Peek and Pop return a zero-initialised value when the queue is empty.
*/

#ifndef SHL_QUEUE_H
#define SHL_QUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef SHL_ALLOCATOR_T_DEFINED
#define SHL_ALLOCATOR_T_DEFINED

#include <stddef.h>

typedef struct shl_allocator_s
{
    void* ctx;
    void* (*mallocFn)(void* ctx, size_t sz);
    void* (*reallocFn)(void* ctx, void* ptr, size_t sz);
    void  (*freeFn)(void* ctx, void* ptr);
} shl_allocator_t;

#endif /* SHL_ALLOCATOR_T_DEFINED */

#ifndef SHL_QUEUE_INITIAL_CAPACITY
#define SHL_QUEUE_INITIAL_CAPACITY 8
#endif

#if SHL_QUEUE_INITIAL_CAPACITY < 1 || SHL_QUEUE_INITIAL_CAPACITY > INT32_MAX
#error "SHL_QUEUE_INITIAL_CAPACITY must be a positive int32_t value"
#endif

static inline void* shl__queueMalloc(void* ctx, size_t sz) { (void)ctx; return malloc(sz); }
static inline void* shl__queueRealloc(void* ctx, void* ptr, size_t sz) { (void)ctx; return realloc(ptr, sz); }
static inline void shl__queueFree(void* ctx, void* ptr) { (void)ctx; free(ptr); }

static inline int32_t shl__queueGrownCapacity(int32_t currentCapacity, int32_t minSize)
{
    int32_t newCapacity = currentCapacity > 0 ? (currentCapacity << 1) : SHL_QUEUE_INITIAL_CAPACITY;
    if (newCapacity < minSize) newCapacity = minSize;
    return newCapacity;
}

static inline bool shl__queueResize(void** items, int32_t* capacity, int32_t* head, int32_t* tail, int32_t count, size_t itemSize, shl_allocator_t* alloc)
{
    if (!alloc || !alloc->mallocFn) return false;
    int32_t oldCapacity = *capacity;
    uint8_t* oldItems = (uint8_t*)*items;
    int32_t newCapacity = shl__queueGrownCapacity(*capacity, *capacity + 1);
    uint8_t* newItems = (uint8_t*)alloc->mallocFn(alloc->ctx, (size_t)newCapacity * itemSize);
    if (!newItems) return false;
    memset(newItems, 0, (size_t)newCapacity * itemSize);

    if (count > 0)
    {
        if (*head >= *tail)
        {
            size_t firstCopySize = (size_t)(oldCapacity - *head) * itemSize;
            size_t secondCopySize = (size_t)((*head + count) % oldCapacity) * itemSize;

            memcpy(newItems, oldItems + (size_t)(*head) * itemSize, firstCopySize);
            memcpy(newItems + firstCopySize, oldItems, secondCopySize);
        }
        else
        {
            memcpy(newItems, oldItems + (size_t)(*head) * itemSize, (size_t)count * itemSize);
        }
    }

    *head = 0;
    *tail = count;
    if (alloc && alloc->freeFn)
        alloc->freeFn(alloc->ctx, *items);
    *capacity = newCapacity;
    *items = newItems;
    return true;
}

#define shlDeclareQueue(typeName, itemType) \
    typedef struct \
    { \
        int32_t head; \
        int32_t tail; \
        int32_t count; \
        int32_t capacity; \
        shl_allocator_t* alloc; \
        itemType* items; \
    } typeName; \
    \
    void typeName ## Init(typeName* queue, shl_allocator_t* alloc); \
    void typeName ## InitFixed(typeName* queue, itemType* buffer, int32_t capacity); \
    void typeName ## Free(typeName* queue); \
    void typeName ## Push(typeName* queue, itemType value); \
    itemType typeName ## Peek(typeName* queue); \
    itemType typeName ## Pop(typeName* queue); \
    bool typeName ## Contains(typeName* queue, itemType value, bool (*equalsFn)(const itemType, const itemType)); \
    void typeName ## Clear(typeName* queue);

#define shlDefineQueue(typeName, itemType) \
    void typeName ## Init(typeName* queue, shl_allocator_t* alloc) \
    { \
        *queue = (typeName){ 0 }; \
        static shl_allocator_t heapAlloc = { NULL, shl__queueMalloc, shl__queueRealloc, shl__queueFree }; \
        if (!alloc) alloc = &heapAlloc; \
        if (!alloc || !alloc->mallocFn) return; \
        queue->alloc    = alloc; \
        queue->capacity = SHL_QUEUE_INITIAL_CAPACITY; \
        queue->count    = 0; \
        queue->head     = 0; \
        queue->tail     = 0; \
        queue->items    = (itemType*)alloc->mallocFn(alloc->ctx, (size_t)queue->capacity * sizeof(itemType)); \
    } \
    \
    void typeName ## InitFixed(typeName* queue, itemType* buffer, int32_t capacity) \
    { \
        queue->alloc    = NULL; \
        queue->capacity = capacity; \
        queue->count    = 0; \
        queue->head     = 0; \
        queue->tail     = 0; \
        queue->items    = buffer; \
    } \
    \
    void typeName ## Free(typeName* queue) \
    { \
        queue->count = 0; \
        queue->head  = 0; \
        queue->tail  = 0; \
        if (queue->items && queue->alloc && queue->alloc->freeFn) \
            queue->alloc->freeFn(queue->alloc->ctx, queue->items); \
        queue->items = NULL; \
    } \
    \
    void typeName ## Push(typeName* queue, itemType value) \
    { \
        if (!queue->items) \
            return; \
        \
        if (queue->count == queue->capacity) \
        { \
            if (!shl__queueResize((void**)&queue->items, &queue->capacity, &queue->head, &queue->tail, queue->count, sizeof(itemType), queue->alloc)) \
                return; \
        } \
        \
        queue->items[queue->tail] = value; \
        queue->tail = (queue->tail + 1) % queue->capacity; \
        queue->count++; \
    } \
    \
    itemType typeName ## Peek(typeName* queue) \
    { \
        if (!queue->items || queue->count == 0) \
        { \
            itemType zero; \
            memset(&zero, 0, sizeof(itemType)); \
            return zero; \
        } \
        return queue->items[queue->head]; \
    } \
    \
    itemType typeName ## Pop(typeName* queue) \
    { \
        if (!queue->items || queue->count == 0) \
        { \
            itemType zero; \
            memset(&zero, 0, sizeof(itemType)); \
            return zero; \
        } \
        \
        itemType value = queue->items[queue->head]; \
        queue->head = (queue->head + 1) % queue->capacity; \
        queue->count--; \
        return value; \
    } \
    \
    bool typeName ## Contains(typeName* queue, itemType value, bool (*equalsFn)(const itemType, const itemType)) \
    { \
        if (!queue->items || !equalsFn) \
            return false; \
        \
        for (int32_t i = 0; i < queue->count; i++) \
        { \
            if (equalsFn(queue->items[(queue->head + i) % queue->capacity], value)) \
                return true; \
        } \
        \
        return false; \
    } \
    \
    void typeName ## Clear(typeName* queue) \
    { \
        queue->count = 0; \
        queue->head  = 0; \
        queue->tail  = 0; \
    }

#endif // SHL_QUEUE_H
