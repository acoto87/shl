/*
    fixed-point.h - acoto87 (acoto87@gmail.com)

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

    Deterministic fixed-point arithmetic library designed for use in RTS engines
    and other applications that require bit-exact, platform-independent math.

    PRECISION
    Define FP_FRAC_BITS before including this header to set the number of
    fractional bits (defaults to 8, giving a 24.8 fixed-point layout with a
    scale factor of 256).

    USAGE
    In exactly one C or C++ translation unit, define FIXED_POINT_IMPLEMENTATION
    before including this header to emit the function bodies:

        #define FIXED_POINT_IMPLEMENTATION
        #include "fixed-point.h"

    In all other files, include the header normally:

        #include "fixed-point.h"

    STATIC / INLINE MODE
    If you prefer zero-overhead inlining rather than a single translation unit,
    define FIXED_POINT_STATIC before including the header.  Every function will
    then be emitted as static inline at every include site.

    TYPES
    fp32      — 32-bit signed fixed-point value (int32_t internally).
    fp_angle  — 8-bit binary angle: 0 maps to 0°, 255 maps to ~359°.
                The type wraps naturally at 360°.

    KNOWN LIMITATIONS
    fp_sin / fp_cos use a fast parabolic approximation; maximum error is
    roughly ±0.056 (in fixed-point units scaled to [-1, 1]).
    fp_atan2 uses an octant-decomposed linear approximation; maximum angular
    error is about ±2 binary-angle units (~2.8°).
*/

#ifndef FIXED_POINT_H
#define FIXED_POINT_H

#include <stdint.h>
#include <stdbool.h>

/* ========================================================================= *
 * COMPILER & LINKAGE CONFIGURATION
 * ========================================================================= */

#ifndef FP_FRAC_BITS
    #define FP_FRAC_BITS 8
#endif

#if FP_FRAC_BITS < 1 || FP_FRAC_BITS > 30
    #error "FP_FRAC_BITS must be between 1 and 30"
#endif

#define FP_SCALE (INT32_C(1) << FP_FRAC_BITS)

#define FP_MAX_INTEGER (INT32_MAX / FP_SCALE)
#define FP_MIN_INTEGER (INT32_MIN / FP_SCALE)

// Setup STB-style linkage macros
#ifndef FIXED_POINT_DEF
    #ifdef FIXED_POINT_STATIC
        #define FIXED_POINT_DEF static inline
    #else
        #define FIXED_POINT_DEF extern
    #endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t fp32;
typedef uint8_t fp_angle; // 0-255 binary angle (naturally wraps 360 degrees)

/* ========================================================================= *
 * CONSTANTS DECLARATIONS
 * ========================================================================= */

#define FP_ZERO ((fp32)0)
#define FP_ONE  ((fp32)FP_SCALE)
#define FP_HALF ((fp32)(FP_SCALE / 2))

/* ========================================================================= *
 * FUNCTION DECLARATIONS (API)
 * ========================================================================= */

// Conversions
FIXED_POINT_DEF fp32 fp_fromRaw(int32_t raw);
FIXED_POINT_DEF int32_t fp_toRaw(fp32 x);
FIXED_POINT_DEF fp32 fp_fromRatio(int32_t num, int32_t den);

FIXED_POINT_DEF fp32 fp_fromInt(int32_t x);
FIXED_POINT_DEF int32_t fp_toInt(fp32 x);
FIXED_POINT_DEF fp32 fp_fromFloat(float x);
FIXED_POINT_DEF float fp_toFloat(fp32 x);

// Arithmetic
FIXED_POINT_DEF fp32 fp_add(fp32 a, fp32 b);
FIXED_POINT_DEF fp32 fp_sub(fp32 a, fp32 b);
FIXED_POINT_DEF fp32 fp_mul(fp32 a, fp32 b);
FIXED_POINT_DEF fp32 fp_div(fp32 a, fp32 b);
FIXED_POINT_DEF fp32 fp_abs(fp32 x);
FIXED_POINT_DEF fp32 fp_sq(fp32 x);

// Rounding & Grid
FIXED_POINT_DEF fp32 fp_floor(fp32 x);
FIXED_POINT_DEF fp32 fp_ceil(fp32 x);
FIXED_POINT_DEF fp32 fp_round(fp32 x);
FIXED_POINT_DEF fp32 fp_frac(fp32 x);

// Boundaries
FIXED_POINT_DEF fp32 fp_min(fp32 a, fp32 b);
FIXED_POINT_DEF fp32 fp_max(fp32 a, fp32 b);
FIXED_POINT_DEF fp32 fp_clamp(fp32 x, fp32 min, fp32 max);
FIXED_POINT_DEF int32_t fp_sign(fp32 a);

// Vectors & Distance
FIXED_POINT_DEF fp32 fp_dot(fp32 x1, fp32 y1, fp32 x2, fp32 y2);
FIXED_POINT_DEF fp32 fp_manhattan(fp32 x1, fp32 y1, fp32 x2, fp32 y2);

