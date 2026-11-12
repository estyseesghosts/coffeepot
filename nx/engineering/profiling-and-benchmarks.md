# Measuring a Switch program: timing, GPU counters and benchmarks

This page is for a developer who has a slow frame and needs to know why. The short answer: a frame time is a sum of things that can overlap (CPU work, GPU work, waits, presentation), so measure each separately, take GPU time from the GPU's own timestamps, never from how long the CPU waited, record enough about the setup that someone else can repeat the run, and prove the program actually drew before trusting a speed. For how other projects measured, see [performance and porting](performance-and-porting.md).

## Before you start

- Toolchain: devkitA64 r30-1, `libnx` 4.12.0-1, `deko3d` 0.5.0-1 ([source versions](source-versions.md)).
- Verification: the instrumentation described here is in `nx/engineering/examples/deko-renderer` (builds; **not run**, see [the renderer page](deko3d-renderer.md)) and `nx/engineering/examples/first-game` (builds; its self-test ran in Eden, which gives no timings). **No benchmark in this page's table has been written or run.** There are no Switch results on this wiki yet; every figure quoted elsewhere is `[reported]`.
- Emulator timings are never results. Eden does not model the console's clocks, caches or GPU.

## Say what you are timing

| Metric | What it is | How to take it |
| ------ | ---------- | -------------- |
| Simulation | CPU time updating game state | `armGetSystemTick` around the code |
| Render preparation | CPU time building draw data and visibility lists | same |
| Command recording | CPU time inside `dkCmdBuf*` calls | same |
| Submission | CPU time in `dkQueueSubmitCommands`, flush, present | same |
| Fence wait | CPU time blocked in `dkFenceWait` | same; this is **waiting**, not work |
| GPU execution | Time between two GPU timestamps | `dkCmdBufReportCounter`, see below |
| Presented-frame interval | Time between successive presents | tick at the present call |
| Audio, storage, network | Reported separately | per operation |

These overlap, so they do not sum to the frame time. A CPU wait on a fence measures how long the CPU waited, not how long the GPU worked: a GPU that finished early shows a zero wait. The `first-game` loop prints its own phases the same way (`begin`, `draw`, `present`, `loop`), each around the call that can block.

`armGetSystemTick()` reads the system counter; convert with `armTicksToNs` (the frequency is `armGetSystemTickFreq()`) `[documented]` (`libnx` `arm/counter.h`).

## GPU time from counter reports

```c
dkCmdBufReportCounter(cb, DkCounter_TimestampPipelineTop, rep);        // before the work: 16 bytes
/* ... draws ... */
dkCmdBufReportCounter(cb, DkCounter_Timestamp, rep + 0x20);            // after the work
/* queue: dkQueueSignalFence(queue, &fence, true);  CPU: dkFenceWait(&fence, -1);  then read */
const uint64_t* r = (const uint64_t*)cpuAddrOf(rep);                   // r[1] and r[5] are the two timestamps
double us = (double)dkTimestampToNs(r[5] - r[1]) / 1000.0;
```

- A report is two 64-bit values: the counter, then the timestamp; `DkCounter_ZcullStats` uses four 32-bit values instead `[documented]` (`Primer.md` "Counters").
- Counter types include timestamps, samples passed, input vertices and primitives, vertex and fragment shader invocations and Zcull statistics (`deko3d.h` `DkCounter_*`).
- Read the report only after the slot's fence has been waited on, and signal that fence with `flush = true` so the CPU sees it ([synchronization](gpu-memory-and-synchronization.md#what-submit-flush-and-a-fence-mean)).
- All counter operations flush the tiled cache `[documented]`. Measuring changes the workload: compare an instrumented run with an uninstrumented one, and keep benchmark builds separate from diagnostic builds.
- A timestamp pair measures one stretch of the command stream. It is not a GPU utilisation figure, and concurrent GPU work is not separately visible.
- NFSMW multiplies raw NVK timestamps by 1.627 on the Switch `[reported]` (`nfsmw-nx` `docs/measuring.md`). That is specific to its NVK driver stack; do not apply it to `deko3d`. Check your own timestamp conversion against an independent clock before trusting a duration `[hypothesis]`.

## Experiments to run (not yet written)

Each is a small program that changes one variable and reports a distribution. Writing them is open work; until then these are specifications, and the answer to each is unknown.

| Experiment | Hold fixed | Vary | Establishes |
| ---------- | ---------- | ---- | ----------- |
| Draw scaling | Triangles and covered pixels | Draws: 10, 100, 1,000, 10,000 | CPU record/submit cost per draw; then repeat with a state change per draw |
| Fill scaling | Geometry, shader | Resolution 320×180 to 1920×1080; trivial vs texture-heavy shader | Pixel cost (not bandwidth in isolation) |
| Texture layout | Scene | Format, mip chain, block-linear vs pitch-linear; use `dkImageLayoutGetSize` | Memory and sampling trade-offs |
| Frames in flight | Scene | 1 to 4 slots | Throughput, CPU stalls, input-to-display latency (report both; never pick from fps alone) |
| Counters vs fences | Scene | GPU timestamps vs fence wait time | How far wait time misleads |
| CPU threads | Work | 1 to 3 workers, priority, core mask | Parallel gain vs scheduling cost |
| Clock profiles | Scene | CPU, GPU, memory clocks | Which resource the workload follows |
| Barrier cost | Verified-correct scene | Barrier mode and invalidates | Cost, only after correctness is established |

