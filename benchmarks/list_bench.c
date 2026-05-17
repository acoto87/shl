/*
    benchmarks/list_bench.c — benchmarks for list.h using ubench.h

    Covers every public API function plus four integration scenarios.

    Build (GCC/Clang, from repo root):
        gcc -std=c11 -O2 -I. -Ibenchmarks benchmarks/list_bench.c -o build/default/list_bench

    Build (MSVC, from repo root):
        cl /std:c11 /O2 /I. /Ibenchmarks benchmarks/list_bench.c /Fe:build/default/list_bench.exe
*/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ubench.h declares ubench_large_integer only under _MSC_VER but uses it for
   MinGW too.  Provide the typedef via <windows.h> before ubench.h sees it. */
#if (defined(__MINGW32__) || defined(__MINGW64__)) && !defined(_MSC_VER)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
   typedef LARGE_INTEGER ubench_large_integer;
#endif

#include "../list.h"
#include "ubench.h"

/* -------------------------------------------------------------------------
   Type declarations
   ---------------------------------------------------------------------- */

static bool int_eq(const int a, const int b) { return a == b; }

static int32_t int_cmp(const int a, const int b, void* ud)
{
    (void)ud;
    return a - b;
}

shlDeclareList(IntList, int)
shlDefineList(IntList, int)

/* -------------------------------------------------------------------------
   Constants
   ---------------------------------------------------------------------- */

enum
{
    N_MEDIUM = 1024,    /* default list size for most benchmarks    */
    N_LARGE  = 16384,   /* large list size for sort scalability     */
    N_RANGE  = 64,      /* number of items used in range ops        */
    N_WARMUP = 512,     /* hot-path runs before first timed sample  */
    N_BATCH  = 128      /* ops per timed sample for sub-us benches  */
};

/* -------------------------------------------------------------------------
   Helpers
   ---------------------------------------------------------------------- */

static IntListOptions make_opts(void)
{
    return (IntListOptions){ .defaultValue = -1, .equalsFn = int_eq };
}

/* Fill list with n pseudo-random positive integers using a simple LCG.
   The sequence is reproducible and contains no negative values. */
static void list_fill_random(IntList* list, int32_t n)
{
    uint32_t s = 0xdeadbeef;
    for (int32_t i = 0; i < n; i++)
    {
        s = s * 1664525u + 1013904223u;
        IntListAdd(list, (int)(s >> 1)); /* shift to keep positive */
    }
}

/* -------------------------------------------------------------------------
   Fixture: ListMedium
   Pre-populated with N_MEDIUM pseudo-random positive integers.
   Used by benchmarks that only read or lightly mutate the list.
   ---------------------------------------------------------------------- */

struct ListMedium { IntList list; };

UBENCH_F_SETUP(ListMedium)
{
    IntListInit(&ubench_fixture->list, make_opts());
    list_fill_random(&ubench_fixture->list, N_MEDIUM);
}

UBENCH_F_TEARDOWN(ListMedium)
{
    IntListFree(&ubench_fixture->list);
}

/* =========================================================================
   Init / Free
   Measures the fixed overhead of creating and destroying an empty list.
   ======================================================================= */

UBENCH_EX(List, Init_Free)
{
    /* Warm up: prime caches and stabilise CPU boost frequency */
    for (int w = 0; w < N_WARMUP; w++)
    {
        IntList list;
        IntListInit(&list, make_opts());
        IntListFree(&list);
    }

    /* N_BATCH pairs per timed sample; divide reported mean by N_BATCH */
    UBENCH_DO_BENCHMARK()
    {
        for (int j = 0; j < N_BATCH; j++)
        {
            IntList list;
            IntListInit(&list, make_opts());
            IntListFree(&list);
            UBENCH_DO_NOTHING(&list);
        }
    }
}

/* =========================================================================
   Add (append)
   ======================================================================= */

/* Append N_MEDIUM items one at a time.
   list.count = 0 is an O(1) reset; allocated capacity is reused after the
   first iteration so no realloc occurs in the steady state. */
