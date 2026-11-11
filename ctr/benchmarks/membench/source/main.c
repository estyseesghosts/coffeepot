// membench: CPU write bandwidth and cache-flush cost on the 3DS.
// Emits CSV-style lines (see ../../common/bench.h) on stdout and
// sdmc:/coffeepot-membench.csv. The whole-linear-heap flush is reported as
// cold/first, warm-clean and dirtied-chunk separately.
#include <3ds.h>
#include <malloc.h>
#include "bench.h"

#define RUNS 31

extern u32 __ctru_linear_heap, __ctru_linear_heap_size;

static void report(const char *test, u32 size, u64 *t)
{
    bench_report("membench", test, "default", size, t, RUNS);
}

static void bench_size(u8 *heap, u8 *linear, u32 size)
{
    u64 t[RUNS];
    volatile u32 sink = 0;

    for (int i = 0; i < RUNS; i++) {               // CPU fill, normal heap
        u64 s = svcGetSystemTick();
        memset(heap, i, size);
        t[i] = svcGetSystemTick() - s;
        sink += heap[size - 1];
    }
    report("memset_heap", size, t);

    for (int i = 0; i < RUNS; i++) {               // CPU fill, linear heap
        u64 s = svcGetSystemTick();
        memset(linear, i, size);
        t[i] = svcGetSystemTick() - s;
        sink += linear[size - 1];
    }
    report("memset_linear", size, t);

    for (int i = 0; i < RUNS; i++) {               // copy heap -> linear
        u64 s = svcGetSystemTick();
        memcpy(linear, heap, size);
        t[i] = svcGetSystemTick() - s;
    }
    report("memcpy_heap_to_linear", size, t);

    for (int i = 0; i < RUNS; i++) {               // kernel flush of a dirty range
        memset(linear, i, size);
        u64 s = svcGetSystemTick();
        Result rc = svcFlushProcessDataCache(CUR_PROCESS_HANDLE, (u32)linear, size);
        t[i] = svcGetSystemTick() - s;
        if (R_FAILED(rc)) bench_error("svcFlushProcessDataCache", rc);
    }
    report("svc_flush_written", size, t);

    for (int i = 0; i < RUNS; i++) {               // GSP flush (IPC) of a dirty range
        memset(linear, i, size);
        u64 s = svcGetSystemTick();
        Result rc = GSPGPU_FlushDataCache(linear, size);
        t[i] = svcGetSystemTick() - s;
        if (R_FAILED(rc)) bench_error("GSPGPU_FlushDataCache", rc);
    }
    report("gsp_flush_written", size, t);
    (void)sink;
}

// GSPGPU_FlushDataCache over the whole linear heap (what C3D_FrameEnd(0) does),
// reported as three separate cache states. Only `chunk` (the largest block we
// could allocate from the free linear heap) is ever written, so "dirtied"
// means "chunk rewritten", not "every byte of the heap".
static void bench_whole_heap(u8 *chunk, u32 chunk_size)
{
    u64 t[RUNS];
    Result rc;

    // 1. cold/first: chunk freshly written, first flush of the whole heap.
    memset(chunk, 0x11, chunk_size);
    u64 s = svcGetSystemTick();
    rc = GSPGPU_FlushDataCache((void *)__ctru_linear_heap, __ctru_linear_heap_size);
    t[0] = svcGetSystemTick() - s;
    if (R_FAILED(rc)) bench_error("GSPGPU_FlushDataCache(whole)", rc);
    bench_line("result,membench,gsp_flush_whole,cold-first,%lu,1,%.2f,%.2f,%.2f",
               (unsigned long)__ctru_linear_heap_size, t[0] / CPU_TICKS_PER_USEC,
               t[0] / CPU_TICKS_PER_USEC, t[0] / CPU_TICKS_PER_USEC);

    // 2. warm-clean: repeated flushes, nothing rewritten in between.
    for (int i = 0; i < RUNS; i++) {
        s = svcGetSystemTick();
        rc = GSPGPU_FlushDataCache((void *)__ctru_linear_heap, __ctru_linear_heap_size);
        t[i] = svcGetSystemTick() - s;
        if (R_FAILED(rc)) bench_error("GSPGPU_FlushDataCache(whole)", rc);
    }
    bench_report("membench", "gsp_flush_whole", "warm-clean", __ctru_linear_heap_size, t, RUNS);

    // 3. dirtied: chunk rewritten (untimed) before every call.
    for (int i = 0; i < RUNS; i++) {
        memset(chunk, 0x20 + i, chunk_size);
        s = svcGetSystemTick();
        rc = GSPGPU_FlushDataCache((void *)__ctru_linear_heap, __ctru_linear_heap_size);
        t[i] = svcGetSystemTick() - s;
        if (R_FAILED(rc)) bench_error("GSPGPU_FlushDataCache(whole)", rc);
    }
    bench_report("membench", "gsp_flush_whole", "dirtied-chunk", __ctru_linear_heap_size, t, RUNS);
    bench_line("meta,dirtied_chunk_bytes,%lu", (unsigned long)chunk_size);
}

int main(void)
{
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

    bool isNew = false;
    bench_begin("membench", &isNew);
    bench_line("meta,runs,%d", RUNS);
    bench_line("meta,linear_heap_bytes,%lu", (unsigned long)__ctru_linear_heap_size);

    const u32 max = 1024 * 1024;
    u8 *heap = memalign(32, max);
    u8 *linear = linearMemAlign(max, 32);
    if (!heap) bench_error("memalign", -1);
    if (!linear) bench_error("linearMemAlign", -1);

    if (heap && linear) {
        // Largest remaining linear block, for the whole-heap experiment.
        u32 chunk_size = linearSpaceFree() & ~0xFFFu;
        u8 *chunk = NULL;
        while (chunk_size >= max && !(chunk = linearMemAlign(chunk_size, 32)))
            chunk_size -= max;
        if (!chunk) bench_error("linearMemAlign(chunk)", -1);
        if (chunk) {
            bench_whole_heap(chunk, chunk_size);
            static const u32 sizes[] = { 16 * 1024, 256 * 1024, 1024 * 1024 };
            for (unsigned i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
                bench_size(heap, linear, sizes[i]);
            linearFree(chunk);
        }
    }

    bench_wait_exit();
    if (linear) linearFree(linear);
    free(heap);
    gfxExit();
    return 0;
}
