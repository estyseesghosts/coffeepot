# Performance engineering for 3DS homebrew

Making a 3DS program fast is mostly about finding out *which* part is slow before changing anything. The console has a slow CPU (268 MHz on an Old 3DS; **New 3DS:** up to 804 MHz), a capable but unusual GPU, and a small amount of fast memory; a frame can be late because the CPU ran out of time, because the GPU did, because one waited for the other, or because something stalled on a file or a system service. This page shows how to measure each of those with the libraries homebrew already uses, how to pace frames, how to use the second CPU core, and which CPU-side habits pay off on this processor.

Related pages: [memory and cache](memory-and-cache.md) (where buffers live and what flushing costs), [GPU rendering](gpu-rendering.md) (draw-call and state costs), [testing on hardware](testing-on-hardware.md) (how to run a benchmark you can trust).

## Before you start

- Checked against libctru 2.7.1, citro3d 1.7.1, devkitARM r68 and Luma3DS v13.5 source.
- Claims carry the [evidence labels](../../AGENTS.md#evidence-labels) used across the wiki. Nothing on this page is `[measured]` by the wiki itself yet; where a number comes from someone else's measurement, it is labelled with its origin.
- Decide your target hardware first. An [Old 3DS](../glossary.md#old-new-3ds) has two slow cores and no L2 cache. **New 3DS:** the CPU can run 3× faster with a 2 MB L2 cache, and a third core is open to `.3dsx` homebrew. A program tuned on a New 3DS tells you nothing about an Old 3DS.

## The budget

At 60 frames per second a frame lasts 16.7 ms. The ARM11 application core on an Old 3DS runs at 268,111,856 Hz ([CPU](../hardware/cpu.md#clocks-and-core-counts)), so one frame is about 4.47 million clock cycles — before the operating system, cache misses, waiting for the GPU and interrupt handling take their share.

> **New 3DS:** with the fast mode enabled the core runs at 804 MHz (3×) with a 2 MB L2 cache, so a frame is about 13.4 million cycles at 60 fps.

| Target | Frame time | Old 3DS core-0 cycles per frame |
| ------ | ---------- | ------------------------------- |
| 60 fps | 16.7 ms | ≈ 4.47 M |
| 30 fps | 33.3 ms | ≈ 8.94 M |
| 20 fps | 50 ms | ≈ 13.4 M |

Commercial games treated these budgets as design constraints from the start; homebrew ports of games written for faster machines usually have to do the same (reduce work, not just optimise it).

### New 3DS: turn on the fast mode yourself

> **New 3DS:** this section applies only to a New 3DS; an Old 3DS has no fast mode.

A `.3dsx` started by Luma3DS gets a generated **[extended header](../glossary.md#exheader)** (exheader) in which the 804 MHz and L2-cache flags are **off** [documented: Luma3DS `sysmodules/loader/source/hbldr.c`, `hbldrPatchExHeaderInfo`]. Call `osSetSpeedupEnable(true)` at start-up; it asks the system (`PTMSYSM_ConfigureNew3DSCPU(3)`) for both the higher clock and the L2 cache and does nothing harmful on an Old 3DS ([libctru](../homebrew/libctru.md#clocks-and-the-new-3ds-speed-up)). An installed `.cia` can instead request them in its exheader. The user's Luma3DS setting `app_launch_new_3ds_cpu` can also force them per title.

## Measuring time

### CPU time: the system tick

`svcGetSystemTick()` returns a 64-bit counter running at the base ARM11 clock, `SYSCLOCK_ARM11` = 268,111,856 Hz, on all models and in every New 3DS clock mode [documented: libctru `os.h` defines `CPU_TICKS_PER_MSEC` from `SYSCLOCK_ARM11`]. Reading it is a system call, so it is cheap but not free; time blocks of work, not individual pixels.

`TickCounter` wraps it: `osTickCounterStart(&c)` records a reference, `osTickCounterUpdate(&c)` stores the elapsed ticks, `osTickCounterRead(&c)` returns milliseconds as a `double` [documented: `os.h`, `source/os.c`]. Keep raw `u64` ticks in hot code and convert once per report: on the ARM11, 64-bit division and `double` arithmetic are comparatively expensive (see [CPU-side habits](#cpu-side-habits)).

```c
#include <3ds.h>

typedef struct { u64 total, worst; u32 count; } Stat;

static inline void stat_add(Stat *s, u64 ticks)
{
    s->total += ticks; s->count++;
    if (ticks > s->worst) s->worst = ticks;
}

/* in the frame loop */
u64 t0 = svcGetSystemTick();
update_game();
stat_add(&update_stat, svcGetSystemTick() - t0);

/* once per second, outside the hot path */
printf("update avg %.2f ms worst %.2f ms\n",
       update_stat.total / (double)update_stat.count / CPU_TICKS_PER_MSEC,
       update_stat.worst / CPU_TICKS_PER_MSEC);
```

Report the **worst** frame and percentiles, not only the average: an average of 12 ms can hide a 200 ms hitch every few seconds, and the hitch is what players notice.

### citro3d's two timers

citro3d keeps two `TickCounter`s and exposes them in milliseconds [documented: citro3d `source/renderqueue.c`]:

| Function | What it measures | What it does *not* include |
| -------- | ---------------- | -------------------------- |
| `C3D_GetProcessingTime()` | CPU time from the moment `C3D_FrameBegin` *returns* (after it has waited for the GPU) until `C3D_FrameEnd` has queued the frame — i.e. the CPU cost of recording your draw calls, plus anything else you did between those two calls | Work done before `C3D_FrameBegin` (game logic, if you put it there), and the time `C3D_FrameBegin` spent waiting |
| `C3D_GetDrawingTime()` | Wall-clock time from `C3D_FrameEnd` starting the GX command queue until the queue-finished callback runs: the GPU executing the command list **plus** the display transfers to the LCD framebuffers queued for that frame, plus the latency of the completion interrupt reaching your process | Anything that happened on a frame that has not finished yet: the value refers to the most recently *completed* frame |

`C3D_FrameEnd` also flushes the CPU data cache over the whole linear heap unless you pass `GX_CMDLIST_FLUSH` (below and in [memory and cache](memory-and-cache.md#the-c3d_frameend-flush)); that flush happens after `C3D_GetProcessingTime`'s counter is stopped, so it shows up in neither number. Time it separately if you suspect it.

### Per-operation GPU times: the GSP performance log

Since libctru 2.5.0, `GSPGPU_SetPerfLogMode(true)` and `GSPGPU_GetPerfLog(&log)` read a performance log kept by the system's GPU service (**[GSP](../glossary.md#gsp)**): for each GPU operation type (`GSPGPU_EVENT_P3D` command-list processing, `GSPGPU_EVENT_PPF` display transfer/texture copy, `GSPGPU_EVENT_PSC0`/`PSC1` memory fills, `VBlank0/1`), the duration of the last one and a running total in microseconds [documented: libctru `gspgpu.h`]. This separates "the GPU is slow drawing" (P3D) from "copying to the screen is slow" (PPF) from "clearing is slow" (PSC). The header's notes matter:

- Use differences of `totalDurationUs` between two points you choose; a frame contains several operations of each type, and the total can overflow.
- For a memory fill that uses both fill units, take the larger of the two "last" values.
- The first frame after enabling the log has nonsense VBlank entries; very short fills may finish before the system records their start.
- Enable it while no GPU operation is in progress.

### A frame-level breakdown

With those three sources you can fill in this table for any frame; it is the starting point for every optimisation decision on this page:

| Column | How |
| ------ | --- |
| Logic (CPU) | ticks around your update code |
| Record (CPU) | `C3D_GetProcessingTime()` |
| Wait for GPU | ticks around `C3D_FrameBegin` |
| GPU draw | P3D delta from the perf log, or `C3D_GetDrawingTime()` minus transfers |
| Transfers / clears | PPF and PSC deltas |
| Frame interval | ticks between successive `C3D_FrameBegin` returns |

## Finding the bottleneck

| Observation | Likely bottleneck | Next step |
| ----------- | ----------------- | --------- |
| Logic + Record ≈ frame interval, Wait ≈ 0 | **CPU** | Profile the CPU work by subsystem; see [CPU-side habits](#cpu-side-habits) and [draw-call costs](gpu-rendering.md#what-a-draw-call-costs) |
| Wait for GPU large, GPU draw ≈ frame interval | **GPU** | Fewer pixels (smaller targets, less overdraw, no stereo when the slider is off), fewer fragments with expensive features, smaller textures — [GPU rendering](gpu-rendering.md) |
| Both CPU and GPU well under budget, but frames still missed | **Synchronisation** — the two take turns instead of overlapping, or you wait for V-blank at the wrong moment | Restructure the frame loop ([frame pacing](frame-pacing.md#overlapping-cpu-and-gpu)) |
| Occasional single long frames | **Hitches**: file reads, texture uploads, allocation, a full cache flush, a system service call | Log the worst frames with their stage breakdown; move loading off the frame thread |
| Audio crackles while frames look fine | Audio refill thread starved or blocked | [Audio](#audio-deadlines) |

Do not optimise a stage that is not on the critical path: halving GPU time does nothing for a CPU-bound frame.

## Frame pacing

Structure the loop so game logic runs *before* `C3D_FrameBegin` (overlapping the GPU's work on the previous frame), lock to 60 or a deliberate 30 with `C3D_FRAME_SYNCDRAW` and `C3D_FrameRate`, and run the simulation on a fixed time step independent of rendering. Details, loop code and missed-frame detection: [frame pacing](frame-pacing.md).

## Using the second core

On an Old 3DS a program can borrow a time slice of the system core for one thread after `APT_SetAppCpuTimeLimit`. **New 3DS:** a third core is fully available to `.3dsx` homebrew. Large core-1 shares slow down the system modules your program calls (a third party measured `DSP_FlushDataCache` at 1–5 ms with an 80% share). Placement, priorities, synchronisation and a worker pattern: [threading](threading.md).

## Audio deadlines

The DSP plays whatever is queued and stops when the queue runs dry. Keep enough buffered audio to cover your worst frame, refill from a thread that never waits for the GPU, and flush each wave buffer before queueing it: [audio](audio.md).

## CPU-side habits

The ARM11 is an ARMv6K core with no NEON, no hardware integer divide, half-rate double-precision multiplies and 16 KB L1 caches (no L2 on Old 3DS). Use `float`, avoid division and random memory access in hot loops, check the generated code, and use the hardware performance counters to find cache misses and mispredicted branches: [CPU optimisation](cpu-optimization.md).

## Real-hardware profiling cookbook

A checklist for getting numbers from an Old 3DS that someone else can trust. Every recipe here is a method; none has been run for this wiki, and no figure in it is `[measured]`. Record the results in the format on [testing on hardware](testing-on-hardware.md#benchmark-record-format).

### Reliable CPU timing

- Time with `svcGetSystemTick()` around *blocks* of work, never around a few instructions (the call is a system call). Keep raw `u64` ticks; convert once at the end.
- Repeat a measured block enough that it lasts at least ~1 ms (about 270,000 ticks), and take the **median** of 15 or more repeats, plus the maximum. The first repeat is cold (caches, page tables); keep it as its own number.
- Make the compiler unable to delete the work (consume results through a `volatile` or a checksum).
- For cycle-level questions (cache misses, mispredicted branches) use the CPU's performance counters ([CPU optimisation](cpu-optimization.md#hardware-performance-counters)); whether they work from a `.3dsx` is an open question, so have a ticks-only fallback. The counters are per core and also count kernel and interrupt time.
- Pin the thing you measure: one core, one thread, no other homebrew running, debugger detached, game plugins off.

### Reliable GPU timing

- `C3D_GetDrawingTime()` and the GSP performance log give GPU-side durations ([above](#per-operation-gpu-times-the-gsp-performance-log)). Neither separates GPU execution from the latency of the completion interrupt reaching your process; treat small differences as noise until a baseline run shows the spread.
- GPU work is asynchronous: timing the CPU call that *submits* a draw tells you about recording, not rendering. To time the GPU, measure frame interval with the CPU deliberately idle (an empty logic loop) so the GPU is the bottleneck, then vary only the GPU workload.
- Isolate by elimination: render at a smaller target, skip the draw, or draw the same geometry twice, and see which change moves the frame interval ([finding the bottleneck](#finding-the-bottleneck)).

### Work time versus waiting for V-blank

A frame that "takes 16.7 ms" because it waited 12 ms for V-blank has 12 ms of headroom. Log both: **work** = logic + record + `C3D_FrameEnd` ticks; **wait** = ticks inside `C3D_FrameBegin` (or `gspWaitForVBlank`). Report work time and the frame interval separately; the ratio tells you the headroom. Never conclude anything from a vsynced frame rate alone: 60 fps is the ceiling, so an optimisation shows only in work time until the frame goes over budget ([frame pacing](frame-pacing.md#detecting-missed-frames)).

### Frame-time distribution and missed frames

Record the interval between successive frame starts for a fixed scene (1,800 frames is 30 s at 60 fps), sort once at the end, and report p50, p95, p99, worst and the count of missed refresh slots. This helper compiles with the wiki's `bench.h` (it uses the same `CPU_TICKS_PER_USEC` and `bench_line`):

```c
#include <3ds.h>
#include <stdlib.h>
#include "bench.h"

#define NFRAMES 1800                 /* 30 s at 60 fps; no allocation in the loop */
static u64 interval[NFRAMES];        /* ticks between successive frame starts */

static int cmp_u64(const void *a, const void *b)
{
    u64 x = *(const u64 *)a, y = *(const u64 *)b;
    return (x > y) - (x < y);
}

static unsigned frames_seen;
static u64 last_start;

/* Call once per frame at the same point (e.g. right after C3D_FrameBegin returns). */
static void frame_mark(void)
{
    u64 now = svcGetSystemTick();
    if (frames_seen > 0 && frames_seen <= NFRAMES) interval[frames_seen - 1] = now - last_start;
    last_start = now;
    frames_seen++;
}

/* After the run: one machine-readable line. A slot is one nominal refresh period;
   an interval of k slots counts k-1 missed. */
static void frame_report(const char *program, const char *variant, u64 nominal_ticks)
{
    unsigned n = frames_seen > NFRAMES ? NFRAMES : (frames_seen ? frames_seen - 1 : 0);
    if (n == 0) return;
    unsigned long missed = 0;
    u64 worst = 0;
    for (unsigned i = 0; i < n; i++) {
        u64 slots = (interval[i] + nominal_ticks / 2) / nominal_ticks;
        if (slots > 1) missed += (unsigned long)(slots - 1);
        if (interval[i] > worst) worst = interval[i];
    }
    qsort(interval, n, sizeof(u64), cmp_u64);
    bench_line("note,pct,%s,%s,%u,%.2f,%.2f,%.2f,%.2f,%lu", program, variant, n,
               interval[n / 2] / CPU_TICKS_PER_USEC,
               interval[(n * 95) / 100] / CPU_TICKS_PER_USEC,
               interval[(n * 99) / 100] / CPU_TICKS_PER_USEC,
               worst / CPU_TICKS_PER_USEC, missed);
}
/* nominal_ticks for 60 fps: (u64)(SYSCLOCK_ARM11 / 60) = 4,468,530. */
```

Output line: `note,pct,<program>,<variant>,<frames>,<p50_us>,<p95_us>,<p99_us>,<worst_us>,<missed_slots>`. Verification: builds with devkitARM r68 and libctru 2.7.1 (`-Wall -Wextra -O2`, inside a stub `main`); not run on hardware. The nominal period is an assumption: the LCD refresh rate is often quoted as about 59.83 Hz and has not been confirmed here ([frame pacing: open questions](frame-pacing.md#open-questions)), so check how many "missed" slots an idle loop reports before trusting the count.

### Synchronous SD logging causes hitches

Writing a log to the SD card from the frame thread blocks it for as long as the file system service takes. `[reported]` The G-Diffuser postmortem records synchronous log flushes of 26-270 ms on the main thread of an Old 3DS build (09-30 entries), "lots of awkward hitching", and no hitches after moving to an asynchronous log; a later never-blocking log gave a race median of 16.5 ms with 0 race hitches (10-02) ([source](https://hypnotize.works/gdx-3ds/postmortem/native.html), read directly; its game and conditions only). Rules that follow regardless of the figures: accumulate samples in memory during the run and write after it (as `bench.h` does), or hand lines to a low-priority writer thread through a bounded queue that drops rather than blocks, and never `printf` per frame.

### Counters, command capture and regression tests

- **Counters:** CPU events per core via `svcControlPerformanceCounter` ([CPU optimisation](cpu-optimization.md#hardware-performance-counters)); GPU durations via the GSP performance log (above). Count events per unit of work, not totals.
- **Command capture:** to see what a frame sent to the GPU, dump the command list and compare counts between variants. The command-list format, a capture hook and a decoder are on [PICA200 command pipeline](pica200-command-pipeline.md); this page does not duplicate them. G-Diffuser reports reducing GX entries per frame from 990 to 771 and completion interrupts from 550 to 331 by submitting both eyes together (1P 3D race, Old 3DS, 10-26 entries) `[reported]`; the same page calls some of its other results "suggestive" (two blocks per arm), which is why this wiki records the counts, not a speed-up, from it.
- **Regression tests:** keep one fixed scene (input script, camera, seed) and run it on every performance-relevant change. A regression test is a stored `note,pct` line plus a stored checksum of the final framebuffer (or a screenshot); fail the change if p99 worsens by more than the run-to-run spread of the stored baseline, or if the checksum differs without an accepted reason.
- **Reporting:** attach the raw `coffeepot-<program>.csv`, not a screenshot of averages; label the claim `[measured]` only with console model, firmware and build ([record format](testing-on-hardware.md#benchmark-record-format)).

## Technical details

- `SYSCLOCK_ARM11` = 268,111,856 Hz; `CPU_TICKS_PER_MSEC` = `SYSCLOCK_ARM11 / 1000.0`; **New 3DS:** its clocks are 2× and 3× that ([libctru](../homebrew/libctru.md#clocks-and-the-new-3ds-speed-up)). The system tick does not change rate with the New 3DS clock (libctru defines it from `SYSCLOCK_ARM11`; the claim is read from the headers, not measured on a New 3DS in 804 MHz mode).
- `C3D_FrameBegin(flags)`: `C3D_FRAME_SYNCDRAW` = bit 0, `C3D_FRAME_NONBLOCK` = bit 1. Returns `false` if already in a frame or (non-blocking) if the GPU is busy.
- `C3D_FrameRate(fps)` accepts `0 < fps ≤ 60`; implemented as a per-screen accumulator decremented by `fps` on each V-blank, incrementing the frame counter whenever it crosses zero.
- `GSPGPU_PerfLogEntry` = `{ u32 lastDurationUs; u32 totalDurationUs; }`, one per `GSPGPU_Event` (`PSC0`, `PSC1`, `VBlank0`, `VBlank1`, `PPF`, `P3D`, `DMA`).
- Core-1 CPU-time resource limit (kernel type 9) values: 0 forbids application threads on core 1; 1–89 is a percentage and enables preemption; values ≥ 10000 exempt a process (applets); 1000 is used internally for system modules [reverse-engineered: Luma3DS `sysmodules/pm/source/reslimit.c`].
- `svcFlushProcessDataCache(CUR_PROCESS_HANDLE, addr, size)` takes a *virtual* address and is handled by the kernel directly; `GSPGPU_FlushDataCache` and `DSP_FlushDataCache` are IPC requests to the `gsp` and `dsp` system modules [documented: libctru sources].

## Pitfalls

- **Measuring in an emulator or on a New 3DS for an Old 3DS target.** Neither predicts Old 3DS timing ([testing on hardware](testing-on-hardware.md)).
- **Reporting averages only.** Always log the worst frame and what it was doing.
- **Profiling with `printf` in the loop.** The print costs more than what you measure.
- **"Faster" with no before/after on the same scene, build and console.** Keep the benchmark fixed and change one thing.
- **Starving the system on core 1.** Symptoms are audio stutter, slow file access or slow GPU flushes, not an obvious CPU hog.

## Open questions

- No wiki-run measurements yet of: CPU write bandwidth to heap vs linear memory vs VRAM on Old 3DS and New 3DS; cost of `C3D_FrameEnd`'s full linear-heap flush as a function of heap size; `GSPGPU_FlushDataCache` latency versus core-1 time limit; benefit of LTO. The [benchmark suite](../benchmarks/README.md) contains programs for these; none has been run on hardware yet.
- The cookbook's helpers (frame-time percentiles, missed-slot count) compile but have not run on any console; the nominal refresh period used for the missed-slot count is an assumption until the LCD refresh rate is measured. The `[reported]` G-Diffuser figures quoted here are for its own game on an Old 3DS and are not reproduced.
- Whether the CP15 cycle counter and the system tick advance 1:1 on an Old 3DS (and 3:1 in New 3DS 804 MHz mode) (see [CPU optimisation](cpu-optimization.md#counters-versus-the-system-tick)); the tick is the only one this page relies on.
- The 25 ms slice model of core-1 scheduling comes from untested notes; a hardware measurement (a core-1 thread recording `svcGetSystemTick` gaps at several percentages) would settle it.

## Sources

- [citro3d](https://github.com/devkitPro/citro3d) — `source/renderqueue.c` (frame begin/end, timers, V-blank counters, frame-rate limiter, linear-heap flush), `include/c3d/renderqueue.h`.
- [libctru](https://github.com/devkitPro/libctru) — `os.h` (clocks, `TickCounter`), `services/gspgpu.h` (perf log), `thread.h` (cores, priorities), `services/apt.h`, `source/system/syscalls.c` (FPSCR), `source/services/gspgpu.c`, `svc.h`.
- [Luma3DS](https://github.com/LumaTeam/Luma3DS) — `sysmodules/loader/source/hbldr.c` (3DSX exheader: clock/L2 flags off, core-1 limit 89, core 2 access), `sysmodules/pm/source/reslimit.c` (scheduler and CPU-time notes).
- [libctru issue #556: Document performance issues of DSP_FlushDataCache](https://github.com/devkitPro/libctru/issues/556) — third-party flush-time measurements versus core-1 time limit.
- [ARM11 MPCore Processor Technical Reference Manual, DDI 0360E](https://developer.arm.com/documentation/ddi0360/e) — VFP11 throughput and latency (table 19-16, section 16.4), cache organisation.
- [3ds-examples](https://github.com/devkitPro/3ds-examples) — template Makefile flags.
- [G-Diffuser: Road to 60, All Over Again](https://hypnotize.works/gdx-3ds/postmortem/native.html) — `[reported]` SD-logging hitches, GX entry counts, scheduling results (read directly; third-party).
