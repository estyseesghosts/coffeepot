# CTR benchmark suite

Small, self-contained 3DS programs that measure costs the wiki's engineering pages depend on: memory and cache behaviour, cache-flush cost, instruction-cache pressure, and citro3d draw-call overhead. Each one builds from source with the standard devkitPro toolchain and prints its results on the console's screen and writes them to a file on the SD card. They exist so that performance claims on this wiki can become `[measured]` instead of `[hypothesis]` ([evidence labels](../../AGENTS.md#evidence-labels)).

**Status: the programs build; none has been run on hardware yet. No results are recorded.** The tables under each benchmark show what to record.

## Before you start

- Build environment: devkitPro with the `3ds-dev` group. Verified to build without warnings with devkitARM r68, libctru 2.7.1, citro3d 1.7.1 and picasso 2.7.2 (the versions on [source versions](../engineering/source-versions.md)), using `tools/build_benchmarks.sh`, which fails on any warning.
- An Old 3DS (the primary target; a New 3DS gives a second data set and must be recorded separately) with [Luma3DS](../homebrew/luma3ds.md); run each `.3dsx` from the Homebrew Menu or with `3dslink` ([testing on hardware](../engineering/testing-on-hardware.md#a-fast-editrun-loop)).
- Insert an SD card (results are written to it). Close other homebrew, detach the debugger and disable game plugins while measuring.

## Verification status

| Benchmark | Builds | Emulator | Old 3DS | New 3DS |
| --------- | ------ | -------- | ------- | ------- |
| [`membench`](#membench) | yes (r68) | not run | not run | not run |
| [`flushbench`](#flushbench) | yes (r68) | not run | not run | not run |
| [`drawbench`](#drawbench) | yes (r68) | not run | not run | not run |
| [`icachebench`](#icachebench) | yes (r68; also compiled directly with `-Wall -Wextra`) | not run | not run | not run |

Emulator runs only show that a program works; never record emulator timings as results.

## Building

```sh
cd ctr/benchmarks/membench && make          # or flushbench, drawbench, icachebench
# or, from the repository root, build all and fail on warnings:
bash tools/build_benchmarks.sh
```

Each project is a copy of the `3ds-examples` application template Makefile (public domain), with `-lcitro3d` added where needed; `common/bench.h` is shared by all four.

## Output format and metadata

Every benchmark writes the same lines to the console (stdout) and to `sdmc:/coffeepot-<program>.csv`; the file is the record to copy results from (the console scrolls). Shared code: `common/bench.h`. Line kinds:

```text
meta,<key>,<value>
result,<program>,<test>,<variant>,<bytes>,<runs>,<min_us>,<med_us>,<max_us>     (membench, flushbench)
columns,...  then  result,drawbench,<variant>,<quads>,<frames>,<record_ms>,<frameend_ms>,<begin_ms>,<gpu_ms>,<cmdbuf_pct>
note,<text>
note,perf,<variant>,<bytes>,<insns>,<ccnt>,<icache_miss>,<fetch_stall>      (icachebench; medians per run)
note,pct,<program>,<variant>,<frames>,<p50_us>,<p95_us>,<p99_us>,<worst_us>,<missed_slots>   (frame-time helper, see the profiling cookbook)
error,<where>,<detail>
```

`meta` lines carry: program, build date and compiler, model (`CFGU_GetSystemModel`), New 3DS flag (`APT_CheckNew3DS`), firmware and kernel version, requested speed-up setting, ticks per microsecond, free linear memory. Two things cannot be read at run time and are recorded as `not-readable`: the libctru and citro3d versions (the installed headers have no version macro; take them from [source versions](../engineering/source-versions.md)) and, **New 3DS:** the 804 MHz / extra-L2 state (libctru only has the setter `osSetSpeedupEnable`; build with `-DBENCH_SPEEDUP=1` or `=0` added to `CFLAGS` if you want the program to set it on a New 3DS, otherwise note the state yourself; on an Old 3DS the option does nothing). Failed API calls and a failed SD-card open produce `error,` lines; the program carries on where it can, and the final `note,done errors=N` line tells you whether to trust the run. A run with any error line is not a result.

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

**Results:** not run on hardware. One row per console and variant (min / med / max in microseconds):

| Console | Variant | Test | Size | Min | Med | Max |
| ------- | ------- | ---- | ---- | --- | --- | --- |
| | | | | | | |

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

**Results:** not run on hardware.

| Console | Test | State | Size | Min | Med | Max |
| ------- | ---- | ----- | ---- | --- | --- | --- |
| | | | | | | |

## drawbench

**Question:** what does a citro3d draw call cost on the CPU, and how much do common state changes add?

**Method:** draws N 4×4-pixel textured quads per frame (default 1000; Up/Down doubles or halves N between 125 and 4096) in six variants, 120 frames each (first 4 discarded), averaging five separately measured quantities:

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

**What it settles:** the draw-call cost table on [GPU rendering](../engineering/gpu-rendering.md#what-a-draw-call-costs), currently labelled with no measured numbers, and the sprite-batching trade-offs on [2D rendering](../engineering/rendering-2d.md#sprites).

**Results:** not run on hardware.

| Console | Quads | Variant | record_ms | frameend_ms | begin_ms | gpu_ms | cmdbuf_pct |
| ------- | ----- | ------- | --------- | ----------- | -------- | ------ | ---------- |
| | | | | | | | |

## icachebench

**Question:** how does the cost per instruction change as the amount of distinct code executed per pass grows past the Old 3DS's 16 KB instruction cache? (An independent test of the figures G-Diffuser reports, `[reported]` on [CPU optimisation](../engineering/cpu-optimization.md#the-g-diffuser-i-cache-probe); not a replication of its probe.)

**Method:** straight-line ARM-mode functions of 4, 8, 12, 16, 20, 24, 32, 48 and 64 KiB, each made only of `add rN, rN, #1` on four registers (no loads, no branches except the final `bx lr`, so the work is the same kind per instruction). Each run calls one function 64 times and times the loop with `svcGetSystemTick`; 15 runs per variant after one untimed warm-up call. The `loop_256b` control executes the same 16,384 instructions as `straight_64k` from a 256-byte loop body, so any time difference between the two is a footprint effect, not an instruction-count effect. If `svcControlPerformanceCounter` accepts `ENABLE` for the process, it also reads the cycle counter, `INST_CACHE_MISS` and `STALL_BY_LACK_OF_INST` around each run and prints `note,perf,...` lines; otherwise it prints `note,perf_unavailable` and only times. Footprint is controlled at build time; the program does not read the cache size from the hardware.

| Variant | Distinct code per pass | Instructions per pass |
| ------- | ---------------------- | --------------------- |
| `straight_4k` ... `straight_16k` | 4-16 KiB (fits in 16 KiB if that is the cache size) | 1,024-4,096 |
| `straight_20k`, `_24k`, `_32k`, `_48k`, `_64k` | 20-64 KiB | 5,120-16,384 |
| `loop_256b` | 256 bytes | 16,384 |

**Reading the output:** compare µs per instruction (`med_us` / (insns per pass x 64)) across footprints; the counters, if available, give cycles per instruction and I-cache misses per instruction directly. Expect, if the 16 KB figure is right, a step between 16 and 24 KiB `[hypothesis]`; the `loop_256b` row is the no-miss reference. **New 3DS:** run it separately (and with `-DBENCH_SPEEDUP=0` and `=1`); the L2 cache may change the shape, and no New 3DS statement is made here. The straight-line code is not typical of game code (no data accesses, perfectly predicted), so this is a probe of fetch cost only, not a prediction for any program.

**Verification status:** builds (devkitARM r68, no warnings with `-Wall -Wextra`; the `make` build was run with `DEVKITPRO=C:/devkitPro`); not run on hardware or an emulator.

**Results:** not run on hardware.

| Console | Variant | Footprint | Min | Med | Max | CPI (counters) | I-cache misses |
| ------- | ------- | --------- | --- | --- | --- | -------------- | -------------- |
| | | | | | | | |

## Planned experiments

Not written, not run; listed so that the question is recorded, not because any result exists. Each would need its own project, README section and `bench.h` output.

| Family | Question | Notes |
| ------ | -------- | ----- |
| GPU workload | Draw overhead, vertex rate, fill rate, texture bandwidth, render-target changes and synchronisation, separately | Would use the GSP performance log ([performance](../engineering/performance.md#per-operation-gpu-times-the-gsp-performance-log)) to separate P3D, PPF and PSC time |
| Scheduling | Frame time with a worker on core 1 at several `APT_SetAppCpuTimeLimit` percentages, versus no worker | Protocol in [threading](../engineering/threading.md#proving-a-worker-improves-frame-time) |
| Real scene | A fixed 2D tilemap scene (for example the [2D tile renderer](../engineering/2d-tile-renderer.md) variants) with frame-time percentiles and missed frames | Needs the frame-time helper from the [profiling cookbook](../engineering/performance.md#frame-time-distribution-and-missed-frames) |
| Indirect dispatch | Per-element function-pointer dispatch versus grouping by operation type | Companion to `icachebench`; would use the same counters |

## Recording results

Add results to this README (one table per benchmark, one row per console and variant) and to the page whose claim they settle, labelled `[measured]` with:

```text
Console:    e.g. Old 3DS XL, system 11.17.0-50U, Luma3DS v13.5
Build:      commit hash of this repository, devkitARM/libctru versions
Variant:    default / speed-up on / core-1 share 80% / N quads
Output:     the coffeepot-<program>.csv file from the SD card (a photo of the screen is a fallback)
```

Run each benchmark at least three times after a fresh boot and report the spread; a single run is not a result.

## Sources

- [3ds-examples](https://github.com/devkitPro/3ds-examples) — application template Makefile.
- [citro3d](https://github.com/devkitPro/citro3d) — `source/renderqueue.c` (frame timing, the linear-heap flush), `source/base.c` (state updates per draw).
- [libctru](https://github.com/devkitPro/libctru) — `svc.h`, `services/gspgpu.h`, `os.h`, `allocator/linear.h`.

## tilerenderer (in the engineering pages)

The scripted mode of the [2D tile renderer](../engineering/2d-tile-renderer.md#experiments) writes `sdmc:/coffeepot-tilerenderer.csv` in the same `meta`/`result`/`note`/`error` layout as above. Differences: the file is opened for append; `result,tilerenderer,<phase>,<variant>,<visible_tiles>,<frames>,<min_us>,<med_us>,<max_us>` has one line per phase (period, update, build, record, framebegin, frameend, c3d_proc, c3d_gpu); a `summary,tilerenderer,<variant>,<key>,<value>` line kind carries means, p99, missed frames, rebuilds, dropped objects, command-buffer peak and end-of-run memory. Status: builds; not run on hardware or an emulator. Experiments E1 to E6 are specified on the tile renderer page.
