/*
 * This implementation of the macro is a variant of: https://github.com/mystborn/GenericMap
 * to make a closed implementation of the map data structure, where each collision is resolved
 * by keeping the index of the next element in the array of cells, and not by merely iterate
 * until we find an empty cell.
 * 
 * A detailed explanation of the hash function can be found here: 
 * https://probablydance.com/2018/06/16/fibonacci-hashing-the-optimization-that-the-world-forgot-or-a-better-alternative-to-integer-modulo/
 *
 * The specific constant was found here:
 * http://book.huihoo.com/data-structures-and-algorithms-with-object-oriented-design-patterns-in-c++/html/page214.html
*/

#ifndef SHL_MAP_H
#define SHL_MAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define shlDeclareMap(typeName, keyType, valueType) \
    typedef struct { \
        bool active; \
        int32_t next; \
        keyType key; \
        valueType value; \
        uint32_t hash; \
    } typeName ## _Cell_; \
    \
    typedef struct { \
        typeName ## _Cell_* cells; \
        uint32_t count; \
        uint32_t capacity; \
        uint32_t loadFactor; \
        uint32_t shift; \
    } typeName; \
    \
    extern void typeName ## Init(typeName* map); \
    extern void typeName ## Free(typeName* map); \
    extern void typeName ## Set(typeName* map, keyType key, valueType value); \
    extern valueType typeName ## Get(typeName* map, keyType key); \
    extern bool typeName ## Remove(typeName* map, keyType key); \
    extern void typeName ## Clear(typeName* map);

#define shlDefineMap(typeName, keyType, valueType, hashFn, equalsFn, defaultValue) \
    static uint32_t typeName ## __fibHash(uint32_t hash, uint32_t shift) \
    { \
        const uint32_t hashConstant = 2654435769u; \
        return (hash * hashConstant) >> shift; \
    } \
    \
    static uint32_t typeName ## __findEmptyCell(typeName* map, uint32_t index) \
    { \
        for(int i = 0; i < map->capacity; i++) \
        { \
            if (!map->cells[(index + i) % map->capacity].active) \
                return (index + i) % map->capacity; \
        } \
        \
        return -1; \
    } \
    \
    static bool typeName ## __insert(typeName* map, keyType key, valueType value) \
    { \
        uint32_t hash, cell, next; \
        hash = cell = typeName ## __fibHash(hashFn(key), map->shift); \
        \
        while (map->cells[cell].active && map->cells[cell].next >= 0) \
        { \
            if(map->cells[cell].hash == hash && equalsFn(map->cells[cell].key, key)) \
            { \
                map->cells[cell].value = value; \
                return true; \
            } \
            \
            cell = map->cells[cell].next; \
        } \
        \
        if (map->cells[cell].active) \
        { \
            if(map->cells[cell].hash == hash && equalsFn(map->cells[cell].key, key)) \
            { \
                map->cells[cell].value = value; \
                return true; \
            } \
        } \
        \
        next = typeName ## __findEmptyCell(map, cell); \
        if (cell != next) \
        { \
            map->cells[cell].next = next; \
        } \
        \
        map->cells[next].active = true; \
        map->cells[next].key = key; \
        map->cells[next].value = value; \
        map->cells[next].hash = hash; \
        map->cells[next].next = -1; \
        map->count++; \
        return true; \
    } \
    \
    static void typeName ## __resize(typeName* map) \
    { \
        uint32_t oldCapacity = map->capacity; \
        typeName ## _Cell_* old = map->cells; \
        \
        map->loadFactor = oldCapacity; \
        map->capacity = 1 << (32 - (--map->shift)); \
        map->cells = calloc(map->capacity, sizeof(typeName ## _Cell_)); \
        map->count = 0; \
        \
        for(int i = 0; i < oldCapacity; i++) \
        { \
            if(old[i].active) \
                typeName ## __insert(map, old[i].key, old[i].value); \
        } \
        free(old); \
    } \
    \
    void typeName ## Init(typeName* map) \
    { \
        map->shift = 29; \
        map->capacity = 8; \
        map->count = 0; \
        map->loadFactor = 4; \
        map->cells = (typeName ## _Cell_ *)calloc(map->capacity, sizeof(typeName ## _Cell_)); \
    } \
    \
    void typeName ## Free(typeName* map) \
    { \
        free(map->cells); \
        map->cells = 0; \
        map->count = 0; \
    } \
    \
    void typeName ## Set(typeName* map, keyType key, valueType value) \
    { \
        if (!map->cells) \
            return; \
        \
        if(map->count == map->loadFactor) \
            typeName ## __resize(map); \
        \
        typeName ## __insert(map, key, value); \
    } \
    \
    valueType typeName ## Get(typeName* map, keyType key) \
    { \
        if (!map->cells) \
            return defaultValue; \
        \
        uint32_t cell, hash; \
        hash = cell = typeName ## __fibHash(hashFn(key), map->shift); \
        \
        valueType value = defaultValue; \
        \
        while (map->cells[cell].active) \
        { \
            if(map->cells[cell].hash == hash && equalsFn(map->cells[cell].key, key)) \
            { \
                value = map->cells[cell].value; \
                break; \
            } \
            \
            if (map->cells[cell].next < 0) \
            { \
                break; \
            } \
            \
            cell = map->cells[cell].next; \
        } \
        \
        return value; \
    } \
    \
    bool typeName ## Contains(typeName* map, keyType key) \
    { \
        if (!map->cells) \
            return false; \
        \
        uint32_t cell, hash; \
        hash = cell = typeName ## __fibHash(hashFn(key), map->shift); \
        \
        bool found = false; \
        \
        while (map->cells[cell].active) \
        { \
            if(map->cells[cell].hash == hash && equalsFn(map->cells[cell].key, key)) \
            { \
                found = true; \
                break; \
            } \
            \
            if (map->cells[cell].next < 0) \
            { \
                break; \
            } \
            \
            cell = map->cells[cell].next; \
        } \
        \
        return found; \
    } \
    \
    bool typeName ## Remove(typeName* map, keyType key) \
    { \
        if (!map->cells) \
            return false; \
        \
        uint32_t prev, cell, hash; \
        \
        hash = prev = cell = typeName ## __fibHash(hashFn(key), map->shift); \
        \
        while (map->cells[cell].active) \
        { \
            if(map->cells[cell].hash == hash && equalsFn(map->cells[cell].key, key)) \
            { \
                map->cells[prev].next = map->cells[cell].next; \
                map->cells[cell].active = false; \
                break; \
            } \
            \
            if (map->cells[cell].next < 0) \
            { \
                return false; \
            } \
            \
            prev = cell; \
            cell = map->cells[cell].next; \
        } \
        map->count--; \
        return true; \
    } \
    \
    void typeName ## Clear(typeName* map) \
    { \
        if (!map->cells) \
            return; \
        \
        for(int i = 0; i < map->capacity; i++) \
        { \
            map->cells[i].active = false; \
        } \
        \
        map->count = 0; \
    }

#endif //SHL_MAP_H