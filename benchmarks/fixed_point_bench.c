/*
    benchmarks/fixed_point_bench.c — benchmarks for fixed_point.h using ubench.h

    Covers every public API function plus several integration scenarios that
    reflect real RTS game-engine hot paths (physics update, pathfinding cost
    accumulation, angle→direction look-ups).

    Build (GCC/Clang, from repo root):
        gcc -std=c11 -O2 -I. -Ibenchmarks benchmarks/fixed_point_bench.c -o build/bench/fixed_point_bench -lm

    Build (MSVC, from repo root):
        cl /std:c11 /O2 /I. /Ibenchmarks benchmarks/fixed_point_bench.c /Fe:build/bench/fixed_point_bench.exe
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

#define FIXED_POINT_STATIC
#include "../fixed_point.h"
#include "ubench.h"

/* -------------------------------------------------------------------------
   Constants
   ---------------------------------------------------------------------- */

enum
{
    N_SMALL  = 256,    /* fast, sub-nanosecond ops — batched per sample  */
    N_MEDIUM = 1024,   /* ops that take a few ns each                    */
    N_LARGE  = 16384   /* used for the integration / hot-path scenarios  */
};

/* -------------------------------------------------------------------------
   Helpers — deterministic pseudo-random data
   ---------------------------------------------------------------------- */

/* A simple 32-bit LCG producing non-zero fixed-point values. */
static uint32_t lcg_state = 0xdeadbeef;

static uint32_t lcg_next(void)
{
    lcg_state = lcg_state * 1664525u + 1013904223u;
    return lcg_state;
}

/* Return a pseudo-random fp32 in the range (0, range_int]. */
static fp32 rand_fp(int32_t range_int)
{
    uint32_t r = lcg_next();
    return (fp32)((r >> 1) % ((uint32_t)range_int * FP_SCALE)) + 1;
}

/* Pre-built operand arrays so data-generation is excluded from timing. */
static fp32 g_a[N_LARGE];
static fp32 g_b[N_LARGE];

static void fill_operands(int n)
{
    lcg_state = 0xdeadbeef;
    for (int i = 0; i < n; i++)
    {
        g_a[i] = rand_fp(100);
        g_b[i] = rand_fp(100);
    }
}

/* =========================================================================
   Conversions
   ========================================================================= */

UBENCH(Conversion, FromInt)
{
    volatile fp32 sink = 0;
    for (int i = 0; i < N_MEDIUM; i++)
        sink = fp_fromInt((int32_t)i);
    (void)sink;
}

UBENCH(Conversion, ToInt)
{
    fp32 v = fp_fromFloat(3.14f);
    volatile int32_t sink = 0;
    for (int i = 0; i < N_MEDIUM; i++)
        sink = fp_toInt(v);
    (void)sink;
}

UBENCH(Conversion, FromFloat)
{
    volatile fp32 sink = 0;
    float f = 3.14159f;
    for (int i = 0; i < N_MEDIUM; i++)
        sink = fp_fromFloat(f);
    (void)sink;
}

UBENCH(Conversion, ToFloat)
{
    fp32 v = fp_fromFloat(3.14f);
    volatile float sink = 0.0f;
    for (int i = 0; i < N_MEDIUM; i++)
        sink = fp_toFloat(v);
    (void)sink;
}

/* =========================================================================
   Arithmetic — Add / Sub / Mul / Div
   ========================================================================= */

UBENCH(Arithmetic, Add)
{
    fill_operands(N_MEDIUM);
    volatile fp32 acc = 0;
    for (int i = 0; i < N_MEDIUM; i++)
        acc = fp_add(g_a[i], g_b[i]);
    (void)acc;
}

UBENCH(Arithmetic, Sub)
{
    fill_operands(N_MEDIUM);
    volatile fp32 acc = 0;
    for (int i = 0; i < N_MEDIUM; i++)
        acc = fp_sub(g_a[i], g_b[i]);
    (void)acc;
}

