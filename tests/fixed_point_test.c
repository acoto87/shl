#define _GNU_SOURCE

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define FIXED_POINT_IMPLEMENTATION
#include "../fixed-point.h"
#include "test_common.h"

/* -------------------------------------------------------------------------
   Helpers
   ---------------------------------------------------------------------- */

/* Maximum absolute error accepted when comparing a fixed-point result to
   a float reference.  One ULP in 24.8 is 1/256 ≈ 0.0039. */
#define FP_TOL (2.0f / FP_SCALE) /* ±2 ULPs */

#define TEST_FP_MAX_WHOLE_RAW ((fp32)((INT32_MAX / FP_SCALE) * FP_SCALE))
#define TEST_FP_MIN_WHOLE_RAW ((fp32)((INT32_MIN / FP_SCALE) * FP_SCALE))

/* Convert a float to fp32, run the operation, convert back for comparison. */
static float fp32_to_f(fp32 v) { return fp_toFloat(v); }

/* =========================================================================
   Conversions
   ========================================================================= */

void test_fromInt_zero(void)
{
    TEST_ASSERT_EQUAL_INT32(0, fp_fromInt(0));
}

void test_fromInt_positive(void)
{
    TEST_ASSERT_EQUAL_INT32(1 * FP_SCALE, fp_fromInt(1));
    TEST_ASSERT_EQUAL_INT32(100 * FP_SCALE, fp_fromInt(100));
}

void test_fromInt_negative(void)
{
    TEST_ASSERT_EQUAL_INT32(-1 * FP_SCALE, fp_fromInt(-1));
    TEST_ASSERT_EQUAL_INT32(-42 * FP_SCALE, fp_fromInt(-42));
}

void test_toInt_positive(void)
{
    TEST_ASSERT_EQUAL_INT32(3, fp_toInt(fp_fromInt(3)));
    TEST_ASSERT_EQUAL_INT32(0, fp_toInt(fp_fromFloat(0.9f)));
}

void test_toInt_negative(void)
{
    TEST_ASSERT_EQUAL_INT32(-3, fp_toInt(fp_fromInt(-3)));
}

void test_fromFloat_zero(void)
{
    TEST_ASSERT_EQUAL_INT32(0, fp_fromFloat(0.0f));
}

void test_fromFloat_positive_fraction(void)
{
    fp32 half = fp_fromFloat(0.5f);
    TEST_ASSERT_EQUAL_INT32(FP_SCALE / 2, half);
}

void test_fromFloat_negative_fraction(void)
{
    fp32 v = fp_fromFloat(-0.5f);
    TEST_ASSERT_EQUAL_INT32(-(FP_SCALE / 2), v);
}

void test_toFloat_round_trip(void)
{
    float values[] = { 0.0f, 1.0f, -1.0f, 3.14f, -2.718f, 0.5f, -0.5f };
    for (int i = 0; i < (int)(sizeof(values) / sizeof(values[0])); i++)
    {
        float result = fp_toFloat(fp_fromFloat(values[i]));
        TEST_ASSERT_FLOAT_WITHIN(FP_TOL, values[i], result);
    }
}

void test_toInt_truncates_negative_fraction_toward_zero(void)
{
    TEST_ASSERT_EQUAL_INT32(0, fp_toInt(-1));
    TEST_ASSERT_EQUAL_INT32(0, fp_toInt(-(FP_SCALE - 1)));
    TEST_ASSERT_EQUAL_INT32(-1, fp_toInt(-(FP_SCALE + 1)));
}

void test_fromRatio_rounds_nearest_away_from_zero(void)
{
    TEST_ASSERT_EQUAL_INT32(26, fp_fromRatio(1, 10));
    TEST_ASSERT_EQUAL_INT32(-26, fp_fromRatio(-1, 10));
    TEST_ASSERT_EQUAL_INT32(FP_SCALE / 3, fp_fromRatio(1, 3));
}

void test_raw_conversion_is_exact(void)
{
    fp32 values[] = {INT32_MIN, -1, 0, 1, INT32_MAX };
    size_t count = sizeof(values) / sizeof(values[0]);

    for (size_t i = 0; i < count; ++i) {
        TEST_ASSERT_EQUAL_INT32(values[i], fp_toRaw(fp_fromRaw(values[i])));
    }
}

void test_fromFloat_half_ulp_ties_away_from_zero(void)
{
    float half_ulp = ldexpf(1.0f, -(FP_FRAC_BITS + 1));
    TEST_ASSERT_EQUAL_INT32(1, fp_fromFloat(half_ulp));
    TEST_ASSERT_EQUAL_INT32(-1, fp_fromFloat(-half_ulp));
}

void test_fromFloat_special_values(void)
{
    TEST_ASSERT_EQUAL_INT32(0, fp_fromFloat(NAN));
    TEST_ASSERT_EQUAL_INT32(INT32_MAX, fp_fromFloat(INFINITY));
    TEST_ASSERT_EQUAL_INT32(INT32_MIN, fp_fromFloat(-INFINITY));
}

/* =========================================================================
   Arithmetic — add / sub
   ========================================================================= */

void test_add_positive(void)
{
    fp32 a = fp_fromFloat(1.5f);
    fp32 b = fp_fromFloat(2.5f);
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, 4.0f, fp32_to_f(fp_add(a, b)));
}

void test_add_negative(void)
{
    fp32 a = fp_fromFloat(-1.5f);
    fp32 b = fp_fromFloat(-2.5f);
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, -4.0f, fp32_to_f(fp_add(a, b)));
}

void test_add_mixed_signs(void)
{
    fp32 a = fp_fromFloat(3.0f);
    fp32 b = fp_fromFloat(-1.0f);
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, 2.0f, fp32_to_f(fp_add(a, b)));
}

void test_sub_positive_result(void)
{
    fp32 a = fp_fromFloat(5.0f);
    fp32 b = fp_fromFloat(3.0f);
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, 2.0f, fp32_to_f(fp_sub(a, b)));
}

