/*
    map.h - acoto87 (acoto87@gmail.com)

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

    Single-header macro library to declare and define strongly typed hash maps.
    Callers provide hash and equality hooks for the generated key type.

    USAGE
    Declare a concrete map type with shlDeclareMap(name, keyType, valueType),
    then place shlDefineMap(name, keyType, valueType) in exactly one C file.

    ALLOCATION
    Pass an shl_allocator_t* to Init to control where the entries buffer lives.
    Pass NULL for the default system heap. Include alloc.h for shl_heap_alloc()
    and include both alloc.h and memzone.h for shl_zone_alloc(zone), in either
    order. Define SHL_ALLOC_IMPLEMENTATION in one C file to emit those helpers.

    Define SHL_MAP_INITIAL_CAPACITY before including this header to override
    the default of 8. The capacity must be a power of two, at least 2.

    KEY / VALUE OWNERSHIP
    The map stores keys and values by copy and does not manage the lifecycle
    of values.  The caller is responsible for freeing any resources owned by
    values before calling Remove, Clear, or Free.

    NOTES
    This map uses open addressing with linked collision chains stored inside
    the entry array.  Get returns a zero-initialised value when the key is
    not found.  Call Free to release internal storage.

    This implementation of the macro is a variant of: https://github.com/mystborn/GenericMap
    to make a closed implementation of the map data structure, where each collision is resolved
    by keeping the index of the next element in the array of cells, and not by merely iterate
    until we find an empty cell.

    A detailed explanation of the hash function can be found here:
    https://probablydance.com/2018/06/16/fibonacci-hashing-the-optimization-that-the-world-forgot-or-a-better-alternative-to-integer-modulo/

    The specific constant was found here:
    http://book.huihoo.com/data-structures-and-algorithms-with-object-oriented-design-patterns-in-c++/html/page214.html
*/

#ifndef SHL_MAP_H
#define SHL_MAP_H

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

#ifndef SHL_MAP_INITIAL_CAPACITY
#define SHL_MAP_INITIAL_CAPACITY 8
#endif

#if SHL_MAP_INITIAL_CAPACITY < 2 || SHL_MAP_INITIAL_CAPACITY > INT32_MAX || \
    (SHL_MAP_INITIAL_CAPACITY & (SHL_MAP_INITIAL_CAPACITY - 1)) != 0
#error "SHL_MAP_INITIAL_CAPACITY must be a power of two from 2 through 1073741824"
#endif

#define SHL__MAP_INITIAL_HASH_LOAD_FACTOR (SHL_MAP_INITIAL_CAPACITY / 2 + SHL_MAP_INITIAL_CAPACITY / 4)

static inline void* shl__mapMalloc(void* ctx, size_t sz) { (void)ctx; return malloc(sz); }
static inline void* shl__mapRealloc(void* ctx, void* ptr, size_t sz) { (void)ctx; return realloc(ptr, sz); }
static inline void shl__mapFree(void* ctx, void* ptr) { (void)ctx; free(ptr); }

static inline int32_t shl__mapInitialShift(void)
{
    int32_t shift = 32;
    for (int32_t capacity = SHL_MAP_INITIAL_CAPACITY; capacity > 1; capacity >>= 1)
        shift--;
    return shift;
}

static inline int32_t shl__mapFibHash(uint32_t hash, int32_t shift)
{
    const uint32_t hashConstant = 2654435769u;
    return (int32_t)((hash * hashConstant) >> shift);
}

static inline int32_t shl__mapFindEmptyBucket(const void* entries, int32_t capacity, int32_t startIndex, size_t entrySize, size_t activeOffset)
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

#define shlDeclareMap(typeName, keyType, valueType) \
    typedef struct { \
        bool active; \
        uint32_t hash; \
        int32_t next; \
        keyType key; \
        valueType value; \
    } typeName ## __Entry__; \
    \
    typedef struct { \
        int32_t count; \
        int32_t capacity; \
        int32_t loadFactor; \
        int32_t shift; \
        shl_allocator_t* alloc; \
        uint32_t (*hashFn)(keyType key); \
        bool (*equalsFn)(keyType item1, keyType item2); \
        typeName ## __Entry__* entries; \
    } typeName; \
    \
    void typeName ## Init(typeName* map, shl_allocator_t* alloc, uint32_t (*hashFn)(keyType key), bool (*equalsFn)(keyType key1, keyType key2)); \
    void typeName ## Free(typeName* map); \
    bool typeName ## Contains(typeName* map, keyType key); \
    valueType typeName ## Get(typeName* map, keyType key); \
    void typeName ## Set(typeName* map, keyType key, valueType value); \
    void typeName ## Remove(typeName* map, keyType key); \
    void typeName ## Clear(typeName* map);