UBENCH_EX(List, Add_1024)
{
    IntList list;
    IntListInit(&list, make_opts());

    /* Warm up: pre-fill capacity so no realloc occurs in the timed loop */
    for (int w = 0; w < 8; w++)
    {
        list.count = 0;
        for (int32_t i = 0; i < N_MEDIUM; i++)
            IntListAdd(&list, i);
    }

    UBENCH_DO_BENCHMARK()
    {
        list.count = 0;
        for (int32_t i = 0; i < N_MEDIUM; i++)
            IntListAdd(&list, i);
        UBENCH_DO_NOTHING(&list);
    }

    IntListFree(&list);
}

/* =========================================================================
   AddRange
   ======================================================================= */

/* Load N_MEDIUM items via a single AddRange call. */
UBENCH_EX(List, AddRange_1024)
{
    int src[N_MEDIUM];
    for (int i = 0; i < N_MEDIUM; i++) src[i] = i;

    IntList list;
    IntListInit(&list, make_opts());

    /* Warm up */
    for (int w = 0; w < 8; w++)
    {
        list.count = 0;
        IntListAddRange(&list, N_MEDIUM, src);
    }

    UBENCH_DO_BENCHMARK()
    {
        list.count = 0;
        IntListAddRange(&list, N_MEDIUM, src);
        UBENCH_DO_NOTHING(&list);
    }

    IntListFree(&list);
}

/* =========================================================================
   Insert
   ======================================================================= */

/* Insert at the front each time — worst-case: memmove shifts all elements. */
UBENCH_EX(List, Insert_front_1024)
{
    IntList list;
    IntListInit(&list, make_opts());

    /* Warm up */
    for (int w = 0; w < 4; w++)
    {
        list.count = 0;
        for (int32_t i = 0; i < N_MEDIUM; i++)
            IntListInsert(&list, 0, i);
    }

    UBENCH_DO_BENCHMARK()
    {
        list.count = 0;
        for (int32_t i = 0; i < N_MEDIUM; i++)
            IntListInsert(&list, 0, i);
        UBENCH_DO_NOTHING(&list);
    }

    IntListFree(&list);
}

/* Insert at the midpoint each time — memmove shifts half the list. */
UBENCH_EX(List, Insert_middle_1024)
{
    IntList list;
    IntListInit(&list, make_opts());

    /* Warm up */
    for (int w = 0; w < 4; w++)
    {
        list.count = 0;
        for (int32_t i = 0; i < N_MEDIUM; i++)
            IntListInsert(&list, list.count / 2, i);
    }

    UBENCH_DO_BENCHMARK()
    {
        list.count = 0;
        for (int32_t i = 0; i < N_MEDIUM; i++)
            IntListInsert(&list, list.count / 2, i);
        UBENCH_DO_NOTHING(&list);
    }

    IntListFree(&list);
}

/* =========================================================================
   InsertRange
   Each iteration inserts N_RANGE items then removes them to keep the list
   at a stable N_MEDIUM size.  The pair cost is 2 × memmove(N_MEDIUM × 4).
   ======================================================================= */

UBENCH_EX(List, InsertRange_front)
{
    int src[N_RANGE];
    for (int i = 0; i < N_RANGE; i++) src[i] = i;

    IntList list;
    IntListInit(&list, make_opts());
    list_fill_random(&list, N_MEDIUM);

    /* Warm up */
    for (int w = 0; w < 8; w++)
    {
        IntListInsertRange(&list, 0, N_RANGE, src);
        IntListRemoveAtRange(&list, 0, N_RANGE);
    }

    UBENCH_DO_BENCHMARK()
    {
        IntListInsertRange(&list, 0, N_RANGE, src);
        IntListRemoveAtRange(&list, 0, N_RANGE);
        UBENCH_DO_NOTHING(&list);
    }

    IntListFree(&list);
}

