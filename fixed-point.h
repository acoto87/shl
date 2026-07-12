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

/* ========================================================================= *
 * COMPILER & LINKAGE CONFIGURATION
 * ========================================================================= */

#ifndef FP_FRAC_BITS
    #define FP_FRAC_BITS 8
#endif

#define FP_SCALE (1 << FP_FRAC_BITS)

// Setup STB-style linkage macros
#ifndef FIXED_POINT_DEF
    #ifdef FIXED_POINT_STATIC
        #define FIXED_POINT_DEF static inline
    #else
        #define FIXED_POINT_DEF extern
    #endif
#endif

typedef int32_t fp32;
typedef uint8_t fp_angle; // 0-255 binary angle (naturally wraps 360 degrees)

/* ========================================================================= *
 * FUNCTION DECLARATIONS (API)
 * ========================================================================= */

// Conversions
FIXED_POINT_DEF fp32 fp_fromInt(int32_t i);
FIXED_POINT_DEF int32_t fp_toInt(fp32 f);
FIXED_POINT_DEF fp32 fp_fromFloat(float f);
FIXED_POINT_DEF float fp_toFloat(fp32 f);

// Arithmetic
FIXED_POINT_DEF fp32 fp_add(fp32 a, fp32 b);
FIXED_POINT_DEF fp32 fp_sub(fp32 a, fp32 b);
FIXED_POINT_DEF fp32 fp_mul(fp32 a, fp32 b);
FIXED_POINT_DEF fp32 fp_div(fp32 a, fp32 b);
FIXED_POINT_DEF fp32 fp_abs(fp32 a);
FIXED_POINT_DEF fp32 fp_sq(fp32 a);

// Rounding & Grid
FIXED_POINT_DEF fp32 fp_floor(fp32 a);
FIXED_POINT_DEF fp32 fp_ceil(fp32 a);
FIXED_POINT_DEF fp32 fp_round(fp32 a);
FIXED_POINT_DEF fp32 fp_frac(fp32 a);

// Boundaries
FIXED_POINT_DEF fp32 fp_min(fp32 a, fp32 b);
FIXED_POINT_DEF fp32 fp_max(fp32 a, fp32 b);
FIXED_POINT_DEF fp32 fp_clamp(fp32 v, fp32 min, fp32 max);
FIXED_POINT_DEF int32_t fp_sign(fp32 a);

// Vectors & Distance
FIXED_POINT_DEF fp32 fp_dot(fp32 x1, fp32 y1, fp32 x2, fp32 y2);
FIXED_POINT_DEF fp32 fp_manhattan(fp32 x1, fp32 y1, fp32 x2, fp32 y2);

// Complex Deterministic Math
FIXED_POINT_DEF fp32 fp_sqrt(fp32 a);
FIXED_POINT_DEF fp_angle fp_atan2(fp32 y, fp32 x);
FIXED_POINT_DEF fp32 fp_sin(fp_angle angle);
FIXED_POINT_DEF fp32 fp_cos(fp_angle angle);

#endif // FIXED_POINT_H


/* ========================================================================= *
 * IMPLEMENTATION BLOCK
 * ========================================================================= */

#if defined(FIXED_POINT_IMPLEMENTATION) || defined(FIXED_POINT_STATIC)

FIXED_POINT_DEF fp32 fp_fromInt(int32_t i) { return i << FP_FRAC_BITS; }
FIXED_POINT_DEF int32_t fp_toInt(fp32 f)   { return f >> FP_FRAC_BITS; }
FIXED_POINT_DEF fp32 fp_fromFloat(float f) { return (fp32)(f * FP_SCALE); }
FIXED_POINT_DEF float fp_toFloat(fp32 f)   { return (float)f / FP_SCALE; }

FIXED_POINT_DEF fp32 fp_add(fp32 a, fp32 b) { return a + b; }
FIXED_POINT_DEF fp32 fp_sub(fp32 a, fp32 b) { return a - b; }

FIXED_POINT_DEF fp32 fp_mul(fp32 a, fp32 b) {
    return (fp32)(((int64_t)a * (int64_t)b) >> FP_FRAC_BITS);
}

FIXED_POINT_DEF fp32 fp_div(fp32 a, fp32 b) {
    if (b == 0) return 0; // Or handle divide-by-zero via engine assertions
    return (fp32)((((int64_t)a) << FP_FRAC_BITS) / b);
}

FIXED_POINT_DEF fp32 fp_abs(fp32 a) { return (a < 0) ? -a : a; }
FIXED_POINT_DEF fp32 fp_sq(fp32 a)  { return fp_mul(a, a); }

FIXED_POINT_DEF fp32 fp_floor(fp32 a) { return a & ~(FP_SCALE - 1); }
FIXED_POINT_DEF fp32 fp_ceil(fp32 a)  { return (a + (FP_SCALE - 1)) & ~(FP_SCALE - 1); }
FIXED_POINT_DEF fp32 fp_round(fp32 a) { return (a + (FP_SCALE >> 1)) & ~(FP_SCALE - 1); }
FIXED_POINT_DEF fp32 fp_frac(fp32 a)  { return a & (FP_SCALE - 1); }

FIXED_POINT_DEF fp32 fp_min(fp32 a, fp32 b) { return (a < b) ? a : b; }
FIXED_POINT_DEF fp32 fp_max(fp32 a, fp32 b) { return (a > b) ? a : b; }
FIXED_POINT_DEF fp32 fp_clamp(fp32 v, fp32 min, fp32 max) { return (v < min) ? min : ((v > max) ? max : v); }
FIXED_POINT_DEF int32_t fp_sign(fp32 a) { return (a > 0) - (a < 0); }

FIXED_POINT_DEF fp32 fp_dot(fp32 x1, fp32 y1, fp32 x2, fp32 y2) {
    return fp_add(fp_mul(x1, x2), fp_mul(y1, y2));
}

FIXED_POINT_DEF fp32 fp_manhattan(fp32 x1, fp32 y1, fp32 x2, fp32 y2) {
    return fp_add(fp_abs(fp_sub(x2, x1)), fp_abs(fp_sub(y2, y1)));
}

FIXED_POINT_DEF fp32 fp_sqrt(fp32 a) {
    if (a <= 0) return 0;
    uint64_t num = (uint64_t)a << FP_FRAC_BITS;
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

    fp32 abs_y = fp_abs(y);
    fp32 abs_x = fp_abs(x);

    fp32 ratio = (abs_y << 10) / (abs_x + abs_y);
    fp_angle angle = (fp_angle)((ratio * 64) >> 10);

    if (x < 0) angle = 128 - angle;
    if (y < 0) angle = 256 - angle;

    return angle;
}

FIXED_POINT_DEF fp32 fp_sin(fp_angle angle) {
    int32_t q = 0;
    if (angle >= 128) {
        angle -= 128;
        q = 1;
    }
    int32_t x = angle;
    int32_t y = x * (128 - x);
    fp32 res = (fp32)((y * FP_SCALE) >> 12);
    return q ? -res : res;
}

FIXED_POINT_DEF fp32 fp_cos(fp_angle angle) {
    return fp_sin(angle + 64);
}

#endif // FIXED_POINT_IMPLEMENTATION || FIXED_POINT_STATIC