UBENCH(Arithmetic, Mul)
{
    fill_operands(N_MEDIUM);
    volatile fp32 acc = 0;
    for (int i = 0; i < N_MEDIUM; i++)
        acc = fp_mul(g_a[i], g_b[i]);
    (void)acc;
}

UBENCH(Arithmetic, Div)
{
    fill_operands(N_MEDIUM);
    volatile fp32 acc = 0;
    for (int i = 0; i < N_MEDIUM; i++)
        acc = fp_div(g_a[i], g_b[i]);
    (void)acc;
}

UBENCH(Arithmetic, Abs)
{
    fill_operands(N_MEDIUM);
    volatile fp32 acc = 0;
    for (int i = 0; i < N_MEDIUM; i++)
        acc = fp_abs(g_a[i]);
    (void)acc;
}

UBENCH(Arithmetic, Sq)
{
    fill_operands(N_MEDIUM);
    volatile fp32 acc = 0;
    for (int i = 0; i < N_MEDIUM; i++)
        acc = fp_sq(g_a[i]);
    (void)acc;
}

/* =========================================================================
   Rounding
   ========================================================================= */

UBENCH(Rounding, Floor)
{
    fill_operands(N_MEDIUM);
    volatile fp32 acc = 0;
    for (int i = 0; i < N_MEDIUM; i++)
        acc = fp_floor(g_a[i]);
    (void)acc;
}

UBENCH(Rounding, Ceil)
{
    fill_operands(N_MEDIUM);
    volatile fp32 acc = 0;
    for (int i = 0; i < N_MEDIUM; i++)
        acc = fp_ceil(g_a[i]);
    (void)acc;
}

UBENCH(Rounding, Round)
{
    fill_operands(N_MEDIUM);
    volatile fp32 acc = 0;
    for (int i = 0; i < N_MEDIUM; i++)
        acc = fp_round(g_a[i]);
    (void)acc;
}

UBENCH(Rounding, Frac)
{
    fill_operands(N_MEDIUM);
    volatile fp32 acc = 0;
    for (int i = 0; i < N_MEDIUM; i++)
        acc = fp_frac(g_a[i]);
    (void)acc;
}

/* =========================================================================
   Boundaries
   ========================================================================= */

UBENCH(Boundary, Min)
{
    fill_operands(N_MEDIUM);
    volatile fp32 acc = 0;
    for (int i = 0; i < N_MEDIUM; i++)
        acc = fp_min(g_a[i], g_b[i]);
    (void)acc;
}

UBENCH(Boundary, Max)
{
    fill_operands(N_MEDIUM);
    volatile fp32 acc = 0;
    for (int i = 0; i < N_MEDIUM; i++)
        acc = fp_max(g_a[i], g_b[i]);
    (void)acc;
}

UBENCH(Boundary, Clamp)
{
    fill_operands(N_MEDIUM);
    fp32 lo = fp_fromInt(10);
    fp32 hi = fp_fromInt(90);
    volatile fp32 acc = 0;
    for (int i = 0; i < N_MEDIUM; i++)
        acc = fp_clamp(g_a[i], lo, hi);
    (void)acc;
}

UBENCH(Boundary, Sign)
{
    fill_operands(N_MEDIUM);
    volatile int32_t acc = 0;
    for (int i = 0; i < N_MEDIUM; i++)
        acc = fp_sign(g_a[i]);
    (void)acc;
}

/* =========================================================================
   Vectors
   ========================================================================= */

/* Fixture: pre-built 2-D vector pairs for dot/manhattan. */
struct VectorPairs
{
    fp32 x1[N_MEDIUM], y1[N_MEDIUM];
    fp32 x2[N_MEDIUM], y2[N_MEDIUM];
};

UBENCH_F_SETUP(VectorPairs)
{
    lcg_state = 0xcafebabe;
    for (int i = 0; i < N_MEDIUM; i++)
    {
        ubench_fixture->x1[i] = rand_fp(50);
        ubench_fixture->y1[i] = rand_fp(50);
        ubench_fixture->x2[i] = rand_fp(50);
        ubench_fixture->y2[i] = rand_fp(50);
    }
}

UBENCH_F_TEARDOWN(VectorPairs) { (void)ubench_fixture; }