void test_sub_negative_result(void)
{
    fp32 a = fp_fromFloat(3.0f);
    fp32 b = fp_fromFloat(5.0f);
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, -2.0f, fp32_to_f(fp_sub(a, b)));
}

void test_add_is_commutative(void)
{
    fp32 a = fp_fromFloat(1.25f);
    fp32 b = fp_fromFloat(2.75f);
    TEST_ASSERT_EQUAL_INT32(fp_add(a, b), fp_add(b, a));
}

/* =========================================================================
   Arithmetic — mul / div
   ========================================================================= */

void test_mul_two_integers(void)
{
    fp32 a = fp_fromInt(3);
    fp32 b = fp_fromInt(4);
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, 12.0f, fp32_to_f(fp_mul(a, b)));
}

void test_mul_fraction(void)
{
    fp32 a = fp_fromFloat(0.5f);
    fp32 b = fp_fromFloat(0.5f);
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, 0.25f, fp32_to_f(fp_mul(a, b)));
}

void test_mul_negative(void)
{
    fp32 a = fp_fromFloat(-2.0f);
    fp32 b = fp_fromFloat(3.0f);
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, -6.0f, fp32_to_f(fp_mul(a, b)));
}

void test_mul_both_negative(void)
{
    fp32 a = fp_fromFloat(-2.0f);
    fp32 b = fp_fromFloat(-3.0f);
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, 6.0f, fp32_to_f(fp_mul(a, b)));
}

void test_mul_by_zero(void)
{
    fp32 a = fp_fromFloat(123.456f);
    TEST_ASSERT_EQUAL_INT32(0, fp_mul(a, fp_fromInt(0)));
}

void test_mul_by_one(void)
{
    fp32 a = fp_fromFloat(7.5f);
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, 7.5f, fp32_to_f(fp_mul(a, fp_fromInt(1))));
}

void test_div_exact(void)
{
    fp32 a = fp_fromInt(10);
    fp32 b = fp_fromInt(2);
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, 5.0f, fp32_to_f(fp_div(a, b)));
}

void test_div_fraction_result(void)
{
    fp32 a = fp_fromInt(1);
    fp32 b = fp_fromInt(4);
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, 0.25f, fp32_to_f(fp_div(a, b)));
}

void test_div_negative_dividend(void)
{
    fp32 a = fp_fromInt(-10);
    fp32 b = fp_fromInt(2);
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, -5.0f, fp32_to_f(fp_div(a, b)));
}

void test_div_by_zero_returns_zero(void)
{
    fp32 a = fp_fromInt(42);
    TEST_ASSERT_EQUAL_INT32(0, fp_div(a, 0));
}

void test_div_by_self(void)
{
    fp32 a = fp_fromFloat(3.14f);
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, 1.0f, fp32_to_f(fp_div(a, a)));
}

/* =========================================================================
   Arithmetic — abs / sq
   ========================================================================= */

void test_abs_positive(void)
{
    fp32 a = fp_fromFloat(3.5f);
    TEST_ASSERT_EQUAL_INT32(a, fp_abs(a));
}

void test_abs_negative(void)
{
    fp32 a = fp_fromFloat(-3.5f);
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, 3.5f, fp32_to_f(fp_abs(a)));
}

void test_abs_zero(void)
{
    TEST_ASSERT_EQUAL_INT32(0, fp_abs(0));
}

void test_sq_positive(void)
{
    fp32 a = fp_fromFloat(3.0f);
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, 9.0f, fp32_to_f(fp_sq(a)));
}

void test_sq_negative_gives_positive(void)
{
    fp32 a = fp_fromFloat(-4.0f);
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, 16.0f, fp32_to_f(fp_sq(a)));
}

void test_sq_fraction(void)
{
    fp32 a = fp_fromFloat(0.5f);
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, 0.25f, fp32_to_f(fp_sq(a)));
}

/* =========================================================================
   Arithmetic — rounding
   ========================================================================= */

void test_mul_ties_away_from_zero(void)
{
    fp32 half = FP_SCALE / 2;

    TEST_ASSERT_EQUAL_INT32(1, fp_mul(1, half));
    TEST_ASSERT_EQUAL_INT32(-1, fp_mul(-1, half));
    TEST_ASSERT_EQUAL_INT32(-1, fp_mul(1, -half));
    TEST_ASSERT_EQUAL_INT32(1, fp_mul(-1, -half));
}

void test_div_ties_away_from_zero(void)
{
    fp32 two = FP_SCALE * 2;

    TEST_ASSERT_EQUAL_INT32(1, fp_div(1, two));
    TEST_ASSERT_EQUAL_INT32(-1, fp_div(-1, two));
    TEST_ASSERT_EQUAL_INT32(-1, fp_div(1, -two));
    TEST_ASSERT_EQUAL_INT32(1, fp_div(-1, -two));
}

void test_mul_is_sign_symmetric_in_safe_range(void)
{
    for (fp32 a = 0; a <= 64; ++a) {
        for (fp32 b = -64; b <= 64; ++b) {
            TEST_ASSERT_EQUAL_INT32(-fp_mul(a, b), fp_mul(-a, b));
        }
    }
}

void test_div_is_sign_symmetric_in_safe_range(void)
{
    for (fp32 a = 0; a <= 64; ++a) {
        for (fp32 b = -64; b <= 64; ++b) {
            if (b == 0) continue;
            TEST_ASSERT_EQUAL_INT32(-fp_div(a, b), fp_div(-a, b));
        }
    }
}

/* =========================================================================
   Overflow / Underflow / Saturation
   ========================================================================= */

void test_arithmetic_saturates_at_raw_boundaries(void)
{
    TEST_ASSERT_EQUAL_INT32(INT32_MAX, fp_add(INT32_MAX, 1));
    TEST_ASSERT_EQUAL_INT32(INT32_MIN, fp_sub(INT32_MIN, 1));
    TEST_ASSERT_EQUAL_INT32(INT32_MAX, fp_abs(INT32_MIN));
}

