#ifndef SHL_STACK_H
#define SHL_STACK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define shlDeclareQueue(typeName, itemType) \
    typedef struct \
    { \
        uint32_t head; \
        uint32_t tail; \
        uint32_t count; \
        uint32_t capacity; \
        uint32_t loadFactor; \
        itemType *items; \
    } typeName; \
    \
    void typeName ## Init(typeName *queue); \
    void typeName ## Free(typeName *queue); \
    void typeName ## Push(typeName *queue, itemType value); \
    itemType typeName ## Peek(typeName *queue); \
    itemType typeName ## Pop(typeName *queue); \
    bool typeName ## Contains(typeName *queue, itemType value);
    
#define shlDefineQueue(typeName, itemType, equalsFn, defaultValue) \
    void typeName ## __resize(typeName *queue) \
    { \
        uint32_t oldCapacity = queue->capacity; \
        uint32_t oldLoadFactor = queue->loadFactor; \
        itemType* old = queue->items; \
        \
        queue->loadFactor = oldLoadFactor << 1; \
        queue->capacity = oldCapacity << 1; \
        queue->items = (itemType *)calloc(queue->capacity, sizeof(itemType)); \
        \
        if (queue->head > queue->tail) \
        { \
            memcpy(queue->items, old + queue->head, (oldCapacity - queue->head) * sizeof(itemType)); \
            memcpy(queue->items + oldCapacity - queue->head, old, ((queue->head + queue->count) % oldCapacity) * sizeof(itemType)); \
        } \
        else \
        { \
            memcpy(queue->items, old + queue->head, queue->count * sizeof(itemType)); \
        } \
        \
        queue->head = 0; \
        queue->tail = queue->count; \
    } \
    \
    void typeName ## Init(typeName *queue) \
    { \
        queue->capacity = 8; \
        queue->loadFactor = 8; \
        queue->count = 0; \
        queue->head = 0; \
        queue->tail = 0; \
        queue->items = (itemType *)calloc(queue->capacity, sizeof(itemType)); \
    } \
    \
    void typeName ## Free(typeName *queue) \
    { \
        free(queue->items); \
        queue->items = 0; \
        queue->count = 0; \
    } \
    \
    void typeName ## Push(typeName *queue, itemType value) \
    { \
        if (!queue->items) \
            return; \
        \
        if (queue->count == queue->loadFactor) \
            typeName ## __resize(queue); \
        \
        queue->items[queue->tail] = value; \
        queue->tail = (queue->tail + 1) % queue->capacity; \
        queue->count++; \
    } \
    \
    itemType typeName ## Peek(typeName *queue) \
    { \
        if (!queue->items || queue->count == 0) \
            return defaultValue; \
        \
        return queue->items[queue->head]; \
    } \
    \
    itemType typeName ## Pop(typeName *queue) \
    { \
        if (!queue->items || queue->count == 0) \
            return defaultValue; \
        \
        itemType value = queue->items[queue->head]; \
        queue->items[queue->head] = defaultValue; \
        queue->head++; \
        queue->count--; \
        return value; \
    } \
    \
    bool typeName ## Contains(typeName *queue, itemType value) \
    { \
        if (!queue->items) \
            return false; \
        \
        for(int32_t i = 0; i < queue->count; i++) \
        { \
            if (equalsFn(queue->items[(queue->head + i) % queue->capacity], value)) \
                return true; \
        } \
        \
        return false; \
    }

#endif // SHL_STACK_H