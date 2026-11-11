# Tile renderer experiment

This project compares four ways to submit a scrolling tilemap on the 3DS. The [guide](../../engineering/PICA200/2d-tile-renderer.md) explains the workloads, compile-time switches, scripted runs, CSV fields and proposed comparisons.

## Method

Use the same console, scene, build settings and scripted path for each strategy. Build one variant at a time, clearing build outputs when flags change. For example, from the repository root:

```sh
make -C ctr/benchmarks/tile-renderer clean
make -C ctr/benchmarks/tile-renderer EXTRA_CFLAGS="-DTILE_STRATEGY=1 -DBENCH_AUTO=1"
```

Copy the resulting `.3dsx` to the console and follow the guide's experiment and output-recording instructions. Record the console model, firmware, library versions, build flags, run conditions and per-phase CSV values. Compare visual output before interpreting timings.

## Results and verification

Strategies 0-3 with `BENCH_AUTO=1` have run on an Old 3DS at default settings (two sessions, no `error,` lines; results in the [guide](../../engineering/PICA200/2d-tile-renderer.md#first-hardware-runs-old-3ds) and the [suite README](../README.md#hardware-runs)). No emulator run and no other configuration is recorded, and the pictures were not compared. The current drop counter covers only selected tile calls, the command-buffer usage sample precedes the final citro2d flush, and automatic mode still draws debug text. Do not treat a zero drop count or the reported usage as proof of a complete, valid frame; see the [guide](../../engineering/PICA200/2d-tile-renderer.md#per-phase-timing). The default project and strategies 1–3 with `BENCH_AUTO=1` build with devkitARM r68, libctru 2.7.1 and citro2d/citro3d 1.7.0/1.7.1; see the [guide's verification table](../../engineering/PICA200/2d-tile-renderer.md#verification-status) for the prior direct-compiler configuration checks. These builds do not establish correct rendering or performance.

## Technical details

`source/main.c` generates its own art; `source/vshader.v.pica` is assembled by picasso. Compile-time options and experiment definitions are documented in the guide. No user game data is required.

## Sources

- [Tile renderer guide](../../engineering/PICA200/2d-tile-renderer.md) — design, experiments, output and verification limits.
- [devkitPro 3DS examples](https://github.com/devkitPro/3ds-examples) — build-system conventions and graphics examples.
