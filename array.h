#ifndef SHL_ARRAY_H
#define SHL_ARRAY_H

#include <stdint.h>
#include <stdlib.h>

#define shlDeclareCreateArray(prefix, itemType) \
    itemType** prefix ## CreateArray(int32_t n, int32_t m);

#define shlDefineCreateArray(prefix, itemType) \
    itemType** prefix ## CreateArray(int32_t n, int32_t m) \
    { \
        itemType* values = (itemType*)calloc(m * n, sizeof(itemType)); \
        itemType** rows = (itemType**)malloc(n * sizeof(itemType*)); \
        for (int i = 0; i < n; ++i) \
        { \
            rows[i] = values + i * m; \
        } \
        return rows; \
    }

#define shlDeclareFreeArray(prefix, itemType) \
    void prefix ## FreeArray(itemType** arr);

#define shlDefineFreeArray(prefix, itemType) \
    void prefix ## FreeArray(itemType** arr) \
    { \
        free(*arr); \
        free(arr); \
    }

#endif // SHL_ARRAY_H
