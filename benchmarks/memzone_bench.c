/*
    benchmarks/memzone_bench.c — benchmarks for memzone.h using ubench.h

    Covers every major API operation in isolation plus integrated scenarios
    and direct comparisons against stdlib malloc / realloc / free.

    Note: mz_alloc always zeroes the returned memory; malloc does not.
    The comparisons therefore include that write cost on the memzone side.

    Build (GCC/Clang, from repo root):
        gcc -std=c99 -O2 -I. -Ibenchmarks benchmarks/memzone_bench.c -lm -o build/bench/memzone_bench

    Build (MSVC, from repo root):
        cl /std:c99 /O2 /I. /Ibenchmarks benchmarks/memzone_bench.c /Fe:build/bench/memzone_bench.exe
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

#define SHL_MZ_IMPLEMENTATION
#include "../memzone.h"
#include "ubench.h"

/* -------------------------------------------------------------------------
   Constants
   ---------------------------------------------------------------------- */

enum
{
    BENCH_ZONE_SIZE = 4 * 1024 * 1024,  /* 4 MB — comfortably fits all scenarios    */
    N_ALLOCS        = 1024,             /* items per sequential batch               */
    ALLOC_SIZE      = 64,               /* payload bytes per allocation             */
    POOL            = 256,              /* items used in the mixed scenario         */
    N_WARMUP        = 512,              /* hot-path iterations before first sample  */
    N_BATCH         = 128               /* ops per timed sample for sub-us benches  */
};

/* =========================================================================
   mz_alloc — sequential allocation
   Allocate N_ALLOCS × ALLOC_SIZE bytes then reset in O(1).
   Measures pure allocation throughput; mz_reset overhead is negligible.
   ======================================================================= */

UBENCH_EX(Memzone, Alloc_seq_1024)
{
    memzone_t* zone = mz_init(BENCH_ZONE_SIZE);

    UBENCH_DO_BENCHMARK()
    {
        for (int i = 0; i < N_ALLOCS; i++)
            mz_alloc(zone, ALLOC_SIZE);
        mz_reset(zone);
        UBENCH_DO_NOTHING(&zone);
    }

    mz_destroy(zone);
}

/* =========================================================================
   stdlib baseline — mz_alloc vs malloc (sequential, 1024)
   Direct counterpart to Memzone.Alloc_seq_1024.  stdlib has no O(1) reset
   so each iteration also frees the N_ALLOCS items.  The difference between
   this and Memzone.Alloc_seq_1024 shows the O(1) teardown advantage of
   mz_reset.
   ======================================================================= */

UBENCH_EX(Stdlib, Alloc_seq_1024)
{
    void* ptrs[N_ALLOCS];

    UBENCH_DO_BENCHMARK()
    {
        for (int i = 0; i < N_ALLOCS; i++)
            ptrs[i] = malloc(ALLOC_SIZE);
        for (int i = 0; i < N_ALLOCS; i++)
            free(ptrs[i]);
        UBENCH_DO_NOTHING(ptrs);
    }
}

/* =========================================================================
   mz_alloc + mz_free — paired batch
   Allocate N_ALLOCS items then free them all.  Each iteration leaves the
   zone in the same state as the start so no drift occurs.
   ======================================================================= */

UBENCH_EX(Memzone, AllocFree_paired_1024)
{
    void* ptrs[N_ALLOCS];
    memzone_t* zone = mz_init(BENCH_ZONE_SIZE);

    UBENCH_DO_BENCHMARK()
    {
        for (int i = 0; i < N_ALLOCS; i++)
            ptrs[i] = mz_alloc(zone, ALLOC_SIZE);
        for (int i = 0; i < N_ALLOCS; i++)
            mz_free(zone, ptrs[i]);
        UBENCH_DO_NOTHING(ptrs);
    }

    mz_destroy(zone);
}

/* =========================================================================
   mz_alloc + mz_free — single pair (batched)
   Performs N_BATCH alloc+free pairs per ubench iteration so each timed
   sample is long enough for a reliable confidence interval on Windows
   (sub-100 ns ops saturate the QPC timer noise at 1 op/sample).
   Divide the reported mean by N_BATCH for the per-operation cost.
   ======================================================================= */

UBENCH_EX(Memzone, AllocFree_single_64)
{
    memzone_t* zone = mz_init(BENCH_ZONE_SIZE);

    /* Warm up: prime caches and stabilise CPU boost frequency */
    for (int w = 0; w < N_WARMUP; w++)
    {
        void* p = mz_alloc(zone, ALLOC_SIZE);
        mz_free(zone, p);
    }

    UBENCH_DO_BENCHMARK()
    {
        for (int j = 0; j < N_BATCH; j++)
        {
            void* p = mz_alloc(zone, ALLOC_SIZE);
            UBENCH_DO_NOTHING(p);
            mz_free(zone, p);
        }
    }

    mz_destroy(zone);
}

/* =========================================================================
   mz_allocAligned — 64-byte aligned allocation
   Same sequential pattern as Alloc_seq_1024 with explicit 64-byte alignment.
   ======================================================================= */

