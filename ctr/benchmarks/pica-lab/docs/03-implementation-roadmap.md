# Engineering roadmap: from a useful starter harness to a hardware reference corpus

The starter is intentionally small: Citro3D legal command lists, offscreen RGBA8 readback, flat-color geometry, exact raw-image dumps, CSV records, and a Python verifier. It is not yet sufficient to establish GPU cache sizes, shader cycles or bit-exact internal floating-point behavior.

## Architecture to build next

```text
harness/
  platform/    clock, console model, IRQ events, SD writer, GPU service permission probe
  case/        declarative case ID, parameter lattice, dependency and risk metadata
  backend/     citro3d (known-good), raw_pica (exact command list), emulator (future)
  capture/     synchronous target readback, dump raw, hash, compare
  timing/      CPU clock, C3D queue time, GSP perf log, gated hotspot hardware sampler
  shaders/     generated assembly variants, archived binaries and disassemblies
  suites/      geometry, shader, tev, texture, memory, GSP engine, GAS
  reference/   CPU models: interpolation, TEV, blending, float24 and depth
  replay/      serialized command stream, initial state and buffer images
```

Runner contract (C):

```c
typedef struct {
    const char *id;
    unsigned risk; /* 0: safe Citro3D, 1: low-level bounded, 2: manual-only */
    bool (*prepare)(void *state);
    bool (*baseline)(void *state);
    bool (*run_one)(void *state, unsigned parameter, unsigned repetition);
    bool (*capture)(void *state, unsigned parameter);
    void (*destroy)(void *state);
} Experiment;
```

The driver always performs `prepare -> baseline -> (run_one -> capture)xN -> destroy`, and a failed baseline aborts. Allow an explicit **capture-free** performance mode and an independent accuracy mode; otherwise SD IO and synchronization dominate timing. Allocate all geometry, texture, and render buffers before the timed region.

## Milestone A: make phase-1 trustworthy

**Task A1: successful Old 3DS build and run.** Build with current devkitPro 3DS kit, resolve compiler/API drift, record exact tool versions. Console must boot, exit, and save `selftest` without hanging. The host parser's self-test is **not** sufficient.

**Task A2: review readback path.** Compare `C3D_SyncDisplayTransfer()` output against a known 4-color checkerboard and an independent raw display-transfer implementation. Probe linear scanline orientation, rotation, channel order, alpha channel, and row stride. Confirm `C3D_FrameEnd()` flushes the framebuffer sufficiently in this context; add explicit legal flush command if required. Add a second capture path from a render-target texture to avoid one potentially wrong transfer mechanism agreeing with itself.

**Task A3: repeatability.** Repeat each accuracy test ten times, across at least three fresh launches, including forward and reverse parameter order. Compare exact raw data and record firmware/build/console. Define noise detection and fail closed for nondeterminism.

**Task A4: result schema.** Replace one overwritten CSV with run-specific timestamp/session ID directories, `manifest.json`, per-case metadata JSON, per-case raw capture, shader binary SHA256, frame-buffer format and dimensions, and errors. Implement SD write error handling and free-space preflight. Stream results only after GPU is finished. No giant in-memory accumulation on Old 3DS.

**Task A5: memory lifetimes.** Move test geometry updates outside active/in-flight GPU work; generate entire buffer before frame submission and flush exactly once. Reserve separate buffers for each concurrent DMA test, each with guards/canaries validated after completion.

Acceptance: three clean boots with identical self-test hashes, unambiguous transfer format, complete metadata, no GPU hangs; no interpretation before then.

## Milestone B: useful instrumentation

**B1: CPU timestamps.** Capture start/end tick with a single chosen monotonic service, distinguish CPU prep, queue submit, queue completion, and SD write. Report frequency and quantization. Avoid comparing wall-clock against GPU cycles without calibration.

**B2: modern GSP performance log.** Add an opt-in mode that calls `GSPGPU_SetPerfLogMode(true)` *when no GPU work is in progress*, and then `GSPGPU_GetPerfLog()`. Inspect the installed libctru `GSPGPU_PerfLog` struct. Record each engine's `lastDurationUs`, `totalDurationUs` delta, completed work count, and error code. Discard the first sample and implement overflow-safe deltas as instructed by the API docs. Don't mislabel GSP work durations as stage occupancy. Compare no-work controls and 1 P3D / 1 PPF / 1 PSC cases.

**B3: hotspot profiler feasibility gate.** First issue **read-only** `GSPGPU_ReadHWRegs` on known-safe identity and busy registers (starter R button); verify service error codes, permissions, hardware model, and register offset. Only after this, isolate profiler writes in an opt-in module with one legal serialized sequence. Offsets are relative to `0x1EB00000` (`0x1EF00064` becomes `0x00400064`). Capture exactly 4 FIFO reads as described by 3dbrew; reading may have side effects. Verify eight counts sum to interval_count for fixed-length tests before interpreting labels. Use timeout/recovery instructions and never run concurrently with Citro3D unless ordering has been validated.