void test_fromInt_saturates(void)
{
    TEST_ASSERT_EQUAL_INT32(INT32_MAX, fp_fromInt(INT32_MAX));
    TEST_ASSERT_EQUAL_INT32(INT32_MIN, fp_fromInt(INT32_MIN));
}

void test_abs_boundary_values(void)
{
    TEST_ASSERT_EQUAL_INT32(0, fp_abs(0));
    TEST_ASSERT_EQUAL_INT32(1, fp_abs(1));
    TEST_ASSERT_EQUAL_INT32(1, fp_abs(-1));

    TEST_ASSERT_EQUAL_INT32(INT32_MAX, fp_abs(INT32_MAX));
    TEST_ASSERT_EQUAL_INT32(INT32_MAX, fp_abs(-INT32_MAX));
    TEST_ASSERT_EQUAL_INT32(INT32_MAX, fp_abs(INT32_MIN));
}

/* =========================================================================
   Rounding & Grid
   ========================================================================= */

void test_floor_positive_integer(void)
{
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, 3.0f, fp32_to_f(fp_floor(fp_fromFloat(3.0f))));
}

void test_floor_positive_fraction(void)
{
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, 3.0f, fp32_to_f(fp_floor(fp_fromFloat(3.9f))));
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, 3.0f, fp32_to_f(fp_floor(fp_fromFloat(3.001f))));
}

void test_floor_negative_fraction(void)
{
    /* floor(-1.5) must be -2, NOT -1 */
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, -2.0f, fp32_to_f(fp_floor(fp_fromFloat(-1.5f))));
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, -1.0f, fp32_to_f(fp_floor(fp_fromFloat(-0.5f))));
}

void test_floor_negative_exact_integer(void)
{
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, -3.0f, fp32_to_f(fp_floor(fp_fromFloat(-3.0f))));
}

void test_ceil_positive_fraction(void)
{
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, 4.0f, fp32_to_f(fp_ceil(fp_fromFloat(3.1f))));
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, 4.0f, fp32_to_f(fp_ceil(fp_fromFloat(3.9f))));
}

void test_ceil_positive_exact_integer(void)
{
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, 3.0f, fp32_to_f(fp_ceil(fp_fromFloat(3.0f))));
}

void test_ceil_negative_fraction(void)
{
    /* ceil(-1.5) must be -1, NOT -2 */
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, -1.0f, fp32_to_f(fp_ceil(fp_fromFloat(-1.5f))));
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, 0.0f,  fp32_to_f(fp_ceil(fp_fromFloat(-0.5f))));
}

void test_ceil_negative_exact_integer(void)
{
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, -3.0f, fp32_to_f(fp_ceil(fp_fromFloat(-3.0f))));
}

void test_round_half_rounds_up(void)
{
    /* 2.5 → 3 */
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, 3.0f, fp32_to_f(fp_round(fp_fromFloat(2.5f))));
}

void test_round_below_half(void)
{
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, 3.0f, fp32_to_f(fp_round(fp_fromFloat(3.2f))));
}

void test_round_above_half(void)
{
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, 4.0f, fp32_to_f(fp_round(fp_fromFloat(3.7f))));
}

void test_round_negative(void)
{
    /* -2.5 → -2 (rounds toward +∞ due to add-half-then-floor) */
    float result = fp32_to_f(fp_round(fp_fromFloat(-2.5f)));
    /* Accept either -2 or -3 depending on convention, just check it's sane */
    TEST_ASSERT_TRUE(result == -2.0f || result == -3.0f);
}

void test_frac_positive(void)
{
    fp32 v = fp_fromFloat(3.75f);
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, 0.75f, fp32_to_f(fp_frac(v)));
}

void test_frac_positive_zero_fraction(void)
{
    fp32 v = fp_fromInt(5);
    TEST_ASSERT_EQUAL_INT32(0, fp_frac(v));
}

void test_frac_negative(void)
{
    /* fp_frac(-1.5) — result should have |value| < 1.0 */
    fp32 v    = fp_fromFloat(-1.5f);
    fp32 frac = fp_frac(v);
    float f   = fp32_to_f(fp_abs(frac));
    TEST_ASSERT_TRUE(f >= 0.0f && f < 1.0f);
}

void test_round_ties_away_from_zero(void)
{
    TEST_ASSERT_EQUAL_INT32(FP_SCALE, fp_round(FP_SCALE / 2));
    TEST_ASSERT_EQUAL_INT32(-FP_SCALE, fp_round(-(FP_SCALE / 2)));
    TEST_ASSERT_EQUAL_INT32(3 * FP_SCALE, fp_round(2 * FP_SCALE + FP_SCALE / 2));
    TEST_ASSERT_EQUAL_INT32(-3 * FP_SCALE, fp_round(-(2 * FP_SCALE + FP_SCALE / 2)));
}

void test_round_values_around_half(void)
{
    TEST_ASSERT_EQUAL_INT32(0, fp_round(FP_SCALE / 2 - 1));
    TEST_ASSERT_EQUAL_INT32(0, fp_round(-(FP_SCALE / 2 - 1)));
    TEST_ASSERT_EQUAL_INT32(FP_SCALE, fp_round(FP_SCALE / 2 + 1));
    TEST_ASSERT_EQUAL_INT32(-FP_SCALE, fp_round(-(FP_SCALE / 2 + 1)));
}

void test_floor_ceil_exact_raw_cases(void)
{
    TEST_ASSERT_EQUAL_INT32(FP_SCALE, fp_floor(FP_SCALE + 1));
    TEST_ASSERT_EQUAL_INT32(FP_SCALE * 2, fp_ceil(FP_SCALE + 1));
    TEST_ASSERT_EQUAL_INT32(-FP_SCALE * 2, fp_floor(-FP_SCALE - 1));
    TEST_ASSERT_EQUAL_INT32(-FP_SCALE, fp_ceil(-FP_SCALE - 1));
}

