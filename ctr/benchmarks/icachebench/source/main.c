// icachebench: code-footprint probe for the ARM11 instruction cache.
// Executes straight-line ARM code of increasing size (4 KiB .. 64 KiB) and a
// small loop that executes the same number of instructions from a 256-byte
// body, and reports time and (if the kernel allows it) cycles, instruction-
// cache misses and fetch stalls per instruction. Old 3DS is the default target;
// nothing here needs a New 3DS. Output lines: see ../../common/bench.h, plus
//   note,perf,<variant>,<bytes>,<insns_per_run>,<ccnt_med>,<icache_miss_med>,<fetch_stall_med>
// Verification status: builds; not run on hardware.
#include <3ds.h>
#include "bench.h"

#define RUNS 15
#define PASSES 64u

#define STR2(x) #x
#define STR(x) STR2(x)

// A function made of (kib*64) groups of four independent ARM `add`s (16 bytes
// per group, so kib KiB of code), then `bx lr`. r0-r3 are caller-saved.
#define ICB_STRAIGHT(name, kib)                                   \
    __asm__(".pushsection .text.icb_" #name ",\"ax\",%progbits\n" \
            ".arm\n.balign 32\n.global " #name "\n" #name ":\n"   \
            ".rept " STR(kib * 64) "\n"                           \
            "add r0, r0, #1\nadd r1, r1, #1\n"                    \
            "add r2, r2, #1\nadd r3, r3, #1\n"                    \
            ".endr\nbx lr\n.popsection\n");                       \
    extern void name(void)

ICB_STRAIGHT(icb_4k, 4);
ICB_STRAIGHT(icb_8k, 8);
ICB_STRAIGHT(icb_12k, 12);
ICB_STRAIGHT(icb_16k, 16);
ICB_STRAIGHT(icb_20k, 20);
ICB_STRAIGHT(icb_24k, 24);
ICB_STRAIGHT(icb_32k, 32);
ICB_STRAIGHT(icb_48k, 48);
ICB_STRAIGHT(icb_64k, 64);

// Same instruction count as icb_64k (16384) from a 256-byte loop body.
__asm__(".pushsection .text.icb_loop,\"ax\",%progbits\n"
        ".arm\n.balign 32\n.global icb_loop\nicb_loop:\n"
        "mov r12, #256\n"
        "1:\n"
        ".rept 15\n"
        "add r0, r0, #1\nadd r1, r1, #1\nadd r2, r2, #1\nadd r3, r3, #1\n"
        ".endr\n"
        "add r0, r0, #1\nadd r1, r1, #1\nadd r2, r2, #1\n"
        "subs r12, r12, #1\nbne 1b\nbx lr\n.popsection\n");
extern void icb_loop(void);

typedef struct {
    const char *name;
    void (*fn)(void);
    u32 bytes;      // code bytes executed per call (distinct footprint)
    u32 insns;      // instructions executed per call
} Variant;

static const Variant variants[] = {
    { "straight_4k", icb_4k, 4096, 1024 },   { "straight_8k", icb_8k, 8192, 2048 },
    { "straight_12k", icb_12k, 12288, 3072 }, { "straight_16k", icb_16k, 16384, 4096 },
    { "straight_20k", icb_20k, 20480, 5120 }, { "straight_24k", icb_24k, 24576, 6144 },
    { "straight_32k", icb_32k, 32768, 8192 }, { "straight_48k", icb_48k, 49152, 12288 },
    { "straight_64k", icb_64k, 65536, 16384 },
    // ~ icb_loop is 16384 instructions in a 256-byte body (64 instructions + loop control).
    { "loop_256b", icb_loop, 256, 16384 },
};

#define PERF_REGS (BIT(PERFCOUNTERREG_CORE_COUNT_REG_0) | BIT(PERFCOUNTERREG_CORE_COUNT_REG_1) | \
                   BIT(PERFCOUNTERREG_CORE_CYCLE_COUNTER))