UBENCH_EX(Memzone, AllocAligned_64_seq_1024)
{
    memzone_t* zone = mz_init(BENCH_ZONE_SIZE);

    UBENCH_DO_BENCHMARK()
    {
        for (int i = 0; i < N_ALLOCS; i++)
            mz_allocAligned(zone, ALLOC_SIZE, 64);
        mz_reset(zone);
        UBENCH_DO_NOTHING(&zone);
    }

    mz_destroy(zone);
}

/* =========================================================================
   mz_realloc — grow in-place (batched)
   alloc(64) + realloc(128) + free per inner step.  The adjacent free tail
   absorbs the growth without a copy; free coalesces back each time.
   Performs N_BATCH triplets per ubench iteration; divide mean by N_BATCH.
   ======================================================================= */

UBENCH_EX(Memzone, Realloc_grow_64_to_128)
{
    memzone_t* zone = mz_init(BENCH_ZONE_SIZE);

    /* Warm up */
    for (int w = 0; w < N_WARMUP; w++)
    {
        void* p = mz_alloc(zone, ALLOC_SIZE);
        void* q = mz_realloc(zone, p, ALLOC_SIZE * 2);
        mz_free(zone, q);
    }

    UBENCH_DO_BENCHMARK()
    {
        for (int j = 0; j < N_BATCH; j++)
        {
            void* p = mz_alloc(zone, ALLOC_SIZE);
            void* q = mz_realloc(zone, p, ALLOC_SIZE * 2);
            UBENCH_DO_NOTHING(q);
            mz_free(zone, q);
        }
    }

    mz_destroy(zone);
}

/* =========================================================================
   mz_realloc — shrink (batched)
   alloc(128) + realloc(64) + free per inner step.  Block is split on
   shrink; free coalesces both parts back each time.
   Performs N_BATCH triplets per ubench iteration; divide mean by N_BATCH.
   ======================================================================= */

UBENCH_EX(Memzone, Realloc_shrink_128_to_64)
{
    memzone_t* zone = mz_init(BENCH_ZONE_SIZE);

    /* Warm up */
    for (int w = 0; w < N_WARMUP; w++)
    {
        void* p = mz_alloc(zone, ALLOC_SIZE * 2);
        void* q = mz_realloc(zone, p, ALLOC_SIZE);
        mz_free(zone, q);
    }

    UBENCH_DO_BENCHMARK()
    {
        for (int j = 0; j < N_BATCH; j++)
        {
            void* p = mz_alloc(zone, ALLOC_SIZE * 2);
            void* q = mz_realloc(zone, p, ALLOC_SIZE);
            UBENCH_DO_NOTHING(q);
            mz_free(zone, q);
        }
    }

    mz_destroy(zone);
}

/* =========================================================================
   Fragmented scenario — fill, fragment, fill holes
   Allocate N_ALLOCS items, free every other one (alternating free / alloc
   blocks), then re-fill the holes.  Exercises the rover's next-fit scan
   across allocated blocks — the key path exercised by the rover fix.
   ======================================================================= */

UBENCH_EX(Memzone, Fragmented_fill_holes_512)
{
    void* ptrs[N_ALLOCS];
    memzone_t* zone = mz_init(BENCH_ZONE_SIZE);

    UBENCH_DO_BENCHMARK()
    {
        /* Fill */
        for (int i = 0; i < N_ALLOCS; i++)
            ptrs[i] = mz_alloc(zone, ALLOC_SIZE);

        /* Fragment: free every other allocation */
        for (int i = 0; i < N_ALLOCS; i += 2)
            mz_free(zone, ptrs[i]);

        /* Fill the holes (rover must traverse allocated blocks to find free slots) */
        for (int i = 0; i < N_ALLOCS; i += 2)
            ptrs[i] = mz_alloc(zone, ALLOC_SIZE);

        /* Clean up for next iteration */
        for (int i = 0; i < N_ALLOCS; i++)
            mz_free(zone, ptrs[i]);

        UBENCH_DO_NOTHING(ptrs);
    }

    mz_destroy(zone);
}

/* =========================================================================
   Mixed integrated scenario
   Simulates a typical frame-based workload:
     Phase 1 — allocate a pool of POOL objects
     Phase 2 — grow every 4th object via realloc
     Phase 3 — free the first half of the pool
     Phase 4 — re-allocate the freed slots
     Phase 5 — free everything
   ======================================================================= */

UBENCH_EX(Memzone, Mixed_alloc_realloc_free)
{
    void* ptrs[POOL];
    memzone_t* zone = mz_init(BENCH_ZONE_SIZE);

    UBENCH_DO_BENCHMARK()
    {
        /* Phase 1: allocate */
        for (int i = 0; i < POOL; i++)
            ptrs[i] = mz_alloc(zone, ALLOC_SIZE);

        /* Phase 2: grow every 4th item */
        for (int i = 0; i < POOL; i += 4)
            ptrs[i] = mz_realloc(zone, ptrs[i], ALLOC_SIZE * 2);

        /* Phase 3: free the first half */
        for (int i = 0; i < POOL / 2; i++)
            mz_free(zone, ptrs[i]);

        /* Phase 4: re-alloc the freed slots */
        for (int i = 0; i < POOL / 2; i++)
            ptrs[i] = mz_alloc(zone, ALLOC_SIZE);

        /* Phase 5: free everything */
        for (int i = 0; i < POOL; i++)
            mz_free(zone, ptrs[i]);

        UBENCH_DO_NOTHING(ptrs);
    }

    mz_destroy(zone);
}