UBENCH_F(VectorPairs, Dot)
{
    volatile fp32 acc = 0;
    for (int i = 0; i < N_MEDIUM; i++)
        acc = fp_dot(ubench_fixture->x1[i], ubench_fixture->y1[i],
                     ubench_fixture->x2[i], ubench_fixture->y2[i]);
    (void)acc;
}

UBENCH_F(VectorPairs, Manhattan)
{
    volatile fp32 acc = 0;
    for (int i = 0; i < N_MEDIUM; i++)
        acc = fp_manhattan(ubench_fixture->x1[i], ubench_fixture->y1[i],
                           ubench_fixture->x2[i], ubench_fixture->y2[i]);
    (void)acc;
}

/* =========================================================================
   Complex Math
   ========================================================================= */

UBENCH(ComplexMath, Sqrt)
{
    fill_operands(N_MEDIUM);
    volatile fp32 acc = 0;
    for (int i = 0; i < N_MEDIUM; i++)
        acc = fp_sqrt(g_a[i]);
    (void)acc;
}

/* Benchmark sqrt on numbers that hit the large-bit inner loop. */
UBENCH(ComplexMath, SqrtLargeValues)
{
    /* Values near INT32_MAX / FP_SCALE to exercise all 32 bit-pairs. */
    fp32 large = fp_fromInt(10000);
    volatile fp32 acc = 0;
    for (int i = 0; i < N_MEDIUM; i++)
        acc = fp_sqrt(large);
    (void)acc;
}

UBENCH(ComplexMath, Atan2)
{
    fill_operands(N_MEDIUM);
    volatile fp_angle acc = 0;
    for (int i = 0; i < N_MEDIUM; i++)
        acc = fp_atan2(g_a[i], g_b[i]);
    (void)acc;
}

UBENCH(ComplexMath, Sin)
{
    volatile fp32 acc = 0;
    for (int angle = 0; angle < N_MEDIUM; angle++)
        acc = fp_sin((fp_angle)(angle & 0xFF));
    (void)acc;
}

UBENCH(ComplexMath, Cos)
{
    volatile fp32 acc = 0;
    for (int angle = 0; angle < N_MEDIUM; angle++)
        acc = fp_cos((fp_angle)(angle & 0xFF));
    (void)acc;
}

/* =========================================================================
   Integration — realistic game-engine hot paths
   ========================================================================= */

/* Hot path 1 — Physics update
   Simulate N_LARGE particles: position += velocity * dt each step.
   Measures add + mul throughput in a realistic sequential pattern. */
UBENCH_EX(Integration, PhysicsUpdate)
{
    static fp32 pos_x[N_LARGE], pos_y[N_LARGE];
    static fp32 vel_x[N_LARGE], vel_y[N_LARGE];

    fp32 dt = fp_fromFloat(1.0f / 60.0f);

    lcg_state = 0x01234567;
    for (int i = 0; i < N_LARGE; i++)
    {
        pos_x[i] = rand_fp(500);
        pos_y[i] = rand_fp(500);
        vel_x[i] = rand_fp(10) - fp_fromInt(5);
        vel_y[i] = rand_fp(10) - fp_fromInt(5);
    }

    UBENCH_DO_BENCHMARK()
    {
        for (int i = 0; i < N_LARGE; i++)
        {
            pos_x[i] = fp_add(pos_x[i], fp_mul(vel_x[i], dt));
            pos_y[i] = fp_add(pos_y[i], fp_mul(vel_y[i], dt));
        }
        UBENCH_DO_NOTHING(pos_x);
    }
}

/* Hot path 2 — Pathfinding cost accumulation
   Manhattan distance is the classic A* heuristic for grid maps.
   Measures the cost of computing N_LARGE heuristic queries. */