// Complex Deterministic Math
FIXED_POINT_DEF fp32 fp_sqrt(fp32 x);
FIXED_POINT_DEF fp_angle fp_atan2(fp32 y, fp32 x);
FIXED_POINT_DEF fp32 fp_sin(fp_angle angle);
FIXED_POINT_DEF fp32 fp_cos(fp_angle angle);

#ifdef __cplusplus
}
#endif

/* ========================================================================= *
 * IMPLEMENTATION BLOCK
 * ========================================================================= */

#if defined(FIXED_POINT_IMPLEMENTATION) || defined(FIXED_POINT_STATIC)

#define FP_MAX_WHOLE_RAW ((fp32)((INT32_MAX / FP_SCALE) * FP_SCALE))
#define FP_MIN_WHOLE_RAW ((fp32)((INT32_MIN / FP_SCALE) * FP_SCALE))

static inline fp32 fp__sat_i64(int64_t x)
{
    if (x > INT32_MAX) return INT32_MAX;
    if (x < INT32_MIN) return INT32_MIN;
    return (fp32)x;
}

static inline uint64_t fp__uabs_i64(int64_t x)
{
    return x < 0 ? UINT64_C(0) - (uint64_t)x : (uint64_t)x;
}

/*
 * Divide and round to nearest, with exact ties rounded away from zero.
 * Precondition: denominator != 0.
 */
static inline int64_t fp__div_round_nearest(int64_t num, int64_t den)
{
    int64_t quotient = num / den;
    int64_t remainder = num % den;

    uint64_t abs_remainder = fp__uabs_i64(remainder);
    uint64_t abs_denominator = fp__uabs_i64(den);

    /*
     * Equivalent to:
     *     2 * abs_remainder >= abs_denominator
     * but without risking overflow from multiplication by 2.
     */
    if (abs_remainder >= abs_denominator - abs_remainder) {
        bool result_is_negative = (num < 0) != (den < 0);
        quotient += result_is_negative ? -1 : 1;
    }

    return quotient;
}

static inline fp32 fp__clamp_safe(int64_t x)
{
    if (x > FP_MAX_WHOLE_RAW) return FP_MAX_WHOLE_RAW;
    if (x < FP_MIN_WHOLE_RAW) return FP_MIN_WHOLE_RAW;
    return (fp32)x;
}

FIXED_POINT_DEF fp32 fp_fromRaw(int32_t raw) { return raw; }
FIXED_POINT_DEF int32_t fp_toRaw(fp32 x) { return x; }
FIXED_POINT_DEF fp32 fp_fromRatio(int32_t num, int32_t den)
{
    if (den == 0) return 0;
    int64_t scaled = (int64_t)num * (int64_t)FP_SCALE;
    int64_t result = fp__div_round_nearest(scaled, den);
    return fp__sat_i64(result);
}

FIXED_POINT_DEF fp32 fp_fromInt(int32_t x) { return fp__sat_i64((int64_t)x * (int64_t)FP_SCALE); }
FIXED_POINT_DEF int32_t fp_toInt(fp32 x) { return x / FP_SCALE; }
FIXED_POINT_DEF fp32 fp_fromFloat(float x)
{
    double scaled = (double)x * (double)FP_SCALE;

    /* NaN */
    if (scaled != scaled) return 0;

    /*
     * Include half an ULP because conversion rounds to nearest,
     * ties away from zero.
     */
    if (scaled >= (double)INT32_MAX + 0.5) return INT32_MAX;
    if (scaled <= (double)INT32_MIN - 0.5) return INT32_MIN;
    if (scaled >= 0.0) return (fp32)(scaled + 0.5);
    return (fp32)(scaled - 0.5);
}
FIXED_POINT_DEF float fp_toFloat(fp32 x) { return (float)x / FP_SCALE; }

FIXED_POINT_DEF fp32 fp_add(fp32 a, fp32 b) { return fp__sat_i64((int64_t)a + (int64_t)b); }
FIXED_POINT_DEF fp32 fp_sub(fp32 a, fp32 b) { return fp__sat_i64((int64_t)a - (int64_t)b); }
FIXED_POINT_DEF fp32 fp_mul(fp32 a, fp32 b) {
    int64_t product = (int64_t)a * (int64_t)b;
    int64_t scaled = fp__div_round_nearest(product, FP_SCALE);
    return fp__sat_i64(scaled);
}
FIXED_POINT_DEF fp32 fp_div(fp32 a, fp32 b) {
    if (b == 0) return 0;
    int64_t numerator = (int64_t)a * (int64_t)FP_SCALE; // Multiplication is used instead of left-shifting a potentially negative signed value.
    int64_t result = fp__div_round_nearest(numerator, b);
    return fp__sat_i64(result);
}