UBENCH_EX(List, InsertRange_middle)
{
    int src[N_RANGE];
    for (int i = 0; i < N_RANGE; i++) src[i] = i;

    IntList list;
    IntListInit(&list, make_opts());
    list_fill_random(&list, N_MEDIUM);

    /* Warm up */
    for (int w = 0; w < 8; w++)
    {
        IntListInsertRange(&list, N_MEDIUM / 2, N_RANGE, src);
        IntListRemoveAtRange(&list, N_MEDIUM / 2, N_RANGE);
    }

    UBENCH_DO_BENCHMARK()
    {
        IntListInsertRange(&list, N_MEDIUM / 2, N_RANGE, src);
        IntListRemoveAtRange(&list, N_MEDIUM / 2, N_RANGE);
        UBENCH_DO_NOTHING(&list);
    }

    IntListFree(&list);
}

/* =========================================================================
   IndexOf
   ======================================================================= */

/* Worst case: target is the very last element — full linear scan required. */
UBENCH_EX_F(ListMedium, IndexOf_hit_end)
{
    int target = ubench_fixture->list.items[N_MEDIUM - 1];

    /* Warm up: prime branch predictor and data caches */
    for (int w = 0; w < 8; w++)
    {
        int32_t r = IntListIndexOf(&ubench_fixture->list, target);
        (void)r;
    }

    UBENCH_DO_BENCHMARK()
    {
        int32_t result = IntListIndexOf(&ubench_fixture->list, target);
        UBENCH_DO_NOTHING(&result);
    }
}

/* Miss: -1 is never produced by the LCG (all values are positive). */
UBENCH_EX_F(ListMedium, IndexOf_miss)
{
    /* Warm up */
    for (int w = 0; w < 8; w++)
    {
        int32_t r = IntListIndexOf(&ubench_fixture->list, -1);
        (void)r;
    }

    UBENCH_DO_BENCHMARK()
    {
        int32_t result = IntListIndexOf(&ubench_fixture->list, -1);
        UBENCH_DO_NOTHING(&result);
    }
}

/* =========================================================================
   Contains
   ======================================================================= */

UBENCH_EX_F(ListMedium, Contains_hit_middle)
{
    int target = ubench_fixture->list.items[N_MEDIUM / 2];

    /* Warm up */
    for (int w = 0; w < 8; w++)
    {
        bool r = IntListContains(&ubench_fixture->list, target);
        (void)r;
    }

    UBENCH_DO_BENCHMARK()
    {
        bool result = IntListContains(&ubench_fixture->list, target);
        UBENCH_DO_NOTHING(&result);
    }
}

UBENCH_EX_F(ListMedium, Contains_miss)
{
    /* Warm up */
    for (int w = 0; w < 8; w++)
    {
        bool r = IntListContains(&ubench_fixture->list, -1);
        (void)r;
    }

    UBENCH_DO_BENCHMARK()
    {
        bool result = IntListContains(&ubench_fixture->list, -1);
        UBENCH_DO_NOTHING(&result);
    }
}

/* =========================================================================
   Get / Set
   ======================================================================= */

/* O(1) direct array read — batched so each sample exceeds timer resolution. */
UBENCH_EX_F(ListMedium, Get_middle)
{
    /* Warm up */
    for (int w = 0; w < N_WARMUP; w++)
    {
        int r = IntListGet(&ubench_fixture->list, N_MEDIUM / 2);
        (void)r;
    }

    UBENCH_DO_BENCHMARK()
    {
        for (int j = 0; j < N_BATCH; j++)
        {
            int result = IntListGet(&ubench_fixture->list, N_MEDIUM / 2);
            UBENCH_DO_NOTHING(&result);
        }
    }
}

/* O(1) direct array write — batched so each sample exceeds timer resolution. */
UBENCH_EX_F(ListMedium, Set_middle)
{
    /* Warm up */
    for (int w = 0; w < N_WARMUP; w++)
        IntListSet(&ubench_fixture->list, N_MEDIUM / 2, 42);

    UBENCH_DO_BENCHMARK()
    {
        for (int j = 0; j < N_BATCH; j++)
            IntListSet(&ubench_fixture->list, N_MEDIUM / 2, j);
        UBENCH_DO_NOTHING(&ubench_fixture->list);
    }
}

