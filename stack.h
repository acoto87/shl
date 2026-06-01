/*
    stack.h - acoto87 (acoto87@gmail.com)

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

    Single-header macro library to declare and define strongly typed stacks
    backed by dynamically resized arrays.

    USAGE
    Declare a stack type with shlDeclareStack(name, type), then define it once
    with shlDefineStack(name, type) in a C source file.

    ALLOCATION
    Pass an shl_allocator_t* to Init to control where the items buffer lives.
    Use shl_heap_alloc() for the default system heap.  To use a memzone_t,
    include memzone.h before this header and call shl_zone_alloc(zone).

    For a fixed-capacity stack with no heap involvement at all, use InitFixed
    and supply a caller-owned buffer.  Free is a safe no-op on fixed stacks.

    ITEM OWNERSHIP
    The stack stores values by copy and does not manage the lifecycle of
    items.  The caller is responsible for freeing any resources owned by
    items before calling Clear or Free.

    SEARCH
    Contains requires an explicit equality function at the call site rather
    than storing one in the stack struct.

    OUT-OF-RANGE READS
    Peek and Pop return a zero-initialised value when the stack is empty.
*/

#ifndef SHL_STACK_H
#define SHL_STACK_H

#include "internal.h"

#define shlDeclareStack(typeName, itemType) \
    typedef struct \
    { \
        int32_t count; \
        int32_t capacity; \
        shl_allocator_t* alloc; \
        itemType* items; \
    } typeName; \
    \
    void typeName ## Init(typeName* stack, shl_allocator_t* alloc); \
    void typeName ## InitFixed(typeName* stack, itemType* buffer, int32_t capacity); \
    void typeName ## Free(typeName* stack); \
    void typeName ## Push(typeName* stack, itemType value); \
    itemType typeName ## Peek(typeName* stack); \
    itemType typeName ## Pop(typeName* stack); \
    bool typeName ## Contains(typeName* stack, itemType value, bool (*equalsFn)(const itemType, const itemType)); \
    void typeName ## Clear(typeName* stack);

#define shlDefineStack(typeName, itemType) \
    void typeName ## Init(typeName* stack, shl_allocator_t* alloc) \
    { \
        if (!alloc) alloc = shl_heap_alloc(); \
        if (!alloc->mallocFn) return; \
        stack->alloc    = alloc; \
        stack->capacity = SHL__INITIAL_CAPACITY; \
        stack->count    = 0; \
        stack->items    = (itemType*)alloc->mallocFn(alloc->ctx, (size_t)stack->capacity * sizeof(itemType)); \
    } \
    \
    void typeName ## InitFixed(typeName* stack, itemType* buffer, int32_t capacity) \
    { \
        stack->alloc    = NULL; \
        stack->capacity = capacity; \
        stack->count    = 0; \
        stack->items    = buffer; \
    } \
    \
    void typeName ## Free(typeName* stack) \
    { \
        stack->count = 0; \
        if (stack->items && stack->alloc && stack->alloc->freeFn) \
            stack->alloc->freeFn(stack->alloc->ctx, stack->items); \
        stack->items = NULL; \
    } \
    \
    void typeName ## Push(typeName* stack, itemType value) \
    { \
        if (!stack->items) \
            return; \
        \
        if (stack->count == stack->capacity) \
        { \
            if (!shl__resizeArray((void**)&stack->items, &stack->capacity, stack->count + 1, sizeof(itemType), stack->alloc)) \
                return; \
        } \
        \
        stack->items[stack->count] = value; \
        stack->count++; \
    } \
    \
    itemType typeName ## Peek(typeName* stack) \
    { \
        if (!stack->items || stack->count == 0) \
        { \
            itemType zero; \
            memset(&zero, 0, sizeof(itemType)); \
            return zero; \
        } \
        return stack->items[stack->count - 1]; \
    } \
    \
    itemType typeName ## Pop(typeName* stack) \
    { \
        if (!stack->items || stack->count == 0) \
        { \
            itemType zero; \
            memset(&zero, 0, sizeof(itemType)); \
            return zero; \
        } \
        \
        itemType item = stack->items[stack->count - 1]; \
        stack->count--; \
        return item; \
    } \
    \
    bool typeName ## Contains(typeName* stack, itemType value, bool (*equalsFn)(const itemType, const itemType)) \
    { \
        if (!stack->items || !equalsFn) \
            return false; \
        \
        for (int32_t i = 0; i < stack->count; i++) \
        { \
            if (equalsFn(stack->items[i], value)) \
                return true; \
        } \
        \
        return false; \
    } \
    \
    void typeName ## Clear(typeName* stack) \
    { \
        stack->count = 0; \
    }

#endif // SHL_STACK_H
