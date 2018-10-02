#ifndef SHL_LIST_H
#define SHL_LIST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define shlDeclareList(typeName, itemType) \
    typedef struct \
    { \
        itemType defaultValue; \
        bool (*equalsFn)(const itemType item1, const itemType item2); \
        void (*freeFn)(itemType item); \
    } typeName ## Options; \
    \
    typedef struct \
    { \
        uint32_t count; \
        uint32_t capacity; \
        bool (*equalsFn)(const itemType item1, const itemType item2); \
        void (*freeFn)(itemType item); \
        itemType defaultValue; \
        itemType* items; \
    } typeName; \
    \
    void typeName ## Init(typeName* list, typeName ## Options options); \
    void typeName ## Free(typeName* list); \
    void typeName ## Add(typeName* list, itemType value); \
    void typeName ## AddRange(typeName* list, int32_t count, itemType value[]); \
    void typeName ## Insert(typeName* list, int32_t index, itemType value); \
    void typeName ## InsertRange(typeName* list, int32_t index, int32_t count, itemType values[]); \
    int32_t typeName ## IndexOf(typeName* list, itemType value); \
    itemType typeName ## Get(typeName* list, int32_t index); \
    void typeName ## Set(typeName* list, int32_t index, itemType value); \
    bool typeName ## Contains(typeName* list, itemType value); \
    void typeName ## Remove(typeName* list, itemType value); \
    void typeName ## RemoveAt(typeName* list, int32_t index); \
    void typeName ## RemoveAtRange(typeName* list, int32_t index, int32_t count); \
    void typeName ## Reverse(typeName* list); \
    void typeName ## Sort(typeName* list, int32_t (*compareFn)(const itemType item1, const itemType item2));

#define shlDefineList(typeName, itemType) \
    void typeName ## __resize(typeName* list, int32_t minSize) \
    { \
        uint32_t oldCapacity = list->capacity; \
        itemType *old = list->items; \
        \
        list->capacity = oldCapacity << 1; \
        if (list->capacity < minSize) \
            list->capacity = minSize; \
        \
        list->items = (itemType *)realloc(list->items, list->capacity * sizeof(itemType)); \
    } \
    \
    void typeName ## __qsort(typeName* list, int32_t left, int32_t right, int32_t (*compareFn)(const itemType item1, const itemType item2)) \
    { \
        if (left >= right) \
            return; \
        \
        int32_t middle = left + ((right - left) >> 1); \
        itemType p = list->items[middle]; \
        \
        int32_t i = left - 1; \
        int32_t j = right + 1; \
        \
        while (i < j) \
        { \
            do { i++; } while (compareFn(list->items[i], p) < 0); \
            do { j--; } while (compareFn(list->items[j], p) > 0); \
            \
            if (i >= j) \
                break; \
            \
            itemType tmp = list->items[i]; \
            list->items[i] = list->items[j]; \
            list->items[j] = tmp; \
        } \
        \
        typeName ## __qsort(list, left, j, compareFn); \
        typeName ## __qsort(list, j + 1, right, compareFn); \
    } \
    \
    typeName ## Options typeName ## DefaultOptions() \
    { \
        return (typeName ## Options){0}; \
    } \
    \
    void typeName ## Init(typeName* list, typeName ## Options options) \
    { \
        list->capacity = 8; \
        list->defaultValue = options.defaultValue; \
        list->equalsFn = options.equalsFn; \
        list->freeFn = options.freeFn; \
        list->count = 0; \
        list->items = (itemType *)malloc(list->capacity * sizeof(itemType)); \
    } \
    \
    void typeName ## Free(typeName* list) \
    { \
        if (!list->items) \
            return; \
        \
        if (list->freeFn) \
        { \
            for(int32_t i = 0; i < list->count; i++) \
                list->freeFn(list->items[i]); \
        } \
        \
        free(list->items); \
        list->items = 0; \
        list->count = 0; \
    } \
    \
    void typeName ## InsertRange(typeName* list, int32_t index, int32_t count, itemType values[]) \
    { \
        if (!list->items) \
            return; \
        \
        if (index < 0 || index > list->count) \
            return; \
        \
        if (list->count + count >= list->capacity) \
            typeName ## __resize(list, list->count + count); \
        \
        memmove(list->items + index + count, list->items + index, (list->count - index) * sizeof(itemType)); \
        memcpy(list->items + index, values, count * sizeof(itemType)); \
        list->count += count; \
    } \
    \
    void typeName ## Insert(typeName* list, int32_t index, itemType value) \
    { \
        typeName ## InsertRange(list, index, 1, &value); \
    } \
    \
    void typeName ## Add(typeName* list, itemType value) \
    { \
        typeName ## Insert(list, list->count, value); \
    } \
    \
    void typeName ## AddRange(typeName* list, int32_t count, itemType values[]) \
    { \
        typeName ## InsertRange(list, list->count, count, values); \
    } \
    \
    int32_t typeName ## IndexOf(typeName* list, itemType value) \
    { \
        if (!list->items) \
            return -1; \
        \
        if (!list->equalsFn) \
            return -1; \
        \
        for(int32_t i = 0; i < list->count; i++) \
        { \
            if (list->equalsFn(list->items[i], value)) \
                return i; \
        } \
        \
        return -1; \
    } \
    \
    bool typeName ## Contains(typeName* list, itemType value) \
    { \
        return typeName ## IndexOf(list, value) >= 0; \
    } \
    \
    itemType typeName ## Get(typeName* list, int32_t index) \
    { \
        if (!list->items) \
            return list->defaultValue; \
        \
        if (index < 0 || index >= list->count) \
            return list->defaultValue; \
        \
        return list->items[index]; \
    } \
    \
    void typeName ## Set(typeName *list, int32_t index, itemType value) \
    { \
        if (!list->items) \
            return; \
        \
        if (index < 0 || index >= list->count) \
            return; \
        \
        itemType currentValue = list->items[index]; \
        if (list->freeFn) \
            list->freeFn(currentValue); \
        \
        list->items[index] = value; \
    } \
    \
    void typeName ## RemoveAtRange(typeName *list, int32_t index, int32_t count) \
    { \
        if (!list->items) \
            return; \
        \
        if (index < 0 || index >= list->count) \
            return; \
        \
        if (index + count > list->count) \
            return; \
        \
        if (list->freeFn) \
        { \
            for(int32_t i = 0; i < count; i++) \
                list->freeFn(list->items[index + i]); \
        } \
        \
        memmove(list->items + index, list->items + index + count, (list->count - index - count) * sizeof(itemType)); \
        list->count -= count; \
    } \
    \
    void typeName ## RemoveAt(typeName *list, int32_t index) \
    { \
        typeName ## RemoveAtRange(list, index, 1); \
    } \
    \
    void typeName ## Remove(typeName *list, itemType value) \
    { \
        int32_t index = typeName ## IndexOf(list, value); \
        typeName ## RemoveAt(list, index); \
    } \
    \
    void typeName ## Reverse(typeName *list) \
    { \
        if (!list->items) \
            return; \
        \
        int32_t count = list->count; \
        for(int32_t i = 0; i < count / 2; i++) \
        { \
            itemType tmp = list->items[i]; \
            list->items[i] = list->items[count - i - 1]; \
            list->items[count - i - 1] = tmp; \
        } \
    } \
    void typeName ## Sort(typeName* list, int32_t (*compareFn)(const itemType item1, const itemType item2)) \
    { \
        typeName ## __qsort(list, 0, list->count - 1, compareFn); \
    }

#endif // SHL_LIST_H