/* =========================================================================
   Remove / RemoveAt / RemoveAtRange
   ======================================================================= */

/* RemoveAt from the front: O(n) memmove shifts the entire list.
   list.count is reset each iteration so the same removal is repeated. */
UBENCH_EX(List, RemoveAt_front)
{
    IntList list;
    IntListInit(&list, make_opts());
    list_fill_random(&list, N_MEDIUM);

    /* Warm up */
    for (int w = 0; w < 8; w++)
    {
        list.count = N_MEDIUM;
        IntListRemoveAt(&list, 0);
    }

    UBENCH_DO_BENCHMARK()
    {
        list.count = N_MEDIUM; /* restore count; items array untouched */
        IntListRemoveAt(&list, 0);
        UBENCH_DO_NOTHING(&list);
    }

    IntListFree(&list);
}

/* RemoveAt from the back: no shift, O(1) — batched for reliable CI. */
UBENCH_EX(List, RemoveAt_back)
{
    IntList list;
    IntListInit(&list, make_opts());
    list_fill_random(&list, N_MEDIUM);

    /* Warm up */
    for (int w = 0; w < N_WARMUP; w++)
    {
        list.count = N_MEDIUM;
        IntListRemoveAt(&list, N_MEDIUM - 1);
    }

    UBENCH_DO_BENCHMARK()
    {
        for (int j = 0; j < N_BATCH; j++)
        {
            list.count = N_MEDIUM;
            IntListRemoveAt(&list, N_MEDIUM - 1);
        }
        UBENCH_DO_NOTHING(&list);
    }

    IntListFree(&list);
}

/* Remove by value from the midpoint: scans half the list then shifts.
   We reinsert the target after each removal to keep the list stable. */
UBENCH_EX(List, Remove_value_middle)
{
    IntList list;
    IntListInit(&list, make_opts());
    list_fill_random(&list, N_MEDIUM);
    int target = list.items[N_MEDIUM / 2];

    /* Warm up */
    for (int w = 0; w < 8; w++)
    {
        IntListRemove(&list, target);
        IntListInsert(&list, N_MEDIUM / 2, target);
    }

    UBENCH_DO_BENCHMARK()
    {
        IntListRemove(&list, target);
        IntListInsert(&list, N_MEDIUM / 2, target);
        UBENCH_DO_NOTHING(&list);
    }

    IntListFree(&list);
}

/* RemoveAtRange from the front then reinsert to keep size constant. */
UBENCH_EX(List, RemoveAtRange_front)
{
    int src[N_RANGE];
    for (int i = 0; i < N_RANGE; i++) src[i] = i;

    IntList list;
    IntListInit(&list, make_opts());
    list_fill_random(&list, N_MEDIUM);

    /* Warm up */
    for (int w = 0; w < 8; w++)
    {
        IntListRemoveAtRange(&list, 0, N_RANGE);
        IntListInsertRange(&list, 0, N_RANGE, src);
    }

    UBENCH_DO_BENCHMARK()
    {
        IntListRemoveAtRange(&list, 0, N_RANGE);
        IntListInsertRange(&list, 0, N_RANGE, src);
        UBENCH_DO_NOTHING(&list);
    }

    IntListFree(&list);
}

/* =========================================================================
   Clear
   For an int list (no freeFn) Clear is just list.count = 0 — O(1).
   We re-fill before each Clear so it always operates on a full list.
   ======================================================================= */

UBENCH_EX(List, Clear_1024)
{
    int src[N_MEDIUM];
    for (int i = 0; i < N_MEDIUM; i++) src[i] = i;

    IntList list;
    IntListInit(&list, make_opts());

    /* Warm up */
    for (int w = 0; w < 8; w++)
    {
        list.count = 0;
        IntListAddRange(&list, N_MEDIUM, src);
        IntListClear(&list);
    }

    UBENCH_DO_BENCHMARK()
    {
        list.count = 0;
        IntListAddRange(&list, N_MEDIUM, src);
        IntListClear(&list);
        UBENCH_DO_NOTHING(&list);
    }

    IntListFree(&list);
}