void test_rounding_at_raw_boundaries(void)
{
    TEST_ASSERT_EQUAL_INT32(INT32_MIN, fp_floor(INT32_MIN));
    TEST_ASSERT_EQUAL_INT32(TEST_FP_MAX_WHOLE_RAW, fp_ceil(INT32_MAX));
    TEST_ASSERT_EQUAL_INT32(TEST_FP_MAX_WHOLE_RAW, fp_round(INT32_MAX));
}

/* =========================================================================
   Faction
   ========================================================================= */

void test_frac_uses_mathematical_fraction(void)
{
    TEST_ASSERT_EQUAL_INT32(FP_SCALE / 4, fp_frac(FP_SCALE + FP_SCALE / 4));
    TEST_ASSERT_EQUAL_INT32((FP_SCALE * 3) / 4, fp_frac(-FP_SCALE - FP_SCALE / 4));
    TEST_ASSERT_EQUAL_INT32(FP_SCALE - 1, fp_frac(-1));
    TEST_ASSERT_EQUAL_INT32(0, fp_frac(INT32_MIN));
}

void test_floor_plus_frac_reconstructs_value(void)
{
    fp32 values[] = {
        INT32_MIN,
        -FP_SCALE - 1,
        -FP_SCALE,
        -1,
        0,
        1,
        FP_SCALE - 1,
        FP_SCALE,
        FP_SCALE + 1,
        INT32_MAX
    };

    size_t count = sizeof(values) / sizeof(values[0]);

    for (size_t i = 0; i < count; ++i) {
        int64_t reconstructed = (int64_t)fp_floor(values[i]) + (int64_t)fp_frac(values[i]);
        TEST_ASSERT_EQUAL_INT64(values[i], reconstructed);
    }
}

void test_rounding_properties_over_small_raw_range(void)
{
    for (fp32 value = -4 * FP_SCALE;
         value <= 4 * FP_SCALE;
         ++value)
    {
        fp32 floor_value = fp_floor(value);
        fp32 ceil_value = fp_ceil(value);
        fp32 rounded_value = fp_round(value);
        fp32 fraction = fp_frac(value);

        TEST_ASSERT_TRUE(floor_value <= value);
        TEST_ASSERT_TRUE(ceil_value >= value);

        TEST_ASSERT_EQUAL_INT32(0, floor_value % FP_SCALE);
        TEST_ASSERT_EQUAL_INT32(0, ceil_value % FP_SCALE);
        TEST_ASSERT_EQUAL_INT32(0, rounded_value % FP_SCALE);

        TEST_ASSERT_TRUE(fraction >= 0);
        TEST_ASSERT_TRUE(fraction < FP_SCALE);

        TEST_ASSERT_EQUAL_INT64(value, (int64_t)floor_value + fraction);
    }
}

/* =========================================================================
   Boundaries — min / max / clamp / sign
   ========================================================================= */

void test_min_returns_smaller(void)
{
    fp32 a = fp_fromFloat(1.0f);
    fp32 b = fp_fromFloat(2.0f);
    TEST_ASSERT_EQUAL_INT32(a, fp_min(a, b));
    TEST_ASSERT_EQUAL_INT32(a, fp_min(b, a));
}

void test_min_equal_values(void)
{
    fp32 a = fp_fromFloat(3.0f);
    TEST_ASSERT_EQUAL_INT32(a, fp_min(a, a));
}

void test_min_negative_values(void)
{
    fp32 a = fp_fromFloat(-5.0f);
    fp32 b = fp_fromFloat(-2.0f);
    TEST_ASSERT_EQUAL_INT32(a, fp_min(a, b));
}

void test_max_returns_larger(void)
{
    fp32 a = fp_fromFloat(1.0f);
    fp32 b = fp_fromFloat(2.0f);
    TEST_ASSERT_EQUAL_INT32(b, fp_max(a, b));
    TEST_ASSERT_EQUAL_INT32(b, fp_max(b, a));
}

void test_max_negative_values(void)
{
    fp32 a = fp_fromFloat(-5.0f);
    fp32 b = fp_fromFloat(-2.0f);
    TEST_ASSERT_EQUAL_INT32(b, fp_max(a, b));
}

void test_clamp_within_range(void)
{
    fp32 lo = fp_fromFloat(0.0f);
    fp32 hi = fp_fromFloat(10.0f);
    fp32 v  = fp_fromFloat(5.0f);
    TEST_ASSERT_EQUAL_INT32(v, fp_clamp(v, lo, hi));
}

void test_clamp_below_min(void)
{
    fp32 lo = fp_fromFloat(0.0f);
    fp32 hi = fp_fromFloat(10.0f);
    fp32 v  = fp_fromFloat(-3.0f);
    TEST_ASSERT_EQUAL_INT32(lo, fp_clamp(v, lo, hi));
}

void test_clamp_above_max(void)
{
    fp32 lo = fp_fromFloat(0.0f);
    fp32 hi = fp_fromFloat(10.0f);
    fp32 v  = fp_fromFloat(15.0f);
    TEST_ASSERT_EQUAL_INT32(hi, fp_clamp(v, lo, hi));
}

void test_clamp_at_boundaries(void)
{
    fp32 lo = fp_fromFloat(1.0f);
    fp32 hi = fp_fromFloat(5.0f);
    TEST_ASSERT_EQUAL_INT32(lo, fp_clamp(lo, lo, hi));
    TEST_ASSERT_EQUAL_INT32(hi, fp_clamp(hi, lo, hi));
}

void test_sign_positive(void)
{
    TEST_ASSERT_EQUAL_INT32(1, fp_sign(fp_fromFloat(3.5f)));
}

void test_sign_negative(void)
{
    TEST_ASSERT_EQUAL_INT32(-1, fp_sign(fp_fromFloat(-3.5f)));
}

void test_sign_zero(void)
{
    TEST_ASSERT_EQUAL_INT32(0, fp_sign(0));
}

/* =========================================================================
   Vectors & Distance
   ========================================================================= */

