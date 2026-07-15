# Fixed-point math library

`fixed_point.h` provides deterministic 32-bit signed fixed-point arithmetic for simulations and other applications that require predictable integer-based math.

The default format uses 8 fractional bits, giving a signed 24.8 layout with a scale factor of 256:

```text
fixed-point value = raw integer / FP_SCALE
FP_SCALE          = 1 << FP_FRAC_BITS
```

Define `FIXED_POINT_IMPLEMENTATION` in exactly one C or C++ translation unit before including `fixed_point.h` to compile the implementation.

```c
#define FIXED_POINT_IMPLEMENTATION
#include "fixed_point.h"
```

In all other translation units, include the header normally:

```c
#include "fixed_point.h"
```

For header-only static inline mode, define `FIXED_POINT_STATIC` before including the header:

```c
#define FIXED_POINT_STATIC
#include "fixed_point.h"
```

## Precision configuration

Define `FP_FRAC_BITS` before including the header to choose the number of fractional bits. The supported range is 1 through 30.

```c
#define FP_FRAC_BITS 16
#define FIXED_POINT_IMPLEMENTATION
#include "fixed_point.h"
```

Every translation unit in the program must use the same `FP_FRAC_BITS` value.

| Name | Description |
| --- | --- |
| `FP_FRAC_BITS` | Number of fractional bits. Defaults to `8`. |
| `FP_SCALE` | Raw units representing `1.0`; equal to `1 << FP_FRAC_BITS`. |
| `FP_MAX_INTEGER` | Largest positive whole integer representable by `fp32`. |
| `FP_MIN_INTEGER` | Smallest negative whole integer representable by `fp32`. |
| `FIXED_POINT_IMPLEMENTATION` | Emits the external function definitions in one translation unit. |
| `FIXED_POINT_STATIC` | Emits all functions as `static inline`. |
| `FIXED_POINT_DEF` | Optional override for the public function linkage macro. |

Changing `FP_FRAC_BITS` trades integer range for fractional precision. With the default Q24.8 format, one raw unit represents `1 / 256`.

## Types and constants

| Name | Description |
| --- | --- |
| `fp32` | Signed 32-bit fixed-point value stored internally as `int32_t`. |
| `fp_angle` | Unsigned 8-bit binary angle. The full range `0` through `255` represents one complete turn. |
| `FP_ZERO` | Fixed-point `0.0`. |
| `FP_ONE` | Fixed-point `1.0`. |
| `FP_HALF` | Fixed-point `0.5`. |

Binary angles wrap naturally because `fp_angle` is an unsigned 8-bit type:

| Angle | Direction |
| --- | --- |
| `0` | 0°, positive X axis |
| `64` | 90°, positive Y axis |
| `128` | 180°, negative X axis |
| `192` | 270°, negative Y axis |

## Arithmetic behavior

Arithmetic functions use saturating overflow. Results above the `fp32` range become `INT32_MAX`, and results below the range become `INT32_MIN`.

Multiplication, division, ratio conversion, floating-point conversion, and `fp_round` round to the nearest representable value. Exact halfway cases round away from zero.

`fp_toInt` truncates toward zero.

Division by zero returns `FP_ZERO`. `fp_sqrt` returns `FP_ZERO` for zero and negative inputs.

`fp_floor`, `fp_ceil`, and `fp_round` return values with no fractional raw bits. At the positive representational boundary, `fp_ceil` and `fp_round` clamp to the largest representable whole fixed-point value.

## Conversion API

| Function | Description | Return type |
| --- | --- | --- |
| `fp_fromRaw`(int32_t raw) | Constructs a fixed-point value from its exact raw representation. | `fp32` |
| `fp_toRaw`(fp32 x) | Returns the exact raw integer representation. | `int32_t` |
| `fp_fromRatio`(int32_t num, int32_t den) | Converts `num / den`, rounding to nearest with ties away from zero. Returns zero when `den` is zero and saturates on overflow. | `fp32` |
| `fp_fromInt`(int32_t x) | Converts a signed integer to fixed point, saturating when it is outside the representable range. | `fp32` |
| `fp_toInt`(fp32 x) | Converts to a signed integer by truncating toward zero. | `int32_t` |
| `fp_fromFloat`(float x) | Converts a floating-point value, rounding to nearest with ties away from zero. NaN becomes zero; infinities and out-of-range values saturate. | `fp32` |
| `fp_toFloat`(fp32 x) | Converts a fixed-point value to `float`. | `float` |

`fp_fromRaw`, `fp_toRaw`, `fp_fromRatio`, and `fp_fromInt` are the preferred interfaces for canonical deterministic simulation data.

`fp_fromFloat` and `fp_toFloat` are convenience functions for tools, rendering, debugging, and tests. Avoid using runtime floating-point calculations to construct authoritative simulation state when bit-exact reproducibility is required.

## Arithmetic API

