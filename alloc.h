/*
    alloc.h - per-instance allocator interface for SHL collections.

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

    Self-contained single-header with no SHL dependencies.  Include it
    directly for allocator helpers. Every SHL collection header embeds only
    the separately guarded allocator type and works without this file.

    USAGE
    Define SHL_ALLOC_IMPLEMENTATION before including this header in exactly
    one translation unit to emit shl_heap_alloc(). Include without the define
    everywhere else. Collection Init(NULL) works without these helpers.

        #define SHL_ALLOC_IMPLEMENTATION
        #include "alloc.h"

    For shl_zone_alloc(), also include memzone.h (or memzone_audit.h) in that
    implementation translation unit and wherever the adapter is used. Both
    headers may be included in either order. Define implementation switches
    before the first include:

        #define SHL_ALLOC_IMPLEMENTATION
        #define SHL_MZ_IMPLEMENTATION
        #include "alloc.h"
        #include "memzone.h"
        #include "list.h"

        memzone_t*      zone  = mz_init(1 << 20);
        shl_allocator_t alloc = shl_zone_alloc(zone);
        IntListInit(&list, &alloc);

    Both 'zone' and 'alloc' must outlive collections using that allocator.

    A NULL shl_allocator_t* stored in a collection means the collection owns
    a fixed, externally-provided buffer and will never allocate or free memory.
*/

#ifndef SHL_ALLOC_H
#define SHL_ALLOC_H

#ifndef SHL_ALLOCATOR_T_DEFINED
#define SHL_ALLOCATOR_T_DEFINED

#include <stddef.h>

typedef struct shl_allocator_s
{
    void* ctx;
    void* (*mallocFn)(void* ctx, size_t sz);
    void* (*reallocFn)(void* ctx, void* ptr, size_t sz);
    void  (*freeFn)(void* ctx, void* ptr);
} shl_allocator_t;

#endif /* SHL_ALLOCATOR_T_DEFINED */

#ifdef __cplusplus
extern "C" {
#endif

/* Returns a pointer to a stable shl_allocator_t backed by the system heap
   (malloc / realloc / free). The returned pointer is valid for the lifetime
   of the program and may be shared freely across collections and threads. */
shl_allocator_t* shl_heap_alloc(void);

#ifdef __cplusplus
}
#endif

#endif /* SHL_ALLOC_H */

/* Outside the declaration guard: a later implementation include is allowed. */
#if defined(SHL_ALLOC_IMPLEMENTATION) && !defined(SHL_ALLOC_IMPLEMENTED)
#define SHL_ALLOC_IMPLEMENTED

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

static void* shl__heap_malloc_fn(void* ctx, size_t sz)             { (void)ctx; return malloc(sz); }
static void* shl__heap_realloc_fn(void* ctx, void* ptr, size_t sz) { (void)ctx; return realloc(ptr, sz); }
static void  shl__heap_free_fn(void* ctx, void* ptr)               { (void)ctx; free(ptr); }

shl_allocator_t* shl_heap_alloc(void)
{
    static shl_allocator_t heap = { NULL, shl__heap_malloc_fn, shl__heap_realloc_fn, shl__heap_free_fn };
    return &heap;
}

#ifdef __cplusplus
}
#endif

#endif /* SHL_ALLOC_IMPLEMENTATION && !SHL_ALLOC_IMPLEMENTED */

/* The second header to be included supplies this optional adapter. */
#if defined(SHL_ALLOC_H) && defined(SHL_MZ_H)

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SHL_ZONE_ALLOC_H
#define SHL_ZONE_ALLOC_H

/* The zone and returned allocator must outlive collections using them. */
shl_allocator_t shl_zone_alloc(memzone_t* zone);

#endif /* SHL_ZONE_ALLOC_H */

#if defined(SHL_ALLOC_IMPLEMENTATION) && !defined(SHL_ZONE_ALLOC_IMPLEMENTED)
#define SHL_ZONE_ALLOC_IMPLEMENTED

static void* shl__mz_malloc_fn(void* ctx, size_t sz)             { return mz_alloc((memzone_t*)ctx, sz); }
static void* shl__mz_realloc_fn(void* ctx, void* ptr, size_t sz) { return mz_realloc((memzone_t*)ctx, ptr, sz); }
static void shl__mz_free_fn(void* ctx, void* ptr)                { mz_free((memzone_t*)ctx, ptr); }

/* Returns an shl_allocator_t value backed by zone.
   Store the returned value and pass its address to collection Init functions. */
shl_allocator_t shl_zone_alloc(memzone_t* zone)
{
    shl_allocator_t a;
    a.ctx       = zone;
    a.mallocFn  = shl__mz_malloc_fn;
    a.reallocFn = shl__mz_realloc_fn;
    a.freeFn    = shl__mz_free_fn;
    return a;
}

#endif /* SHL_ALLOC_IMPLEMENTATION && !SHL_ZONE_ALLOC_IMPLEMENTED */

#ifdef __cplusplus
}
#endif

#endif /* SHL_ALLOC_H && SHL_MZ_H */
