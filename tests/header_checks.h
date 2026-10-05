/* Build-time checks and maintenance for the distributed single headers.
   Included by nob.c; this is not a library header. */

static bool read_header_text(const char* path, Nob_String_Builder* text)
{
    if (!nob_read_entire_file(path, text)) return false;
    /* Compare equally on LF and CRLF checkouts. */
    size_t count = 0;
    for (size_t i = 0; i < text->count; ++i)
        if (text->items[i] != '\r') text->items[count++] = text->items[i];
    text->count = count;
    nob_sb_append_null(text);
    return true;
}

typedef struct
{
    const char* label;
    const char* source;
    const char* start;
    const char* end;
} Header_Sync_Section;

static const Header_Sync_Section HeaderSyncAllocType = {
    "alloc.h:type",
    "alloc.h",
    "#ifndef SHL_ALLOCATOR_T_DEFINED\n",
    "#endif /* SHL_ALLOCATOR_T_DEFINED */"
};

static const Header_Sync_Section HeaderSyncAllocZone = {
    "alloc.h:zone",
    "alloc.h",
    "#if defined(SHL_ALLOC_H) && defined(SHL_MZ_H)\n",
    "#endif /* SHL_ALLOC_H && SHL_MZ_H */"
};

static bool sync_embedded_header(const char* target, const Header_Sync_Section* section, bool update)
{
    bool result = false;
    Nob_String_Builder original = {0}, dependency = {0}, replacement = {0};
    if (!read_header_text(target, &original) || !read_header_text(section->source, &dependency))
        goto cleanup;

    const char* begin = strstr(original.items, section->start);
    const char* end = begin ? strstr(begin, section->end) : NULL;
    const char* body = strstr(dependency.items, section->start);
    const char* body_end = body ? strstr(body, section->end) : NULL;
    if (!begin || !end || !body || !body_end)
    {
        nob_log(NOB_ERROR, "Missing sync section: %s <- %s", target, section->label);
        goto cleanup;
    }
    size_t section_end_size = strlen(section->end);
    size_t body_size = (size_t)((body_end + section_end_size) - body);
    if (end && (size_t)((end + section_end_size) - begin) == body_size && memcmp(begin, body, body_size) == 0)
    {
        result = true;
        goto cleanup;
    }
    if (!update)
    {
        nob_log(NOB_ERROR, "%s has stale embedded %s; run ./nob sync-headers", target, section->label);
        goto cleanup;
    }

    nob_sb_append_buf(&replacement, original.items, (size_t)(begin - original.items));
    nob_sb_append_buf(&replacement, body, body_size);
    nob_sb_append_cstr(&replacement, end + section_end_size);
    result = nob_write_entire_file(target, replacement.items, replacement.count);
    if (result) nob_log(NOB_INFO, "Updated %s from %s", target, section->label);

cleanup:
    nob_sb_free(original);
    nob_sb_free(dependency);
    nob_sb_free(replacement);
    return result;
}

static bool sync_embedded_headers(bool update)
{
    static const char* collections[] = {
        "list.h", "stack.h", "queue.h", "binary_heap.h", "map.h", "set.h"
    };
    for (size_t i = 0; i < NOB_ARRAY_LEN(collections); ++i)
        if (!sync_embedded_header(collections[i], &HeaderSyncAllocType, update)) return false;
    return sync_embedded_header("memzone.h", &HeaderSyncAllocZone, update) &&
           sync_embedded_header("memzone_audit.h", &HeaderSyncAllocZone, update);
}

static const char* HeaderImplementationDefines =
    "#define SHL_ALLOC_IMPLEMENTATION\n"
    "#define SHL_MZ_IMPLEMENTATION\n"
    "#define SHL_MZ_AUDIT_IMPLEMENTATION\n"
    "#define FIXED_POINT_IMPLEMENTATION\n"
    "#define SHL_FLIC_IMPLEMENTATION\n"
    "#define SHL_MEMORY_BUFFER_IMPLEMENTATION\n"
    "#define MINIVOC_IMPLEMENTATION\n"
    "#define MINIWAVE_IMPLEMENTATION\n"
    "#define SHL_WSTR_IMPLEMENTATION\n"
    "#define XMI2MID_IMPLEMENTATION\n";