void test_dot_perpendicular_is_zero(void)
{
    /* (1,0)·(0,1) = 0 */
    fp32 dot = fp_dot(fp_fromInt(1), fp_fromInt(0), fp_fromInt(0), fp_fromInt(1));
    TEST_ASSERT_EQUAL_INT32(0, dot);
}

void test_dot_parallel_is_product(void)
{
    /* (3,0)·(3,0) = 9 */
    fp32 dot = fp_dot(fp_fromInt(3), fp_fromInt(0), fp_fromInt(3), fp_fromInt(0));
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, 9.0f, fp32_to_f(dot));
}

void test_dot_general(void)
{
    /* (1,2)·(3,4) = 11 */
    fp32 dot = fp_dot(fp_fromInt(1), fp_fromInt(2), fp_fromInt(3), fp_fromInt(4));
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, 11.0f, fp32_to_f(dot));
}

void test_dot_negative_components(void)
{
    /* (-1,-2)·(3,4) = -11 */
    fp32 dot = fp_dot(fp_fromInt(-1), fp_fromInt(-2), fp_fromInt(3), fp_fromInt(4));
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, -11.0f, fp32_to_f(dot));
}

void test_manhattan_same_point(void)
{
    fp32 p = fp_fromFloat(5.0f);
    TEST_ASSERT_EQUAL_INT32(0, fp_manhattan(p, p, p, p));
}

void test_manhattan_axis_aligned(void)
{
    /* (0,0) to (3,4) → 3+4=7 */
    fp32 d = fp_manhattan(fp_fromInt(0), fp_fromInt(0), fp_fromInt(3), fp_fromInt(4));
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, 7.0f, fp32_to_f(d));
}

void test_manhattan_is_symmetric(void)
{
    fp32 x1 = fp_fromFloat(1.5f), y1 = fp_fromFloat(2.5f);
    fp32 x2 = fp_fromFloat(4.0f), y2 = fp_fromFloat(-1.0f);
    TEST_ASSERT_EQUAL_INT32(fp_manhattan(x1, y1, x2, y2), fp_manhattan(x2, y2, x1, y1));
}

void test_manhattan_negative_coords(void)
{
    /* (-3,-4) to (0,0) → 3+4=7 */
    fp32 d = fp_manhattan(fp_fromInt(-3), fp_fromInt(-4), fp_fromInt(0), fp_fromInt(0));
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, 7.0f, fp32_to_f(d));
}

void test_dot_combines_fractional_products_before_rounding(void)
{
    /*
     * Each product contributes exactly 0.5 raw ULP.
     * Combined result must be one raw ULP.
     */
    TEST_ASSERT_EQUAL_INT32(1, fp_dot(1, 1, FP_SCALE / 2, FP_SCALE / 2));
    TEST_ASSERT_EQUAL_INT32(-1, fp_dot(1, 1, -FP_SCALE / 2, -FP_SCALE / 2));
}

void test_dot_extreme_terms_can_cancel_exactly(void)
{
    TEST_ASSERT_EQUAL_INT32(0, fp_dot(INT32_MAX, INT32_MAX, INT32_MAX, -INT32_MAX));
}

void test_dot_saturates_without_intermediate_overflow(void)
{
    TEST_ASSERT_EQUAL_INT32(INT32_MAX, fp_dot(INT32_MIN, INT32_MIN, INT32_MIN, INT32_MIN));
    TEST_ASSERT_EQUAL_INT32(INT32_MIN, fp_dot(INT32_MAX, INT32_MAX, INT32_MIN, INT32_MIN));
}

void test_dot_is_commutative_over_raw_sample(void)
{
    fp32 vectors[][4] = {
        { 1, 2, 3, 4 },
        { -1, 7, 13, -9 },
        { FP_SCALE / 2, FP_SCALE / 3, 5, -11 },
        { INT32_MAX, 0, 0, INT32_MIN }
    };

    size_t count = sizeof(vectors) / sizeof(vectors[0]);

    for (size_t i = 0; i < count; ++i) {
        fp32 x1 = vectors[i][0];
        fp32 y1 = vectors[i][1];
        fp32 x2 = vectors[i][2];
        fp32 y2 = vectors[i][3];

        TEST_ASSERT_EQUAL_INT32(fp_dot(x1, y1, x2, y2), fp_dot(x2, y2, x1, y1));
    }
}

void test_dot_mixed_sign_half_ulp_rounds_away_from_zero(void)
{
    /*
     * (256 - 128) / 256 = +0.5 raw ULP.
     */
    TEST_ASSERT_EQUAL_INT32(1, fp_dot(1, 1, FP_SCALE, -FP_HALF));

    /*
     * (-256 + 128) / 256 = -0.5 raw ULP.
     */
    TEST_ASSERT_EQUAL_INT32(-1, fp_dot(1, 1, -FP_SCALE, FP_HALF));
}

void test_dot_mixed_sign_values_around_half_ulp(void)
{
    /* 127 / 256: below positive half, rounds to zero. */
    TEST_ASSERT_EQUAL_INT32(0, fp_dot(1, 1, FP_SCALE, -(FP_HALF + 1)));

    /* 128 / 256: exact positive half, rounds away from zero. */
    TEST_ASSERT_EQUAL_INT32(1, fp_dot(1, 1, FP_SCALE, -FP_HALF));

    /* 129 / 256: above positive half, rounds to one. */
    TEST_ASSERT_EQUAL_INT32(1, fp_dot(1, 1, FP_SCALE, -(FP_HALF - 1)));

    /* Mirrored negative cases. */
    TEST_ASSERT_EQUAL_INT32(0, fp_dot(1, 1, -FP_SCALE, FP_HALF + 1));

    TEST_ASSERT_EQUAL_INT32(-1, fp_dot(1, 1, -FP_SCALE, FP_HALF));

    TEST_ASSERT_EQUAL_INT32(-1, fp_dot(1, 1, -FP_SCALE, FP_HALF - 1));
}

/* =========================================================================
   Complex Math — sqrt
   ========================================================================= */

void test_sqrt_zero(void)
{
    TEST_ASSERT_EQUAL_INT32(0, fp_sqrt(0));
}

