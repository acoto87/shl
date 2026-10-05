/* Appended after the copied header includes by nob's isolated-header checks. */
#include <assert.h>

int header_helper(void);

#ifdef SHL_ALLOCATOR_T_DEFINED
shl_allocator_t* header_helper_custom_alloc(void);
#endif
#ifdef SHL_ALLOC_H
shl_allocator_t* header_helper_alloc(void);
#endif
#ifdef SHL_ZONE_ALLOC_H
shl_allocator_t header_helper_zone_alloc(memzone_t* zone);
#endif

#ifdef SHL_LIST_H
shlDeclareList(HeaderList, int)
shlDefineList(HeaderList, int)
#endif
#ifdef SHL_STACK_H
shlDeclareStack(HeaderStack, int)
shlDefineStack(HeaderStack, int)
#endif
#ifdef SHL_QUEUE_H
shlDeclareQueue(HeaderQueue, int)
shlDefineQueue(HeaderQueue, int)
#endif
#ifdef SHL_HEAP_H
shlDeclareBinaryHeap(HeaderHeap, int)
shlDefineBinaryHeap(HeaderHeap, int)
static int32_t header_compare(int a, int b) { return (a > b) - (a < b); }
#endif
#if defined(SHL_MAP_H) || defined(SHL_SET_H)
static uint32_t header_hash(int value) { return (uint32_t)value; }
static bool header_equals(int a, int b) { return a == b; }
#endif
#ifdef SHL_MAP_H
shlDeclareMap(HeaderMap, int, int)
shlDefineMap(HeaderMap, int, int)
#endif
#ifdef SHL_SET_H
shlDeclareSet(HeaderSet, int)
shlDefineSet(HeaderSet, int)
#endif
#ifdef SHL_ARRAY_H
shlDeclareCreateArray(Header, int)
shlDefineCreateArray(Header, int)
shlDeclareFreeArray(Header, int)
shlDefineFreeArray(Header, int)
#endif

#ifdef FIXED_POINT_H
/* Including math.h after fixed_point.h must also preserve its standard macro. */
#include <math.h>
#endif

#ifdef SHL_ALLOCATOR_T_DEFINED
static void header_check_allocator(shl_allocator_t* alloc)
{
    if (alloc)
    {
        void* p = alloc->mallocFn(alloc->ctx, 16);
        assert(p);
        p = alloc->reallocFn(alloc->ctx, p, 32);
        assert(p);
        alloc->freeFn(alloc->ctx, p);
    }

#ifdef SHL_LIST_H
    HeaderList list;
    HeaderListInit(&list, alloc);
    assert(list.capacity == SHL_LIST_INITIAL_CAPACITY && list.alloc);
    assert(!alloc || list.alloc == alloc);
    int list_count = SHL_LIST_INITIAL_CAPACITY + 32;
    for (int i = 0; i < list_count; ++i) HeaderListAdd(&list, i);
    assert(list.count == list_count && HeaderListGet(&list, list_count - 1) == list_count - 1);
    HeaderListFree(&list);
    int fixed[2];
    HeaderListInitFixed(&list, fixed, 2);
    HeaderListAdd(&list, 42);
    assert(HeaderListGet(&list, 0) == 42);
    HeaderListFree(&list);
#endif
#ifdef SHL_STACK_H
    HeaderStack stack;
    HeaderStackInit(&stack, alloc);
    assert(stack.capacity == SHL_STACK_INITIAL_CAPACITY && stack.alloc);
    int stack_count = SHL_STACK_INITIAL_CAPACITY + 32;
    for (int i = 0; i < stack_count; ++i) HeaderStackPush(&stack, i);
    for (int i = stack_count - 1; i >= 0; --i) assert(HeaderStackPop(&stack) == i);
    HeaderStackFree(&stack);
#endif
#ifdef SHL_QUEUE_H
    HeaderQueue queue;
    HeaderQueueInit(&queue, alloc);
    assert(queue.capacity == SHL_QUEUE_INITIAL_CAPACITY && queue.alloc);
    int queue_count = SHL_QUEUE_INITIAL_CAPACITY + 32;
    int popped = SHL_QUEUE_INITIAL_CAPACITY / 2;
    for (int i = 0; i < SHL_QUEUE_INITIAL_CAPACITY; ++i) HeaderQueuePush(&queue, i);
    for (int i = 0; i < popped; ++i) assert(HeaderQueuePop(&queue) == i);
    for (int i = SHL_QUEUE_INITIAL_CAPACITY; i < queue_count; ++i) HeaderQueuePush(&queue, i);
    for (int i = popped; i < queue_count; ++i) assert(HeaderQueuePop(&queue) == i);
    HeaderQueueFree(&queue);
#endif
#ifdef SHL_HEAP_H
    HeaderHeap heap;
    HeaderHeapInit(&heap, alloc, header_compare);
    assert(heap.capacity == SHL_HEAP_INITIAL_CAPACITY && heap.alloc);
    int heap_count = SHL_HEAP_INITIAL_CAPACITY + 32;
    for (int i = heap_count - 1; i >= 0; --i) HeaderHeapPush(&heap, i);
    for (int i = 0; i < heap_count; ++i) assert(HeaderHeapPop(&heap) == i);
    HeaderHeapFree(&heap);
#endif
#ifdef SHL_MAP_H
    HeaderMap map;
    HeaderMapInit(&map, alloc, header_hash, header_equals);
    assert(map.capacity == SHL_MAP_INITIAL_CAPACITY && map.alloc);
    int map_count = SHL_MAP_INITIAL_CAPACITY + 32;
    for (int i = 0; i < map_count; ++i) HeaderMapSet(&map, i, i + 1);
    for (int i = 0; i < map_count; ++i) assert(HeaderMapGet(&map, i) == i + 1);
    HeaderMapFree(&map);
#endif
#ifdef SHL_SET_H
    HeaderSet set;
    HeaderSetInit(&set, alloc, header_hash, header_equals);
    assert(set.capacity == SHL_SET_INITIAL_CAPACITY && set.alloc);
    int set_count = SHL_SET_INITIAL_CAPACITY + 32;
    for (int i = 0; i < set_count; ++i) assert(HeaderSetAdd(&set, i));
    for (int i = 0; i < set_count; ++i) assert(HeaderSetContains(&set, i));
    HeaderSetFree(&set);
#endif
}
#endif