**B4: command capture.** Add a command-buffer interception layer at `GX_ProcessCommandList` (or build an explicit command list backend), recording bytes, register header decoding, buffer base addresses and address relocation. Hashing only the high-level API calls is not enough to reproduce a GPU discrepancy.

Acceptance: isolated P3D, PPF, PSC activity distinguishable; identical command streams under each repeated configuration; profiler sum property reproduced before profiling labels are published.

## Milestone C: turn images into exact oracles

**C1: CPU reference implementations** of raster edge coverage, framebuffer format conversions, depth test, blending and TEV arithmetic with explicit quantization modes. Design input cases where at least two reference models produce different outputs; if all models agree, the case has no discriminatory value.

**C2: pixel-exact differential runner.** Take `.rgba` from console and emulator on the same shader, test parameters and command sequence; produce `mismatch_count`, first mismatched coordinate, histogram by channel, coverage overlay, and minimal reproduction. Archive both images, not just the mismatch report. Include CI that reruns archived fixtures against CPU reference models.

**C3: shader arithmetic observation channel.** Build and validate a technique to encode meaningful components of float24 outputs into multiple RGBA8 passes or into a controlled binary search via vertex-position/depth outputs. Do not claim raw float24 exactness without proving it recovers known values, including inexact results. Initial task: compare two arithmetic models known to disagree while producing distinguishable *observed* image/depth outcomes.

**C4: shader generator.** Write a host Python generator for `.v.pica` known-good base shaders with precisely N independent/dependent operations, integer/address reads, and guarded branching. Preserve all shader binaries and emitted opcode disassembly. Static lint: output components written exactly once, no adjacent MOVA, stack bounds, instruction budget, no invalid registers. Compare each generated shader to a handwritten known-good 3DS example before automation.

Acceptance: at least 30 deliberately chosen *discriminating* cases, each stable across hardware boots and correctly reproduced by the chosen reference model.

## Milestone D: narrow architecture, not merely guess

**D1: stage timing tests.** Use S01–S04, vary both shader length and vertex count, then isolate negligible pixel coverage. Fit performance intercept/slope with confidence intervals and save residuals. Claim a latency or throughput ceiling only if competing memory/command setup explanations have been excluded.

**D2: textures/cache.** Implement tiled texture generator and test harness for 8–1024 pixel sizes, format, alignment, access-pattern permutation and hot/cold reuse. Group experiments so cache size, tag aliasing, texture filtering and VRAM bandwidth are independently varied; a single size threshold alone proves nothing.

**D3: bank placement/concurrency.** Use `vramAllocAt` or supported bank-directed API, verify real bank of each allocation; handle failed layouts as skipped rather than silently falling back. Run split buffers, concurrent P3D/PPF/PSC, real-time per-engine log, and strict barriers. Check independent output hashes after workload overlap.

**D4: geometry processor.** Begin with official devkitPro `geoshader` and `loop_subdivision` samples. Investigate valid EMIT/reuse/primitive restart semantics, then measure throughput only after output semantics have been established.

Acceptance: each asserted architectural fact has at least two independent, nontrivial observations and an explicitly rejected alternative explanation.

## Milestone E: difficult gaps, separately gated

- GAS: Citro3D's existing experimental `gas.c` is the baseline. Build a minimal functioning GAS scene first; then sweep accumulation, depth, light and LUT values individually.
- Procedural textures and lighting LUT: use Citro3D setup, dump exact register writes, construct discontinuity-focused data to reveal interpolation rules.
- Undocumented registers and opcodes: manual-only opt-in; whitelist addresses; never blindly scan write locations or run invalid shader encodings. Prioritize interpreting read-only changes and cross-referencing existing GPU implementations before writing anything.
- Negative tests of hazards: keep *outside* the general suite. An illegal stream that might freeze the GPU is not a safe default regression fixture.

## Integration with Azahar and independent emulator work

1. Record a minimal Citro3D + shader fixture and its exact emitted PICA command list.
2. Execute with hardware and with Azahar, using the *same* initial state and buffer inputs.
3. Compare final pixels; categorize mismatch: attribute/vertex transform, primitive setup, raster coverage, TEV, blend/depth, PPF, or synchronization.
4. Reduce one failing case by removing draws/registers while preserving failure.
5. Submit emulator regression fixtures with expected output hash, raw image, source, code blob, clear setup, and version metadata. Avoid timing assertions in ordinary emulator correctness CI.
6. For accuracy-focused emulator designs, consider creating a separate deterministic software rasterizer/float24 model independent of the emulator under test. Two renderers using identical buggy helper functions are not independent confirmation.

## Definition of complete

Completion is **not** 'thirty benchmark modes ran.' It is: (a) reference output fixtures on real Old 3DS, (b) measured limitations for each claim, (c) a searchable table of what remains unknown, (d) minimal commands to recreate interesting behaviors, (e) tests that reject at least one plausible model, and (f) practical developer guidance when the research reveals a stable optimization lever.

## Sources

- Project references and prior art: [sources and knowns](04-sources-and-knowns.md).
- Measured results so far: [state log](../STATE-LOG-2026-11-13.md).