FIXED_POINT_DEF fp32 fp_abs(fp32 x) {
    if (x == INT32_MIN) return INT32_MAX;
    return x < 0 ? -x : x;
}
FIXED_POINT_DEF fp32 fp_sq(fp32 x)  { return fp_mul(x, x); }

FIXED_POINT_DEF fp32 fp_floor(fp32 x)
{
    fp32 remainder = x % FP_SCALE;
    if (remainder == 0) return x;
    int64_t result = (int64_t)x - remainder;
    if (x < 0) result -= FP_SCALE;
    return fp__clamp_safe(result);
}

FIXED_POINT_DEF fp32 fp_ceil(fp32 x)
{
    fp32 remainder = x % FP_SCALE;
    if (remainder == 0) return x;
    int64_t result = (int64_t)x - remainder;
    if (x >= 0) result += FP_SCALE;
    return fp__clamp_safe(result);
}

FIXED_POINT_DEF fp32 fp_round(fp32 x)
{
    int64_t whole = fp__div_round_nearest(x, FP_SCALE);
    int64_t result = whole * (int64_t)FP_SCALE;
    return fp__clamp_safe(result);
}

FIXED_POINT_DEF fp32 fp_frac(fp32 x)
{
    fp32 remainder = x % FP_SCALE;
    if (remainder < 0) remainder += FP_SCALE; // Convert C's signed remainder into the mathematical fractional interval [0, 1).
    return remainder;
}

FIXED_POINT_DEF fp32 fp_min(fp32 a, fp32 b) { return (a < b) ? a : b; }
FIXED_POINT_DEF fp32 fp_max(fp32 a, fp32 b) { return (a > b) ? a : b; }
FIXED_POINT_DEF fp32 fp_clamp(fp32 x, fp32 min, fp32 max) { return (x < min) ? min : ((x > max) ? max : x); }
FIXED_POINT_DEF int32_t fp_sign(fp32 x) { return (x > 0) - (x < 0); }

FIXED_POINT_DEF fp32 fp_dot(fp32 x1, fp32 y1, fp32 x2, fp32 y2) {
    int64_t xx = (int64_t)x1 * (int64_t)x2;
    int64_t yy = (int64_t)y1 * (int64_t)y2;

     /*
     * Decompose each product:
     *  product = quotient * FP_SCALE + remainder
     */
    int64_t qx = xx / FP_SCALE;
    int64_t rx = xx % FP_SCALE;

    int64_t qy = yy / FP_SCALE;
    int64_t ry = yy % FP_SCALE;

    /*
     * For FP_FRAC_BITS >= 1, qx + qy fits int64_t.
     * The remainders are each smaller than FP_SCALE.
     */
    int64_t s = qx + qy;
    int64_t r = rx + ry;
    int64_t rr = fp__div_round_nearest(r, FP_SCALE);
    return fp__sat_i64(s + rr);
}

FIXED_POINT_DEF fp32 fp_manhattan(fp32 x1, fp32 y1, fp32 x2, fp32 y2) {
    int64_t dx = (int64_t)x2 - (int64_t)x1;
    int64_t dy = (int64_t)y2 - (int64_t)y1;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    return fp__sat_i64(dx + dy);
}

FIXED_POINT_DEF fp32 fp_sqrt(fp32 x) {
    if (x <= 0) return 0;
    uint64_t num = (uint64_t)x << FP_FRAC_BITS;
    uint64_t res = 0;
    uint64_t bit = (uint64_t)1 << 62;

    while (bit > num) { bit >>= 2; }
    while (bit != 0) {
        if (num >= res + bit) {
            num -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return (fp32)res;
}

FIXED_POINT_DEF fp_angle fp_atan2(fp32 y, fp32 x) {
    if (x == 0 && y == 0) return 0;

    uint64_t ax = fp__uabs_i64((int64_t)x);
    uint64_t ay = fp__uabs_i64((int64_t)y);
    uint64_t sum = ax + ay;

    /* Linear approximation, rounded rather than truncated. */
    uint32_t angle = (uint32_t)((ay * UINT64_C(64) + sum / 2) / sum);
    if (x < 0) angle = 128u - angle;
    if (y < 0) angle = 256u - angle;
    return (fp_angle)angle;
}

FIXED_POINT_DEF fp32 fp_sin(fp_angle angle) {
    int32_t q = 0;
    if (angle >= 128) {
        angle -= 128;
        q = 1;
    }
    int32_t x = angle;
    int32_t y = x * (128 - x);
    int64_t scaled = (int64_t)y * (int64_t)FP_SCALE;
    fp32 result = (fp32)(scaled / INT64_C(4096));
    return q ? -result : result;
}

FIXED_POINT_DEF fp32 fp_cos(fp_angle angle) {
    return fp_sin(angle + 64);
}

#endif // FIXED_POINT_IMPLEMENTATION || FIXED_POINT_STATIC

#endif // FIXED_POINT_H