void test_sqrt_negative_returns_zero(void)
{
    TEST_ASSERT_EQUAL_INT32(0, fp_sqrt(fp_fromFloat(-1.0f)));
}

void test_sqrt_perfect_squares(void)
{
    int perfect[] = { 1, 4, 9, 16, 25, 36, 49, 64, 100 };
    for (int i = 0; i < (int)(sizeof(perfect) / sizeof(perfect[0])); i++)
    {
        float expected = sqrtf((float)perfect[i]);
        float result   = fp32_to_f(fp_sqrt(fp_fromInt(perfect[i])));
        TEST_ASSERT_FLOAT_WITHIN(FP_TOL, expected, result);
    }
}

void test_sqrt_fraction(void)
{
    /* sqrt(0.25) = 0.5 */
    float result = fp32_to_f(fp_sqrt(fp_fromFloat(0.25f)));
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, 0.5f, result);
}

void test_sqrt_is_nonnegative(void)
{
    fp32 v = fp_fromFloat(7.0f);
    TEST_ASSERT_TRUE(fp_sqrt(v) >= 0);
}

void test_sqrt_pythagorean_triple(void)
{
    /* 3²+4²=5² → sqrt(fp(9)+fp(16)) = fp(5) */
    fp32 sum = fp_add(fp_fromInt(9), fp_fromInt(16));
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, 5.0f, fp32_to_f(fp_sqrt(sum)));
}

/* =========================================================================
   Complex Math — atan2
   ========================================================================= */

void test_atan2_zero_zero_returns_zero(void)
{
    TEST_ASSERT_EQUAL_INT32(0, fp_atan2(0, 0));
}

void test_atan2_positive_x_axis(void)
{
    /* Positive X axis → angle near 0 */
    fp_angle a = fp_atan2(fp_fromInt(0), fp_fromInt(1));
    TEST_ASSERT_TRUE(a <= 4);   /* within ~5° of 0 */
}

void test_atan2_positive_y_axis(void)
{
    /* Positive Y axis → angle near 64 (90°) */
    fp_angle a = fp_atan2(fp_fromInt(1), fp_fromInt(0));
    TEST_ASSERT_INT_WITHIN(4, 64, (int)a);
}

void test_atan2_negative_x_axis(void)
{
    /* Negative X axis → angle near 128 (180°) */
    fp_angle a = fp_atan2(fp_fromInt(0), fp_fromInt(-1));
    TEST_ASSERT_INT_WITHIN(4, 128, (int)a);
}

void test_atan2_negative_y_axis(void)
{
    /* Negative Y axis → angle near 192 (270°) */
    fp_angle a = fp_atan2(fp_fromInt(-1), fp_fromInt(0));
    TEST_ASSERT_INT_WITHIN(4, 192, (int)a);
}

void test_atan2_45_degrees(void)
{
    /* (1,1) → angle near 32 (45°) */
    fp_angle a = fp_atan2(fp_fromInt(1), fp_fromInt(1));
    TEST_ASSERT_INT_WITHIN(4, 32, (int)a);
}

void test_atan2_approximation_error_bound(void)
{
    for (int y = -128; y <= 128; ++y) {
        for (int x = -128; x <= 128; ++x) {
            if (x == 0 && y == 0) {
                continue;
            }

            fp_angle actual = fp_atan2(y, x);

            double radians = atan2((double)y, (double)x);
            if (radians < 0.0) {
                radians += 2.0 * M_PI;
            }

            int expected = (int)llround(radians * 256.0 / (2.0 * M_PI));

            expected &= 0xFF;

            int difference = abs((int)actual - expected);
            if (difference > 128) {
                difference = 256 - difference;
            }

            TEST_ASSERT_LESS_OR_EQUAL_INT(4, difference);
        }
    }
}

void test_atan2_large_coordinates(void)
{
    TEST_ASSERT_INT_WITHIN(1, 32, fp_atan2(3000000, 3000000));
    TEST_ASSERT_INT_WITHIN(1, 160, fp_atan2(-3000000, -3000000));
}

void test_atan2_raw_extremes(void)
{
    TEST_ASSERT_INT_WITHIN(1, 128, fp_atan2(0, INT32_MIN));
    TEST_ASSERT_INT_WITHIN(1, 192, fp_atan2(INT32_MIN, 0));
}

/* =========================================================================
   Complex Math — sin / cos
   ========================================================================= */

void test_sin_zero_is_zero(void)
{
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, 0.0f, fp32_to_f(fp_sin(0)));
}

void test_sin_quarter_is_one(void)
{
    /* angle=64 → 90° → sin≈1 */
    TEST_ASSERT_FLOAT_WITHIN(0.06f, 1.0f, fp32_to_f(fp_sin(64)));
}

void test_sin_half_is_zero(void)
{
    /* angle=128 → 180° → sin≈0 */
    TEST_ASSERT_FLOAT_WITHIN(0.06f, 0.0f, fp32_to_f(fp_sin(128)));
}

void test_sin_three_quarter_is_minus_one(void)
{
    /* angle=192 → 270° → sin≈-1 */
    TEST_ASSERT_FLOAT_WITHIN(0.06f, -1.0f, fp32_to_f(fp_sin(192)));
}

void test_sin_is_bounded(void)
{
    /* sin must stay in [-1, 1] for all 256 angles */
    for (int angle = 0; angle < 256; angle++)
    {
        float s = fp32_to_f(fp_sin((fp_angle)angle));
        TEST_ASSERT_TRUE(s >= -1.1f && s <= 1.1f);
    }
}

void test_cos_zero_is_one(void)
{
    /* angle=0 → 0° → cos≈1 */
    TEST_ASSERT_FLOAT_WITHIN(0.06f, 1.0f, fp32_to_f(fp_cos(0)));
}

void test_cos_quarter_is_zero(void)
{
    /* angle=64 → 90° → cos≈0 */
    TEST_ASSERT_FLOAT_WITHIN(0.06f, 0.0f, fp32_to_f(fp_cos(64)));
}