/* =========================================================================
   Reverse
   O(n) element swaps.  Calling it each iteration alternates the order but
   the work is identical in both directions.
   ======================================================================= */

/* Even warmup count leaves the list in its original order before measurement. */
UBENCH_EX_F(ListMedium, Reverse)
{
    for (int w = 0; w < 8; w++)
        IntListReverse(&ubench_fixture->list);

    UBENCH_DO_BENCHMARK()
    {
        IntListReverse(&ubench_fixture->list);
        UBENCH_DO_NOTHING(&ubench_fixture->list);
    }
}

/* =========================================================================
   Sort
   We refill from a fixed pseudo-random source array each iteration so that
   sort always operates on unsorted data.  AddRange (O(n)) is included but
   Sort (O(n log n)) dominates for n >= 128.
   ======================================================================= */

UBENCH_EX(List, Sort_1024)
{
    int src[N_MEDIUM];
    {
        uint32_t s = 0xdeadbeef;
        for (int i = 0; i < N_MEDIUM; i++)
        {
            s = s * 1664525u + 1013904223u;
            src[i] = (int)(s >> 1);
        }
    }

    IntList list;
    IntListInit(&list, make_opts());

    /* Warm up */
    for (int w = 0; w < 4; w++)
    {
        list.count = 0;
        IntListAddRange(&list, N_MEDIUM, src);
        IntListSort(&list, int_cmp, NULL);
    }

    UBENCH_DO_BENCHMARK()
    {
        list.count = 0;
        IntListAddRange(&list, N_MEDIUM, src);
        IntListSort(&list, int_cmp, NULL);
        UBENCH_DO_NOTHING(&list);
    }

    IntListFree(&list);
}

UBENCH_EX(List, Sort_16384)
{
    int* src = (int*)malloc((size_t)N_LARGE * sizeof(int));
    {
        uint32_t s = 0xdeadbeef;
        for (int i = 0; i < N_LARGE; i++)
        {
            s = s * 1664525u + 1013904223u;
            src[i] = (int)(s >> 1);
        }
    }

    IntList list;
    IntListInit(&list, make_opts());

    /* Warm up (2 passes — body is heavy, keep overhead low) */
    for (int w = 0; w < 2; w++)
    {
        list.count = 0;
        IntListAddRange(&list, N_LARGE, src);
        IntListSort(&list, int_cmp, NULL);
    }

    UBENCH_DO_BENCHMARK()
    {
        list.count = 0;
        IntListAddRange(&list, N_LARGE, src);
        IntListSort(&list, int_cmp, NULL);
        UBENCH_DO_NOTHING(&list);
    }

    IntListFree(&list);
    free(src);
}

/* =========================================================================
   CopyTo / ToArray
   ======================================================================= */

UBENCH_EX_F(ListMedium, CopyTo)
{
    int* dest = (int*)malloc((size_t)N_MEDIUM * sizeof(int));

    /* Warm up */
    for (int w = 0; w < 8; w++)
        IntListCopyTo(&ubench_fixture->list, dest, 0);

    UBENCH_DO_BENCHMARK()
    {
        IntListCopyTo(&ubench_fixture->list, dest, 0);
        UBENCH_DO_NOTHING(dest);
    }

    free(dest);
}

UBENCH_EX_F(ListMedium, ToArray)
{
    /* Warm up: prime the allocator's free-list */
    for (int w = 0; w < 8; w++)
    {
        int* arr = IntListToArray(&ubench_fixture->list);
        free(arr);
    }

    UBENCH_DO_BENCHMARK()
    {
        int* arr = IntListToArray(&ubench_fixture->list);
        UBENCH_DO_NOTHING(arr);
        free(arr);
    }
}

/* =========================================================================
   Integration benchmarks
   ======================================================================= */

/*
 * Build_Sort_Search
 * Models a collect-then-query workflow: append N_MEDIUM random items,
 * sort ascending, then search for the sorted median via IndexOf.
 */
