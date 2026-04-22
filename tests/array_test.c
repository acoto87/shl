#include <stdlib.h>

#include "../array.h"
#include "test_common.h"

static const int32_t N = 10;
static const int32_t M = 10;

typedef struct
{
    float x;
    float y;
} Point;

shlDefineCreateArray(float, float)
shlDefineFreeArray(float, float)
shlDefineCreateArray(point, Point*)
shlDefineFreeArray(point, Point*)

void test_value_array_layout_is_contiguous(void)
{
    float** arr = floatCreateArray(N, M);

    TEST_ASSERT_NOT_NULL(arr);
    TEST_ASSERT_NOT_NULL(arr[0]);
    TEST_ASSERT_EQUAL_PTR(arr[0] + M, arr[1]);
    TEST_ASSERT_EQUAL_PTR(arr[0] + (N - 1) * M, arr[N - 1]);

    floatFreeArray(arr);
}

void test_value_array_round_trip_preserves_values(void)
{
    float** arr = floatCreateArray(N, M);

    for (int32_t i = 0; i < N; i++)
    {
        for (int32_t j = 0; j < M; j++)
        {
            arr[i][j] = (float)(i * j);
        }
    }

    TEST_ASSERT_EQUAL_FLOAT(0.0f, arr[0][0]);
    TEST_ASSERT_EQUAL_FLOAT(9.0f, arr[1][9]);
    TEST_ASSERT_EQUAL_FLOAT(81.0f, arr[9][9]);

    floatFreeArray(arr);
}

void test_reference_array_can_store_allocated_points(void)
{
    Point*** arr = pointCreateArray(N, M);

    for (int32_t i = 0; i < N; i++)
    {
        for (int32_t j = 0; j < M; j++)
        {
            arr[i][j] = (Point*)malloc(sizeof(Point));
            TEST_ASSERT_NOT_NULL(arr[i][j]);
            arr[i][j]->x = (float)i;
            arr[i][j]->y = (float)j;
        }
    }

    TEST_ASSERT_EQUAL_PTR(arr[0] + M, arr[1]);
    TEST_ASSERT_EQUAL_FLOAT(3.0f, arr[3][4]->x);
    TEST_ASSERT_EQUAL_FLOAT(4.0f, arr[3][4]->y);

    for (int32_t i = 0; i < N; i++)
    {
        for (int32_t j = 0; j < M; j++)
        {
            free(arr[i][j]);
        }
    }

    pointFreeArray(arr);
}

void test_large_array_stress_preserves_tail_value(void)
{
    const int32_t rows = 256;
    const int32_t cols = 256;
    float** arr = floatCreateArray(rows, cols);

    TEST_ASSERT_NOT_NULL(arr);
    TEST_ASSERT_EQUAL_PTR(arr[0] + (rows - 1) * cols, arr[rows - 1]);

    for (int32_t i = 0; i < rows; i++)
    {
        for (int32_t j = 0; j < cols; j++)
        {
            arr[i][j] = (float)(i + j);
        }
    }

    TEST_ASSERT_EQUAL_FLOAT(0.0f, arr[0][0]);
    TEST_ASSERT_EQUAL_FLOAT(510.0f, arr[255][255]);

    floatFreeArray(arr);
}

void setUp(void)
{
}

void tearDown(void)
{
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_value_array_layout_is_contiguous);
    RUN_TEST(test_value_array_round_trip_preserves_values);
    RUN_TEST(test_reference_array_can_store_allocated_points);
    RUN_TEST(test_large_array_stress_preserves_tail_value);
    return UNITY_END();
}