#define shlDefineMap(typeName, keyType, valueType) \
    static bool typeName ## __resize(typeName* map); \
    \
    static bool typeName ## __insert(typeName* map, keyType key, valueType value) \
    { \
        uint32_t hash; \
        int32_t index; \
        int32_t next; \
        hash = index = shl__mapFibHash(map->hashFn(key), map->shift); \
        \
        while (map->entries[index].active && map->entries[index].next >= 0) \
        { \
            if(map->entries[index].hash == hash && map->equalsFn(map->entries[index].key, key)) \
            { \
                map->entries[index].value = value; \
                return true; \
            } \
            \
            index = map->entries[index].next; \
        } \
        \
        if (map->entries[index].active) \
        { \
            if(map->entries[index].hash == hash && map->equalsFn(map->entries[index].key, key)) \
            { \
                map->entries[index].value = value; \
                return true; \
            } \
        } \
        \
        next = shl__mapFindEmptyBucket(map->entries, map->capacity, index, sizeof(typeName ## __Entry__), offsetof(typeName ## __Entry__, active)); \
        if (next < 0) \
            return typeName ## __resize(map) && typeName ## __insert(map, key, value); \
        \
        if (index != next) \
            map->entries[index].next = next; \
        \
        map->entries[next].active = true; \
        map->entries[next].key = key; \
        map->entries[next].value = value; \
        map->entries[next].hash = hash; \
        map->entries[next].next = -1; \
        map->count++; \
        return true; \
    } \
    \
    static bool typeName ## __resize(typeName* map) \
    { \
        if (!map->alloc || !map->alloc->mallocFn) return false; \
        int32_t oldCapacity = map->capacity; \
        typeName ## __Entry__* old = map->entries; \
        \
        map->loadFactor = oldCapacity; \
        map->capacity = 1 << (32 - (--map->shift)); \
        map->entries = (typeName ## __Entry__*)map->alloc->mallocFn(map->alloc->ctx, (size_t)map->capacity * sizeof(typeName ## __Entry__)); \
        if (!map->entries) \
        { \
            map->entries = old; \
            map->capacity = oldCapacity; \
            map->shift++; \
            map->loadFactor = map->capacity; \
            return false; \
        } \
        memset(map->entries, 0, (size_t)map->capacity * sizeof(typeName ## __Entry__)); \
        map->count = 0; \
        \
        for(int32_t i = 0; i < oldCapacity; i++) \
        { \
            if(old[i].active && !typeName ## __insert(map, old[i].key, old[i].value)) \
            { \
                if (map->alloc && map->alloc->freeFn) \
                    map->alloc->freeFn(map->alloc->ctx, map->entries); \
                map->entries = old; \
                map->capacity = oldCapacity; \
                map->shift++; \
                map->loadFactor = map->capacity; \
                return false; \
            } \
        } \
        if (map->alloc && map->alloc->freeFn) \
            map->alloc->freeFn(map->alloc->ctx, old); \
        return true; \
    } \
    \
    void typeName ## Init(typeName* map, shl_allocator_t* alloc, uint32_t (*hashFn)(keyType key), bool (*equalsFn)(keyType key1, keyType key2)) \
    { \
        *map = (typeName){ 0 }; \
        static shl_allocator_t heapAlloc = { NULL, shl__mapMalloc, shl__mapRealloc, shl__mapFree }; \
        if (!alloc) alloc = &heapAlloc; \
        if (!alloc || !alloc->mallocFn || !hashFn || !equalsFn) return; \
        map->alloc     = alloc; \
        map->hashFn    = hashFn; \
        map->equalsFn  = equalsFn; \
        map->shift     = shl__mapInitialShift(); \
        map->capacity  = SHL_MAP_INITIAL_CAPACITY; \
        map->loadFactor = SHL__MAP_INITIAL_HASH_LOAD_FACTOR; \
        map->count     = 0; \
        map->entries   = (typeName ## __Entry__*)alloc->mallocFn(alloc->ctx, (size_t)map->capacity * sizeof(typeName ## __Entry__)); \
        if (map->entries) memset(map->entries, 0, (size_t)map->capacity * sizeof(typeName ## __Entry__)); \
    } \
    \
    void typeName ## Free(typeName* map) \
    { \
        map->count = 0; \
        if (map->entries && map->alloc && map->alloc->freeFn) \
            map->alloc->freeFn(map->alloc->ctx, map->entries); \
        map->entries = NULL; \
    } \
    \
    bool typeName ## Contains(typeName* map, keyType key) \
    { \
        if (!map->entries) \
            return false; \
        \
        int32_t index; \
        uint32_t hash; \
        hash = index = shl__mapFibHash(map->hashFn(key), map->shift); \
        \
        bool found = false; \
        \
        while (map->entries[index].active) \
        { \
            if(map->entries[index].hash == hash && map->equalsFn(map->entries[index].key, key)) \
            { \
                found = true; \
                break; \
            } \
            \
            if (map->entries[index].next < 0) \
            { \
                break; \
            } \
            \
            index = map->entries[index].next; \
        } \
        \
        return found; \
    } \
    \
    valueType typeName ## Get(typeName* map, keyType key) \
    { \
        if (!map->entries) \
        { \
            valueType zero; \
            memset(&zero, 0, sizeof(valueType)); \
            return zero; \
        } \
        \
        int32_t index; \
        uint32_t hash; \
        hash = index = shl__mapFibHash(map->hashFn(key), map->shift); \
        \
        valueType value; \
        memset(&value, 0, sizeof(valueType)); \
        \
        while (map->entries[index].active) \
        { \
            if(map->entries[index].hash == hash && map->equalsFn(map->entries[index].key, key)) \
            { \
                value = map->entries[index].value; \
                break; \
            } \
            \
            if (map->entries[index].next < 0) \
            { \
                break; \
            } \
            \
            index = map->entries[index].next; \
        } \
        \
        return value; \
    } \
    \
    void typeName ## Set(typeName* map, keyType key, valueType value) \
    { \
        if (!map->entries) \
            return; \
        \
        if(map->count == map->loadFactor) {\
            if (!typeName ## __resize(map)) \
                return; \
        } \
        \
        if (!typeName ## __insert(map, key, value)) return; \
    } \
    \
    void typeName ## Remove(typeName* map, keyType key) \
    { \
        if (!map->entries) \
            return; \
        \
        int32_t prevIndex, index; \
        uint32_t hash; \
        hash = prevIndex = index = shl__mapFibHash(map->hashFn(key), map->shift); \
        \
        while (map->entries[index].active) \
        { \
            if(map->entries[index].hash == hash && map->equalsFn(map->entries[index].key, key)) \
            { \
                int32_t nextIndex = map->entries[index].next; \
                if (nextIndex >= 0) \
                { \
                    map->entries[index] = map->entries[nextIndex]; \
                    memset(&map->entries[nextIndex].value, 0, sizeof(valueType)); \
                    map->entries[nextIndex].next = -1; \
                    map->entries[nextIndex].active = false; \
                } \
                else \
                { \
                    if (prevIndex != index) \
                        map->entries[prevIndex].next = -1; \
                    memset(&map->entries[index].value, 0, sizeof(valueType)); \
                    map->entries[index].next = -1; \
                    map->entries[index].active = false; \
                } \
                \
                map->count--; \
                \
                break; \
            } \
            \
            if (map->entries[index].next < 0) \
            { \
                break; \
            } \
            \
            prevIndex = index; \
            index = map->entries[index].next; \
        } \
    } \
    \
    void typeName ## Clear(typeName* map) \
    { \
        if (!map->entries) \
            return; \
        \
        for(int32_t i = 0; i < map->capacity; i++) \
        { \
            if (map->entries[i].active) \
            { \
                memset(&map->entries[i].value, 0, sizeof(valueType)); \
                map->entries[i].next = -1; \
                map->entries[i].active = false; \
            } \
        } \
        \
        map->count = 0; \
    }

#endif //SHL_MAP_H
