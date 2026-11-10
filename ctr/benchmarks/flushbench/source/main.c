// flushbench: what does flushing the CPU data cache cost on the 3DS?
//
// 1. GSPGPU_FlushDataCache, svcFlushProcessDataCache and svcStoreProcessDataCache
//    over 1, 4, 16 and 32 MiB of the linear heap, with the range dirty and clean.
// 2. C3D_FrameEnd(0) (citro3d flushes the whole linear heap) versus
//    C3D_FrameEnd(GX_CMDLIST_FLUSH) (only the command list), for a frame that
//    only clears the screen.
//
// Results: min / median / max in microseconds over RUNS runs, on the bottom screen.
#include <3ds.h>
#include <citro3d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RUNS 15

extern u32 __ctru_linear_heap, __ctru_linear_heap_size;

typedef enum { OP_GSP, OP_SVC_FLUSH, OP_SVC_STORE } FlushOp;
static const char *op_names[] = { "GSP flush", "svc flush", "svc store" };

static int cmp_u64(const void *a, const void *b)
{
    u64 x = *(const u64 *)a, y = *(const u64 *)b;
    return (x > y) - (x < y);
}

static void report(const char *name, const char *variant, u32 kib, u64 *t, int n)
{
    qsort(t, n, sizeof(u64), cmp_u64);
    printf("%-9s %-6s %5luK %8.0f %8.0f %8.0f\n", name, variant, kib,
           t[0] / CPU_TICKS_PER_USEC, t[n / 2] / CPU_TICKS_PER_USEC,
           t[n - 1] / CPU_TICKS_PER_USEC);
}

static void do_flush(FlushOp op, void *p, u32 size)
{
    switch (op) {
    case OP_GSP:       GSPGPU_FlushDataCache(p, size); break;
    case OP_SVC_FLUSH: svcFlushProcessDataCache(CUR_PROCESS_HANDLE, (u32)p, size); break;
    case OP_SVC_STORE: svcStoreProcessDataCache(CUR_PROCESS_HANDLE, (u32)p, size); break;
    }
}

static void bench_flushes(u8 *buf, u32 bufsize)
{
    static const u32 sizes_mib[] = { 1, 4, 16, 32 };
    u64 t[RUNS];

    for (unsigned s = 0; s < sizeof(sizes_mib) / sizeof(sizes_mib[0]); s++) {
        u32 size = sizes_mib[s] << 20;
        if (size > bufsize) break;
        for (int op = OP_GSP; op <= OP_SVC_STORE; op++) {
            for (int dirty = 1; dirty >= 0; dirty--) {
                for (int i = 0; i < RUNS; i++) {
                    if (dirty) {
                        // Touch one word per 32-byte cache line; only the
                        // last 16 KiB can still be in the L1 data cache.
                        for (u32 off = 0; off < size; off += 32) buf[off] = (u8)i;
                    } else {
                        do_flush(OP_SVC_FLUSH, buf, size); // make it clean first
                    }
                    u64 start = svcGetSystemTick();
                    do_flush((FlushOp)op, buf, size);
                    t[i] = svcGetSystemTick() - start;
                }
                report(op_names[op], dirty ? "dirty" : "clean", size >> 10, t, RUNS);
            }
        }
    }
}

static void bench_frame_end(C3D_RenderTarget *target)
{
    static const u8 flags[] = { 0, GX_CMDLIST_FLUSH };
    static const char *names[] = { "flags=0", "CMDLIST" };
    u64 t[RUNS];

    for (int f = 0; f < 2; f++) {
        for (int i = 0; i < RUNS; i++) {
            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
            C3D_RenderTargetClear(target, C3D_CLEAR_ALL, 0x203040FF, 0);
            C3D_FrameDrawOn(target);
            u64 start = svcGetSystemTick();
            C3D_FrameEnd(flags[f]);
            t[i] = svcGetSystemTick() - start;
        }
        report("FrameEnd", names[f], __ctru_linear_heap_size >> 10, t, RUNS);
    }
}

int main(void)
{
    gfxInitDefault();
    consoleInit(GFX_BOTTOM, NULL);
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C3D_RenderTarget *top = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    C3D_RenderTargetSetOutput(top, GFX_TOP, GFX_LEFT,
        GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8));

    bool isNew = false;
    APT_CheckNew3DS(&isNew);
    printf("flushbench %s 3DS\nlinear heap %lu KiB, free %lu KiB\n",
           isNew ? "New" : "Old", __ctru_linear_heap_size >> 10, linearSpaceFree() >> 10);
    printf("%-9s %-6s %6s %8s %8s %8s\n", "op", "state", "size", "min us", "med us", "max us");

    // Largest power-of-two buffer up to 32 MiB that fits in the free linear heap.
    u32 bufsize = 32u << 20;
    u8 *buf = NULL;
    while (bufsize >= (1u << 20) && !(buf = linearMemAlign(bufsize, 32))) bufsize >>= 1;

    if (buf) bench_flushes(buf, bufsize);
    else printf("could not allocate a test buffer\n");
    if (buf) linearFree(buf);

    bench_frame_end(top);

    printf("\nPress START to exit.\n");
    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & KEY_START) break;
        gspWaitForVBlank();
    }
    C3D_Fini();
    gfxExit();
    return 0;
}
