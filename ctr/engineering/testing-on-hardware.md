# Testing 3DS homebrew on hardware

A program that works in an emulator, or on your own New 3DS, can still crash, stutter or run out of memory on the console your players own. This page describes which devices to test on and why, how to set up a fast test loop on a real console, how to run a performance experiment whose result you can trust, and a small benchmark program for measuring memory and cache costs yourself. Its companion pages are [debugging](debugging.md) and [performance](performance.md).

## Before you start

- A console with [Luma3DS](../homebrew/luma3ds.md) and the Homebrew Menu, on the same network as your PC.
- The benchmark below compiles without warnings with devkitARM r68 and libctru 2.7.1 (`3ds-examples` application template). It has **not** been run on hardware for this page; no results are recorded yet.

## Test tiers

| Tier | Good for | Not evidence of |
| ---- | -------- | --------------- |
| Emulator ([Azahar](debugging.md#emulators)) | Fast iteration, logic bugs, rendering bugs, catching crashes | Speed, timing, memory limits; that a GPU configuration is safe on hardware |
| New 3DS / New 2DS XL | Correctness on hardware, stereo and head-tracking behaviour, New 3DS speed | Old 3DS speed (even in 268 MHz mode, the cache, cores and memory mode differ) |
| Old 3DS / 3DS XL / 2DS | Old 3DS speed and memory limits — the slowest target | — |

If you claim a program runs on "3DS", test on an Old 3DS. If you only have a New 3DS, say so in your release notes. The [models page](../hardware/models.md) lists what each model has; for software the differences that matter most are the CPU and memory ([CPU](../hardware/cpu.md)), wide mode (not on the original 2DS — libctru `gfx.h`), and the New-only head tracking, extra buttons and built-in NFC.

## A fast edit–run loop

1. Build on the PC.
2. On the console, open the Homebrew Menu and press **Y** (netloader).
3. `3dslink -s build/myapp.3dsx` sends the program and keeps a terminal open for its output ([debugging](debugging.md#logging-over-the-network-3dslink)).
4. Exit the program back to the Homebrew Menu, and repeat.

To move larger data sets (RomFS test content, logs) without removing the SD card, an FTP server homebrew such as `ftpd` is the usual choice.

## What to test beyond "it runs"

| Area | Check |
| ---- | ----- |
| System integration | HOME button and return; closing from the HOME Menu; sleep (closing the lid) and wake during gameplay and during loading; power button; low battery |
| Memory | Peak use in the largest level, after long play (leaks, fragmentation); start-up in the smallest memory mode you support ([memory and cache](memory-and-cache.md#how-much-memory-you-get)) |
| Display | 3D slider from off to full and back while running; both screens; wide mode if used |
| Audio | No crackle during loading, scene changes and heavy frames |
| Storage | Missing files, a full SD card, interrupted saves |
| Input | Circle Pad and touch-screen differences between units |

`aptMainLoop()` must be called every frame by one thread; it is how the program learns about HOME, sleep and exit requests.

## Running a performance experiment

A result is only useful if someone else could repeat it. Record:

```text
Hypothesis:   e.g. "Flushing only the dirty vertex range instead of the whole linear heap cuts frame time."
Console:      model (e.g. Old 3DS XL), system version, Luma3DS version, SD card
Build:        commit hash, toolchain/library versions, compiler flags, .3dsx hash
Scene:        exact save file / input script / camera position; warm-up time
Change:       the one variable changed between A and B
Metrics:      frame time median, 95th/99th percentile and worst; per-stage breakdown
Runs:         at least 5 per variant, alternating A and B; first run after a cold boot noted separately
Result:       A vs B with spread; does the difference exceed run-to-run noise?
Correctness:  screenshots or checksums identical (or the accepted difference stated)
```

Rules that keep results honest:

- **Change one thing at a time.** Two simultaneous changes give an uninterpretable result.
- **Use the same scene.** "Faster" in a different level means nothing.
- **Measure the profiler.** Run once with instrumentation off and compare frame times, so you know the measurement is not what you are measuring.
- **Report distributions.** Medians hide hitches; worst-case frames are what players feel.
- **Label the result** with the wiki's [evidence labels](../../AGENTS.md#evidence-labels) when adding it to a page: `[measured]` plus the console and conditions.

## Microbenchmarks worth running

The wiki has no measured numbers yet for the costs below; they are the questions that come up most often when optimising ([performance: open questions](performance.md#open-questions)):

- CPU write bandwidth to the application heap versus the linear heap, at sizes inside and outside the 16 KB L1 data cache.
- The cost of flushing a range with the kernel call versus the GSP service, as a function of size.
- The cost of `C3D_FrameEnd(0)`'s whole-linear-heap flush.
- How these change on a New 3DS with and without `osSetSpeedupEnable(true)`, and with an `APT_SetAppCpuTimeLimit` share active.

### `membench`

This program measures the first three. Put it in `source/main.c` of a copy of `3ds-examples/templates/application`, build, and run it with 3dslink; it prints min / median / max microseconds over 31 runs for each operation and size to the top screen. Add `osSetSpeedupEnable(true);` after `gfxInitDefault()` for the New 3DS fast-mode variant, or `APT_SetAppCpuTimeLimit(80);` to see the effect of a core-1 share on the GSP flush.

```c
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
```

How to read the output:

- `memset heap` vs `memset linear`: whether the linear heap is slower for the CPU to write than the ordinary heap on your model.
- `svcFlush` vs `GSPGPU_Flush`: the kernel call versus the service request, for the same dirty range.
- `whole linear`: the flush `C3D_FrameEnd(0)` performs every frame. Compare it with the frame budget (16,667 µs at 60 fps) to decide whether targeted flushing ([memory and cache](memory-and-cache.md#the-c3d_frameend-flush)) is worth the bookkeeping.

When you have results, add them to the relevant page with the console model, system and Luma3DS versions, and the build details.

## Technical details

- Timing uses `svcGetSystemTick()` (268,111,856 Hz on every model) and `CPU_TICKS_PER_USEC` from `os.h`.
- `__ctru_linear_heap` and `__ctru_linear_heap_size` are the linear heap's start address and size as set up by libctru at start-up ([memory and cache](memory-and-cache.md#how-libctru-splits-it)).
- The benchmark runs on the main thread (core 0); system activity can add outliers, which is why it reports the median and the maximum.

## Pitfalls

- **Testing only on the developer's console.** Units differ in SD card speed, calibration and model.
- **Running benchmarks with Rosalina's debugger attached** or with game plugins active: both add work.
- **Discarding the first run.** Cold caches and data still on the SD card make the first run slower; record it separately rather than silently dropping it.

## Open questions

- No results yet from `membench` on any console. Contributions should record model, system version, Luma3DS version, and whether fast mode or a core-1 share was active.

## Sources

- [3ds-examples](https://github.com/devkitPro/3ds-examples) — application template used to build the benchmark.
- [libctru](https://github.com/devkitPro/libctru) — `os.h` (tick constants), `svc.h`, `services/gspgpu.h`, `services/apt.h`, `allocator/linear.h`, `gfx.h` (wide mode on the original 2DS).
- [citro3d](https://github.com/devkitPro/citro3d) — `source/renderqueue.c` (the whole-heap flush the benchmark reproduces).
- [3ds-hbmenu](https://github.com/devkitPro/3ds-hbmenu) — netloader.
