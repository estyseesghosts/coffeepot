# CTR benchmark suite

Small, self-contained 3DS programs that measure costs the wiki's engineering pages depend on: memory and cache behaviour, cache-flush cost, and citro3d draw-call overhead. Each one builds from source with the standard devkitPro toolchain and prints its results on the console's screen. They exist so that performance claims on this wiki can become `[measured]` instead of `[hypothesis]` ([evidence labels](../../AGENTS.md#evidence-labels)).

**Status: the programs build; none has been run on hardware yet. No results are recorded.** The tables under each benchmark show what to record.

## Before you start

- Build environment: devkitPro with the `3ds-dev` group. Verified to build without warnings with devkitARM r68, libctru 2.7.1, citro3d 1.7.1 and picasso 2.7.2 (the versions on [source versions](../engineering/source-versions.md)), using `tools/build_benchmarks.sh`, which fails on any warning.
- A console with [Luma3DS](../homebrew/luma3ds.md); run each `.3dsx` from the Homebrew Menu or with `3dslink` ([testing on hardware](../engineering/testing-on-hardware.md#a-fast-editrun-loop)).
- Close other homebrew, detach the debugger and disable game plugins while measuring.

## Verification status

| Benchmark | Builds | Emulator | Old 3DS | New 3DS |
| --------- | ------ | -------- | ------- | ------- |
| [`membench`](#membench) | yes (r68) | not run | not run | not run |
| [`flushbench`](#flushbench) | yes (r68) | not run | not run | not run |
| [`drawbench`](#drawbench) | yes (r68) | not run | not run | not run |

Emulator runs only show that a program works; never record emulator timings as results.

## Building

```sh
cd ctr/benchmarks/membench && make          # or flushbench, drawbench
# or, from the repository root, build all and fail on warnings:
bash tools/build_benchmarks.sh
```

Each project is a copy of the `3ds-examples` application template Makefile (public domain), with `-lcitro3d` added where needed.

## membench

**Question:** how fast can the CPU write to the ordinary heap versus the linear heap, and what does a cache flush cost for a given range?

**Method:** for 16 KiB, 256 KiB and 1 MiB buffers (inside and outside the 16 KB L1 data cache), 31 runs each of: `memset` into heap memory, `memset` into linear memory, `memcpy` heap→linear, `svcFlushProcessDataCache` of a dirtied range, `GSPGPU_FlushDataCache` of a dirtied range; then 31 flushes of the whole linear heap (what `C3D_FrameEnd(0)` does). Timing with `svcGetSystemTick` (268,111,856 Hz); min / median / max in microseconds.

**Variants to run:** default; with `osSetSpeedupEnable(true)` (New 3DS); with `APT_SetAppCpuTimeLimit(80)` set before the run.

**Record:** model, system version, Luma3DS version, variant, and the printed table.

## flushbench

**Question:** what does flushing 1, 4, 16 or 32 MiB of linear memory cost; does it depend on how much of the range is dirty; how do the GSP service flush, the kernel flush and the kernel clean-only (`svcStoreProcessDataCache`) compare; and what does `C3D_FrameEnd(0)` cost compared with `C3D_FrameEnd(GX_CMDLIST_FLUSH)`?

**Method:** allocates the largest power-of-two buffer up to 32 MiB that fits in the linear heap. For each size and each of the three operations: 15 runs with the range dirty (one byte written per 32-byte line beforehand) and 15 with it clean (flushed beforehand). Then 15 frames that only clear the top screen, timing `C3D_FrameEnd` with flags `0` and with `GX_CMDLIST_FLUSH`. Results in microseconds on the bottom screen.

**What it settles:** the open question on [memory and cache](../engineering/memory-and-cache.md#the-c3d_frameend-flush) — whether targeted flushing is worth the bookkeeping. It measures the flush in isolation; the effect on a real scene needs a scene benchmark.

## drawbench

**Question:** what does a citro3d draw call cost on the CPU, and how much do common state changes add?

**Method:** draws N 4×4-pixel textured quads per frame (default 1000; Up/Down doubles or halves N between 125 and 4096) in six variants, 120 frames each, averaging `C3D_GetProcessingTime()` (CPU time to record the frame) and `C3D_GetDrawingTime()` (GPU time of the last completed frame, including the display transfer):

| Variant | What changes per quad |
| ------- | --------------------- |
| `batched` | Nothing: one `C3D_DrawArrays` for all quads |
| `per-quad` | One `C3D_DrawArrays` per quad |
| `+uniform` | Plus a uniform update before each draw |
| `+texbind` | Plus `C3D_TexBind` of the *same* texture (citro3d marks it dirty anyway) |
| `+texenv` | Plus alternating between two combiner configurations |
| `+texswap` | Plus alternating between two textures |

The last column printed, `us/quad`, is the extra CPU time per quad relative to `batched`. The shader (`source/vshader.v.pica`) transforms position by a projection matrix plus an offset uniform.

**What it settles:** the draw-call cost table on [GPU rendering](../engineering/gpu-rendering.md#what-a-draw-call-costs), currently labelled with no measured numbers, and the sprite-batching trade-offs on [2D rendering](../engineering/rendering-2d.md#sprites).

## Recording results

Add results to this README (one table per benchmark, one row per console and variant) and to the page whose claim they settle, labelled `[measured]` with:

```text
Console:    e.g. Old 3DS XL, system 11.17.0-50U, Luma3DS v13.5
Build:      commit hash of this repository, devkitARM/libctru versions
Variant:    default / speed-up on / core-1 share 80% / N quads
Output:     the table as printed (a photo of the screen is fine as raw data)
```

Run each benchmark at least three times after a fresh boot and report the spread; a single run is not a result.

## Sources

- [3ds-examples](https://github.com/devkitPro/3ds-examples) — application template Makefile.
- [citro3d](https://github.com/devkitPro/citro3d) — `source/renderqueue.c` (frame timing, the linear-heap flush), `source/base.c` (state updates per draw).
- [libctru](https://github.com/devkitPro/libctru) — `svc.h`, `services/gspgpu.h`, `os.h`, `allocator/linear.h`.
