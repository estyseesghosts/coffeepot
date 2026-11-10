# Performance engineering for 3DS homebrew

Making a 3DS program fast is mostly about finding out *which* part is slow before changing anything. The console has a slow CPU (268 MHz on the original models), a capable but unusual GPU, and a small amount of fast memory; a frame can be late because the CPU ran out of time, because the GPU did, because one waited for the other, or because something stalled on a file or a system service. This page shows how to measure each of those with the libraries homebrew already uses, how to pace frames, how to use the second CPU core, and which CPU-side habits pay off on this processor.

Related pages: [memory and cache](memory-and-cache.md) (where buffers live and what flushing costs), [GPU rendering](gpu-rendering.md) (draw-call and state costs), [testing on hardware](testing-on-hardware.md) (how to run a benchmark you can trust).

## Before you start

- Checked against libctru 2.7.1, citro3d 1.7.1, devkitARM r68 and Luma3DS v13.5 source.
- Claims carry the [evidence labels](../../AGENTS.md#evidence-labels) used across the wiki. Nothing on this page is `[measured]` by the wiki itself yet; where a number comes from someone else's measurement, it is labelled with its origin.
- Decide your target hardware first. An [Old 3DS](../glossary.md#old-new-3ds) (3DS, 3DS XL, 2DS) has two slow cores and no L2 cache; a New 3DS can run 3× faster with an L2 cache. A program tuned on a New 3DS tells you nothing about an Old 3DS.

## The budget

At 60 frames per second a frame lasts 16.7 ms. The ARM11 application core on an Old 3DS runs at 268,111,856 Hz ([CPU](../hardware/cpu.md#clocks-and-core-counts)), so one frame is about 4.47 million clock cycles — before the operating system, cache misses, waiting for the GPU and interrupt handling take their share. On a New 3DS with the fast mode enabled the core runs at 804 MHz (3×) with a 2 MB L2 cache.

| Target | Frame time | Old 3DS core-0 cycles per frame |
| ------ | ---------- | ------------------------------- |
| 60 fps | 16.7 ms | ≈ 4.47 M |
| 30 fps | 33.3 ms | ≈ 8.94 M |
| 20 fps | 50 ms | ≈ 13.4 M |

Commercial games treated these budgets as design constraints from the start; homebrew ports of games written for faster machines usually have to do the same (reduce work, not just optimise it).

### Turn on the New 3DS fast mode yourself

A `.3dsx` started by Luma3DS gets a generated **[extended header](../glossary.md#exheader)** (exheader) in which the 804 MHz and L2-cache flags are **off** [documented: Luma3DS `sysmodules/loader/source/hbldr.c`, `hbldrPatchExHeaderInfo`]. Call `osSetSpeedupEnable(true)` at start-up; it asks the system (`PTMSYSM_ConfigureNew3DSCPU(3)`) for both the higher clock and the L2 cache and does nothing harmful on an Old 3DS ([libctru](../homebrew/libctru.md#clocks-and-the-new-3ds-speed-up)). An installed `.cia` can instead request them in its exheader. The user's Luma3DS setting `app_launch_new_3ds_cpu` can also force them per title.

## Measuring time

### CPU time: the system tick

`svcGetSystemTick()` returns a 64-bit counter running at the base ARM11 clock, `SYSCLOCK_ARM11` = 268,111,856 Hz, on every model and in every New 3DS clock mode [documented: libctru `os.h` defines `CPU_TICKS_PER_MSEC` from `SYSCLOCK_ARM11`]. Reading it is a system call, so it is cheap but not free; time blocks of work, not individual pixels.

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
| Both CPU and GPU well under budget, but frames still missed | **Synchronisation** — the two take turns instead of overlapping, or you wait for V-blank at the wrong moment | Restructure the frame loop ([below](#overlapping-cpu-and-gpu)) |
| Occasional single long frames | **Hitches**: file reads, texture uploads, allocation, a full cache flush, a system service call | Log the worst frames with their stage breakdown; move loading off the frame thread |
| Audio crackles while frames look fine | Audio refill thread starved or blocked | [Audio](#audio-deadlines) |

Do not optimise a stage that is not on the critical path: halving GPU time does nothing for a CPU-bound frame.

## Frame pacing

### Overlapping CPU and GPU

`C3D_FrameBegin` waits until the GPU has finished the previous frame's queue (unless you pass `C3D_FRAME_NONBLOCK`, in which case it returns `false` instead of waiting) [documented: `renderqueue.c`, `C3Di_WaitAndClearQueue`]. Everything you do *before* calling it runs while the GPU is still drawing the previous frame. So:

```c
while (aptMainLoop()) {
    hidScanInput();
    update_game();                      // overlaps with the GPU drawing the previous frame
    build_vertex_data_in_cpu_memory();  // also overlaps — but must not touch buffers the GPU is reading

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW); // now the GPU is idle: safe to rewrite its buffers
    upload_dynamic_buffers();
    record_draws();
    C3D_FrameEnd(0);
}
```

Doing game logic between `C3D_FrameBegin` and `C3D_FrameEnd` serialises CPU and GPU: the GPU sits idle while the CPU updates, and the CPU sits idle in the next `C3D_FrameBegin` while the GPU draws [documented mechanism; the gain depends on the workload and is not measured here]. The cost of overlapping is that any buffer the GPU reads must not be rewritten until `C3D_FrameBegin` has returned — keep two copies (double-buffering) if you must prepare data earlier.

### V-blank and frame-rate limiting

- `C3D_FRAME_SYNCDRAW` makes `C3D_FrameBegin` call `C3D_FrameSync()` first, which waits until **both** screens' V-blank counters have advanced — it locks the loop to the display (about 60 Hz) [documented].
- `C3D_FrameRate(fps)` (values up to 60) makes those counters advance only every *60/fps* V-blanks, so `C3D_FRAME_SYNCDRAW` then paces the loop to e.g. 30 fps. It returns the previous setting [documented].
- The LCD swap itself happens in citro3d's queue-finished callback, after the frame's display transfer: a frame becomes visible at the next V-blank after the GPU finishes, not when `C3D_FrameEnd` returns.

A steady 30 fps usually feels better than a rate that swings between 40 and 60. If the game cannot hold 60 on the slowest target, lock to 30 deliberately.

### Keep the game clock separate from the picture

If the simulation advances once per rendered frame, a slow frame slows the game down. For ports especially, keep the game's own tick (input, logic, timers, audio) on a fixed schedule and let presentation drop or repeat frames when it falls behind. Count both rates separately: "logic ticks per second" and "distinct frames shown per second". A game running its logic at full speed while showing 12 new pictures a second is not running at 60 fps, whatever a single `fps` counter says.

## Using the second core

### Old 3DS: core 1 is shared with the system

Core 0 belongs to the application. Core 1 runs system modules; an application may borrow a percentage of it:

1. Call `APT_SetAppCpuTimeLimit(percent)` (`<3ds/services/apt.h>`), then optionally `APT_GetAppCpuTimeLimit(&p)` to read back what was granted.
2. Create **one** thread on core 1: `threadCreate(fn, arg, stack, prio, 1, false)`. libctru's `thread.h` states that with a time limit set "it is possible to create a single thread on this core"; check the result for `NULL` [documented].
3. Without step 1, creating an application thread on core 1 fails: since system version 2.1 the kernel returns an error while the application's core-1 limit is 0 [reverse-engineered: Luma3DS `pm` notes].

For a `.3dsx` under Luma3DS, the loader sets the core-1 limit descriptor to 89% in "mode 1" (single-thread) scheduling, which still requires the `APT_SetAppCpuTimeLimit` call [documented: `hbldr.c` comment "max. 89% of CPU time (default 0, requires a APT_SetAppCpuTimeLimit call)"]. For installed titles, the limit comes from the exheader.

How the time is shared, per Luma3DS's `pm` reimplementation notes (TuxSH, who mark them as "nothing was tested" and in need of proof-testing) [reverse-engineered]: in mode 1 the kernel divides core 1's time into 25 ms slices; the application thread gets *percent*% of each slice, system modules share (90 − *percent*)%, and the remaining 10% goes to other threads. Two consequences follow:

- A core-1 thread does **not** run continuously; work handed to it may wait up to a slice before running. Use it for throughput work (decoding, mixing ahead, geometry preparation), not for anything that must finish within a few milliseconds of being requested.
- A high percentage takes time away from system modules that your program depends on. An open libctru issue reports, on an Old 3DS, `DSP_FlushDataCache` taking ~0.1 ms with no limit set but 1–5 ms (average 2.5 ms) at 80%, 0.1–1.2 ms at 50% and ~0.1 ms at 10%, independent of data size, while the kernel call `svcFlushProcessDataCache` was unaffected [measured by a third party: libctru issue #556, Old 3DS, not yet confirmed by maintainers]. A plausible reason is that `DSP_FlushDataCache` and `GSPGPU_FlushDataCache` are requests to system modules, which then have less core-1 time to answer [hypothesis — the same would then apply to GSP flushes].

Treat the percentage as a tuning parameter: measure your frame breakdown and your audio flush times at several values.

### New 3DS: cores 2 and 3

On a New 3DS, core 2 is available to applications whose exheader kernel flags include `0x2000`; Luma3DS's 3DSX exheader sets it ("Access core2"), so `.3dsx` homebrew can create threads on core 2 [documented: `thread.h`, `hbldr.c` kernel capabilities]. Core 3 is not available to normal applications. According to the same `pm` notes, CPU-time preemption only exists on core 1 [reverse-engineered], so a core-2 thread is not sliced against system modules — the natural home for a worker thread on a New 3DS. Detect the model with `APT_CheckNew3DS(&isNew)` and choose the core at runtime; never assume core 2 exists.

### Thread priorities

Priorities run from `0x18` (highest allowed for userland) to `0x3F`; **lower numbers are higher priority**, and the main thread usually has `0x30` (read it with `svcGetThreadPriority` rather than assuming) [documented: `thread.h`]. Scheduling on core 0 is priority-based and mostly cooperative ([kernel](../software/kernel.md#scheduling)): a higher-priority thread that never blocks starves everything below it on that core, and threads of equal priority do not get fair time slices. Every worker should block on an event (`LightEvent`, `svcWaitSynchronization`) when idle; polling with `svcSleepThread(0)` wastes the core.

## Audio deadlines

The DSP plays whatever is queued and stops when the queue runs dry. Keep enough buffered audio to cover your worst frame (not your average frame), refill from a thread that does not wait for the GPU, flush each wave buffer before `ndspChnWaveBufAdd`, and measure flush times if you use core 1 (see above). Audio buffer flushing rules are on [memory and cache](memory-and-cache.md#dsp-buffers).

## CPU-side habits

The ARM11 implements **ARMv6K** with the VFPv2 floating-point unit ([CPU](../hardware/cpu.md#the-mp11-core-from-arms-manual)). Things that follow directly from the hardware [documented: ARM DDI 0360E]:

| Fact | Consequence |
| ---- | ----------- |
| No NEON, no ARMv7 instructions | Desktop SIMD code and intrinsics do not apply. The ARMv6 SIMD instructions (packed 8- and 16-bit add/subtract/saturate in ordinary registers) are available and the compiler can use them for some loops |
| No hardware integer divide | `/` and `%` by a non-constant call a library routine. Hoist divisions out of loops; divide by powers of two with shifts (for unsigned values or where rounding of negatives is handled) |
| VFP11: single-cycle throughput for single-precision operations and most double operations, but double multiply and multiply-accumulate have **two-cycle** throughput; divide and square root take about 15 cycles (single) and 29 cycles (double) per instruction | Use `float`, not `double`, in hot code; write `1.0f`, call `sqrtf`/`sinf`, and enable `-Wdouble-promotion` to catch accidental promotions. Multiply by a precomputed reciprocal instead of dividing in loops |
| Thumb-1 only (no Thumb-2) | `-mthumb` makes code smaller but slower and restricted; the toolchain defaults to ARM code |
| L1 caches: 16 KB instruction + 16 KB data per core, 32-byte lines; no L2 on Old 3DS ([CPU](../hardware/cpu.md#clocks-and-core-counts)) | Working sets larger than 16 KB stream from slow FCRAM. Prefer compact data, sequential access, arrays of small structs or structure-of-arrays for hot loops; a large lookup table can cost more in cache misses than the arithmetic it replaces |

libctru sets the floating-point control register on every thread to **default-NaN and flush-to-zero** mode with round-to-nearest [documented: `source/system/syscalls.c`]. Denormal numbers are therefore treated as zero: fast, but different from desktop IEEE behaviour if your code depends on very small values (some physics and audio filters do).

### Compiler flags

The devkitPro templates build with `-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft -O2 -mword-relocations` ([development workflow](../homebrew/development-workflow.md)). Before changing them:

- `-O3` and aggressive inlining or unrolling increase code size, and with a 16 KB instruction cache that can make code slower [hypothesis — measure per project].
- `-Os` is a reasonable choice for cold code (menus, loaders) to save memory; keep hot files at `-O2`.
- `-ffast-math` changes results (reassociation, NaN handling); only enable it for files whose results you have tested.
- Link-time optimisation (`-flto`) is supported by the toolchain; its effect has not been measured here.

Always check the result: `arm-none-eabi-objdump -drwC file.o` for the generated code of a hot function, `arm-none-eabi-nm -S --size-sort app.elf` for what is taking space, and grep the disassembly for calls such as `__aeabi_idiv`, `__aeabi_d*` (software `double` helpers, e.g. for conversions) or `__aeabi_ldivmod` in hot paths.

## Technical details

- `SYSCLOCK_ARM11` = 268,111,856 Hz; `CPU_TICKS_PER_MSEC` = `SYSCLOCK_ARM11 / 1000.0`; New 3DS clocks are 2× and 3× that ([libctru](../homebrew/libctru.md#clocks-and-the-new-3ds-speed-up)). The system tick does not change rate with the New 3DS clock.
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

- No wiki-run measurements yet of: CPU write bandwidth to heap vs linear memory vs VRAM on each model; cost of `C3D_FrameEnd`'s full linear-heap flush as a function of heap size; `GSPGPU_FlushDataCache` latency versus core-1 time limit; benefit of LTO. The [testing page](testing-on-hardware.md#microbenchmarks-worth-running) lists the experiments.
- The 25 ms slice model of core-1 scheduling comes from untested notes; a hardware measurement (a core-1 thread recording `svcGetSystemTick` gaps at several percentages) would settle it.

## Sources

- [citro3d](https://github.com/devkitPro/citro3d) — `source/renderqueue.c` (frame begin/end, timers, V-blank counters, frame-rate limiter, linear-heap flush), `include/c3d/renderqueue.h`.
- [libctru](https://github.com/devkitPro/libctru) — `os.h` (clocks, `TickCounter`), `services/gspgpu.h` (perf log), `thread.h` (cores, priorities), `services/apt.h`, `source/system/syscalls.c` (FPSCR), `source/services/gspgpu.c`, `svc.h`.
- [Luma3DS](https://github.com/LumaTeam/Luma3DS) — `sysmodules/loader/source/hbldr.c` (3DSX exheader: clock/L2 flags off, core-1 limit 89, core 2 access), `sysmodules/pm/source/reslimit.c` (scheduler and CPU-time notes).
- [libctru issue #556: Document performance issues of DSP_FlushDataCache](https://github.com/devkitPro/libctru/issues/556) — third-party flush-time measurements versus core-1 time limit.
- [ARM11 MPCore Processor Technical Reference Manual, DDI 0360E](https://developer.arm.com/documentation/ddi0360/e) — VFP11 throughput and latency (table 19-16, section 16.4), cache organisation.
- [3ds-examples](https://github.com/devkitPro/3ds-examples) — template Makefile flags.
