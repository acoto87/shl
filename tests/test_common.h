#ifndef SHL_TEST_COMMON_H
#define SHL_TEST_COMMON_H

#include <assert.h>

#include "vendor/unity/src/unity.h"

#ifdef assert
#undef assert
#endif

#define assert(expr) TEST_ASSERT((expr))

#endif // SHL_TEST_COMMON_H