static bool perf_begin(void)
{
    u64 unused;
    if (R_FAILED(svcControlPerformanceCounter(&unused, PERFCOUNTEROP_ENABLE, 0, 0))) return false;
    svcControlPerformanceCounter(&unused, PERFCOUNTEROP_SET_VIRTUAL_COUNTER_ENABLED, true, 0);
    svcControlPerformanceCounter(&unused, PERFCOUNTEROP_SET_EVENT, PERFCOUNTERREG_CORE_COUNT_REG_0,
                                 PERFCOUNTEREVT_CORE_INST_CACHE_MISS);
    svcControlPerformanceCounter(&unused, PERFCOUNTEROP_SET_EVENT, PERFCOUNTERREG_CORE_COUNT_REG_1,
                                 PERFCOUNTEREVT_CORE_STALL_BY_LACK_OF_INST);
    svcControlPerformanceCounter(&unused, PERFCOUNTEROP_SET_EVENT, PERFCOUNTERREG_CORE_CYCLE_COUNTER,
                                 PERFCOUNTEREVT_CORE_CYCLE_COUNT);
    return true;
}

static void perf_read(u64 v[3])
{
    svcControlPerformanceCounter(&v[0], PERFCOUNTEROP_GET_VALUE, PERFCOUNTERREG_CORE_CYCLE_COUNTER, 0);
    svcControlPerformanceCounter(&v[1], PERFCOUNTEROP_GET_VALUE, PERFCOUNTERREG_CORE_COUNT_REG_0, 0);
    svcControlPerformanceCounter(&v[2], PERFCOUNTEROP_GET_VALUE, PERFCOUNTERREG_CORE_COUNT_REG_1, 0);
}

static u64 median(u64 *a, int n)
{
    qsort(a, n, sizeof(u64), bench_cmp_u64);
    return a[n / 2];
}

int main(void)
{
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);
    bool isNew;
    bench_begin("icachebench", &isNew);
    bench_line("meta,runs,%d", RUNS);
    bench_line("meta,passes_per_run,%u", PASSES);

    bool have_perf = perf_begin();
    if (have_perf) {
        u64 unused;
        svcControlPerformanceCounter(&unused, PERFCOUNTEROP_RESET, PERF_REGS, PERF_REGS);
    } else {
        bench_line("note,perf_unavailable (svcControlPerformanceCounter ENABLE refused); timing only");
    }

    for (unsigned v = 0; v < sizeof(variants) / sizeof(variants[0]); v++) {
        const Variant *var = &variants[v];
        u64 t[RUNS], cyc[RUNS], miss[RUNS], stall[RUNS];
        var->fn(); // warm-up pass, not timed
        for (int r = 0; r < RUNS; r++) {
            u64 p0[3] = { 0, 0, 0 }, p1[3] = { 0, 0, 0 };
            if (have_perf) perf_read(p0);
            u64 s = svcGetSystemTick();
            for (unsigned p = 0; p < PASSES; p++) var->fn();
            t[r] = svcGetSystemTick() - s;
            if (have_perf) perf_read(p1);
            cyc[r] = p1[0] - p0[0];
            miss[r] = p1[1] - p0[1];
            stall[r] = p1[2] - p0[2];
        }
        u64 tt[RUNS];
        for (int r = 0; r < RUNS; r++) tt[r] = t[r];
        bench_report("icachebench", "footprint", var->name, var->bytes, tt, RUNS);
        unsigned long insns = (unsigned long)var->insns * PASSES;
        if (have_perf)
            bench_line("note,perf,%s,%lu,%lu,%llu,%llu,%llu", var->name, (unsigned long)var->bytes,
                       insns, (unsigned long long)median(cyc, RUNS),
                       (unsigned long long)median(miss, RUNS), (unsigned long long)median(stall, RUNS));
    }

    if (have_perf) {
        u64 unused;
        svcControlPerformanceCounter(&unused, PERFCOUNTEROP_DISABLE, 0, 0);
    }
    bench_wait_exit();
    gfxExit();
    return 0;
}