/* =========================================================================
   stdlib baseline — malloc / realloc / free
   Mirror of every Memzone benchmark above for direct comparison.
   ======================================================================= */

UBENCH_EX(Stdlib, AllocFree_paired_1024)
{
    void* ptrs[N_ALLOCS];

    UBENCH_DO_BENCHMARK()
    {
        for (int i = 0; i < N_ALLOCS; i++)
            ptrs[i] = malloc(ALLOC_SIZE);
        for (int i = 0; i < N_ALLOCS; i++)
            free(ptrs[i]);
        UBENCH_DO_NOTHING(ptrs);
    }
}

UBENCH_EX(Stdlib, AllocFree_single_64)
{
    /* Warm up */
    for (int w = 0; w < N_WARMUP; w++)
    {
        void* p = malloc(ALLOC_SIZE);
        free(p);
    }

    UBENCH_DO_BENCHMARK()
    {
        for (int j = 0; j < N_BATCH; j++)
        {
            void* p = malloc(ALLOC_SIZE);
            UBENCH_DO_NOTHING(p);
            free(p);
        }
    }
}

UBENCH_EX(Stdlib, Realloc_grow_64_to_128)
{
    /* Warm up */
    for (int w = 0; w < N_WARMUP; w++)
    {
        void* p = malloc(ALLOC_SIZE);
        void* q = realloc(p, ALLOC_SIZE * 2);
        free(q);
    }

    UBENCH_DO_BENCHMARK()
    {
        for (int j = 0; j < N_BATCH; j++)
        {
            void* p = malloc(ALLOC_SIZE);
            void* q = realloc(p, ALLOC_SIZE * 2);
            UBENCH_DO_NOTHING(q);
            free(q);
        }
    }
}

UBENCH_EX(Stdlib, Realloc_shrink_128_to_64)
{
    /* Warm up */
    for (int w = 0; w < N_WARMUP; w++)
    {
        void* p = malloc(ALLOC_SIZE * 2);
        void* q = realloc(p, ALLOC_SIZE);
        free(q);
    }

    UBENCH_DO_BENCHMARK()
    {
        for (int j = 0; j < N_BATCH; j++)
        {
            void* p = malloc(ALLOC_SIZE * 2);
            void* q = realloc(p, ALLOC_SIZE);
            UBENCH_DO_NOTHING(q);
            free(q);
        }
    }
}

UBENCH_EX(Stdlib, Fragmented_fill_holes_512)
{
    void* ptrs[N_ALLOCS];

    /* Warm up: run the same pattern several times to stabilise the
       stdlib heap's internal free-list state before the timed measurement. */
    for (int pass = 0; pass < 5; pass++)
    {
        for (int i = 0; i < N_ALLOCS; i++) ptrs[i] = malloc(ALLOC_SIZE);
        for (int i = 0; i < N_ALLOCS; i += 2) { free(ptrs[i]); ptrs[i] = NULL; }
        for (int i = 0; i < N_ALLOCS; i += 2) ptrs[i] = malloc(ALLOC_SIZE);
        for (int i = 0; i < N_ALLOCS; i++) free(ptrs[i]);
    }

    UBENCH_DO_BENCHMARK()
    {
        for (int i = 0; i < N_ALLOCS; i++)
            ptrs[i] = malloc(ALLOC_SIZE);

        for (int i = 0; i < N_ALLOCS; i += 2)
        {
            free(ptrs[i]);
            ptrs[i] = NULL;
        }

        for (int i = 0; i < N_ALLOCS; i += 2)
            ptrs[i] = malloc(ALLOC_SIZE);

        for (int i = 0; i < N_ALLOCS; i++)
            free(ptrs[i]);

        UBENCH_DO_NOTHING(ptrs);
    }
}

UBENCH_EX(Stdlib, Mixed_alloc_realloc_free)
{
    void* ptrs[POOL];

    UBENCH_DO_BENCHMARK()
    {
        for (int i = 0; i < POOL; i++)
            ptrs[i] = malloc(ALLOC_SIZE);

        for (int i = 0; i < POOL; i += 4)
            ptrs[i] = realloc(ptrs[i], ALLOC_SIZE * 2);

        for (int i = 0; i < POOL / 2; i++)
        {
            free(ptrs[i]);
            ptrs[i] = NULL;
        }

        for (int i = 0; i < POOL / 2; i++)
            ptrs[i] = malloc(ALLOC_SIZE);

        for (int i = 0; i < POOL; i++)
            free(ptrs[i]);

        UBENCH_DO_NOTHING(ptrs);
    }
}

/* =========================================================================
   Entry point
   ======================================================================= */

UBENCH_MAIN()
