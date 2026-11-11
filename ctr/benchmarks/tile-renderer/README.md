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

The 20-variant suite has run once on an Old 3DS and once on a New 2DS XL at 268 MHz, with no `error,` lines; results in the [guide](../../engineering/PICA200/2d-tile-renderer.md#suite-results-old-3ds-and-new-3ds) and the [suite README](../README.md#suite-runs-old-3ds-and-new-3ds). No other configuration has run on hardware. In an emulator, 17 variants draw identical pictures at one frame ([picture equivalence](../README.md#picture-equivalence-emulator)); a hardware picture comparison is owed. Since those hardware runs, the drop counter covers every citro2d draw that reports failure and the command-buffer sample is taken after the final citro2d flush; the recorded hardware CSVs predate that. Automatic mode still draws debug text; see the [guide](../../engineering/PICA200/2d-tile-renderer.md#per-phase-timing). `BENCH_DUMP_FRAME=N` (with `BENCH_AUTO=1`) freezes the simulation at frame N and dumps both screens as raw RGB8 for comparison. The default project and strategies 1–3 with `BENCH_AUTO=1` build with devkitARM r68, libctru 2.7.1 and citro2d/citro3d 1.7.0/1.7.1; see the [guide's verification table](../../engineering/PICA200/2d-tile-renderer.md#verification-status) for the prior direct-compiler configuration checks. These builds do not establish correct rendering or performance.

## Technical details

`source/main.c` generates its own art; `source/vshader.v.pica` is assembled by picasso. Compile-time options and experiment definitions are documented in the guide. No user game data is required.

## Sources

- [Tile renderer guide](../../engineering/PICA200/2d-tile-renderer.md) — design, experiments, output and verification limits.
- [devkitPro 3DS examples](https://github.com/devkitPro/3ds-examples) — build-system conventions and graphics examples.