| Function | Description | Return type |
| --- | --- | --- |
| `fp_add`(fp32 a, fp32 b) | Adds two values with saturating overflow. | `fp32` |
| `fp_sub`(fp32 a, fp32 b) | Subtracts two values with saturating overflow. | `fp32` |
| `fp_mul`(fp32 a, fp32 b) | Multiplies two values, rounds to nearest with ties away from zero, and saturates on overflow. | `fp32` |
| `fp_div`(fp32 a, fp32 b) | Divides `a` by `b`, rounds to nearest with ties away from zero, and saturates on overflow. Returns zero when `b` is zero. | `fp32` |
| `fp_abs`(fp32 x) | Returns the absolute value. `INT32_MIN` saturates to `INT32_MAX`. | `fp32` |
| `fp_sq`(fp32 x) | Returns `x * x` using `fp_mul`. | `fp32` |

## Rounding API

| Function | Description | Return type |
| --- | --- | --- |
| `fp_floor`(fp32 x) | Returns the greatest representable whole fixed-point value less than or equal to `x`. | `fp32` |
| `fp_ceil`(fp32 x) | Returns the smallest representable whole fixed-point value greater than or equal to `x`, except when clamped at the positive whole-value boundary. | `fp32` |
| `fp_round`(fp32 x) | Rounds to the nearest whole fixed-point value, with exact ties away from zero. | `fp32` |
| `fp_frac`(fp32 x) | Returns the mathematical fractional part in the range `[FP_ZERO, FP_ONE)`. | `fp32` |

`fp_frac` follows the mathematical definition `x - floor(x)`. For example, the fractional part of `-1.25` is `0.75`.

## Boundary API

| Function | Description | Return type |
| --- | --- | --- |
| `fp_min`(fp32 a, fp32 b) | Returns the smaller value. | `fp32` |
| `fp_max`(fp32 a, fp32 b) | Returns the larger value. | `fp32` |
| `fp_clamp`(fp32 x, fp32 min, fp32 max) | Clamps `x` to the inclusive range `[min, max]`. The caller should provide `min <= max`. | `fp32` |
| `fp_sign`(fp32 x) | Returns `-1`, `0`, or `1` according to the sign of `x`. | `int32_t` |

## Vector and distance API

| Function | Description | Return type |
| --- | --- | --- |
| `fp_dot`(fp32 x1, fp32 y1, fp32 x2, fp32 y2) | Computes the 2D dot product `x1 * x2 + y1 * y2`, applies fixed-point scaling, rounds to nearest, and saturates on overflow. | `fp32` |
| `fp_manhattan`(fp32 x1, fp32 y1, fp32 x2, fp32 y2) | Computes `abs(x2 - x1) + abs(y2 - y1)` with saturating overflow. | `fp32` |

## Deterministic math API

| Function | Description | Return type |
| --- | --- | --- |
| `fp_sqrt`(fp32 x) | Computes an integer fixed-point square-root approximation. Returns zero for zero and negative inputs. | `fp32` |
| `fp_atan2`(fp32 y, fp32 x) | Returns the approximate binary angle of vector `(x, y)`. `(0, 0)` returns zero. | `fp_angle` |
| `fp_sin`(fp_angle angle) | Returns an approximate sine in the range `[-FP_ONE, FP_ONE]`. | `fp32` |
| `fp_cos`(fp_angle angle) | Returns an approximate cosine in the range `[-FP_ONE, FP_ONE]`. | `fp32` |

## Approximation limits

`fp_sin` and `fp_cos` use a fast parabolic approximation. Their maximum error is approximately `0.056` relative to an exact result in the range `[-1, 1]`.

`fp_atan2` uses a quadrant-corrected linear ratio approximation. Its error is approximately ±3.5 binary-angle units in the worst case, including output quantization, or about ±5 degrees.

These functions favor deterministic integer arithmetic and low cost over high-precision transcendental results.

## Deterministic state and serialization

Store or serialize fixed-point values using their raw `int32_t` representation:

```c
fp32 position = fp_fromRatio(25, 4);
int32_t storedPosition = fp_toRaw(position);

/* Write storedPosition to a replay, snapshot, or save file. */

fp32 restoredPosition = fp_fromRaw(storedPosition);
```

A replay or state hash should also record the chosen `FP_FRAC_BITS` configuration, or otherwise guarantee that every producer and consumer was built with the same value.

## Example

```c
#include <stdio.h>

#define FIXED_POINT_IMPLEMENTATION
#include "fixed_point.h"

int main(void)
{
    fp32 positionX = fp_fromInt(10);
    fp32 positionY = fp_fromInt(5);

    fp32 velocityX = fp_fromRatio(3, 2);
    fp32 velocityY = fp_fromRatio(-1, 2);
    fp32 tickTime = fp_fromRatio(1, 30);

    positionX = fp_add(positionX, fp_mul(velocityX, tickTime));
    positionY = fp_add(positionY, fp_mul(velocityY, tickTime));

    fp_angle direction = fp_atan2(velocityY, velocityX);

    printf("position raw: (%d, %d)\n",
           fp_toRaw(positionX),
           fp_toRaw(positionY));

    printf("position: (%f, %f)\n",
           fp_toFloat(positionX),
           fp_toFloat(positionY));

    printf("direction: %u\n", (unsigned)direction);
    return 0;
}
```