int main(void)
{
    assert(header_helper() == 42);
#ifdef FIXED_POINT_H
    assert(SHL_FP_ZERO == fp_fromInt(0));
    assert(fpclassify(0.0) == FP_ZERO);
#endif
#ifdef SHL_ALLOCATOR_T_DEFINED
    header_check_allocator(NULL);
    header_check_allocator(header_helper_custom_alloc());
#endif
#ifdef SHL_ALLOC_H
    /* The implementation pattern exports one stable allocator across TUs. */
    assert(shl_heap_alloc() == header_helper_alloc());
    header_check_allocator(shl_heap_alloc());
    header_check_allocator(header_helper_alloc());
#endif
#ifdef SHL_MZ_H
#if defined(SHL_TEST_ZONE_ALLOC) && defined(SHL_MZ_AUDIT_H)
    memzone_t* zone = mz_initAudit(1 << 20, SHL_MZ_AUDIT_FORMAT_COMPACT, SHL_HEADER_AUDIT_PATH);
#else
    memzone_t* zone = mz_init(1 << 20);
#endif
    assert(zone);
#ifdef SHL_TEST_ZONE_ALLOC
    shl_allocator_t alloc = shl_zone_alloc(zone);
    header_check_allocator(&alloc);
    shl_allocator_t other = header_helper_zone_alloc(zone);
    assert(other.ctx == alloc.ctx && other.mallocFn == alloc.mallocFn);
#ifdef SHL_MZ_AUDIT_H
    mz_auditClose(zone);
    FILE* log = fopen(SHL_HEADER_AUDIT_PATH, "r");
    assert(log);
    char events[8192];
    size_t length = fread(events, 1, sizeof(events) - 1, log);
    events[length] = '\0';
    fclose(log);
    assert(strstr(events, "ALLOC") && strstr(events, "REALLOC") && strstr(events, "FREE"));
    remove(SHL_HEADER_AUDIT_PATH);
#endif
#endif
    void* p = mz_alloc(zone, 32);
    assert(p && mz_contains(zone, p));
    mz_free(zone, p);
    assert(mz_validate(zone));
#ifdef SHL_MZ_AUDIT_H
    mz_auditFlush(zone);
    mz_auditClose(zone);
#endif
    mz_destroy(zone);
#endif
#ifdef SHL_ARRAY_H
    int** array = HeaderCreateArray(2, 2);
    array[1][1] = 42;
    assert(array[1][1] == 42);
    HeaderFreeArray(array);
#endif
    return 0;
}
