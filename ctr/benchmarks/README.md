# CTR benchmark suite

Small, self-contained 3DS programs that measure costs the wiki's engineering pages depend on: memory and cache behaviour, cache-flush cost, instruction-cache pressure, and citro3d draw-call overhead. Each one builds from source with the standard devkitPro toolchain and prints its results on the console's screen and writes them to a file on the SD card. They exist so that performance claims on this wiki can become `[measured]` instead of `[hypothesis]` ([evidence labels](../../AGENTS.md#evidence-labels)).

**Status: the programs build and have run on two consoles: an Old 3DS and a New 2DS XL (New 3DS family) in its default 268 MHz mode. The micro suite ran three sessions on each, the 20-variant tile suite one session on each, and the first standalone builds two sessions on the Old 3DS. The New 3DS at 804 MHz with its L2 cache, the emulator and the non-default variants are not run.** The tables under each benchmark hold the results, flagged `[measured]` with the caveats in [Hardware runs](#hardware-runs). Raw CSV files are in [`hardware-runs/`](hardware-runs/).

## Before you start

- Build environment: devkitPro with the `3ds-dev` group. Verified to build without warnings with devkitARM r68, libctru 2.7.1, citro3d 1.7.1 and picasso 2.7.2 (the versions on [source versions](../engineering/source-versions.md)), using `tools/build_benchmarks.sh`, which fails on any warning.
- An Old 3DS (the primary target; a New 3DS gives a second data set and must be recorded separately) with [Luma3DS](../homebrew/luma3ds.md); run each `.3dsx` from the Homebrew Menu or with `3dslink` ([testing on hardware](../engineering/testing-on-hardware.md#a-fast-editrun-loop)).
- Insert an SD card (results are written to it). Close other homebrew, detach the debugger and disable game plugins while measuring.

## Verification status

| Benchmark | Builds | Emulator | Old 3DS | New 3DS |
| --------- | ------ | -------- | ------- | ------- |
| [`membench`](#membench) | yes (r68) | not run | 5 sessions (2 standalone, 3 in the suite), no errors | 3 sessions in the suite, 268 MHz mode, no errors |
| [`flushbench`](#flushbench) | yes (r68) | not run | 5 sessions, no errors | 3 sessions, 268 MHz mode, no errors |
| [`drawbench`](#drawbench) | yes (r68) | not run | 2 standalone sessions at 1000 quads, 3 suite sessions over 125-4096 quads, no errors | 3 suite sessions, 268 MHz mode, no errors |
| [`icachebench`](#icachebench) | yes (r68; also compiled directly with `-Wall -Wextra`) | not run | 5 sessions, counters worked; corrected control run in 3 | 3 sessions, counters worked, 268 MHz mode |
| [`tile-renderer`](../engineering/PICA200/2d-tile-renderer.md) | default builds (r68) | [picture check](#picture-equivalence-emulator): 17 variants, identical | 4 strategies x 2 standalone sessions; the 20-variant suite x 1 session, no errors | the 20-variant suite x 1 session, 268 MHz mode, no errors |
| [`coffeebench`](#building-and-running) (micro suite) | yes (r68) | not run | 3 sessions | 3 sessions, 268 MHz mode |
| [tile suite](#tile-renderer-suite) (20 chained binaries) | yes (r68) | not run | 1 session; the chain ran all 20 | 1 session; the chain ran all 20 |
| [New 3DS full-speed suites and `corebench`](#new-3ds-full-speed-suites) | yes (r68) | not run | n/a | not run |

All New 3DS runs so far are a New 2DS XL at the default 268 MHz clock, which a program gets unless it calls `osSetSpeedupEnable(true)`. They show how the New 3DS behaves in compatibility mode, not its full speed.

Emulator runs only show that a program works; never record emulator timings as results.

## Hardware runs

All results below come from one console, run twice:

```text
Console:    Old 3DS (2011 unit), sysNAND, system 11.17.0-50U, Luma3DS v13.0.2
            (CSV: model "Old 3DS", firm_version / kernel_version 2.58.0)
Build:      this repository's working tree, not a commit; devkitARM r68 (GCC 16.1.0),
            libctru 2.7.1, citro3d 1.7.1, citro2d 1.7.0 (versions from source-versions, not from the CSV)
            membench/flushbench/drawbench built Nov 10 2026 22:33, icachebench Nov 11 00:02,
            tile renderer (BENCH_AUTO=1, TILE_STRATEGY 0..3) Nov 11 12:49-12:50
Variant:    default for all four microbenchmarks (no core-1 limit; the Old 3DS has no speed-up);
            tile renderer: TILE=16, NPC_COUNT=40, CAM_SPEED=2, BENCH_PATH=0, 600 frames after 120 warm-up
Output:     hardware-runs/o3ds-0 and hardware-runs/o3ds-1 (the coffeepot-<program>.csv files)
```

Every file ends with `note,done errors=0`. Read the tables with these limits in mind:

- **Session counts.** The rule under [Recording results](#recording-results) asks for at least three runs after a fresh boot. The micro suite has three sessions per console, which meets it. The tile suite has one session per console, so its numbers are single runs (the two standalone Old 3DS sessions of strategies 0-3 agreed within 0.5%, and the suite's baseline matches them). Boot state before each session was not recorded.
- **`o3ds-1/coffeepot-tilerenderer.csv` contains two sessions.** The build that produced these files opened one file for append (current builds write one file per session and variant) and the first file was left on the SD card, so `o3ds-1` begins with a byte-for-byte copy of `o3ds-0` and then adds the second session. Split the file at the second `meta,program,tilerenderer` block, before computing statistics. The other four programs overwrote their file. These files predate the session directories.
- **The libctru and citro3d versions are `not-readable`** in every file, as designed.
- **Three benchmarks are single points.** `drawbench` ran only at 1000 quads; the tile renderer only at 416 visible tiles, 40 sprites and the default path. Nothing here fits a curve.
- The SD-card file timestamps read 2011: the console clock was not set. That is irrelevant to the timings.

## Suite runs (Old 3DS and New 3DS)

Raw files: [`hardware-runs/o3ds-2`](hardware-runs/o3ds-2/) (Old 3DS, the 2011 unit, Luma3DS 13.0.2, system 11.17.0-50U) and [`hardware-runs/n3ds-0`](hardware-runs/n3ds-0/) (New 2DS XL, Luma3DS 13.3.3, system 11.17.0-50U), each as the SD card's `coffeepot/` directory: sessions `s001` to `s003` are the micro suite and `s004` the tile suite. All 64 CSV files end `note,done errors=0`. Both builds report `repo_commit 458e8a9_dirty`, recorded before the work was committed; the source corresponds to the commit that added the suites. Both consoles ran at 268 MHz (the `loop_256b` control takes 3,857 µs on both), because the programs did not call `osSetSpeedupEnable`; whether the New 3DS's L2 cache was on is `not-readable`. **The New 3DS columns below are therefore a New 3DS in its compatibility clock mode, not its full speed.**

Medians of the three micro-suite sessions, Old 3DS / New 3DS (268 MHz mode). Sessions agree within 1% except where noted.

| Measurement | Old 3DS | New 3DS | Note |
| ----------- | ------- | ------- | ---- |
| `memset` 16 KiB, heap or linear | 24.5 µs | 24.5 µs | Inside L1 |
| `memset` 1 MiB, heap or linear | 2,692 µs (371 MiB/s) | 2,576 µs (388 MiB/s) | Heap and linear identical on both |
| `memcpy` heap to linear, 256 KiB / 1 MiB | 2,984 / 12,016 µs | 3,171 / 12,743 µs | See the memcpy note below |
| `svcFlushProcessDataCache`, clean, 1 KiB / 16 KiB / 16 MiB | 23.7 / 34.2 / 34.5 µs | 31.9 / 50.9 / 50.9 µs | Flat above 16 KiB on both |
| `GSPGPU_FlushDataCache`, clean, 1 KiB / 16 KiB / 16 MiB | 60.7 / 84.2 / 85.3 µs | 78.6 / 107.6 / 102.3 µs | |
| Flush after writing 16 MiB, kernel / GSP | 79.0 / 136.0 µs | 96.4 / 157.5 µs | |
| Whole 32 MiB linear heap via GSP, warm / dirtied | 80.8 / 135.3 µs | 97.4 / 157.8 µs | |
| `C3D_FrameEnd(0)` / `C3D_FrameEnd(GX_CMDLIST_FLUSH)` | 149.1 / 60.0 µs | 173.5 / 61.2 µs | Heap flush costs 89 / 112 µs |
| `icachebench` CPI at 4-12 / 16 / 20 / 24 / 32+ KiB | 1.02-1.04 / 1.07 / 2.89 / 4.10 / 5.59 | 1.02-1.05 / 1.07 / 3.11 / 4.45 / 6.11 | Same cliff, deeper on the New 3DS |
| I-cache miss cost (stall cycles per miss) | 36.5 cycles | 40.6 cycles | Without the L2 benefit |
| `loop_256b` CPI (corrected control) | 0.99 | 0.99 | 3,857 µs on both: same clock |
| `drawbench` batched, 125 to 4096 quads (record + `C3D_FrameEnd`) | 0.21 ms, flat | 0.24 ms, flat | |
| `drawbench` per-quad at 125 / 1000 / 4096 quads | 1.42 / 9.81 / 39.5 ms | 1.45 / 9.84 / 39.6 ms | 9.6-9.8 µs per draw |
| ... plus uniform / bind / combiner / texture swap, 4096 quads | +4.8 / +8.7 / +3.7 / +8.5 ms | +4.8 / +8.7 / +3.7 / +8.6 ms | 1.2 / 2.1 / 0.9 / 2.1 µs per draw |

What the later runs add:

- **The draw-call cost is linear in the draw count and the same on both models.** From 125 to 4,096 quads the per-quad variants grow in proportion (about 9.6-9.8 µs per draw, with a fixed 0.2 ms), while the batched variant stays at 0.21-0.24 ms. About 1,700 separate draws fill a 16.7 ms frame. The New 3DS at 268 MHz is within 1% on all draw variants, so the CPU-side cost of citro3d is the clock, not the model.
- **The I-cache cliff is the same on the New 3DS; only the miss price differs.** The cliff sits at the same footprint (CPI 1.07 at 16 KiB, 3.11 at 20 KiB), so the L1 instruction cache is 16 KiB on that model too, as far as a step can show. A miss costs about 40.6 cycles against 36.5 on the Old 3DS, 11% more. An enabled L2 cache should make misses cheaper; this run did not test that.
- **Flushes cost more on the New 3DS at the same clock**: 16-17 µs more for the kernel call (51 against 34 µs from 16 KiB up) and 20-25 µs more for the GSP call. `C3D_FrameEnd(0)` costs 174 µs against 149 µs. `GX_CMDLIST_FLUSH` costs the same, so the avoidable heap flush is 112 µs on the New 3DS (0.7% of a frame) against 89 µs on the Old 3DS (0.5%).
- **`memcpy` from the heap to linear memory is 12.0 ms per MiB in the suite, against 10.7 ms in the two standalone sessions on the same Old 3DS** while `memset` and every other row stayed within 1%. The cause is not established; buffer alignment after other modules' allocations is the candidate. Treat 10.7-12.0 ms/MiB as the range on an Old 3DS, and the New 3DS's 12.7 ms as within it plus 6%.
- **The `straight_16k` row moved** (CPI 1.07 in the suite, 1.10-1.15 standalone): with a footprint that just fits, where the function sits in the binary changes how many lines collide. Code placement matters at the boundary, away from it the rows are stable.
- **The corrected `loop_256b` control gives CPI 0.99 on both models** with no misses, so the earlier control was close enough; the cliff is unchanged.

The 20-variant tile-suite results, and what they settle for E1 to E6, are on the [tile page](../engineering/PICA200/2d-tile-renderer.md#suite-results-old-3ds-and-new-3ds).

## New 3DS full-speed suites

The first New 3DS runs were at 268 MHz ([suite runs](#suite-runs-old-3ds-and-new-3ds)) because a program gets the Old 3DS clock unless it calls `osSetSpeedupEnable(true)`, which switches the CPU to 804 MHz and enables the L2 cache. Two builds run at full speed. Stage them with `bash tools/stage_n3ds.sh dist-n3ds` and copy `dist-n3ds/sdmc/*` to the card.

| What | Binary | What it does |
| ---- | ------ | ------------ |
| **Micro suite, N3DS** | `3ds/coffeebench-n3ds/coffeebench-n3ds.3dsx` (`make N3DS=1` in `suite/`) | Refuses to run on an Old 3DS. Runs `icachebench`, `membench`, `flushbench`, `drawbench` and `corebench` twice in one boot: first with the CPU forced to 268 MHz (`osSetSpeedupEnable(false)`), then at 804 MHz + L2. Files are `<program>-sp0.csv` and `<program>-sp1.csv` in one session directory, so each pair is a same-boot, same-binary A/B. |
| **Tile suite, N3DS** | `3ds/coffeepot-tiles-n3ds/coffeepot-tiles-n3ds.3dsx`, then `t02.3dsx` to `t20.3dsx` | The same 20 variants as the [tile suite](#tile-renderer-suite), each calling `osSetSpeedupEnable(true)` first, chained inside `/3ds/coffeepot-tiles-n3ds/`. Compare with the 268 MHz session `s004` of `n3ds-0`; the ids carry a `_n3ds` suffix. |

Every CSV records `meta,speedup_requested` and `meta,clock_probe_us`, a fixed 2,000,000-iteration loop timed with the system tick: the New 3DS at 804 MHz takes about a third of the time it takes at 268 MHz, and that figure, not the request, is the evidence of which clock a run had. The `-sp0` files from the micro suite give the 268 MHz reference for the probe. Whether the L2 cache is on is not readable; `osSetSpeedupEnable` is documented to switch both, so treat 804 MHz and L2 as one setting.

### corebench

`corebench` asks whether the New 3DS's extra cores run beside the first one at full speed. It pins threads to application core 0 (the main thread), 2 and 3 (core 1 is the system core and is left alone) and runs a fixed amount of work on every core at the same moment, reporting the slowest core's time: perfect scaling is the same time as one core alone. Two workloads: `compute` (four independent adds per iteration, no memory traffic) and `memory` (24 passes of `memset` over a private 512 KiB buffer per core). Configurations: `c0`, `c2`, `c3`, `c0+c2`, `c0+c2+c3`. A core the kernel refuses to give a homebrew program is reported as `note,... threadCreate failed` and the configurations that need it are skipped, so whether homebrew can use cores 2 and 3 at all is part of what the first run will show. The rows are `result,corebench,<compute|memory>,<config>,<cores>,<runs>,<min_us>,<med_us>,<max_us>`.

**Status:** all three build without warnings (devkitARM r68). None has run on hardware. The 804 MHz setting, the speed-up request surviving a chainload (each tile binary sets it again itself), and the thread placement on cores 2 and 3 are the unconfirmed parts.

## Lessons for running and reading these benchmarks

- **The performance counters work from a `.3dsx` under Luma3DS 13.0.2.** `icachebench` printed `note,perf,...` lines, not `note,perf_unavailable`, so `svcControlPerformanceCounter` accepted `ENABLE` for a homebrew process. The cycle counter ran at the CPU clock: the `loop_256b` control took 1,051,362 counted cycles in 3,917 µs, which is 268.4 cycles per microsecond against `cpu_ticks_per_usec` 268.11 for the system tick.
- **Results repeat very closely.** Across the three suite sessions on each console, drawbench rows differ by under 1% and icachebench CPI by 0.01-0.03; flush and memory medians by 1-3%, with outliers in the max column. The next point is the same, from the first sessions.
- **Results repeat very closely on the first console.** Between the two sessions the `drawbench` `record_ms` values differ by at most 0.003 ms, the `icachebench` medians by under 1% (one 16 KiB point aside), and the tile renderer's `mean_cpu_build_record_frameend_us` by under 0.5%. That says nothing about other consoles.
- **Outliers are in the max column, not the median.** A handful of flush calls are 2-5 times slower than their median (for example 193 µs against 53 µs for one `svc_store` at 16 KiB; 276 µs against 128 µs for one GSP flush). Report the median and the spread across sessions, never a minimum or a single sample. The `cold-first` rows are single samples by design.
- **`begin_ms` is slack, not cost.** In `drawbench`, `begin_ms + record_ms + frameend_ms` is 16.7 ms for every variant (for example 6.797 + 9.710 + 0.202), because `C3D_FrameBegin(C3D_FRAME_SYNCDRAW)` waits for the display. When a variant's CPU work grows, `begin_ms` shrinks by the same amount.
- **The loop period is the display period.** In the tile renderer the median `period` is 16,712-16,717 µs for all four strategies. That is a refresh rate of 59.83 Hz `[measured]`, the same for every strategy that fits the frame, which is why the wiki decides on CPU and GPU cost, not on `period`.

## Building and running

There are two things to run, and a third way to run the four microbenchmarks one at a time.

| What | Binary | How it runs | Output |
| ---- | ------ | ----------- | ------ |
| **Micro suite** | `coffeebench.3dsx` | One binary; runs `icachebench`, `membench`, `flushbench` and `drawbench` in that order with no input. Each module is the standalone program's `main.c` compiled with `main` renamed (`suite/source/mod_*.c`), so its code and CSV are the same as the standalone one | `sdmc:/coffeepot/s<NNN>/<program>.csv`, one file per module |
| **Tile suite** | `coffeepot-tiles.3dsx`, then `t02.3dsx` to `t20.3dsx` | A chain: each of the 20 binaries measures one [variant](#tile-renderer-suite), writes its CSV and launches the next through Luma3DS's `hb:ldr` ([`common/chain.h`](common/chain.h)). Start only the first from the Homebrew Menu | `sdmc:/coffeepot/s<NNN>/tile-<id>.csv` and `tile-<id>-frames.csv` per variant |
| Single programs | `membench`, `flushbench`, `drawbench`, `icachebench`, `tile-renderer` | Built from their own folders, as before; each starts a new session and waits for START | `sdmc:/coffeepot/s<NNN>/<program>.csv` |

Why two mechanisms: the microbenchmarks are four short programs that need no isolation from each other beyond what each already does (every module sets up and tears down its own graphics state), so one binary is simplest and cannot fail halfway for a loader reason. The tile variants differ in compile-time constants, so 20 binaries are unavoidable and chaining is what keeps the run hands-free. Chaining gives each variant a fresh process (clean heap and linear memory), which is the point.

**Stage everything for the SD card** from the repository root (needs the devkitPro shell, so `DEVKITPRO` and `DEVKITARM` are set):

```sh
bash tools/stage_benchmarks.sh dist     # builds all, fails on any warning, writes dist/sdmc/3ds/...
```

Copy `dist/sdmc/*` to the SD card root. You get `3ds/coffeebench/coffeebench.3dsx` and `3ds/coffeepot-tiles/` with `coffeepot-tiles.3dsx` (the Homebrew Menu entry; a copy of `t01.3dsx`), `t01.3dsx` to `t20.3dsx` and `suite.txt`. `tools/build_benchmarks.sh` builds every folder and the tile suite and fails on any warning; the single programs build with `make` in their folder. Each project is a copy of the `3ds-examples` application template Makefile (public domain), with `-lcitro3d` added where needed; `common/bench.h` is shared by all of them.

**Running it.** Boot, close everything else, run `coffeebench` from the Homebrew Menu and wait for "finished"; then run `coffeepot-tiles` and leave it alone (about 15 seconds per variant, 5 minutes in all). Each suite run is a new session, `s001`, `s002`, ..., so run each suite at least three times and nothing is overwritten or appended. The chain stops, with a message on screen and `error,` or a missing file as evidence, at the first run with errors or a failed launch; START at any point stops it. **The chained launch is confirmed on hardware:** on an Old 3DS (Luma3DS 13.0.2) and a New 2DS XL (13.3.3) all 20 binaries ran in turn from one start. If it ever stops with "chainload FAILED", the CSV of the variant that just ran is still valid and the remaining binaries can be started by hand (they are plain `.3dsx` files, but the Homebrew Menu lists only `coffeepot-tiles.3dsx`; use 3dslink or move them into their own folders).

## Output format and metadata

Every benchmark writes the same lines to the console (stdout) and to `sdmc:/coffeepot/s<NNN>/<program>.csv` (the tile suite: `tile-<id>.csv`); the file is the record to copy results from (the console scrolls). `<NNN>` is the session number from `sdmc:/coffeepot/session`, incremented by whoever starts a run, so sessions never share or overwrite a file. Shared code: `common/bench.h` and `common/session.h`. Line kinds:

```text
meta,<key>,<value>
result,<program>,<test>,<variant>,<bytes>,<runs>,<min_us>,<med_us>,<max_us>     (membench, flushbench)
columns,...  then  result,drawbench,<variant>,<quads>,<frames>,<record_ms>,<frameend_ms>,<begin_ms>,<gpu_ms>,<cmdbuf_pct>
note,<text>
note,perf,<variant>,<bytes>,<insns>,<ccnt>,<icache_miss>,<fetch_stall>      (icachebench; medians per run)
note,pct,<program>,<variant>,<frames>,<p50_us>,<p95_us>,<p99_us>,<worst_us>,<missed_slots>   (frame-time helper, see the profiling cookbook)
error,<where>,<detail>
```

`meta` lines carry: program, session number, repository commit (`not-recorded` unless built by `tools/build_benchmarks.sh` or `stage_benchmarks.sh`, which add `_dirty` for an uncommitted tree), Luma3DS version (`svcGetSystemInfo` type `0x10000`, or `not-readable`), system version string (or `not-readable`), build date and compiler, model (`CFGU_GetSystemModel`), New 3DS flag (`APT_CheckNew3DS`), firmware and kernel version, requested speed-up setting, ticks per microsecond, free linear memory. Two things cannot be read at run time and are recorded as `not-readable`: the libctru and citro3d versions (the installed headers have no version macro; take them from [source versions](../engineering/source-versions.md)) and, **New 3DS:** the 804 MHz / extra-L2 state (libctru only has the setter `osSetSpeedupEnable`; build with `-DBENCH_SPEEDUP=1` or `=0` added to `CFLAGS` if you want the program to set it on a New 3DS, otherwise note the state yourself; on an Old 3DS the option does nothing). Failed API calls and a failed SD-card open produce `error,` lines; the program carries on where it can, and the final `note,done errors=N` line tells you whether to trust the run. A run with any error line is not a result.

## membench

**Question:** how fast can the CPU write to the ordinary heap versus the linear heap, and what does a cache flush cost for a given range and cache state?

**Method:** for 16 KiB, 256 KiB and 1 MiB buffers, 31 runs each of: `memset` into heap memory, `memset` into linear memory, `memcpy` heap→linear, `svcFlushProcessDataCache` after rewriting the range, `GSPGPU_FlushDataCache` after rewriting the range (the rewrite is not timed). Timing with `svcGetSystemTick` (268,111,856 Hz on all models); min / median / max in microseconds.

Whole-linear-heap flush (what `C3D_FrameEnd(0)` does) is reported as three separate rows, because they measure different things:

| Variant | What it is | What it shows |
| ------- | ---------- | ------------- |
| `cold-first` | One call, right after the largest free linear block was written; no earlier flush in this run. A single sample (min = med = max). | Cost of the first flush including real write-back of a freshly written block; not repeatable by design. |
| `warm-clean` | 31 consecutive calls with no writes in between. | Per-call plus per-line walk cost when nothing is dirty. |
| `dirtied-chunk` | 31 calls, the largest free linear block (size in `meta,dirtied_chunk_bytes`) rewritten, untimed, before each. | Cost with a large dirty region. Only that block is dirtied, not every byte of the heap; and the part of it larger than the L1 data cache was already written back during the rewrite, so only the cache-resident tail is dirty at call time. |

The `*_written` flush rows (`svc_flush_written`, `gsp_flush_written`) have the same limitation for 256 KiB and 1 MiB: they time a flush after a sequential rewrite, not after a dirty-resident state. Use [flushbench](#flushbench) to separate those.

**Variants to run:** default (works on an Old 3DS, which is the primary target); with `APT_SetAppCpuTimeLimit(80)` added before the run; and, **New 3DS:** with `-DBENCH_SPEEDUP=1`.

**Results:**

`[measured]` Old 3DS, system 11.17.0-50U, Luma3DS 13.0.2, default variant only (the 80% core-1 limit variant has not been run). Medians in microseconds; two sessions (`o3ds-0` / `o3ds-1`), 31 runs each; the full min / median / max rows are in the CSV files.

| Test | Size | Median, session 0 / 1 (µs) | Derived |
| ---- | ---- | -------------------------- | ------- |
| `memset_heap` | 16 KiB | 24.60 / 24.61 | 635 MiB/s |
| `memset_linear` | 16 KiB | 24.46 / 24.53 | 635 MiB/s |
| `memset_heap` | 256 KiB | 668.0 / 665.2 | 374 MiB/s |
| `memset_linear` | 256 KiB | 668.5 / 668.0 | 374 MiB/s |
| `memset_heap` | 1 MiB | 2,691.9 / 2,691.3 | 371 MiB/s |
| `memset_linear` | 1 MiB | 2,692.8 / 2,691.7 | 371 MiB/s |
| `memcpy_heap_to_linear` | 16 KiB | 140.2 / 140.6 | 112 MiB/s |
| `memcpy_heap_to_linear` | 256 KiB | 2,667.7 / 2,679.5 | 94 MiB/s |
| `memcpy_heap_to_linear` | 1 MiB | 10,728.3 / 10,728.4 | 93 MiB/s |
| `svc_flush_written` | 16 KiB, 256 KiB, 1 MiB | 63.7, 62.7, 73.4 / 68.1, 62.9, 74.6 | |
| `gsp_flush_written` | 16 KiB, 256 KiB, 1 MiB | 117.7, 121.1, 130.1 / 114.0, 117.1, 128.4 | |
| `gsp_flush_whole` `cold-first` | 32 MiB (single sample) | 137.4 / 140.4 | |
| `gsp_flush_whole` `warm-clean` | 32 MiB | 79.9 / 89.3 | |
| `gsp_flush_whole` `dirtied-chunk` | 32 MiB (30.9 MB rewritten before each call) | 135.6 / 136.2 | |

What these show (the page they settle: [memory and cache](../engineering/memory-and-cache.md#measured-costs-old-3ds)):

- Writing the linear heap is as fast as writing the ordinary heap: both are cached, and the two rows agree to within 0.5%.
- Sequential writes run at about 370 MiB/s once the range is larger than the 16 KiB L1 data cache, and about 635 MiB/s inside it. A heap-to-linear `memcpy` runs at about 93 MiB/s, roughly four times slower than `memset` of the same size.
- A flush after a write costs about 60-75 µs through the kernel call and 115-130 µs through the GSP, and barely grows from 16 KiB to 1 MiB.
- Flushing the whole 32 MiB linear heap through the GSP costs 80-140 µs, whether clean or with 30 MB just rewritten. That is the flush `C3D_FrameEnd(0)` does each frame.

## flushbench

**Question:** what does a data-cache flush cost, and how much of that is fixed overhead, how much is walking the range, and how much is writing dirty lines back? How do the GSP service flush (`GSPGPU_FlushDataCache`), the kernel flush (`svcFlushProcessDataCache`: clean and invalidate) and the kernel clean-only call (`svcStoreProcessDataCache`) compare, and what does `C3D_FrameEnd(0)` cost compared with `C3D_FrameEnd(GX_CMDLIST_FLUSH)`?

**Method:** allocates the largest power-of-two linear buffer up to 32 MiB. For each size (1 KiB to 32 MiB) and operation, 15 runs per state. All set-up work is outside the timed region. The Old 3DS L1 data cache is 16 KiB, so "resident" states only run for ranges up to 16 KiB (`l1_resident_max_bytes`).

| State | Set-up before each timed call | Can show | Cannot show |
| ----- | ----------------------------- | -------- | ----------- |
| `clean-cold` | `svcFlushProcessDataCache` of the range (cleans and invalidates) | Fixed call overhead plus per-line walk with nothing to write back and little resident | Cost with resident lines; a "clean" range here is also absent from the cache |
| `clean-warm` (<= 16 KiB) | flush, then read one byte per 32-byte line | Walk cost when lines are resident but clean | Any write-back cost |
| `dirty-warm` (<= 16 KiB) | flush, `memset` every byte, then read one byte per line | Write-back cost of a fully dirty, fully resident range; the closest this suite gets to a per-line dirty cost | Behaviour for ranges larger than L1; the read-back is there to ensure residency in case the cache does not allocate on write |
| `written-all` (any size) | flush, then sequential `memset` of the whole range | What a program sees after writing a big buffer once and then flushing | Per-line dirty cost over the whole range: lines written earlier were already evicted to memory during the write, so only the cache-resident tail (at most the L1 size) is still dirty when the call starts. Large-range numbers are dominated by walking the range, not by write-back. |

An earlier version of this benchmark wrote one byte per line and labelled the result "dirty" for ranges up to 32 MiB; that overstated the dirty amount for anything above the L1 size, which is why the states above exist. Compare `dirty-warm` with `clean-warm` at the same size for the write-back cost, `clean-cold` across sizes for the walk cost, and `written-all` for the realistic total. `svcStoreProcessDataCache` leaves lines valid, so after it a range is clean but still resident.

`C3D_FrameEnd`: 15 frames that only clear the top screen, timing the `C3D_FrameEnd` call with flags `0` and with `GX_CMDLIST_FLUSH` (the `meta,linear_heap_bytes` value is the range flushed by flags `0`). This is CPU time inside the call only; it does not show GPU time or whether the next frame stalls.

**What it settles:** the open question on [memory and cache](../engineering/memory-and-cache.md#the-c3d_frameend-flush), whether targeted flushing is worth the bookkeeping. It measures the flush in isolation; the effect on a real scene needs a scene benchmark.

**Results:** `[measured]` Old 3DS, system 11.17.0-50U, Luma3DS 13.0.2, two sessions (`o3ds-0` / `o3ds-1`), 15 runs each; medians in microseconds, selected sizes (session 0 / session 1). All 68 rows are in the CSV files.

| State | Size | `gsp_flush` | `svc_flush` | `svc_store` |
| ----- | ---- | ----------- | ----------- | ----------- |
| `clean-cold` | 1 KiB | 58.8 / 58.2 | 21.5 / 23.0 | 22.5 / 21.8 |
| `clean-cold` | 16 KiB | 89.2 / 85.1 | 35.4 / 35.0 | 30.7 / 29.4 |
| `clean-cold` | 256 KiB | 92.9 / 84.4 | 35.8 / 36.2 | 30.2 / 29.1 |
| `clean-cold` | 1 MiB | 90.5 / 93.6 | 34.2 / 34.1 | 30.5 / 30.3 |
| `clean-cold` | 16 MiB | 85.8 / 91.8 | 34.7 / 35.0 | 29.9 / 29.6 |
| `clean-warm` | 16 KiB | 87.6 / 89.8 | 33.6 / 32.7 | 29.7 / 30.7 |
| `dirty-warm` | 16 KiB | 120.7 / 121.5 | 65.1 / 57.9 | 52.6 / 53.1 |
| `written-all` | 16 KiB | 119.8 / 127.9 | 60.4 / 60.2 | 57.3 / 60.5 |
| `written-all` | 256 KiB | 124.2 / 124.8 | 62.7 / 62.6 | 57.1 / 57.8 |
| `written-all` | 1 MiB | 129.8 / 129.8 | 76.2 / 75.4 | 70.5 / 69.9 |
| `written-all` | 16 MiB | 134.6 / 133.4 | 79.7 / 80.4 | 75.0 / 75.6 |

| `C3D_FrameEnd` (15 frames, top screen cleared; 32 MiB linear heap) | Median, session 0 / 1 (µs) |
| ----------------------------------------------------------------- | --------------------------- |
| flags `0` (flushes the whole linear heap) | 147.4 / 147.8 |
| `GX_CMDLIST_FLUSH` (no heap flush) | 57.3 / 57.5 |

What these show:

- **The cost of a flush stops growing with the range.** The kernel flush costs 21-23 µs for 1 KiB, 28 µs for 4 KiB and 35 µs for 16 KiB, and then stays at 34-36 µs for every size up to 16 MiB. The GSP flush has the same shape about 55 µs higher (58 µs at 1 KiB, 85-93 µs from 16 KiB up), and the clean-only call is 4-5 µs cheaper than the flush. The mechanism was not read from the kernel; a switch to a whole-cache operation above some range size would fit, with the break between 16 KiB and 64 KiB `[hypothesis]`.
- **The GSP route adds a fixed 37-57 µs per call** over the kernel call: the round trip to the `gsp` module.
- **Dirty data costs a little.** At 16 KiB, a fully dirty and resident range costs about 30 µs more than a clean one (65 against 34 µs for `svc_flush`; 53 against 30 µs for `svc_store`), about 60 ns per 32-byte line for 512 lines. `written-all` rises from 60 to 80 µs between 16 KiB and 16 MiB, and the caveat above applies: only the cache-resident tail was dirty.
- **The default whole-heap flush costs about 90 µs per frame.** `C3D_FrameEnd(0)` took 147 µs and `C3D_FrameEnd(GX_CMDLIST_FLUSH)` 57 µs on an empty scene; the difference matches the 80-90 µs of the `warm-clean` whole-heap flush in `membench`. That is about 0.5% of a 16.7 ms frame.

## drawbench

**Question:** what does a citro3d draw call cost on the CPU, and how much do common state changes add?

**Method:** draws N 4×4-pixel textured quads per frame in six variants (the program now sweeps N itself through 125, 250, 500, 1000, 2000 and 4096; the first hardware run used the earlier interactive build at its default of 1000), 120 frames each (first 4 discarded), averaging five separately measured quantities:

| Column | Source | Covers | Does not cover |
| ------ | ------ | ------ | -------------- |
| `record_ms` | `C3D_GetProcessingTime()` | CPU time citro3d reports for recording the frame's draw commands | `C3D_FrameEnd`, including the linear-heap cache flush |
| `frameend_ms` | `svcGetSystemTick()` around `C3D_FrameEnd(0)` | Command-list finalisation, cache flush, GX queueing | GPU execution |
| `begin_ms` | `svcGetSystemTick()` around `C3D_FrameBegin(C3D_FRAME_SYNCDRAW)` | Mostly waiting for the previous frame | CPU work (it is a stall indicator) |
| `gpu_ms` | `C3D_GetDrawingTime()` | GPU time of the last completed frame, including the display transfer | CPU cost |
| `cmdbuf_pct` | `C3D_GetCmdBufUsage()` before `C3D_FrameEnd`, peak over the variant | How full the 1 MiB command buffer got | n/a |

Compare variants using `record_ms + frameend_ms` for total CPU cost per frame; `record_ms` alone understates it. The screen also prints the extra `us/quad` of (record + frameend) relative to `batched`. If `cmdbuf_pct` reaches 98% an `error,cmdbuf_overflow_risk` line is written and that row must not be used. Setup failures (`C3D_Init`, render target, shader parse, texture allocation, buffer binding) are reported and the program idles instead of running a broken measurement. Limitation: `C3D_GetProcessingTime` is a citro3d-internal figure whose exact start and end points were not verified on hardware, hence the independent tick timing.

| Variant | What changes per quad |
| ------- | --------------------- |
| `batched` | Nothing: one `C3D_DrawArrays` for all quads |
| `per-quad` | One `C3D_DrawArrays` per quad |
| `+uniform` | Plus a uniform update before each draw |
| `+texbind` | Plus `C3D_TexBind` of the *same* texture (citro3d marks it dirty anyway) |
| `+texenv` | Plus alternating between two combiner configurations |
| `+texswap` | Plus alternating between two textures |

The shader (`source/vshader.v.pica`) transforms position by a projection matrix plus an offset uniform.

**What it settles:** the draw-call cost table on [GPU rendering](../engineering/PICA200/gpu-rendering.md#what-a-draw-call-costs), currently labelled with no measured numbers, and the sprite-batching trade-offs on [2D rendering](../engineering/PICA200/rendering-2d.md#sprites).

**Results:** `[measured]` Old 3DS, system 11.17.0-50U, Luma3DS 13.0.2, 1000 quads only (the Up/Down size change was not used), 116 measured frames per variant, session 0; session 1 differs by at most 0.012 ms in any column (`hardware-runs/o3ds-1`). Command buffer 1 MiB (`meta,cmdbuf_bytes`).

| Quads | Variant | record_ms | frameend_ms | begin_ms | gpu_ms | cmdbuf_pct | record + frameend |
| ----- | ------- | --------- | ----------- | -------- | ------ | ---------- | ----------------- |
| 1000 | `batched` | 0.047 | 0.164 | 16.495 | 1.039 | 0.0 | 0.211 ms |
| 1000 | `per-quad` | 9.710 | 0.202 | 6.797 | 1.297 | 8.4 | 9.912 ms |
| 1000 | `+uniform` | 10.916 | 0.204 | 5.587 | 1.551 | 11.5 | 11.120 ms |
| 1000 | `+texbind` | 11.798 | 0.204 | 4.706 | 1.529 | 13.8 | 12.002 ms |
| 1000 | `+texenv` | 10.623 | 0.214 | 5.870 | 1.488 | 10.7 | 10.837 ms |
| 1000 | `+texswap` | 11.768 | 0.207 | 4.732 | 1.525 | 13.8 | 11.975 ms |

Per quad (divide by 1000; arithmetic from the table): a separate `C3D_DrawArrays` costs 9.7 µs of recording time and about 88 bytes of command buffer (8.4% of 1 MiB); the uniform update adds 1.2 µs and about 32 bytes; a redundant `C3D_TexBind` adds 2.1 µs and about 56 bytes; the combiner change adds 0.9 µs and about 24 bytes; swapping between two textures costs the same as the redundant bind (2.1 µs). One draw call per quad is 47 times the CPU time of one batched call for the same 1000 quads (9.9 ms against 0.21 ms), and 1000 separate draws use about 60% of the 16.7 ms frame. The GPU side stays small: 1.04 ms for the batched frame, 1.3-1.55 ms for the per-quad variants (`gpu_ms` includes the display transfer, so about 1 ms of it looks like a floor).

## icachebench

**Question:** how does the cost per instruction change as the amount of distinct code executed per pass grows past the Old 3DS's 16 KB instruction cache? (An independent test of the figures G-Diffuser reports, `[reported]` on [CPU optimisation](../engineering/cpu-optimization.md#the-g-diffuser-i-cache-probe); not a replication of its probe.)

**Method:** straight-line ARM-mode functions of 4, 8, 12, 16, 20, 24, 32, 48 and 64 KiB, each made only of `add rN, rN, #1` on four registers (no loads, no branches except the final `bx lr`, so the work is the same kind per instruction). Each run calls one function 64 times and times the loop with `svcGetSystemTick`; 15 runs per variant after one untimed warm-up call. The `loop_256b` control executes the same 16,384 instructions as `straight_64k` from a 256-byte loop body, so any time difference between the two is a footprint effect, not an instruction-count effect. If `svcControlPerformanceCounter` accepts `ENABLE` for the process, it also reads the cycle counter, `INST_CACHE_MISS` and `STALL_BY_LACK_OF_INST` around each run and prints `note,perf,...` lines; otherwise it prints `note,perf_unavailable` and only times. Footprint is controlled at build time; the program does not read the cache size from the hardware.

| Variant | Distinct code per pass | Instructions per pass |
| ------- | ---------------------- | --------------------- |
| `straight_4k` ... `straight_16k` | 4-16 KiB (fits in 16 KiB if that is the cache size) | 1,024-4,096 |
| `straight_20k`, `_24k`, `_32k`, `_48k`, `_64k` | 20-64 KiB | 5,120-16,384 |
| `loop_256b` | 256 bytes | 16,384 |

**Reading the output:** compare µs per instruction (`med_us` / (insns per pass x 64)) across footprints; the counters, if available, give cycles per instruction and I-cache misses per instruction directly. Expect, if the 16 KB figure is right, a step between 16 and 24 KiB `[hypothesis]`; the `loop_256b` row is the no-miss reference. **New 3DS:** run it separately (and with `-DBENCH_SPEEDUP=0` and `=1`); the L2 cache may change the shape, and no New 3DS statement is made here. The straight-line code is not typical of game code (no data accesses, perfectly predicted), so this is a probe of fetch cost only, not a prediction for any program.

**Changes after the first hardware run.** (1) `loop_256b` was built from a 260-byte, 65-instruction loop body that executed 16,642 instructions, not the 256 bytes and 16,384 the label said, so it was not an exact control for `straight_64k`. It is now a line-aligned 64-instruction (256-byte) body run 256 times: 16,384 instructions in the loop plus `mov`, `b` and `bx lr` once per call, and the `insns` column counts every executed instruction (the straight functions add their `bx lr`). The tables below used the old counts: the straight-line rows are off by at most 0.1% (their `bx lr`), but the `loop_256b` row is not: it executed 16,642 instructions per call, not 16,384, so its CPI was 0.99 rather than 1.00 (the ARM11 folds the taken loop branch). A CPI below 1 is what that fold would produce, not an error in the counters. (2) Every `svcControlPerformanceCounter` call is now checked: a failure is reported as `error,perf ...`, turns the counters off, and the run is no longer a result. A zero cycle count is also an error. The first-build results are below; the corrected control has run since ([later runs](#suite-runs-old-3ds-and-new-3ds)): `loop_256b` CPI 0.99 on both consoles, no counter errors, and the straight-line rows unchanged.

**Verification status:** builds (devkitARM r68, no warnings with `-Wall -Wextra`); run on an Old 3DS (below), the first build only.

**Results:** `[measured]` Old 3DS, system 11.17.0-50U, Luma3DS 13.0.2, two sessions (`o3ds-0` / `o3ds-1`), 15 runs each. Medians per run of 64 passes. CPI, misses and stall share come from the performance counters of session 0 (session 1 agrees within 0.05 CPI, except at 16 KiB where both are shown). Misses per line fetched is `icache_miss` divided by the 32-byte lines executed (footprint / 32 x 64 passes).

| Variant | Footprint | Med µs, session 0 / 1 | CPI | I-cache misses per line fetched | Cycles stalled on fetch |
| ------- | --------- | --------------------- | --- | ------------------------------- | ----------------------- |
| `straight_4k` | 4 KiB | 249.4 / 250.6 | 1.04 | 0.00 | 0.5% |
| `straight_8k` | 8 KiB | 494.4 / 496.1 | 1.02 | 0.00 | 0.6% |
| `straight_12k` | 12 KiB | 742.2 / 741.6 | 1.02 | 0.00 | 0.9% |
| `straight_16k` | 16 KiB | 1,116 / 1,065 | 1.15 / 1.10 | 0.03 / 0.02 | 12% / 8% |
| `straight_20k` | 20 KiB | 3,529 / 3,514 | 2.89 | 0.41 | 65% |
| `straight_24k` | 24 KiB | 6,020 / 5,983 | 4.11 | 0.67 | 75% |
| `straight_32k` | 32 KiB | 10,947 / 10,935 | 5.60 | 1.01 | 82% |
| `straight_48k` | 48 KiB | 16,390 / 16,401 | 5.59 | 1.00 | 82% |
| `straight_64k` | 64 KiB | 21,832 / 21,865 | 5.58 | 1.00 | 82% |
| `loop_256b` | 260 B in the first build (16,642 instructions; see above) | 3,917 / 3,917 | 0.99 | 0.00 | 0% |

What this shows (it settles the open question on [CPU optimisation](../engineering/cpu-optimization.md#the-g-diffuser-i-cache-probe)):

- **A cliff just above 16 KiB.** Up to 12 KiB the core runs one instruction per cycle (CPI 1.02-1.04). At 16 KiB it is nearly there (CPI 1.10-1.15, about 15 misses per pass). At 20 KiB the same kind of instruction costs 2.9 cycles, and from 32 KiB it costs 5.6 cycles however far the footprint grows: about 5.6 times slower per instruction than the 260-byte loop that has almost the same instruction count.
- **Every line misses once the footprint is twice the cache.** Misses per line fetched are 0.41 at 20 KiB, 0.67 at 24 KiB and 1.00 from 32 KiB. A strict least-recently-used cache would miss on every line as soon as the footprint passed 16 KiB, so the gradual rise suggests a non-LRU replacement policy `[hypothesis]` (DDI 0360E lists random and round-robin; the policy register was not read).
- **A miss costs about 36.5 cycles** (stall cycles divided by misses: 36.4-36.7 from 20 KiB up, in both sessions), or about 136 ns. At 32 KiB and above, 82% of all cycles are fetch stalls.
- The figures match the G-Diffuser probe within 2% (CPI 1.06 / 4.10 / 5.62 at 16 / 24 / 32 KB; about 37 cycles per miss; `[reported]`) here at 1.10-1.15 / 4.11 / 5.60.
- The step is the I-cache, not the instruction count: `loop_256b` runs about the same number of instructions with no misses at all.
- No New 3DS statement is made. The cache-type register was still not read, so 16 KiB is inferred from the step, not confirmed.

## Tile-renderer suite

Twenty builds of the [2D tile renderer](../engineering/PICA200/2d-tile-renderer.md), chosen from experiments E1 to E6 by how much each settles; the source of truth is [`tile-renderer/suite.txt`](tile-renderer/suite.txt). Everything not listed stays at its default (`TILE=16`, 416 visible tiles, `NPC_COUNT=40`, `CAM_SPEED=2`, square loop, 120 warm-up and 600 measured frames). The id is also the CSV name suffix.

| t | Id | Flags | Settles |
| - | -- | ----- | ------- |
| 01-04 | `direct_t16`, `manual_t16`, `chunks16_t16`, `cache_t16` | `TILE_STRATEGY` 0, 1, 2, 3 | Baseline; repeats the first run |
| 05, 06 | `direct_t32`, `manual_t32` | `TILE=32` (126 tiles) | E1 crossover, low end |
| 07, 08, 09 | `direct_t8`, `manual_t8`, `chunks16_t8` | `TILE=8` (1,581 tiles) | E1 crossover, high end; whether chunks scale |
| 10 | `cache_still` | strategy 3, `BENCH_PATH=1` | E2: the cache with a standing camera (no redraws) |
| 11 | `cache_speed4` | strategy 3, `CAM_SPEED=4` | E2: redraw rate against speed |
| 12 | `cache_edit30` | strategy 3, `MAP_EDIT_PERIOD=30` | E2: edits force full redraws |
| 13, 14 | `chunks8_t16`, `chunks32_t16` | `CHUNK_TILES` 8, 32 | E4: chunk size |
| 15 | `chunks16_edit1` | strategy 2, `MAP_EDIT_PERIOD=1` | E4: rebuild cost per edited chunk |
| 16, 17 | `direct_tex1`, `direct_tex2` | `TEX_MODE` 1, 2 | E3: cost of texture changes (compare with 01) |
| 18 | `direct_bottom4` | `BOTTOM_PERIOD=4` | E5: static bottom screen |
| 19, 20 | `direct_shape1`, `direct_shape2` | `SHAPE_MODE` 1, 2 | E6: shape batching (compare with 01) |

Not in the suite: `TILE=12` and 24, `NPC_COUNT=0`, other speeds and edit rates, `TEX_MODE` with a raised command buffer. Add lines to `suite.txt`; the build script derives the count.

Per variant the suite writes two files. `tile-<id>.csv` has the usual `meta`, `result` and `summary` lines (and the suite index, id, session and provenance in `meta`). `tile-<id>-frames.csv` has one row per measured frame with every phase in microseconds (`period_us`, `update_us`, `build_us`, `record_us`, `framebegin_us`, `frameend_us`, `c3d_proc_us`, `c3d_gpu_us`), so rebuild spikes and what they do to the loop period can be seen, not only summarised. The suite does not compare pictures itself, because a variant that silently omits work would look fast; the [emulator picture check](#picture-equivalence-emulator) does, for 17 variants, and has found no difference. Accounting: since the hardware runs, `dropped_objects` counts every citro2d draw call that reports failure (tiles, sprites and shapes; `C2D_DrawText` returns nothing and is not counted) and `cmdbuf_peak_pct` is sampled after the final citro2d flush. The hardware CSVs recorded here predate that change: in them zero covers only the tile calls and the occupancy is a lower bound.

**Status:** all 20 build without warnings (devkitARM r68) and ran on an Old 3DS and a New 2DS XL (268 MHz mode), one session each, with no errors, no missed frames and no dropped objects. Results: [tile page](../engineering/PICA200/2d-tile-renderer.md#suite-results-old-3ds-and-new-3ds). Pictures were compared only in an emulator ([picture equivalence](#picture-equivalence-emulator)), not on hardware.

## Picture equivalence (emulator)

The tile renderer's strategies are timed against each other, so they have to draw the same picture. `tools/emu_pixel_check.py` checks that in the Azahar emulator, and nothing else: **emulator output is never a timing result, and it is not a hardware picture comparison.**

How it works. `tools/build_pixel_check.sh` builds 17 variants listed in [`tile-renderer/pixel-check.txt`](tile-renderer/pixel-check.txt) with `BENCH_AUTO=1 BENCH_DUMP_FRAME=300`. At frame 300 the simulation freezes (camera, sprites and animation stop; the scene is deterministic, so every variant freezes in the same state) and six frames later both screens are written to the SD card as raw RGB8. Freezing means every buffer of the display double buffer holds the same image, so it does not matter which one is read. The harness starts each variant in Azahar, copies the two files out, stops the emulator and compares each variant with its reference byte for byte.

Result (17 variants, 13 comparisons, one run each):

| Compared with the direct-tile picture | Result |
| ------------------------------------- | ------ |
| Manual, chunks (16 tiles) and cache at 16 px tiles | identical, top and bottom screens |
| Manual, chunks and cache at 32 px tiles | identical |
| Manual and chunks at 8 px tiles (the cache does not support 8) | identical |
| Chunks of 8 and of 32 tiles at 16 px | identical |
| `TEX_MODE` 1 (two atlases alternating) and 2 (two runs) | identical |
| `SHAPE_MODE` 2 (shapes grouped) against `SHAPE_MODE` 1 (interleaved) | identical |
| **Control:** 16 px against 32 px tiles (must differ) | 91,677 of 96,000 top-screen pixels differ, so the comparison does detect a different picture |

What this establishes: in Azahar, at that frame, the four tile strategies (and the chunk size, texture-mode and shape-order variants) draw the same top-screen picture and the same bottom-screen bytes, so their timings in the tables above are timings of the same visible result. The top-screen picture was looked at: water, sand, grass, trees and sprites, not a blank or noise frame ([reference pictures](emulator-checks/), results in `emulator-checks/results.json`).

What it does not establish:

- **Hardware.** The PICA200 and the display transfer may round or filter differently from the emulator; the same comparison on a console (a dump from `BENCH_DUMP_FRAME` builds, compared on a PC) is still owed.
- **One frame, one camera position.** Frame 300 of the default path, after the cache has been built. Frames with a cache rebuild, map edits (`MAP_EDIT_PERIOD`), `TILE=12` or 24, `NPC_COUNT=0` and other camera speeds were not compared.
- **The bottom screen is compared as bytes, not as a picture.** Azahar writes the RGB8 display-transfer output so that each 4 KiB page of the framebuffer holds whole 3-byte pixels and one unused byte (found by inspecting a flat colour: its byte phase changed every 4,096 bytes); the harness removes that skew. The top screen decodes to a clear scene; the bottom screen's dump did not decode to a clean picture, and its geometry in the emulator was not worked out. Whether real hardware has the page skew is unknown (libctru code treats the framebuffer as contiguous).

Run it (Windows, Azahar installed, devkitPro shell for the build):

```sh
bash tools/build_pixel_check.sh /tmp/pixel                  # builds the 17 variants
python tools/emu_pixel_check.py /tmp/pixel out-dir          # runs each in Azahar, compares, writes out-dir/results.json
```

The same harness approach is used for a smoke test of the suites: `tools/emu_smoke.py` starts a `.3dsx` in the emulator and reports whether each expected CSV reaches `note,done` with no `error,` line ([results](#emulator-smoke-tests)).

### Emulator smoke tests

`tools/emu_smoke.py` runs a `.3dsx` in Azahar and checks that each expected CSV reaches `note,done` with no `error,` line. It shows that a program starts, finishes and writes its log; it says nothing about timing or counters. Azahar in this installation is configured as an Old 3DS; the New 3DS run below temporarily set `is_new_3ds=true` and restored it.

- **`coffeebench`** (micro suite, one run, 111 s): `membench`, `flushbench` and `drawbench` ran to `note,done errors=0`. `icachebench` ran to the end but reported 10 `error,perf_cycle_counter_zero` lines: Azahar accepts `svcControlPerformanceCounter` `ENABLE` and then every counter reads 0. That is the corrected harness doing its job (the first build would have printed the zeros as results), and it means the counters can only be checked on hardware.
- **`coffeebench-n3ds`**: on an Old 3DS configuration it started, advanced the session counter and wrote no CSV, its refusal path (the message was not looked at). With Azahar set to a New 3DS (227 s) all ten files, both passes of all five modules, reached `note,done`; `corebench` created its threads on cores 2 and 3 and reported `errors=0`, and `icachebench` showed the same 10 zero-counter errors per pass as above. What this does **not** show: the emulator ignores the speed-up (`clock_probe_us` is 14,984 in both the 268 and 804 MHz passes, so the probe does separate clocks only on hardware), its threads scale perfectly because it simulates them, and none of its timings means anything. Whether a real New 3DS gives a homebrew program cores 2 and 3, and what 804 MHz does, remain for hardware. This run also found a bug the Old 3DS configuration could not: a module's error count was not reset when the suite ran it a second time, so the second pass reported `errors=20` for 10 errors; fixed in `bench.h`.
- **Tile-renderer suite:** the chained launch needs Luma3DS's `hb:ldr` port, which an emulator does not provide, so the chain itself was not tested in the emulator (it ran on both consoles).


## Planned experiments

Not written, not run; listed so that the question is recorded, not because any result exists. Each would need its own project, README section and `bench.h` output.

| Family | Question | Notes |
| ------ | -------- | ----- |
| GPU workload | Draw overhead, vertex rate, fill rate, texture bandwidth, render-target changes and synchronisation, separately | Would use the GSP performance log ([performance](../engineering/performance.md#per-operation-gpu-times-the-gsp-performance-log)) to separate P3D, PPF and PSC time |
| Scheduling | Frame time with a worker on core 1 at several `APT_SetAppCpuTimeLimit` percentages, versus no worker | Protocol in [threading](../engineering/threading.md#proving-a-worker-improves-frame-time) |
| Real scene | A fixed 2D tilemap scene (for example the [2D tile renderer](../engineering/PICA200/2d-tile-renderer.md) variants) with frame-time percentiles and missed frames | Needs the frame-time helper from the [profiling cookbook](../engineering/performance.md#frame-time-distribution-and-missed-frames) |
| Indirect dispatch | Per-element function-pointer dispatch versus grouping by operation type | Companion to `icachebench`; would use the same counters |
| GPU throughput | Vertex rate, fill rate and texture bandwidth with large primitives | `drawbench` uses 4x4-pixel quads, so it characterises submission, not the GPU. Needs large triangles, many vertices, and textures of several sizes and formats |
| GPU stages | Geometry, pixel processing, display transfer and queue stalls separately | Uses the GSP performance log, where it is readable |
| CPU memory | Data-cache and FCRAM latency, random versus sequential reads, branch behaviour, instruction mixes | `membench` measures library-call bandwidth, not latency or read bandwidth; extend it with pointer-chasing and a read loop |
| Threads and services | Core-1 scheduling, synchronisation, and the cost of an IPC call to a system service | Protocol in [threading](../engineering/threading.md) and [IPC](../engineering/ipc-and-services.md) |
| Storage | SD-card and RomFS read, write and open latency; asset-loading patterns | None written |
| Frame pacing | Distinguishing GPU completion, VBlank waits, presentation and frame-time spikes | Per-frame files from the tile suite are a start |
| Audio and network | Buffering deadlines, latency, throughput and contention with system services | None written |
| Reproducibility | At least three sessions after a fresh boot, alternating A and B runs for renderer comparisons | Each suite run is one session; run each suite three times |

## Recording results

Add results to this README (one table per benchmark, one row per console and variant) and to the page whose claim they settle, labelled `[measured]` with:

```text
Console:    e.g. Old 3DS XL, system 11.17.0-50U, Luma3DS v13.5
Build:      commit hash of this repository, devkitARM/libctru versions
Variant:    default / speed-up on / core-1 share 80% / N quads
Output:     the sdmc:/coffeepot/s<NNN>/ directory from the SD card (a photo of the screen is a fallback)
```

Run each benchmark at least three times after a fresh boot and report the spread; a single run is not a result. The Old 3DS data above has two sessions, so it is recorded as `[measured]` with that stated; a third session closes the gap.

## Sources

- [3ds-examples](https://github.com/devkitPro/3ds-examples) — application template Makefile.
- [citro3d](https://github.com/devkitPro/citro3d) — `source/renderqueue.c` (frame timing, the linear-heap flush), `source/base.c` (state updates per draw).
- [libctru](https://github.com/devkitPro/libctru) — `svc.h`, `services/gspgpu.h`, `os.h`, `allocator/linear.h`.

## tilerenderer (in the engineering pages)

The scripted mode of the [2D tile renderer](../engineering/PICA200/2d-tile-renderer.md#experiments) writes `sdmc:/coffeepot/s<NNN>/tile-<id>.csv` (a standalone `BENCH_AUTO=1` build uses the id `tilerenderer`) in the same `meta`/`result`/`note`/`error` layout as above. Differences: one file per session and variant (older builds opened a single file for append); `result,tilerenderer,<phase>,<variant>,<visible_tiles>,<frames>,<min_us>,<med_us>,<max_us>` has one line per phase (period, update, build, record, framebegin, frameend, c3d_proc, c3d_gpu); a `summary,tilerenderer,<variant>,<key>,<value>` line kind carries means, p99, missed frames, rebuilds, dropped objects, command-buffer peak and end-of-run memory. Status: builds; not run on hardware or an emulator. Experiments E1 to E6 are specified on the tile renderer page.