void test_cos_is_sin_shifted(void)
{
    /* cos(θ) == sin(θ+64) for all θ */
    for (int angle = 0; angle < 256; angle++)
    {
        fp32 c = fp_cos((fp_angle)angle);
        fp32 s = fp_sin((fp_angle)((angle + 64) & 0xFF));
        TEST_ASSERT_EQUAL_INT32(c, s);
    }
}

void test_sin_symmetry_around_half(void)
{
    /* sin(x) == sin(128-x) for x in [0,64] (reflected around the peak) */
    for (int x = 0; x <= 64; x++)
    {
        fp32 left  = fp_sin((fp_angle)x);
        fp32 right = fp_sin((fp_angle)(128 - x));
        TEST_ASSERT_INT_WITHIN(1, (int)left, (int)right);
    }
}

void test_sin_cos_raw_bounds_for_every_angle(void)
{
    for (int angle = 0; angle < 256; ++angle) {
        fp32 sine = fp_sin((fp_angle)angle);
        fp32 cosine = fp_cos((fp_angle)angle);

        TEST_ASSERT_TRUE(sine >= -FP_SCALE);
        TEST_ASSERT_TRUE(sine <= FP_SCALE);

        TEST_ASSERT_TRUE(cosine >= -FP_SCALE);
        TEST_ASSERT_TRUE(cosine <= FP_SCALE);
    }
}

/* =========================================================================
   Integration / cross-function
   ========================================================================= */

void test_pythagorean_distance_via_sqrt(void)
{
    /* Distance from (0,0) to (3,4) = sqrt(3²+4²) = 5 */
    fp32 dx   = fp_fromInt(3);
    fp32 dy   = fp_fromInt(4);
    fp32 dist = fp_sqrt(fp_add(fp_sq(dx), fp_sq(dy)));
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, 5.0f, fp32_to_f(dist));
}

void test_unit_circle_sin_cos_norm(void)
{
    /* sin²(θ) + cos²(θ) should be ≈1 for all θ (allowing parabolic error) */
    for (int angle = 0; angle < 256; angle++)
    {
        fp32 s    = fp_sin((fp_angle)angle);
        fp32 c    = fp_cos((fp_angle)angle);
        fp32 norm = fp_add(fp_sq(s), fp_sq(c));
        TEST_ASSERT_FLOAT_WITHIN(0.15f, 1.0f, fp32_to_f(norm));
    }
}

void test_clamp_with_computed_bounds(void)
{
    fp32 lo   = fp_fromFloat(-1.0f);
    fp32 hi   = fp_fromFloat(1.0f);
    fp32 v    = fp_clamp(fp_fromFloat(2.5f), lo, hi);
    TEST_ASSERT_EQUAL_INT32(hi, v);
}

void test_floor_ceil_consistency(void)
{
    /* For any x, floor(x) <= x <= ceil(x) */
    float inputs[] = { 1.5f, -1.5f, 2.0f, -2.0f, 0.0f, 3.99f, -0.01f };
    for (int i = 0; i < (int)(sizeof(inputs) / sizeof(inputs[0])); i++)
    {
        fp32 x  = fp_fromFloat(inputs[i]);
        fp32 fl = fp_floor(x);
        fp32 cl = fp_ceil(x);
        TEST_ASSERT_TRUE(fl <= x);
        TEST_ASSERT_TRUE(cl >= x);
        TEST_ASSERT_TRUE(fl <= cl);
    }
}

void test_abs_sign_relationship(void)
{
    /* a == fp_abs(a) * fp_sign(a)  (when a != 0) */
    float values[] = { 3.5f, -2.25f, 1.0f, -1.0f };
    for (int i = 0; i < (int)(sizeof(values) / sizeof(values[0])); i++)
    {
        fp32    a    = fp_fromFloat(values[i]);
        int32_t sign = fp_sign(a);
        fp32    ab   = fp_abs(a);
        TEST_ASSERT_FLOAT_WITHIN(FP_TOL, fp32_to_f(a), fp32_to_f(ab) * (float)sign);
    }
}

void test_mul_div_round_trip(void)
{
    /* (a * b) / b ≈ a for exact divisors */
    fp32 a = fp_fromInt(7);
    fp32 b = fp_fromInt(3);
    fp32 result = fp_div(fp_mul(a, b), b);
    TEST_ASSERT_FLOAT_WITHIN(FP_TOL, fp32_to_f(a), fp32_to_f(result));
}

/* =========================================================================
   Test runner
   ========================================================================= */

void setUp(void)   {}
void tearDown(void) {}

