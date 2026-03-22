#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>

#include "../array.h"
#include "test_common.h"

static const int32_t N = 10;
static const int32_t M = 10;

shlDefineCreateArray(float, float)
shlDefineFreeArray(float, float)

void floatPrintArray(int n, int m, float** arr)
{
    for(int i = 0; i < n; i++)
    {
        printf("{ ");

        for(int j = 0; j < m; j++)
        {
            if (j > 0)
                printf(", ");

            printf("%.2f", arr[i][j]);
        }

        printf(" }\n");
    }
}

void valueTypeTest()
{
    float** arr = floatCreateArray(N, M);
    assert(arr);
    assert(arr[0]);
     
    for(int i = 0; i < N; i++)
    {
        for(int j = 0; j < M; j++)
        {
            arr[i][j] = i * j;
        }
    }

    assert(arr[1] == arr[0] + M);
    assert(arr[N - 1] == arr[0] + (N - 1) * M);

    floatPrintArray(N, M, arr);

    floatFreeArray(arr);    
}

typedef struct
{
    float x, y;
} Point;

shlDefineCreateArray(point, Point*)
shlDefineFreeArray(point, Point*)

void pointPrintArray(int n, int m, Point*** arr)
{
    for(int i = 0; i < n; i++)
    {
        printf("{ ");

        for(int j = 0; j < m; j++)
        {
            if (j > 0)
                printf(", ");

            printf("(%.2f, %.2f)", arr[i][j]->x, arr[i][j]->y);
        }

        printf(" }\n");
    }
}

void referenceTypeTest()
{
    Point*** arr = pointCreateArray(N, M);
    assert(arr);
    assert(arr[0]);
     
    for(int i = 0; i < N; i++)
    {
        for(int j = 0; j < M; j++)
        {
            arr[i][j] = (Point*)malloc(sizeof(Point));
            arr[i][j]->x = (float)i;
            arr[i][j]->y = (float)j;
        }
    }

    assert(arr[1] == arr[0] + M);
    assert(arr[N - 1] == arr[0] + (N - 1) * M);

    pointPrintArray(N, M, arr);

    for(int i = 0; i < N; i++)
    {
        for(int j = 0; j < M; j++)
            free(arr[i][j]);
    }

    pointFreeArray(arr);
}

void largeArrayTest()
{
    const int32_t rows = 256;
    const int32_t cols = 256;
    float** arr = floatCreateArray(rows, cols);
    assert(arr);
    assert(arr[rows - 1] == arr[0] + (rows - 1) * cols);

    for (int32_t i = 0; i < rows; i++)
    {
        for (int32_t j = 0; j < cols; j++)
            arr[i][j] = (float)(i + j);
    }

    assert(arr[0][0] == 0.0f);
    assert(arr[255][255] == 510.0f);

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
    /* initialize random seed: */
    srand(time(NULL));

    UNITY_BEGIN();
    RUN_TEST(valueTypeTest);
    RUN_TEST(largeArrayTest);
    RUN_TEST(referenceTypeTest);
    return UNITY_END();
}