UBENCH_EX(Integration, PathfindingHeuristic)
{
    static fp32 sx[N_LARGE], sy[N_LARGE];
    static fp32 gx[N_LARGE], gy[N_LARGE];

    lcg_state = 0xf00dcafe;
    for (int i = 0; i < N_LARGE; i++)
    {
        sx[i] = rand_fp(256);
        sy[i] = rand_fp(256);
        gx[i] = rand_fp(256);
        gy[i] = rand_fp(256);
    }

    volatile fp32 total = 0;
    UBENCH_DO_BENCHMARK()
    {
        total = 0;
        for (int i = 0; i < N_LARGE; i++)
            total = fp_add(total, fp_manhattan(sx[i], sy[i], gx[i], gy[i]));
        UBENCH_DO_NOTHING((void*)&total);
    }
    (void)total;
}

/* Hot path 3 — Angle → direction vector look-up
   RTS units often convert a binary angle to a (dx, dy) direction each frame.
   Measures sin + cos per slot. */
UBENCH_EX(Integration, AngleToDirection)
{
    static fp_angle angles[N_LARGE];

    lcg_state = 0xabcd1234;
    for (int i = 0; i < N_LARGE; i++)
        angles[i] = (fp_angle)(lcg_next() & 0xFF);

    volatile fp32 dx = 0, dy = 0;
    UBENCH_DO_BENCHMARK()
    {
        for (int i = 0; i < N_LARGE; i++)
        {
            dx = fp_cos(angles[i]);
            dy = fp_sin(angles[i]);
        }
        UBENCH_DO_NOTHING((void*)&dx);
    }
    (void)dx; (void)dy;
}

/* Hot path 4 — Distance comparison
   Avoidance AI compares squared Euclidean distances to a threat radius without
   paying for a sqrt.  Measures mul + add + comparison overhead. */
UBENCH_EX(Integration, SquaredDistanceFilter)
{
    static fp32 ex[N_LARGE], ey[N_LARGE]; /* entity positions  */
    static fp32 tx[N_LARGE], ty[N_LARGE]; /* threat positions  */

    fp32 radius_sq = fp_sq(fp_fromInt(10));

    lcg_state = 0x98765432;
    for (int i = 0; i < N_LARGE; i++)
    {
        ex[i] = rand_fp(200); ey[i] = rand_fp(200);
        tx[i] = rand_fp(200); ty[i] = rand_fp(200);
    }

    volatile int32_t inside = 0;
    UBENCH_DO_BENCHMARK()
    {
        inside = 0;
        for (int i = 0; i < N_LARGE; i++)
        {
            fp32 dx = fp_sub(ex[i], tx[i]);
            fp32 dy = fp_sub(ey[i], ty[i]);
            fp32 d2 = fp_add(fp_sq(dx), fp_sq(dy));
            if (d2 <= radius_sq) inside++;
        }
        UBENCH_DO_NOTHING((void*)&inside);
    }
    (void)inside;
}

/* Hot path 5 — Clamped velocity magnitude
   Normalise a 2-D velocity vector to a maximum speed: uses sqrt, div, mul,
   clamp.  Representative of every frame spent on unit movement. */
UBENCH_EX(Integration, ClampedVelocityMagnitude)
{
    static fp32 vx[N_MEDIUM], vy[N_MEDIUM];
    fp32 max_speed = fp_fromInt(5);

    lcg_state = 0x11223344;
    for (int i = 0; i < N_MEDIUM; i++)
    {
        vx[i] = rand_fp(10) - fp_fromInt(5);
        vy[i] = rand_fp(10) - fp_fromInt(5);
    }

    volatile fp32 ax = 0, ay = 0;
    UBENCH_DO_BENCHMARK()
    {
        for (int i = 0; i < N_MEDIUM; i++)
        {
            fp32 mag = fp_sqrt(fp_add(fp_sq(vx[i]), fp_sq(vy[i])));
            if (mag > max_speed && mag != 0)
            {
                fp32 scale = fp_div(max_speed, mag);
                ax = fp_mul(vx[i], scale);
                ay = fp_mul(vy[i], scale);
            }
            else
            {
                ax = vx[i];
                ay = vy[i];
            }
        }
        UBENCH_DO_NOTHING((void*)&ax);
    }
    (void)ax; (void)ay;
}

/* =========================================================================
   Entry point
   ========================================================================= */

UBENCH_MAIN();