int main(void)
{
    UNITY_BEGIN();

    /* Conversions */
    RUN_TEST(test_fromInt_zero);
    RUN_TEST(test_fromInt_positive);
    RUN_TEST(test_fromInt_negative);
    RUN_TEST(test_toInt_positive);
    RUN_TEST(test_toInt_negative);
    RUN_TEST(test_fromFloat_zero);
    RUN_TEST(test_fromFloat_positive_fraction);
    RUN_TEST(test_fromFloat_negative_fraction);
    RUN_TEST(test_toFloat_round_trip);
    RUN_TEST(test_toInt_truncates_negative_fraction_toward_zero);
    RUN_TEST(test_fromRatio_rounds_nearest_away_from_zero);
    RUN_TEST(test_raw_conversion_is_exact);
    RUN_TEST(test_fromFloat_half_ulp_ties_away_from_zero);
    RUN_TEST(test_fromFloat_special_values);
    RUN_TEST(test_arithmetic_saturates_at_raw_boundaries);
    RUN_TEST(test_fromInt_saturates);
    RUN_TEST(test_abs_boundary_values);

    /* Add / Sub */
    RUN_TEST(test_add_positive);
    RUN_TEST(test_add_negative);
    RUN_TEST(test_add_mixed_signs);
    RUN_TEST(test_sub_positive_result);
    RUN_TEST(test_sub_negative_result);
    RUN_TEST(test_add_is_commutative);


    /* Mul / Div */
    RUN_TEST(test_mul_two_integers);
    RUN_TEST(test_mul_fraction);
    RUN_TEST(test_mul_negative);
    RUN_TEST(test_mul_both_negative);
    RUN_TEST(test_mul_by_zero);
    RUN_TEST(test_mul_by_one);
    RUN_TEST(test_div_exact);
    RUN_TEST(test_div_fraction_result);
    RUN_TEST(test_div_negative_dividend);
    RUN_TEST(test_div_by_zero_returns_zero);
    RUN_TEST(test_div_by_self);
    RUN_TEST(test_mul_ties_away_from_zero);
    RUN_TEST(test_div_ties_away_from_zero);
    RUN_TEST(test_mul_is_sign_symmetric_in_safe_range);
    RUN_TEST(test_div_is_sign_symmetric_in_safe_range);

    /* Abs / Sq */
    RUN_TEST(test_abs_positive);
    RUN_TEST(test_abs_negative);
    RUN_TEST(test_abs_zero);
    RUN_TEST(test_sq_positive);
    RUN_TEST(test_sq_negative_gives_positive);
    RUN_TEST(test_sq_fraction);

    /* Rounding */
    RUN_TEST(test_floor_positive_integer);
    RUN_TEST(test_floor_positive_fraction);
    RUN_TEST(test_floor_negative_fraction);
    RUN_TEST(test_floor_negative_exact_integer);
    RUN_TEST(test_ceil_positive_fraction);
    RUN_TEST(test_ceil_positive_exact_integer);
    RUN_TEST(test_ceil_negative_fraction);
    RUN_TEST(test_ceil_negative_exact_integer);
    RUN_TEST(test_round_half_rounds_up);
    RUN_TEST(test_round_below_half);
    RUN_TEST(test_round_above_half);
    RUN_TEST(test_round_negative);
    RUN_TEST(test_frac_positive);
    RUN_TEST(test_frac_positive_zero_fraction);
    RUN_TEST(test_frac_negative);
    RUN_TEST(test_round_ties_away_from_zero);
    RUN_TEST(test_round_values_around_half);
    RUN_TEST(test_floor_ceil_exact_raw_cases);
    RUN_TEST(test_rounding_at_raw_boundaries);
    RUN_TEST(test_frac_uses_mathematical_fraction);
    RUN_TEST(test_floor_plus_frac_reconstructs_value);
    RUN_TEST(test_rounding_properties_over_small_raw_range);

    /* Boundaries */
    RUN_TEST(test_min_returns_smaller);
    RUN_TEST(test_min_equal_values);
    RUN_TEST(test_min_negative_values);
    RUN_TEST(test_max_returns_larger);
    RUN_TEST(test_max_negative_values);
    RUN_TEST(test_clamp_within_range);
    RUN_TEST(test_clamp_below_min);
    RUN_TEST(test_clamp_above_max);
    RUN_TEST(test_clamp_at_boundaries);
    RUN_TEST(test_sign_positive);
    RUN_TEST(test_sign_negative);
    RUN_TEST(test_sign_zero);

    /* Vectors */
    RUN_TEST(test_dot_perpendicular_is_zero);
    RUN_TEST(test_dot_parallel_is_product);
    RUN_TEST(test_dot_general);
    RUN_TEST(test_dot_negative_components);
    RUN_TEST(test_manhattan_same_point);
    RUN_TEST(test_manhattan_axis_aligned);
    RUN_TEST(test_manhattan_is_symmetric);
    RUN_TEST(test_manhattan_negative_coords);
    RUN_TEST(test_dot_combines_fractional_products_before_rounding);
    RUN_TEST(test_dot_extreme_terms_can_cancel_exactly);
    RUN_TEST(test_dot_saturates_without_intermediate_overflow);
    RUN_TEST(test_dot_is_commutative_over_raw_sample);
    RUN_TEST(test_dot_mixed_sign_half_ulp_rounds_away_from_zero);
    RUN_TEST(test_dot_mixed_sign_values_around_half_ulp);

    /* Sqrt */
    RUN_TEST(test_sqrt_zero);
    RUN_TEST(test_sqrt_negative_returns_zero);
    RUN_TEST(test_sqrt_perfect_squares);
    RUN_TEST(test_sqrt_fraction);
    RUN_TEST(test_sqrt_is_nonnegative);
    RUN_TEST(test_sqrt_pythagorean_triple);

    /* Atan2 */
    RUN_TEST(test_atan2_zero_zero_returns_zero);
    RUN_TEST(test_atan2_positive_x_axis);
    RUN_TEST(test_atan2_positive_y_axis);
    RUN_TEST(test_atan2_negative_x_axis);
    RUN_TEST(test_atan2_negative_y_axis);
    RUN_TEST(test_atan2_45_degrees);
    RUN_TEST(test_atan2_approximation_error_bound);
    RUN_TEST(test_atan2_large_coordinates);
    RUN_TEST(test_atan2_raw_extremes);

    /* Sin / Cos */
    RUN_TEST(test_sin_zero_is_zero);
    RUN_TEST(test_sin_quarter_is_one);
    RUN_TEST(test_sin_half_is_zero);
    RUN_TEST(test_sin_three_quarter_is_minus_one);
    RUN_TEST(test_sin_is_bounded);
    RUN_TEST(test_cos_zero_is_one);
    RUN_TEST(test_cos_quarter_is_zero);
    RUN_TEST(test_cos_is_sin_shifted);
    RUN_TEST(test_sin_symmetry_around_half);
    RUN_TEST(test_sin_cos_raw_bounds_for_every_angle);

    /* Integration */
    RUN_TEST(test_pythagorean_distance_via_sqrt);
    RUN_TEST(test_unit_circle_sin_cos_norm);
    RUN_TEST(test_clamp_with_computed_bounds);
    RUN_TEST(test_floor_ceil_consistency);
    RUN_TEST(test_abs_sign_relationship);
    RUN_TEST(test_mul_div_round_trip);

    return UNITY_END();
}
