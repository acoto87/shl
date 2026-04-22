#ifndef SHL_TEST_COMMON_H
#define SHL_TEST_COMMON_H

#include <assert.h>
#include <stdint.h>

#include "vendor/unity/src/unity.h"

#if defined(SHL_LEAK_CHECK)
#define SHL_TEST_STRESS_COUNT 2048
#define SHL_TEST_MEDIUM_COUNT 512
#else
#define SHL_TEST_STRESS_COUNT 16384
#define SHL_TEST_MEDIUM_COUNT 4096
#endif

#ifdef assert
#undef assert
#endif

#define assert(expr) TEST_ASSERT((expr))

#endif // SHL_TEST_COMMON_H
