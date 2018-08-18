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
        uint32_t count; \
        uint32_t capacity; \
        uint32_t loadFactor; \
        itemType *items; \
    } typeName; \
    \
    extern void typeName ## Init(typeName *queue); \
    extern void typeName ## Free(typeName *queue); \
    extern void typeName ## Enqueue(typeName *queue, itemType value); \
    extern itemType typeName ## Peek(typeName *queue); \
    extern itemType typeName ## Dequeue(typeName *queue); \
    extern bool typeName ## Contains(typeName *queue, itemType value); \
    extern void typeName ## Clear(typeName *queue); \
    
#define shlDefineQueue(typeName, itemType, equalsFn, defaultValue) \
    void typeName ## __Resize(typeName *queue) \
    { \
        uint32_t oldCapacity = queue->capacity; \
        itemType* old = queue->items; \
        \
        queue->loadFactor = oldCapacity; \
        queue->capacity = oldCapacity << 1; \
        queue->items = (itemType *)calloc(queue->capacity, sizeof(itemType)); \
        \
        for(int i = 0; i < queue->count; i++) \
            queue->items[i] = old[(queue->head + i) % queue->count]; \
        \
        queue->head = 0; \
    } \
    \
    void typeName ## Init(typeName *queue) \
    { \
        queue->capacity = 8; \
        queue->count = 0; \
        queue->loadFactor = 8; \
        queue->head = 0; \
        queue->items = (itemType *)calloc(queue->capacity, sizeof(itemType)); \
    } \
    \
    void typeName ## Free(typeName *queue) \
    { \
        free(queue->items); \
    } \
    \
    void typeName ## Enqueue(typeName *queue, itemType value) \
    { \
        if (queue->count == queue->loadFactor) \
            typeName ## __Resize(queue); \
        \
        queue->items[queue->count % queue->capacity] = value; \
        queue->count++; \
    } \
    \
    itemType typeName ## Peek(typeName *queue) \
    { \
        if (queue->count == 0) \
            return defaultValue; \
        \
        return queue->items[queue->head]; \
    } \
    \
    itemType typeName ## Dequeue(typeName *queue) \
    { \
        if (queue->count == 0) \
            return defaultValue; \
        \
        itemType value = queue->items[queue->head]; \
        queue->head++; \
        queue->count--; \
        return value; \
    } \
    \
    bool typeName ## Contains(typeName *queue, itemType value) \
    { \
        for(int i = 0; i < queue->count; i++) \
        { \
            if (equalsFn(queue->items[(queue->head + i) % queue->count], value)) \
                return true; \
        } \
        \
        return false; \
    } \
    \
    void typeName ## Clear(typeName *queue) \
    { \
        queue->head = 0; \
        queue->count = 0; \
    }

#endif // SHL_STACK_H