Existing `[reported]` guidance for these questions is on [performance and porting](performance-and-porting.md) and [rendering](rendering.md); none transfers to your scene.

## Make a run repeatable

Write a machine-readable manifest next to the samples. Fields the sources do not provide stay `null` and are marked unavailable, never guessed.

```json
{
  "test": "draw-submission", "revision": "<git commit>", "nro_sha256": "<sha256>",
  "console": "<model and SoC revision>", "firmware": "<version>", "launch_mode": "application | library applet",
  "cpu_mhz": null, "gpu_mhz": null, "memory_mhz": null,
  "resolution": [1280, 720], "draws_per_frame": 1000,
  "warmup_frames": 600, "sample_frames": 3600,
  "instrumentation": "gpu-timestamps", "correctness": "pass",
  "samples_file": "samples.csv"
}
```

- Keep raw samples or a stated distribution (p50, p95, p99, worst, hitch count), not only a mean.
- Pin clocks and record them; a reported operating mode is not a measured frequency. Record temperature and power where accessible. See [stock clocks](performance-and-porting.md#stock-clocks-a-port-reported-table).
- Compare a baseline and a candidate by alternating runs (several boots each, fixed scene), so drift hits both; include a no-change control to measure the noise.
- Launch mode matters: the Album applet and title override differ in memory ([memory](memory-management.md)).

## Prove the work happened

A renderer that draws nothing is fast. USF4's first renderer comparison "won" at 60 fps with 0 draws because its shader package was missing `[reported]` (`usf4-nx` `docs/deko3d-renderer-usf4.md`). Fail the run when a check fails, whatever the speed:

- a non-zero draw count, and the expected shader and resource configuration;
- GPU counters that match the expected work (the renderer example compares `DkCounter_FragmentShaderInvocations` with the pixels it read back);
- an image comparison on a fixed frame, or a scene-specific invariant.

## Treat an optimisation as an experiment

For each change write down: the suspected bottleneck, the measurement that would expose it, the change, the expected direction, the same-workload comparison, and the decision (keep, revise, revert). Batching, threading, NEON, compression and PGO are not universal improvements; Zcull and render-target compression cannot be credited when the workload cannot exercise them.

## Technical details

- `first-game` timing phases: `begin` = `framebufferBegin` (dequeues a buffer; waits if the display holds all of them), `draw` = update and pixel writes, `present` = `framebufferEnd` (in linear mode it also copies and converts the image, then queues it) `[documented]` (`libnx` `display/framebuffer.h`), `loop` = interval between iteration starts.
- Image footprint: 1920×1080 RGBA8 is 8,294,400 bytes before layout overhead; the layout call gives the real size. A footprint times the frame rate is not memory traffic.
- Desktop GM204 figures (throughput, caches) are not Tegra X1 figures; do not transfer them `[hypothesis]`.

## Pitfalls

- **Treating a fence wait as GPU time.** Use timestamps.
- **Always-on counters in a benchmark build.** The measured pass shrinks when they are removed.
- **A single mean.** Hitches live in the tail.
- **Overclocked or emulator runs as acceptance.** Neither passes.

## Open questions

- Every experiment in the table: none has been run on a console.
- How accurately do `deko3d` timestamps convert to wall time? Needs a comparison against `armGetSystemTick` over a long GPU workload.
- How much do counter reports perturb a frame? Needs instrumented and uninstrumented runs.

## Sources

- [deko3d](https://github.com/devkitPro/deko3d/blob/373bfd43d8b2d805fa2eb7d406dbb823032df958/Primer.md) at `373bfd43d8b2d805fa2eb7d406dbb823032df958` — `Primer.md` "Counters", `include/deko3d.h` (`DkCounter_*`, `dkTimestampToNs`).
- [libnx](https://github.com/switchbrew/libnx) at `b156b4774fb2df7b93402169509266da7547fc4c` — `nx/include/switch/arm/counter.h`, `display/framebuffer.h`.
- [nfsmw-nx](https://github.com/StevensND/nfsmw-nx) at `20f8dc1` — `docs/measuring.md` `[reported]`; [SwitchWakerHD](https://github.com/centollOS/SwitchWakerHD) at `9aec4e3` — `docs/perf-results.md` `[reported]`; [usf4-nx](https://github.com/Thorhax/usf4-nx) at `30ffae0` `[reported]`.
- `nx/engineering/examples/deko-renderer`, `nx/engineering/examples/first-game` — the instrumentation described.