UBENCH_EX(List, Integration_build_sort_search)
{
    int src[N_MEDIUM];
    {
        uint32_t s = 0xdeadbeef;
        for (int i = 0; i < N_MEDIUM; i++)
        {
            s = s * 1664525u + 1013904223u;
            src[i] = (int)(s >> 1);
        }
    }

    IntList list;
    IntListInit(&list, make_opts());

    /* Warm up */
    for (int w = 0; w < 4; w++)
    {
        list.count = 0;
        IntListAddRange(&list, N_MEDIUM, src);
        IntListSort(&list, int_cmp, NULL);
    }

    UBENCH_DO_BENCHMARK()
    {
        list.count = 0;
        IntListAddRange(&list, N_MEDIUM, src);
        IntListSort(&list, int_cmp, NULL);
        int mid = list.items[N_MEDIUM / 2];
        int32_t result = IntListIndexOf(&list, mid);
        UBENCH_DO_NOTHING(&result);
    }

    IntListFree(&list);
}

/*
 * FIFO_pattern
 * Simulates a sliding window: remove from the front (O(n) memmove) then
 * append to the back (O(1) amortised).  Models event-queue or pipeline usage.
 */
UBENCH_EX(List, Integration_fifo_pattern)
{
    IntList list;
    IntListInit(&list, make_opts());
    list_fill_random(&list, N_MEDIUM);

    int next_val = 0;

    /* Warm up */
    for (int w = 0; w < 8; w++)
    {
        IntListRemoveAt(&list, 0);
        IntListAdd(&list, next_val++);
    }

    UBENCH_DO_BENCHMARK()
    {
        IntListRemoveAt(&list, 0);
        IntListAdd(&list, next_val++);
        UBENCH_DO_NOTHING(&list);
    }

    IntListFree(&list);
}

/*
 * Batch_insert_remove
 * Repeatedly inserts N_RANGE items at the front and removes N_RANGE items
 * from the back, keeping the list near N_MEDIUM.  Exercises the memmove
 * path for InsertRange and RemoveAtRange in an interleaved pattern.
 */
UBENCH_EX(List, Integration_batch_insert_remove)
{
    int src[N_RANGE];
    for (int i = 0; i < N_RANGE; i++) src[i] = i;

    IntList list;
    IntListInit(&list, make_opts());
    list_fill_random(&list, N_MEDIUM);

    /* Warm up */
    for (int w = 0; w < 8; w++)
    {
        IntListInsertRange(&list, 0, N_RANGE, src);
        IntListRemoveAtRange(&list, list.count - N_RANGE, N_RANGE);
    }

    UBENCH_DO_BENCHMARK()
    {
        IntListInsertRange(&list, 0, N_RANGE, src);
        IntListRemoveAtRange(&list, list.count - N_RANGE, N_RANGE);
        UBENCH_DO_NOTHING(&list);
    }

    IntListFree(&list);
}

/*
 * Reverse_Sort
 * Fills the list with ascending data, reverses it (worst-case input for
 * qsort), then sorts it back — models a sort-under-adversarial-input scenario.
 */
UBENCH_EX(List, Integration_reverse_sort)
{
    int src[N_MEDIUM];
    for (int i = 0; i < N_MEDIUM; i++) src[i] = i; /* ascending */

    IntList list;
    IntListInit(&list, make_opts());

    /* Warm up */
    for (int w = 0; w < 4; w++)
    {
        list.count = 0;
        IntListAddRange(&list, N_MEDIUM, src);
        IntListReverse(&list);
        IntListSort(&list, int_cmp, NULL);
    }

    UBENCH_DO_BENCHMARK()
    {
        list.count = 0;
        IntListAddRange(&list, N_MEDIUM, src);
        IntListReverse(&list);                     /* now descending */
        IntListSort(&list, int_cmp, NULL);
        UBENCH_DO_NOTHING(&list);
    }

    IntListFree(&list);
}

/* =========================================================================
   Entry point
   ======================================================================= */

UBENCH_MAIN()