static const char* HeaderImplementationUndefines =
    "#undef SHL_ALLOC_IMPLEMENTATION\n"
    "#undef SHL_MZ_IMPLEMENTATION\n"
    "#undef SHL_MZ_AUDIT_IMPLEMENTATION\n"
    "#undef FIXED_POINT_IMPLEMENTATION\n"
    "#undef SHL_FLIC_IMPLEMENTATION\n"
    "#undef SHL_MEMORY_BUFFER_IMPLEMENTATION\n"
    "#undef MINIVOC_IMPLEMENTATION\n"
    "#undef MINIWAVE_IMPLEMENTATION\n"
    "#undef SHL_WSTR_IMPLEMENTATION\n"
    "#undef XMI2MID_IMPLEMENTATION\n";

static bool check_header_case(const char* name, const char* const* headers, size_t count,
                              bool zone_allocator, const char* capacity_flags)
{
    bool result = false;
    Nob_String_Builder implementation = {0}, helper = {0}, smoke = {0};
    Nob_Cmd cmd = {0};
    const char* dir = nob_temp_sprintf("build/headers/%s", name);
    const char* main_path = nob_temp_sprintf("%s/main.c", dir);
    const char* helper_path = nob_temp_sprintf("%s/helper.c", dir);
    const char* output = nob_temp_sprintf("%s/check", dir);

    if (!nob_mkdir_if_not_exists(dir)) goto cleanup;
    nob_sb_append_cstr(&implementation, HeaderImplementationDefines);
    if (zone_allocator) nob_sb_append_cstr(&implementation, "#define SHL_TEST_ZONE_ALLOC\n");
    nob_sb_appendf(&implementation, "#define SHL_HEADER_AUDIT_PATH \"%s/audit.log\"\n", dir);

    for (size_t i = 0; i < count; ++i)
    {
        if (!nob_copy_file(headers[i], nob_temp_sprintf("%s/%s", dir, headers[i]))) goto cleanup;
        if (strcmp(headers[i], "memzone_audit.h") == 0 &&
            !nob_copy_file("memzone.h", nob_temp_sprintf("%s/memzone.h", dir))) goto cleanup;
        nob_sb_appendf(&implementation, "#include \"%s\"\n", headers[i]);
    }
    /* Independent implementation guards must tolerate repeated allocator includes. */
    nob_sb_append_cstr(&implementation, "#ifdef SHL_ALLOC_H\n#include \"alloc.h\"\n#endif\n");
    /* STB-style headers can put their implementation outside the public guard.
       Emit it once, then repeat declaration includes in reverse order. */
    nob_sb_append_cstr(&implementation, HeaderImplementationUndefines);
    for (size_t i = count; i > 0; --i)
    {
        nob_sb_appendf(&implementation, "#include \"%s\"\n", headers[i - 1]);
        nob_sb_appendf(&helper, "#include \"%s\"\n", headers[i - 1]);
    }
    nob_sb_append_cstr(&helper,
        "#ifdef SHL_ALLOCATOR_T_DEFINED\n"
        "#include <stdlib.h>\n"
        "static void* custom_malloc(void* ctx, size_t sz) { (void)ctx; return malloc(sz); }\n"
        "static void* custom_realloc(void* ctx, void* ptr, size_t sz) { (void)ctx; return realloc(ptr, sz); }\n"
        "static void custom_free(void* ctx, void* ptr) { (void)ctx; free(ptr); }\n"
        "shl_allocator_t* header_helper_custom_alloc(void) {\n"
        "    static shl_allocator_t alloc = { NULL, custom_malloc, custom_realloc, custom_free };\n"
        "    return &alloc;\n"
        "}\n"
        "#endif\n"
        "#ifdef SHL_ALLOC_H\n"
        "shl_allocator_t* header_helper_alloc(void) { return shl_heap_alloc(); }\n"
        "#endif\n"
        "#ifdef SHL_ZONE_ALLOC_H\n"
        "shl_allocator_t header_helper_zone_alloc(memzone_t* zone) { return shl_zone_alloc(zone); }\n"
        "#endif\n"
        "int header_helper(void) { return 42; }\n");
    if (!nob_read_entire_file("tests/header_smoke.c", &smoke)) goto cleanup;
    nob_sb_append_buf(&implementation, smoke.items, smoke.count);
    if (!nob_write_entire_file(main_path, implementation.items, implementation.count) ||
        !nob_write_entire_file(helper_path, helper.items, helper.count)) goto cleanup;

    /* No repository include path: only the copied headers may satisfy includes. */
    nob_cc(&cmd);
    nob_cmd_append(&cmd, "-std=c99", "-Wall", "-Wextra", "-Werror", "-Wpedantic");
    if (capacity_flags)
        nob_cmd_append(&cmd, capacity_flags);
    nob_cc_output(&cmd, output);
    nob_cmd_append(&cmd, main_path, helper_path, "-lm");
    if (!nob_cmd_run_sync(cmd)) goto cleanup;
    cmd.count = 0;
    nob_cmd_append(&cmd, output);
    result = nob_cmd_run_sync(cmd);

cleanup:
    nob_cmd_free(cmd);
    nob_sb_free(implementation);
    nob_sb_free(helper);
    nob_sb_free(smoke);
    return result;
}

