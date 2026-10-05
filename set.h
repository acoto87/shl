/*
    set.h - acoto87 (acoto87@gmail.com)

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

    Single-header macro library to declare and define strongly typed hash sets.
    Callers provide hash and equality hooks for the generated item type.

    USAGE
    Declare a set type with shlDeclareSet(name, type), then define it once
    with shlDefineSet(name, type) in a C source file.

    ALLOCATION
    Pass an shl_allocator_t* to Init to control where the entries buffer lives.
    Pass NULL for the default system heap. Include alloc.h for shl_heap_alloc()
    and include both alloc.h and memzone.h for shl_zone_alloc(zone), in either
    order. Define SHL_ALLOC_IMPLEMENTATION in one C file to emit those helpers.

    Define SHL_SET_INITIAL_CAPACITY before including this header to override
    the default of 8. The capacity must be a power of two, at least 2.

    ITEM OWNERSHIP
    The set stores items by copy and does not manage the lifecycle of items.
    The caller is responsible for freeing any resources owned by items before
    calling Remove, Clear, or Free.

    NOTES
    This set uses hash buckets with collision chains stored inside the entry
    array.  Add returns false when the item is already present.  Call Free
    to release internal storage.
*/

#ifndef SHL_SET_H
#define SHL_SET_H

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

#ifndef SHL_SET_INITIAL_CAPACITY
#define SHL_SET_INITIAL_CAPACITY 8
#endif

#if SHL_SET_INITIAL_CAPACITY < 2 || SHL_SET_INITIAL_CAPACITY > INT32_MAX || \
    (SHL_SET_INITIAL_CAPACITY & (SHL_SET_INITIAL_CAPACITY - 1)) != 0
#error "SHL_SET_INITIAL_CAPACITY must be a power of two from 2 through 1073741824"
#endif

#define SHL__SET_INITIAL_HASH_LOAD_FACTOR (SHL_SET_INITIAL_CAPACITY / 2 + SHL_SET_INITIAL_CAPACITY / 4)

static inline void* shl__setMalloc(void* ctx, size_t sz) { (void)ctx; return malloc(sz); }
static inline void* shl__setRealloc(void* ctx, void* ptr, size_t sz) { (void)ctx; return realloc(ptr, sz); }
static inline void shl__setFree(void* ctx, void* ptr) { (void)ctx; free(ptr); }

static inline int32_t shl__setInitialShift(void)
{
    int32_t shift = 32;
    for (int32_t capacity = SHL_SET_INITIAL_CAPACITY; capacity > 1; capacity >>= 1)
        shift--;
    return shift;
}

static inline int32_t shl__setFibHash(uint32_t hash, int32_t shift)
{
    const uint32_t hashConstant = 2654435769u;
    return (int32_t)((hash * hashConstant) >> shift);
}

static inline int32_t shl__setFindEmptyBucket(const void* entries, int32_t capacity, int32_t startIndex, size_t entrySize, size_t activeOffset)
{
    const unsigned char* bytes = (const unsigned char*)entries;

    for (int32_t i = 0; i < capacity; i++)
    {
        int32_t currentIndex = (startIndex + i) % capacity;
        bool active = false;

        memcpy(&active, bytes + (size_t)currentIndex * entrySize + activeOffset, sizeof(active));
        if (!active)
            return currentIndex;
    }

    return -1;
}

#define shlDeclareSet(typeName, itemType) \
    typedef struct { \
        bool active; \
        uint32_t hash; \
        int32_t next; \
        itemType item; \
    } typeName ## __Entry__; \
    \
    typedef struct { \
        int32_t count; \
        int32_t capacity; \
        int32_t loadFactor; \
        int32_t shift; \
        shl_allocator_t* alloc; \
        uint32_t (*hashFn)(const itemType item); \
        bool (*equalsFn)(const itemType item1, const itemType item2); \
        typeName ## __Entry__* entries; \
    } typeName; \
    \
    void typeName ## Init(typeName* set, shl_allocator_t* alloc, uint32_t (*hashFn)(const itemType item), bool (*equalsFn)(const itemType item1, const itemType item2)); \
    void typeName ## Free(typeName* set); \
    bool typeName ## Add(typeName* set, itemType item); \
    bool typeName ## Contains(typeName* set, itemType item); \
    void typeName ## Remove(typeName* set, itemType item); \
    void typeName ## Clear(typeName* set);

