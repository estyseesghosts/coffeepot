// flushbench: what does flushing the CPU data cache cost on the 3DS?
//
// Part 1: GSPGPU_FlushDataCache, svcFlushProcessDataCache and
// svcStoreProcessDataCache, in separate cache states (see README):
//   clean-cold   range flushed (cleaned + invalidated) right before timing, so
//                nothing of it is dirty and almost nothing is resident:
//                pure per-call + per-line walk overhead.
//   clean-warm   range <= L1 size, read just before timing so it is resident
//                but clean: walk cost with hits and nothing to write back.
//   dirty-warm   range <= L1 size, every byte written then read back just
//                before timing: all lines resident and dirty. Write-back cost.
//   written-all  whole range of any size written sequentially (every byte)
//                since the last flush; only the cache-resident tail (at most
//                the L1 D-cache size) is still dirty at flush time, the rest
//                was already evicted to memory during the write (not timed).
// Part 2: C3D_FrameEnd(0) versus C3D_FrameEnd(GX_CMDLIST_FLUSH) for a frame
// that only clears the screen.
//
// Output: CSV-style lines on stdout and sdmc:/coffeepot-flushbench.csv.
#include <3ds.h>
#include <citro3d.h>
#include "bench.h"

#define RUNS 15
#define LINE 32u
#define L1_RESIDENT_MAX (16u * 1024u)   // Old 3DS L1 D-cache; ranges up to this are "resident"

extern u32 __ctru_linear_heap, __ctru_linear_heap_size;

typedef enum { OP_GSP, OP_SVC_FLUSH, OP_SVC_STORE, OP_COUNT } FlushOp;
static const char *op_names[OP_COUNT] = { "gsp_flush", "svc_flush", "svc_store" };

typedef enum { ST_CLEAN_COLD, ST_CLEAN_WARM, ST_DIRTY_WARM, ST_WRITTEN_ALL, ST_COUNT } State;
static const char *state_names[ST_COUNT] = { "clean-cold", "clean-warm", "dirty-warm", "written-all" };

static Result do_flush(FlushOp op, void *p, u32 size)
{
    switch (op) {
    case OP_GSP:       return GSPGPU_FlushDataCache(p, size);
    case OP_SVC_FLUSH: return svcFlushProcessDataCache(CUR_PROCESS_HANDLE, (u32)p, size);
    case OP_SVC_STORE: return svcStoreProcessDataCache(CUR_PROCESS_HANDLE, (u32)p, size);
    default:           return -1;
    }
}

// Read one byte per line so the range is resident; returns a value so the
// loop cannot be optimised away.
static u32 touch_read(const volatile u8 *p, u32 size)
{
    u32 sum = 0;
    for (u32 off = 0; off < size; off += LINE) sum += p[off];
    return sum;
}

static void prepare(State st, FlushOp op, u8 *buf, u32 size, int i)
{
    (void)op;
    switch (st) {
    case ST_CLEAN_COLD:
        do_flush(OP_SVC_FLUSH, buf, size);          // clean + invalidate
        break;
    case ST_CLEAN_WARM:
        do_flush(OP_SVC_FLUSH, buf, size);
        (void)touch_read(buf, size);                // resident, clean
        break;
    case ST_DIRTY_WARM:
        do_flush(OP_SVC_FLUSH, buf, size);
        memset(buf, 0x40 + i, size);                // dirty every byte
        (void)touch_read(buf, size);                // and make sure lines are resident
        break;
    case ST_WRITTEN_ALL:
        do_flush(OP_SVC_FLUSH, buf, size);          // known starting state
        memset(buf, 0x40 + i, size);                // sequential write of the whole range
        break;
    default:
        break;
    }
}

static void bench_flushes(u8 *buf, u32 bufsize)
{
    static const u32 sizes[] = {
        1u << 10, 4u << 10, 16u << 10, 64u << 10, 256u << 10,
        1u << 20, 4u << 20, 16u << 20, 32u << 20
    };
    u64 t[RUNS];

    for (unsigned s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
        u32 size = sizes[s];
        if (size > bufsize) break;
        for (int st = 0; st < ST_COUNT; st++) {
            // Resident-state variants are only meaningful when the range fits in L1.
            if ((st == ST_CLEAN_WARM || st == ST_DIRTY_WARM) && size > L1_RESIDENT_MAX) continue;
            for (int op = 0; op < OP_COUNT; op++) {
                Result bad = 0;
                for (int i = 0; i < RUNS; i++) {
                    prepare((State)st, (FlushOp)op, buf, size, i);   // never timed
                    u64 start = svcGetSystemTick();
                    Result rc = do_flush((FlushOp)op, buf, size);
                    t[i] = svcGetSystemTick() - start;
                    if (R_FAILED(rc)) bad = rc;
                }
                if (R_FAILED(bad)) bench_error(op_names[op], bad);
                bench_report("flushbench", op_names[op], state_names[st], size, t, RUNS);
            }
        }
    }
}

static void bench_frame_end(C3D_RenderTarget *target)
{
    static const u8 flags[] = { 0, GX_CMDLIST_FLUSH };
    static const char *names[] = { "flags=0", "CMDLIST_FLUSH" };
    u64 t[RUNS];

    for (int f = 0; f < 2; f++) {
        for (int i = 0; i < RUNS; i++) {
            if (!C3D_FrameBegin(C3D_FRAME_SYNCDRAW)) {
                bench_error("C3D_FrameBegin", -1);
                t[i] = 0;
                continue;
            }
            C3D_RenderTargetClear(target, C3D_CLEAR_ALL, 0x203040FF, 0);
            C3D_FrameDrawOn(target);
            u64 start = svcGetSystemTick();
            C3D_FrameEnd(flags[f]);
            t[i] = svcGetSystemTick() - start;
        }
        bench_report("flushbench", "c3d_frameend", names[f], __ctru_linear_heap_size, t, RUNS);
    }
}

int main(void)
{
    gfxInitDefault();
    consoleInit(GFX_BOTTOM, NULL);

    bool isNew = false;
    bench_begin("flushbench", &isNew);
    bench_line("meta,runs,%d", RUNS);
    bench_line("meta,linear_heap_bytes,%lu", (unsigned long)__ctru_linear_heap_size);
    bench_line("meta,l1_resident_max_bytes,%u", L1_RESIDENT_MAX);

    // Largest power-of-two buffer up to 32 MiB that fits in the free linear heap.
    u32 bufsize = 32u << 20;
    u8 *buf = NULL;
    while (bufsize >= (1u << 10) && !(buf = linearMemAlign(bufsize, LINE))) bufsize >>= 1;

    if (buf) {
        bench_line("meta,test_buffer_bytes,%lu", (unsigned long)bufsize);
        bench_flushes(buf, bufsize);
        linearFree(buf);
    } else {
        bench_error("linearMemAlign", -1);
    }

    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) {
        bench_error("C3D_Init", -1);
    } else {
        C3D_RenderTarget *top = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
        if (!top) {
            bench_error("C3D_RenderTargetCreate", -1);
        } else {
            C3D_RenderTargetSetOutput(top, GFX_TOP, GFX_LEFT,
                GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8));
            bench_frame_end(top);
        }
        C3D_Fini();
    }

    bench_wait_exit();
    gfxExit();
    return 0;
}
