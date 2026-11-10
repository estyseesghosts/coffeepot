// membench: CPU write bandwidth and cache-flush cost on the 3DS.
// Prints min / median / max microseconds per operation for several sizes.
#include <3ds.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RUNS 31

extern u32 __ctru_linear_heap, __ctru_linear_heap_size;

static int cmp_u64(const void *a, const void *b)
{
    u64 x = *(const u64 *)a, y = *(const u64 *)b;
    return (x > y) - (x < y);
}

static void report(const char *name, u32 size, u64 *t)
{
    qsort(t, RUNS, sizeof(u64), cmp_u64);
    printf("%-14s %5luK %7.1f %7.1f %7.1f\n", name, size / 1024,
           t[0] / CPU_TICKS_PER_USEC, t[RUNS / 2] / CPU_TICKS_PER_USEC,
           t[RUNS - 1] / CPU_TICKS_PER_USEC);
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
    report("memset heap", size, t);

    for (int i = 0; i < RUNS; i++) {               // CPU fill, linear heap
        u64 s = svcGetSystemTick();
        memset(linear, i, size);
        t[i] = svcGetSystemTick() - s;
        sink += linear[size - 1];
    }
    report("memset linear", size, t);

    for (int i = 0; i < RUNS; i++) {               // copy heap -> linear
        u64 s = svcGetSystemTick();
        memcpy(linear, heap, size);
        t[i] = svcGetSystemTick() - s;
    }
    report("memcpy h->lin", size, t);

    for (int i = 0; i < RUNS; i++) {               // kernel flush of a dirty range
        memset(linear, i, size);
        u64 s = svcGetSystemTick();
        svcFlushProcessDataCache(CUR_PROCESS_HANDLE, (u32)linear, size);
        t[i] = svcGetSystemTick() - s;
    }
    report("svcFlush", size, t);

    for (int i = 0; i < RUNS; i++) {               // GSP flush (IPC) of a dirty range
        memset(linear, i, size);
        u64 s = svcGetSystemTick();
        GSPGPU_FlushDataCache(linear, size);
        t[i] = svcGetSystemTick() - s;
    }
    report("GSPGPU_Flush", size, t);
    (void)sink;
}

int main(void)
{
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

    bool isNew = false;
    APT_CheckNew3DS(&isNew);
    printf("membench  %s 3DS, linear heap %lu KiB\n", isNew ? "New" : "Old",
           __ctru_linear_heap_size / 1024);
    printf("%-14s %6s %7s %7s %7s  (us)\n", "op", "size", "min", "med", "max");

    const u32 max = 1024 * 1024;
    u8 *heap = memalign(32, max);
    u8 *linear = linearMemAlign(max, 32);
    if (!heap || !linear) {
        printf("allocation failed\n");
    } else {
        static const u32 sizes[] = { 16 * 1024, 256 * 1024, 1024 * 1024 };
        for (unsigned i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
            bench_size(heap, linear, sizes[i]);

        u64 t[RUNS];                               // what C3D_FrameEnd(0) does
        for (int i = 0; i < RUNS; i++) {
            u64 s = svcGetSystemTick();
            GSPGPU_FlushDataCache((void *)__ctru_linear_heap, __ctru_linear_heap_size);
            t[i] = svcGetSystemTick() - s;
        }
        report("whole linear", __ctru_linear_heap_size, t);
    }
    printf("\nPress START to exit.\n");

    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & KEY_START) break;
        gspWaitForVBlank();
    }
    linearFree(linear);
    free(heap);
    gfxExit();
    return 0;
}
