#define _POSIX_C_SOURCE 200809L
#define NOB_IMPLEMENTATION
#include "nob.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    const char* source;
    const char* output;
} TestTarget;

typedef enum
{
    BuildModeDefault,
    BuildModeAsan,
    BuildModeValgrind
} BuildMode;

static const TestTarget TestTargets[] =
{
    { "tests/array_test.c", "array_test" },
    { "tests/binary_heap_test.c", "binary_heap_test" },
    { "tests/canvas2d_test.c", "canvas2d_test" },
    { "tests/flic_test.c", "flic_test" },
    { "tests/list_test.c", "list_test" },
    { "tests/map_test.c", "map_test" },
    { "tests/memory_buffer_test.c", "memory_buffer_test" },
    { "tests/memzone_test.c", "memzone_test" },
    { "tests/queue_test.c", "queue_test" },
    { "tests/set_test.c", "set_test" },
    { "tests/stack_test.c", "stack_test" },
    { "tests/wave_writer_test.c", "wave_writer_test" },
};

static void append_mode_flags(Nob_Cmd* cmd, BuildMode mode)
{
    nob_cmd_append(cmd, "-std=c99", "-Wall", "-Wextra", "-Wpedantic", "-I.", "-Itests", "-Itests/vendor/unity/src");

    switch (mode)
    {
        case BuildModeAsan:
            nob_cmd_append(cmd, "-fsanitize=address", "-fno-omit-frame-pointer", "-g", "-DSHL_LEAK_CHECK=1");
            break;
        case BuildModeValgrind:
            nob_cmd_append(cmd, "-O0", "-g", "-DSHL_LEAK_CHECK=1");
            break;
        case BuildModeDefault:
        default:
            break;
    }
}

static bool build_tests(BuildMode mode, const char* out_dir)
{
    if (!nob_mkdir_if_not_exists("build")) return false;
    if (!nob_mkdir_if_not_exists(out_dir)) return false;

    for (size_t i = 0; i < NOB_ARRAY_LEN(TestTargets); ++i)
    {
        Nob_Cmd cmd = {0};
        const TestTarget target = TestTargets[i];
        const char* output_path = nob_temp_sprintf("%s/%s", out_dir, target.output);

        nob_cc(&cmd);
        append_mode_flags(&cmd, mode);
        nob_cc_output(&cmd, output_path);
        nob_cmd_append(&cmd, target.source, "tests/vendor/unity/src/unity.c", "-lm");

        if (!nob_cmd_run_sync(cmd))
            return false;
    }

    return true;
}

static bool run_tests(const char* out_dir, BuildMode mode)
{
    for (size_t i = 0; i < NOB_ARRAY_LEN(TestTargets); ++i)
    {
        Nob_Cmd cmd = {0};
        const char* output_path = nob_temp_sprintf("%s/%s", out_dir, TestTargets[i].output);

        if (mode == BuildModeValgrind)
        {
            if (!nob_file_exists("/usr/bin/valgrind"))
            {
                nob_log(NOB_ERROR, "valgrind is required to run this mode");
                return false;
            }

            nob_cmd_append(&cmd, "valgrind", "--leak-check=full", "--show-leak-kinds=all", "--error-exitcode=1", output_path);
        }
        else
        {
            nob_cmd_append(&cmd, output_path);
        }

        if (!nob_cmd_run_sync(cmd))
            return false;
    }

    return true;
}

int main(int argc, char** argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    const char* command = argc > 1 ? argv[1] : "build";
    BuildMode mode = BuildModeDefault;
    const char* out_dir = "build/default";
    bool run = false;

    if (strcmp(command, "build") == 0)
    {
        mode = BuildModeDefault;
        out_dir = "build/default";
    }
    else if (strcmp(command, "test") == 0)
    {
        mode = BuildModeDefault;
        out_dir = "build/default";
        run = true;
    }
    else if (strcmp(command, "asan") == 0)
    {
        mode = BuildModeAsan;
        out_dir = "build/asan";
        run = true;
    }
    else if (strcmp(command, "valgrind") == 0)
    {
        mode = BuildModeValgrind;
        out_dir = "build/valgrind";
        run = true;
    }
    else
    {
        nob_log(NOB_ERROR, "Unknown command `%s`. Expected build, test, asan, or valgrind.", command);
        return 1;
    }

    if (!build_tests(mode, out_dir))
        return 1;

    if (run && !run_tests(out_dir, mode))
        return 1;

    return 0;
}
