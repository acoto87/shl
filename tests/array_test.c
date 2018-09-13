#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>

#include "../array.h"

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
    
    for(int i = 0; i < N; i++)
    {
        for(int j = 0; j < M; j++)
        {
            arr[i][j] = i * j;
        }
    }

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
    
    for(int i = 0; i < N; i++)
    {
        for(int j = 0; j < M; j++)
        {
            arr[i][j] = (Point*)malloc(sizeof(Point));
            arr[i][j]->x = (float)i;
            arr[i][j]->y = (float)j;
        }
    }

    pointPrintArray(N, M, arr);

    pointFreeArray(arr);
}

int main(int argc, char **argv)
{
    /* initialize random seed: */
    srand(time(NULL));

    valueTypeTest();

    printf("\n\n");

    referenceTypeTest();

    return 0;
}