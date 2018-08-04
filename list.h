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
    extern itemType typeName ## Get(typeName *list, int32_t index); \
    extern bool typeName ## Contains(typeName *list, itemType value); \
    extern bool typeName ## RemoveByIndex(typeName *list, int32_t index); \
    extern bool typeName ## RemoveByValue(typeName *list, itemType value);

#define shlDefineList(typeName, itemType, equalsFn, defaultValue) \
    void typeName ## __Resize(typeName *list) \
    { \
        uint32_t oldCapacity = list->capacity; \
        itemType* old = list->items; \
        \
        list->loadFactor = oldCapacity; \
        list->capacity = oldCapacity << 1; \
        list->items = (itemType *)calloc(list->capacity, sizeof(itemType)); \
        \
        for(int i = 0; i < list->count; i++) \
            list->items[i] = old[i]; \
    } \
    \
    void typeName ## Init(typeName *list) \
    { \
        list->capacity = 8; \
        list->count = 0; \
        list->loadFactor = 4; \
        list->items = (itemType *)calloc(list->capacity, sizeof(itemType)); \
    } \
    \
    void typeName ## Free(typeName *list) \
    { \
        free(list->items); \
    } \
    \
    void typeName ## Insert(typeName *list, int32_t index, itemType value) \
    { \
        if (list->count == list->loadFactor) \
            typeName ## __Resize(list); \
        \
        for(int i = list->count - 1; i >= index; i--) \
            list->items[i + 1] = list->items[i]; \
        \
        list->items[index] = value; \
        list->count++; \
    } \
    \
    void typeName ## Add(typeName *list, itemType value) \
    { \
        typeName ## Insert(list, list->count, value); \
    } \
    \
    bool typeName ## Contains(typeName *list, itemType value) \
    { \
        for(int i = 0; i < list->count; i++) \
        { \
            if (equalsFn(list->items[i], value)) \
                return true; \
        } \
        \
        return false; \
    } \
    \
    itemType typeName ## Get(typeName *list, int32_t index) \
    { \
        if (index < 0 && index >= list->count) \
            return defaultValue; \
        \
        return list->items[index]; \
    } \
    \
    bool typeName ## RemoveByIndex(typeName *list, int32_t index) \
    { \
        if (index < 0 && index >= list->count) \
            return false; \
        \
        for(int i = index; i < list->count - 1; i++) \
            list->items[i] = list->items[i + 1]; \
        \
        list->count--; \
        return true; \
    } \
    \
    bool typeName ## RemoveByValue(typeName *list, itemType value) \
    { \
        for(int i = 0; i < list->count; i++) \
        { \
            if (equalsFn(list->items[i], value)) \
                return typeName ## RemoveByIndex(list, i); \
        } \
        \
        return false; \
    }

#endif // SHL_LIST_H