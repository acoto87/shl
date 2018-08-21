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
    extern void typeName ## Init(typeName *list); \
    extern void typeName ## Free(typeName *list); \
    extern void typeName ## Add(typeName *list, itemType value); \
    extern void typeName ## Insert(typeName *list, int32_t index, itemType value); \
    extern int32_t typeName ## IndexOf(typeName *list, itemType value); \
    extern itemType typeName ## Get(typeName *list, int32_t index); \
    extern bool typeName ## Contains(typeName *list, itemType value); \
    extern void typeName ## Clear(typeName *list); \
    extern bool typeName ## RemoveAt(typeName *list, int32_t index); \
    extern bool typeName ## Remove(typeName *list, itemType value);

#define shlDefineList(typeName, itemType, equalsFn, defaultValue) \
    void typeName ## __Resize(typeName *list) \
    { \
        uint32_t oldCapacity = list->capacity; \
        uint32_t oldLoadFactor = list->loadFactor; \
        itemType* old = list->items; \
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
            typeName ## __Resize(list); \
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
        for(int i = 0; i < list->count; i++) \
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
    bool typeName ## RemoveAt(typeName *list, int32_t index) \
    { \
        if (!list->items) \
            return false; \
        \
        if (index < 0 && index >= list->count) \
            return false; \
        \
        memmove(list->items + index, list->items + index + 1, (list->count - index - 1) * sizeof(itemType)); \
        \
        list->count--; \
        return true; \
    } \
    \
    bool typeName ## Remove(typeName *list, itemType value) \
    { \
        int32_t index = typeName ## IndexOf(list, value); \
        return index >= 0 && typeName ## RemoveAt(list, index); \
    } \
    \
    void typeName ## Clear(typeName *list) \
    { \
        if (!list->items) \
            return; \
        \
        list->count = 0; \
    }

#endif // SHL_LIST_H