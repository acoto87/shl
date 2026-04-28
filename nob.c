#define _POSIX_C_SOURCE 200809L
#define nob_cc(cmd) nob_cmd_append(cmd, "gcc")
#define NOB_IMPLEMENTATION
#include "nob.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    const char* source;
    const char* output;
    const char* extraSource;  /* optional second source file (multi-TU tests) */
} TestTarget;

typedef enum
{
    BuildModeDefault,
    BuildModeAsan,
    BuildModeValgrind
} BuildMode;

static const TestTarget TestTargets[] =
{
    { "tests/array_test.c",           "array_test",           NULL },
    { "tests/binary_heap_test.c",     "binary_heap_test",     NULL },
    { "tests/flic_test.c",            "flic_test",            NULL },
    { "tests/list_test.c",            "list_test",            NULL },
    { "tests/map_test.c",             "map_test",             NULL },
    { "tests/memory_buffer_test.c",   "memory_buffer_test",   NULL },
    { "tests/memzone_test.c",         "memzone_test",         NULL },
    { "tests/memzone_audit_test.c",   "memzone_audit_test",   NULL },
    { "tests/queue_test.c",           "queue_test",           NULL },
    { "tests/set_test.c",             "set_test",             NULL },
    { "tests/stack_test.c",           "stack_test",           NULL },
    { "tests/wav_test.c",             "wav_test",             NULL },
    { "tests/wstr_test.c",            "wstr_test",            NULL },
    { "tests/multi_tu_test.c",        "multi_tu_test",        "tests/multi_tu_helper.c" },
};

static const TestTarget* find_test_target(const char* name)
{
    if (name == NULL || strcmp(name, "all") == 0)
    {
        return NULL;
    }

    for (size_t i = 0; i < NOB_ARRAY_LEN(TestTargets); ++i)
    {
        if (strcmp(TestTargets[i].output, name) == 0)
        {
            return &TestTargets[i];
        }
    }

    return NULL;
}

static void print_usage(const char* program)
{
    nob_log(NOB_INFO, "Usage: %s [build|test|asan|valgrind] [all|test_name]", program);
    nob_log(NOB_INFO, "Examples: %s test, %s test wstr_test, %s asan array_test", program, program, program);
}

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

static bool build_tests(BuildMode mode, const char* out_dir, const TestTarget* selected_target)
{
    if (!nob_mkdir_if_not_exists("build")) return false;
    if (!nob_mkdir_if_not_exists(out_dir)) return false;

    for (size_t i = 0; i < NOB_ARRAY_LEN(TestTargets); ++i)
    {
        Nob_Cmd cmd = {0};
        const TestTarget target = TestTargets[i];
        const char* output_path = nob_temp_sprintf("%s/%s", out_dir, target.output);

        if (selected_target != NULL && strcmp(target.output, selected_target->output) != 0)
        {
            continue;
        }

        nob_cc(&cmd);
        append_mode_flags(&cmd, mode);
        nob_cc_output(&cmd, output_path);
        nob_cmd_append(&cmd, target.source);
        if (target.extraSource != NULL)
            nob_cmd_append(&cmd, target.extraSource);
        nob_cmd_append(&cmd, "tests/vendor/unity/src/unity.c", "-lm");

        if (!nob_cmd_run_sync(cmd))
            return false;
    }

    return true;
}

static bool run_tests(const char* out_dir, BuildMode mode, const TestTarget* selected_target)
{
    for (size_t i = 0; i < NOB_ARRAY_LEN(TestTargets); ++i)
    {
        Nob_Cmd cmd = {0};
        const TestTarget target = TestTargets[i];
        const char* output_path = nob_temp_sprintf("%s/%s", out_dir, target.output);

        if (selected_target != NULL && strcmp(target.output, selected_target->output) != 0)
        {
            continue;
        }

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
    const char* test_name = argc > 2 ? argv[2] : NULL;
    BuildMode mode = BuildModeDefault;
    const char* out_dir = "build/default";
    const TestTarget* selected_target = NULL;
    bool run = false;

    if (argc > 3)
    {
        nob_log(NOB_ERROR, "Too many arguments.");
        print_usage(argv[0]);
        return 1;
    }

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
        print_usage(argv[0]);
        return 1;
    }

    if (test_name != NULL)
    {
        selected_target = find_test_target(test_name);
        if (selected_target == NULL && strcmp(test_name, "all") != 0)
        {
            nob_log(NOB_ERROR, "Unknown test `%s`.", test_name);
            print_usage(argv[0]);
            nob_log(NOB_INFO, "Available tests:");
            for (size_t i = 0; i < NOB_ARRAY_LEN(TestTargets); ++i)
            {
                nob_log(NOB_INFO, "  %s", TestTargets[i].output);
            }
            return 1;
        }
    }

    if (!build_tests(mode, out_dir, selected_target))
        return 1;

    if (run && !run_tests(out_dir, mode, selected_target))
        return 1;

    return 0;
}
