#ifndef SHL_STACK_H
#define SHL_STACK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define shlDeclareStack(typeName, itemType) \
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
    void typeName ## Push(typeName *list, itemType value); \
    bool typeName ## Contains(typeName *list, itemType value); \
    void typeName ## Clear(typeName *list); \
    itemType typeName ## Peek(typeName *list); \
    itemType typeName ## Pop(typeName *list); \

#define shlDefineStack(typeName, itemType, equalsFn, defaultValue) \
    void typeName ## __resize(typeName *stack) \
    { \
        uint32_t oldCapacity = stack->capacity; \
        uint32_t oldLoadFactor = stack->loadFactor; \
        itemType *old = stack->items; \
        \
        stack->loadFactor = oldLoadFactor << 1; \
        stack->capacity = oldCapacity << 1; \
        stack->items = (itemType *)calloc(stack->capacity, sizeof(itemType)); \
        \
        memcpy(stack->items, old, stack->count * sizeof(itemType)); \
    } \
    \
    void typeName ## Init(typeName *stack) \
    { \
        stack->capacity = 8; \
        stack->loadFactor = 8; \
        stack->count = 0; \
        stack->items = (itemType *)calloc(stack->capacity, sizeof(itemType)); \
    } \
    \
    void typeName ## Free(typeName *stack) \
    { \
        free(stack->items); \
        stack->items = 0; \
        stack->count = 0; \
    } \
    \
    void typeName ## Push(typeName *stack, itemType value) \
    { \
        if (!stack->items) \
            return; \
        \
        if (stack->count == stack->loadFactor) \
            typeName ## __resize(stack); \
        \
        stack->items[stack->count] = value; \
        stack->count++; \
    } \
    \
    itemType typeName ## Peek(typeName *stack) \
    { \
        if (!stack->items || stack->count == 0) \
            return defaultValue; \
        \
        return stack->items[stack->count - 1]; \
    } \
    \
    itemType typeName ## Pop(typeName *stack) \
    { \
        if (!stack->items || stack->count == 0) \
            return defaultValue; \
        \
        itemType item = stack->items[stack->count - 1]; \
        stack->count--; \
        return item; \
    } \
    \
    bool typeName ## Contains(typeName *stack, itemType value) \
    { \
        if (!stack->items) \
            return false; \
        \
        for(int32_t i = 0; i < stack->count; i++) \
        { \
            if (equalsFn(stack->items[i], value)) \
                return true; \
        } \
        \
        return false; \
    } \
    \
    void typeName ## Clear(typeName *stack) \
    { \
        if (!stack->items) \
            return; \
        \
        stack->count = 0; \
    }

#endif // SHL_STACK_H