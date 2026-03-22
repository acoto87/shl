#include "test_common.h"

void canvas2dRequiresExternalDependencies(void)
{
    TEST_IGNORE_MESSAGE("Canvas2D depends on external OpenGL and NanoVG libraries that are not bundled in this repository.");
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
    RUN_TEST(canvas2dRequiresExternalDependencies);
    return UNITY_END();
}
