#ifndef SHL_LIST_H
#define SHL_LIST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define shlDeclareList(typeName, itemType) \
    typedef struct \
    { \
        uint32_t count; \
        uint32_t capacity; \
        uint32_t loadFactor; \
        itemType *items; \
    } typeName; \
    \
    void typeName ## Init(typeName *list); \
    void typeName ## Free(typeName *list); \
    void typeName ## Add(typeName *list, itemType value); \
    void typeName ## Insert(typeName *list, int32_t index, itemType value); \
    int32_t typeName ## IndexOf(typeName *list, itemType value); \
    itemType typeName ## Get(typeName *list, int32_t index); \
    itemType typeName ## Set(typeName *list, int32_t index, itemType value); \
    bool typeName ## Contains(typeName *list, itemType value); \
    itemType typeName ## RemoveAt(typeName *list, int32_t index); \
    itemType typeName ## Remove(typeName *list, itemType value);

#define shlDefineList(typeName, itemType, equalsFn, defaultValue) \
    void typeName ## __resize(typeName *list) \
    { \
        uint32_t oldCapacity = list->capacity; \
        uint32_t oldLoadFactor = list->loadFactor; \
        itemType *old = list->items; \
        \
        list->loadFactor = oldLoadFactor << 1; \
        list->capacity = oldCapacity << 1; \
        list->items = (itemType *)calloc(list->capacity, sizeof(itemType)); \
        \
        memcpy(list->items, old, list->count * sizeof(itemType)); \
    } \
    \
    void typeName ## Init(typeName *list) \
    { \
        list->capacity = 8; \
        list->loadFactor = 8; \
        list->count = 0; \
        list->items = (itemType *)calloc(list->capacity, sizeof(itemType)); \
    } \
    \
    void typeName ## Free(typeName *list) \
    { \
        free(list->items); \
        list->items = 0; \
        list->count = 0; \
    } \
    \
    void typeName ## Add(typeName *list, itemType value) \
    { \
        typeName ## Insert(list, list->count, value); \
    } \
    \
    void typeName ## Insert(typeName *list, int32_t index, itemType value) \
    { \
        if (!list->items) \
            return; \
        \
        if (index < 0 && index > list->count) \
            return; \
        \
        if (list->count == list->loadFactor) \
            typeName ## __resize(list); \
        \
        memmove(list->items + index + 1, list->items + index, (list->count - index) * sizeof(itemType)); \
        \
        list->items[index] = value; \
        list->count++; \
    } \
    \
    int32_t typeName ## IndexOf(typeName *list, itemType value) \
    { \
        if (!list->items) \
            return -1; \
        \
        for(int32_t i = 0; i < list->count; i++) \
        { \
            if (equalsFn(list->items[i], value)) \
                return i; \
        } \
        \
        return -1; \
    } \
    \
    bool typeName ## Contains(typeName *list, itemType value) \
    { \
        return typeName ## IndexOf(list, value) >= 0; \
    } \
    \
    itemType typeName ## Get(typeName *list, int32_t index) \
    { \
        if (!list->items) \
            return defaultValue; \
        \
        if (index < 0 && index >= list->count) \
            return defaultValue; \
        \
        return list->items[index]; \
    } \
    \
    itemType typeName ## Set(typeName *list, int32_t index, itemType value) \
    { \
        if (!list->items) \
            return defaultValue; \
        \
        if (index < 0 && index >= list->count) \
            return defaultValue; \
        \
        itemType currentValue = list->items[index]; \
        list->items[index] = value; \
        return currentValue; \
    } \
    \
    itemType typeName ## RemoveAt(typeName *list, int32_t index) \
    { \
        if (!list->items) \
            return defaultValue; \
        \
        if (index < 0 && index >= list->count) \
            return defaultValue; \
        \
        itemType value = list->items[index]; \
        memmove(list->items + index, list->items + index + 1, (list->count - index - 1) * sizeof(itemType)); \
        list->count--; \
        return value; \
    } \
    \
    itemType typeName ## Remove(typeName *list, itemType value) \
    { \
        int32_t index = typeName ## IndexOf(list, value); \
        if (index < 0) \
            return defaultValue; \
        \
        return typeName ## RemoveAt(list, index); \
    }

#endif // SHL_LIST_H