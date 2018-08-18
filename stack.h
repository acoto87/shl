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
    extern void typeName ## Init(typeName *list); \
    extern void typeName ## Free(typeName *list); \
    extern void typeName ## Push(typeName *list, itemType value); \
    extern bool typeName ## Contains(typeName *list, itemType value); \
    extern void typeName ## Clear(typeName *list); \
    extern itemType typeName ## Peek(typeName *list); \
    extern itemType typeName ## Pop(typeName *list); \

#define shlDefineStack(typeName, itemType, equalsFn, defaultValue) \
    void typeName ## __Resize(typeName *stack) \
    { \
        uint32_t oldCapacity = stack->capacity; \
        itemType* old = stack->items; \
        \
        stack->loadFactor = oldCapacity; \
        stack->capacity = oldCapacity << 1; \
        stack->items = (itemType *)calloc(stack->capacity, sizeof(itemType)); \
        \
        for(int i = 0; i < stack->count; i++) \
            stack->items[i] = old[i]; \
    } \
    \
    void typeName ## Init(typeName *stack) \
    { \
        stack->capacity = 8; \
        stack->count = 0; \
        stack->loadFactor = 4; \
        stack->items = (itemType *)calloc(stack->capacity, sizeof(itemType)); \
    } \
    \
    void typeName ## Free(typeName *stack) \
    { \
        free(stack->items); \
    } \
    \
    void typeName ## Push(typeName *stack, itemType value) \
    { \
        if (stack->count == stack->loadFactor) \
            typeName ## __Resize(stack); \
        \
        stack->items[stack->count] = value; \
        stack->count++; \
    } \
    \
    itemType typeName ## Peek(typeName *stack) \
    { \
        if (stack->count == 0) \
            return defaultValue; \
        \
        return stack->items[stack->count - 1]; \
    } \
    \
    itemType typeName ## Pop(typeName *stack) \
    { \
        if (stack->count == 0) \
            return defaultValue; \
        \
        itemType item = stack->items[stack->count-1]; \
        stack->count--; \
        return item; \
    } \
    \
    bool typeName ## Contains(typeName *stack, itemType value) \
    { \
        for(int i = 0; i < stack->count; i++) \
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
        stack->count = 0; \
    }

#endif // SHL_STACK_H