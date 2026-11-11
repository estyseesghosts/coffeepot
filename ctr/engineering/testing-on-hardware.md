# Testing 3DS homebrew on hardware

A program that works in an emulator, or on a New 3DS alone, can still crash, stutter or run out of memory on the console your players own. This page describes which devices to test on and why, how to set up a fast test loop on a real console, how to run a performance experiment whose result you can trust, and the wiki's benchmark programs for measuring memory, cache and draw-call costs yourself. Its companion pages are [debugging](debugging.md) and [performance](performance.md).

## Before you start

- A console with [Luma3DS](../homebrew/luma3ds.md) and the Homebrew Menu, on the same network as your PC.
- The [benchmark suite](../benchmarks/README.md) builds without warnings with devkitARM r68 and libctru 2.7.1. It has **not** been run on hardware (Old 3DS: not run; New 3DS: not run); no results are recorded yet.

## Test tiers

| Tier | Good for | Not evidence of |
| ---- | -------- | --------------- |
| Emulator ([Azahar](debugging.md#emulators)) | Fast iteration, logic bugs, rendering bugs, catching crashes | Speed, timing, memory limits; that a GPU configuration is safe on hardware |
| Old 3DS | Old 3DS speed and memory limits — the slowest target | — |
| New 3DS | Correctness on hardware, New 3DS speed, head-tracking behaviour | Old 3DS speed (even in 268 MHz mode, the cache, cores and memory mode differ) |

If you claim a program runs on "3DS", test on an Old 3DS. If you only have a New 3DS, say so in your release notes. The Nintendo 2DS counts as an Old 3DS for speed; **New 3DS:** the New Nintendo 2DS XL has New 3DS speed. Neither has stereoscopic 3D, so test the 3D-off path there too. The [models page](../hardware/models.md) lists what each model has; for software the differences that matter most are the CPU and memory ([CPU](../hardware/cpu.md)), wide mode (not on the Nintendo 2DS, but it works on the New Nintendo 2DS XL — libctru `gfx.h`), and, **New 3DS:** head tracking, extra buttons and built-in NFC.

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
Console:      model (e.g. Old 3DS XL, or New 3DS), system version, Luma3DS version, SD card
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

### Compare correctness as well as speed

For renderer changes, replay the same captured state through the reference and candidate paths, then compare images and log mismatches, unsupported features and queue overflows. Include palette changes, external state changes, overlap and blend order, scanline boundaries and scene transitions. Test emulator and hardware separately: the [SM64DS Enhanced case study](case-studies/case-studies-rendering-tradeoffs.md#ds-geometry-translated-to-native-draws) describes a presentation flicker seen on hardware but not in Azahar [reported: project renderer, revision and model unspecified]. Record each intentional approximation alongside timings so a faster but different image is not mistaken for an equivalent result.

### Prove the path is exercised

A fixture can prove an optimization correct on constructed input without proving that the game ever takes it. Record attempts, successful takes, rejection reasons and work removed, then compare complete output with an independent reference. Include the *previous* output state when pixels or alpha are retained. Capture registers, palettes, graphics memory and object state from the same game phase. If reference and candidate share code or assumptions, compare an independent capture as well. HGSS found zero live band takes despite passing fixtures and later found a bug shared by its PC reference and port [reported: project notes, November 2026; [case study](case-studies/case-study-hgss-port.md#fast-paths-prove-admission-work-and-output-separately)].

Keep baseline binaries immutable in separate output directories. Record their hashes alongside the save, script, configuration and model for each run; check the runtime banner and feature activation in every log. Confirm flags reached the library that contains the optimized code. Test PROFILE versus OFF with a matched, repeated A/B rather than attributing an unmatched difference to instrumentation.

### Integrated game-system checks

Individual renderer, audio and file tests do not prove the combined program works under load. With a reproducible scene, load a second area while sound plays; request HOME or sleep during loading; resume and check whether application-owned caches or targets need rebuilding; exercise slow and failed writes; and close with producer/consumer queues active. Check input and simulation progression throughout, and verify any worker cancellation and shutdown ordering. The required behavior depends on the application's ownership design; do not assume every target is lost or retained on resume. Record failures and target model separately. These integrated tests have not been run for the wiki's examples.

## Benchmark record format

A benchmark record has a human part (this table, filled in for a README or page) and a machine part (the lines `common/bench.h` already writes). The human table below is the shape suggested by the review that prompted it; the example values are **illustrative, not measured**.

| Field | Meaning | Where it comes from in `bench.h` output |
| ----- | ------- | --------------------------------------- |
| Hardware | Model, firmware, CFW | `meta,model`, `meta,new3ds`, `meta,firm_version`, `meta,kernel_version` |
| Build | Compiler, library versions, date | `meta,compiler`, `meta,build_date`; libctru/citro3d versions are `not-readable` at run time, so copy them from [source versions](source-versions.md) |
| Clock mode | **New 3DS:** speed-up requested | `meta,speedup_requested` (the actual L2/804 MHz state is `not-readable`) |
| Target | e.g. 60 fps / 16.67 ms budget | written by hand |
| Scene | e.g. scrolling 2D tilemap, 40 sprites | `result` columns `test` and `bytes_or_n` (size or count), plus a hand-written description |
| Variants | e.g. citro2d / manual batch / cached chunks | `result` column `variant` |
| CPU metrics | update, build, submission, cache flush, wait | `result,<program>,<test>,<variant>,<n>,<runs>,<min_us>,<med_us>,<max_us>`; drawbench's `columns,...` row for per-stage milliseconds |
| GPU metrics | command processing, transfer, clear | drawbench `gpu_ms`; GSP performance log deltas if you add them |
| Frame-time results | median, p95, p99, worst, missed frames | `note,pct,<program>,<variant>,<frames>,<p50_us>,<p95_us>,<p99_us>,<worst_us>,<missed_slots>` (helper in the [profiling cookbook](performance.md#frame-time-distribution-and-missed-frames)); `note,perf,...` lines in `icachebench` for counters |
| Verification | screenshot, framebuffer checksum, crash test | by hand; `error,` lines invalidate a run (`note,done errors=N`) |

Illustrative only (not measured):

```text
Hardware:     Old 3DS XL, system 11.17.0-50U, Luma3DS v13.5      (model is not a measured fact here)
Target:       60 fps / 16.67 ms
Scene:        scrolling 2D tilemap, 40 sprites (input script X)
Variants:     citro2d | manual batch | cached chunks
Frame time:   p50 / p95 / p99 / worst / missed slots  -> from the note,pct line
Verification: framebuffer checksum identical across variants
```

Rules: one record per console and variant; keep the raw `coffeepot-<program>.csv`; never fill a field with a value you did not read from a run; a record with an `error,` line is not a result. The suite README ([recording results](../benchmarks/README.md#recording-results)) holds the same fields for the programs it ships.

## Microbenchmarks worth running

The wiki has no measured numbers yet for the costs below; they are the questions that come up most often when optimising ([performance: open questions](performance.md#open-questions)):

- CPU write bandwidth to the application heap versus the linear heap, at sizes inside and outside the 16 KB L1 data cache.
- The cost of flushing a range with the kernel call versus the GSP service, as a function of size.
- The cost of `C3D_FrameEnd(0)`'s whole-linear-heap flush.
- Whether the ARM11 performance counters work from a `.3dsx` under Luma3DS, and how the cycle counter relates to the system tick ([CPU optimisation: open questions](cpu-optimization.md#open-questions)); `icachebench` tries the counters and reports `perf_unavailable` if the kernel refuses, so a first run answers the first half.
- How code footprint beyond 16 KB affects cycles per instruction (`icachebench`).
- How these change with an `APT_SetAppCpuTimeLimit` share active on an Old 3DS, and **New 3DS:** with and without `osSetSpeedupEnable(true)`.

### The benchmark suite

Four ready-to-build programs live in [`ctr/benchmarks/`](../benchmarks/README.md); a fourth, [`icachebench`](../benchmarks/README.md#icachebench), probes instruction-cache pressure by executing code of growing footprint and reporting time and (if the kernel allows) cycles and I-cache misses. The three below came first:

- **`membench`** — CPU write and copy speed for the heap and the linear heap at sizes inside and outside the L1 cache, kernel versus GSP flushes of a dirty range, and the whole-linear-heap flush. Prints min / median / max microseconds over 31 runs.
- **`flushbench`** — flush cost at 1–32 MiB, dirty versus clean ranges, GSP flush versus kernel flush versus clean-only, and `C3D_FrameEnd(0)` versus `C3D_FrameEnd(GX_CMDLIST_FLUSH)`.
- **`drawbench`** — CPU and GPU time per frame for N quads drawn as one batch, one draw each, and one draw each with a uniform update, a redundant texture bind, a combiner change or a texture swap.

Build with `make` in each folder (or `tools/build_benchmarks.sh` for all), run with 3dslink, and record results as described in the [suite's README](../benchmarks/README.md#recording-results). For `membench`, add `APT_SetAppCpuTimeLimit(80);` to see the effect of a core-1 share on the GSP flush. **New 3DS:** add `osSetSpeedupEnable(true);` after `gfxInitDefault()` for the fast-mode variant (or build with `-DBENCH_SPEEDUP=1`); it is not needed, and does nothing, on an Old 3DS.

How to read `membench`'s output:

- `memset heap` vs `memset linear`: whether the linear heap is slower for the CPU to write than the ordinary heap on your model.
- `svcFlush` vs `GSPGPU_Flush`: the kernel call versus the service request, for the same dirty range.
- `whole linear`: the flush `C3D_FrameEnd(0)` performs every frame. Compare it with the frame budget (16,667 µs at 60 fps) to decide whether targeted flushing ([memory and cache](memory-and-cache.md#the-c3d_frameend-flush)) is worth the bookkeeping.

When you have results, add them to the relevant page with the console model, system and Luma3DS versions, and the build details.

## Technical details

- Timing uses `svcGetSystemTick()` (268,111,856 Hz on all models) and `CPU_TICKS_PER_USEC` from `os.h`.
- `__ctru_linear_heap` and `__ctru_linear_heap_size` are the linear heap's start address and size as set up by libctru at start-up ([memory and cache](memory-and-cache.md#how-libctru-splits-it)).
- The benchmark runs on the main thread (core 0); system activity can add outliers, which is why it reports the median and the maximum.

## Pitfalls

- **Testing only on the developer's console.** Units differ in SD card speed, calibration and model.
- **Running benchmarks with Rosalina's debugger attached** or with game plugins active: both add work.
- **Discarding the first run.** Cold caches and data still on the SD card make the first run slower; record it separately rather than silently dropping it.

## Open questions

- No results yet from the benchmark suite on any console. Contributions should record model, system version, Luma3DS version, and whether fast mode or a core-1 share was active.

## Sources

- [3ds-examples](https://github.com/devkitPro/3ds-examples) — application template used to build the benchmark.
- [libctru](https://github.com/devkitPro/libctru) — `os.h` (tick constants), `svc.h`, `services/gspgpu.h`, `services/apt.h`, `allocator/linear.h`, `gfx.h` (wide mode on the original 2DS).
- [citro3d](https://github.com/devkitPro/citro3d) — `source/renderqueue.c` (the whole-heap flush the benchmark reproduces).
- [3ds-hbmenu](https://github.com/devkitPro/3ds-hbmenu) — netloader.
