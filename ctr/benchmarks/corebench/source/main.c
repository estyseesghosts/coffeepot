// corebench: do the extra cores of a New 3DS run beside the first one at full speed?
//
// Threads are pinned to application core 0 (the main thread), 2 and 3. Core 1 is the system core and
// is left alone. Each configuration runs a fixed amount of work on every participating core at the same
// moment (all workers wait on a flag, the main thread releases them) and reports the time of the slowest
// one, so perfect scaling is "same time as one core alone".
//   compute: 4,000,000 iterations of four independent adds, subs and bne (no memory traffic)
//   memory:  24 passes of memset over a private 512 KiB buffer per core (all cores write at once)
// Configurations: c0, c2, c3 (alone), c0+c2, c0+c2+c3. A core the kernel will not give this program
// (threadCreate returns NULL) is reported as a note, not an error, and the configurations needing it are
// skipped: whether homebrew can use cores 2 and 3 is itself part of the answer. On an Old 3DS the
// program says so and exits (it has no cores 2 and 3).
//   result,corebench,<compute|memory>,<config>,<cores>,<runs>,<min_us>,<med_us>,<max_us>
// Output otherwise as ../../common/bench.h. Verification status: builds; not run on hardware.
#include <3ds.h>
#include <malloc.h>
#include "bench.h"

#define RUNS 9
#define COMPUTE_ITERS 4000000u
#define MEM_BYTES (512u * 1024u)
#define MEM_PASSES 24u
#define STACK 0x4000

typedef struct {
    int kind;                  // 0 compute, 1 memory
    u8 *buf;
    volatile u64 ticks;
    volatile int done;
} Job;

static volatile int go;

static void work(Job *j)
{
    u64 t0 = svcGetSystemTick();
    if (j->kind == 0) {
        u32 a = 0, b = 0, c = 0, d = 0, n = COMPUTE_ITERS;
        __asm__ volatile("1: add %0, %0, #1\n\tadd %1, %1, #1\n\tadd %2, %2, #1\n\tadd %3, %3, #1\n\t"
                         "subs %4, %4, #1\n\tbne 1b"
                         : "+r"(a), "+r"(b), "+r"(c), "+r"(d), "+r"(n)
                         :
                         : "cc");
    } else {
        for (u32 p = 0; p < MEM_PASSES; p++) memset(j->buf, (int)p, MEM_BYTES);
    }
    j->ticks = svcGetSystemTick() - t0;
    j->done = 1;
}

static void worker(void *arg)
{
    Job *j = (Job *)arg;
    while (!go) svcSleepThread(0);   // wait for the release; yields so the thread can be created on busy cores
    work(j);
}

// cores[i] is the application core (0 = this thread). Returns false if a thread could not be created.
static bool run_config(int kind, const int *cores, int n, u64 *slowest_ticks, Job *jobs, int prio)
{
    Thread th[3] = { NULL, NULL, NULL };
    bool main_works = false;
    go = 0;
    for (int i = 0; i < n; i++) {
        jobs[i].kind = kind;
        jobs[i].done = 0;
        jobs[i].ticks = 0;
        if (cores[i] == 0) { main_works = true; continue; }
        th[i] = threadCreate(worker, &jobs[i], STACK, prio, cores[i], false);
        if (!th[i]) {
            go = 1;   // release any thread already started so it can finish
            for (int k = 0; k < i; k++) if (th[k]) { threadJoin(th[k], U64_MAX); threadFree(th[k]); }
            return false;
        }
    }
    svcSleepThread(2 * 1000 * 1000);   // 2 ms for the workers to reach their wait loops
    __sync_synchronize();
    go = 1;
    if (main_works) for (int i = 0; i < n; i++) if (cores[i] == 0) work(&jobs[i]);
    u64 worst = 0;
    for (int i = 0; i < n; i++) {
        if (th[i]) { threadJoin(th[i], U64_MAX); threadFree(th[i]); }
        if (jobs[i].ticks > worst) worst = jobs[i].ticks;
    }
    *slowest_ticks = worst;
    return true;
}

int main(void)
{
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);
    bool isNew;
    bench_begin("corebench", &isNew);
    if (!isNew) {
        bench_line("note,corebench needs a New 3DS (cores 2 and 3); nothing measured");
        bench_wait_exit();
        gfxExit();
        return 0;
    }
    bench_line("meta,runs,%d", RUNS);
    bench_line("meta,compute_iterations,%u", COMPUTE_ITERS);
    bench_line("meta,memory_bytes_per_core,%u", MEM_BYTES);
    bench_line("meta,memory_passes,%u", MEM_PASSES);

    s32 prio = 0x30;
    svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
    static Job jobs[3];
    for (int k = 0; k < 3; k++) {
        jobs[k].buf = memalign(32, MEM_BYTES);
        if (!jobs[k].buf) bench_error("memalign", -1);
    }

    static const struct { const char *name; int cores[3]; int n; } configs[] = {
        { "c0", { 0, 0, 0 }, 1 },       { "c2", { 2, 0, 0 }, 1 },       { "c3", { 3, 0, 0 }, 1 },
        { "c0+c2", { 0, 2, 0 }, 2 },    { "c0+c2+c3", { 0, 2, 3 }, 3 },
    };
    static const char *const kinds[] = { "compute", "memory" };
    bool core_missing[4] = { false, false, false, false };

    for (int kind = 0; kind < 2; kind++) {
        for (unsigned c = 0; c < sizeof configs / sizeof configs[0]; c++) {
            bool skip = false;
            for (int i = 0; i < configs[c].n; i++) if (core_missing[configs[c].cores[i]]) skip = true;
            if (skip) continue;
            u64 t[RUNS];
            int got = 0;
            for (int r = 0; r < RUNS; r++) {
                u64 worst;
                if (!run_config(kind, configs[c].cores, configs[c].n, &worst, jobs, prio)) {
                    for (int i = 0; i < configs[c].n; i++) core_missing[configs[c].cores[i]] = true;
                    bench_line("note,config %s: threadCreate failed (core not available to this program)", configs[c].name);
                    break;
                }
                t[got++] = worst;
            }
            if (got == RUNS) bench_report("corebench", kinds[kind], configs[c].name, (unsigned long)configs[c].n, t, RUNS);
        }
    }
    for (int k = 0; k < 3; k++) free(jobs[k].buf);
    bench_wait_exit();
    gfxExit();
    return 0;
}