#define shlDefineSet(typeName, itemType) \
    static bool typeName ## __resize(typeName* set) \
    { \
        if (!set->alloc || !set->alloc->mallocFn) return false; \
        int32_t oldCapacity = set->capacity; \
        typeName ## __Entry__* old = set->entries; \
        \
        set->loadFactor = oldCapacity; \
        set->capacity = 1 << (32 - (--set->shift)); \
        set->entries = (typeName ## __Entry__*)set->alloc->mallocFn(set->alloc->ctx, (size_t)set->capacity * sizeof(typeName ## __Entry__)); \
        if (!set->entries) \
        { \
            set->entries = old; \
            set->capacity = oldCapacity; \
            set->shift++; \
            set->loadFactor = set->capacity; \
            return false; \
        } \
        memset(set->entries, 0, (size_t)set->capacity * sizeof(typeName ## __Entry__)); \
        set->count = 0; \
        \
        for (int32_t i = 0; i < oldCapacity; i++) \
        { \
            if (old[i].active) \
                typeName ## Add(set, old[i].item); \
        } \
        if (set->alloc && set->alloc->freeFn) \
            set->alloc->freeFn(set->alloc->ctx, old); \
        return true; \
    } \
    \
    void typeName ## Init(typeName* set, shl_allocator_t* alloc, uint32_t (*hashFn)(const itemType item), bool (*equalsFn)(const itemType item1, const itemType item2)) \
    { \
        *set = (typeName){ 0 }; \
        static shl_allocator_t heapAlloc = { NULL, shl__setMalloc, shl__setRealloc, shl__setFree }; \
        if (!alloc) alloc = &heapAlloc; \
        if (!alloc || !alloc->mallocFn || !hashFn || !equalsFn) return; \
        set->alloc      = alloc; \
        set->hashFn     = hashFn; \
        set->equalsFn   = equalsFn; \
        set->shift      = shl__setInitialShift(); \
        set->capacity   = SHL_SET_INITIAL_CAPACITY; \
        set->loadFactor = SHL__SET_INITIAL_HASH_LOAD_FACTOR; \
        set->count      = 0; \
        set->entries    = (typeName ## __Entry__*)alloc->mallocFn(alloc->ctx, (size_t)set->capacity * sizeof(typeName ## __Entry__)); \
        if (set->entries) memset(set->entries, 0, (size_t)set->capacity * sizeof(typeName ## __Entry__)); \
    } \
    \
    void typeName ## Free(typeName* set) \
    { \
        set->count = 0; \
        if (set->entries && set->alloc && set->alloc->freeFn) \
            set->alloc->freeFn(set->alloc->ctx, set->entries); \
        set->entries = NULL; \
    } \
    \
    bool typeName ## Add(typeName* set, itemType item) \
    { \
        if (!set->entries) \
            return false; \
        \
        if (set->count == set->loadFactor) {\
            if (!typeName ## __resize(set)) \
                return false; \
        } \
        uint32_t hash; \
        int32_t index; \
        int32_t next; \
        hash = index = shl__setFibHash(set->hashFn(item), set->shift); \
        \
        while (set->entries[index].active) \
        { \
            if (set->entries[index].hash == hash && set->equalsFn(set->entries[index].item, item)) \
                return false; \
            \
            if (set->entries[index].next < 0) \
                break; \
            \
            index = set->entries[index].next; \
        } \
        \
        next = shl__setFindEmptyBucket(set->entries, set->capacity, index, sizeof(typeName ## __Entry__), offsetof(typeName ## __Entry__, active)); \
        if (next < 0) \
        { \
            if (!typeName ## __resize(set)) \
                return false; \
            return typeName ## Add(set, item); \
        } \
        if (index != next) \
            set->entries[index].next = next; \
        \
        set->entries[next].active = true; \
        set->entries[next].item   = item; \
        set->entries[next].hash   = hash; \
        set->entries[next].next   = -1; \
        set->count++; \
        return true; \
    } \
    \
    bool typeName ## Contains(typeName* set, itemType item) \
    { \
        if (!set->entries) \
            return false; \
        \
        int32_t index; \
        uint32_t hash; \
        hash = index = shl__setFibHash(set->hashFn(item), set->shift); \
        \
        bool found = false; \
        \
        while (set->entries[index].active) \
        { \
            if (set->entries[index].hash == hash && set->equalsFn(set->entries[index].item, item)) \
            { \
                found = true; \
                break; \
            } \
            \
            if (set->entries[index].next < 0) \
                break; \
            \
            index = set->entries[index].next; \
        } \
        \
        return found; \
    } \
    \
    void typeName ## Remove(typeName* set, itemType item) \
    { \
        if (!set->entries) \
            return; \
        \
        int32_t prevIndex, index; \
        uint32_t hash; \
        hash = prevIndex = index = shl__setFibHash(set->hashFn(item), set->shift); \
        \
        while (set->entries[index].active) \
        { \
            if (set->entries[index].hash == hash && set->equalsFn(set->entries[index].item, item)) \
            { \
                int32_t nextIndex = set->entries[index].next; \
                if (nextIndex >= 0) \
                { \
                    set->entries[index] = set->entries[nextIndex]; \
                    memset(&set->entries[nextIndex].item, 0, sizeof(itemType)); \
                    set->entries[nextIndex].next   = -1; \
                    set->entries[nextIndex].active = false; \
                } \
                else \
                { \
                    if (prevIndex != index) \
                        set->entries[prevIndex].next = -1; \
                    memset(&set->entries[index].item, 0, sizeof(itemType)); \
                    set->entries[index].next   = -1; \
                    set->entries[index].active = false; \
                } \
                \
                set->count--; \
                break; \
            } \
            \
            if (set->entries[index].next < 0) \
                break; \
            \
            prevIndex = index; \
            index = set->entries[index].next; \
        } \
    } \
    \
    void typeName ## Clear(typeName* set) \
    { \
        if (!set->entries) \
            return; \
        \
        for (int32_t i = 0; i < set->capacity; i++) \
        { \
            if (set->entries[i].active) \
            { \
                memset(&set->entries[i].item, 0, sizeof(itemType)); \
                set->entries[i].next   = -1; \
                set->entries[i].active = false; \
            } \
        } \
        \
        set->count = 0; \
    }

#endif // SHL_SET_H