static bool run_header_checks(void)
{
    static const char* headers[] = {
        "alloc.h", "array.h", "binary_heap.h", "fixed_point.h", "flic.h",
        "list.h", "map.h", "memory_buffer.h", "memzone.h",
        "memzone_audit.h", "queue.h", "set.h", "stack.h", "voc.h", "wav.h",
        "wstr.h", "xmi2mid.h"
    };
    static const char* collections[] = {
        "list.h", "stack.h", "queue.h", "binary_heap.h", "map.h", "set.h",
        "alloc.h"
    };
    if (!sync_embedded_headers(false)) return false;
    if (!nob_mkdir_if_not_exists("build") || !nob_mkdir_if_not_exists("build/headers")) return false;

    for (size_t i = 0; i < NOB_ARRAY_LEN(headers); ++i)
        if (!check_header_case(headers[i], &headers[i], 1, false, NULL)) return false;

    /* Every shared-definition provider gets a turn being included first. */
    for (size_t first = 0; first < NOB_ARRAY_LEN(collections); ++first)
    {
        const char* order[NOB_ARRAY_LEN(collections)];
        for (size_t i = 0; i < NOB_ARRAY_LEN(collections); ++i)
            order[i] = collections[(first + i) % NOB_ARRAY_LEN(collections)];
        if (!check_header_case(nob_temp_sprintf("collections-%zu", first), order,
                               NOB_ARRAY_LEN(order), false, NULL)) return false;
    }

    if (!check_header_case("all-forward", headers, NOB_ARRAY_LEN(headers), false, NULL)) return false;
    const char* reverse[NOB_ARRAY_LEN(headers)];
    for (size_t i = 0; i < NOB_ARRAY_LEN(headers); ++i)
        reverse[i] = headers[NOB_ARRAY_LEN(headers) - 1 - i];
    if (!check_header_case("all-reverse", reverse, NOB_ARRAY_LEN(reverse), false, NULL)) return false;

    /* Both versions of the zone allocator must work with every collection. */
    const char* zone_headers[NOB_ARRAY_LEN(collections) + 1];
    for (size_t i = 0; i < NOB_ARRAY_LEN(collections); ++i)
        zone_headers[i + 1] = collections[i];
    zone_headers[0] = "memzone.h";
    if (!check_header_case("zone-collections", zone_headers, NOB_ARRAY_LEN(zone_headers), true, NULL)) return false;
    zone_headers[0] = "memzone_audit.h";
    if (!check_header_case("audit-collections", zone_headers, NOB_ARRAY_LEN(zone_headers), true, NULL)) return false;

    /* The adapter must be available with either header order, even if a collection
       supplied the allocator type before the helpers were requested. */
    static const char* zone_orders[][3] = {
        { "list.h", "alloc.h", "memzone.h" },
        { "list.h", "memzone.h", "alloc.h" },
        { "list.h", "alloc.h", "memzone_audit.h" },
        { "list.h", "memzone_audit.h", "alloc.h" }
    };
    for (size_t i = 0; i < NOB_ARRAY_LEN(zone_orders); ++i)
        if (!check_header_case(nob_temp_sprintf("zone-order-%zu", i), zone_orders[i],
                               NOB_ARRAY_LEN(zone_orders[i]), true, NULL)) return false;

    /* Each capacity is overridden separately via the compiler command line. */
    static const char* capacity_flags[] = {
        "-DSHL_LIST_INITIAL_CAPACITY=3", "-DSHL_STACK_INITIAL_CAPACITY=5",
        "-DSHL_QUEUE_INITIAL_CAPACITY=7", "-DSHL_HEAP_INITIAL_CAPACITY=9",
        "-DSHL_MAP_INITIAL_CAPACITY=2", "-DSHL_SET_INITIAL_CAPACITY=2",
        "-DSHL_MAP_INITIAL_CAPACITY=64", "-DSHL_SET_INITIAL_CAPACITY=64"
    };
    for (size_t i = 0; i < NOB_ARRAY_LEN(capacity_flags); ++i)
        if (!check_header_case(nob_temp_sprintf("capacity-%zu", i), collections,
                               NOB_ARRAY_LEN(collections), false, capacity_flags[i])) return false;

    nob_log(NOB_INFO, "All standalone-header checks passed");
    return true;
}
