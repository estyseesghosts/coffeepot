# Experimental method and failure containment

## What an observation proves

**Functional test:** identical legal GPU inputs under controlled initial state produce a specific byte sequence. Two image hashes matching does not prove identical mathematical pipelines; a full captured image at each parameter makes a useful regression case.

**Timing test:** measured queue duration changes when a variable changes. That establishes an association, not a precise microarchitectural mechanism. Discriminate proposed mechanisms by varying a second orthogonal variable. Confidence is stronger if the effect is stable under ABBA interleaving and cold-start repetition.

**Architecture inference:** a family of observations excludes alternative hypotheses. Report the hypotheses that remain possible, not a guessed cache size or instruction latency.

## The six required test controls

1. **Known-good baseline.** Always run a control (clear framebuffer, constant flat triangle, known shader). Verify the control before admitting later results.
2. **Exactly one independent variable.** E.g. address stride while fixing texture size and total texel requests. If a parameter changes cache footprint and total writes together, it cannot isolate the cache.
3. **Reference/no-op pair.** E.g. same draw calls with all triangles fully clipped; or no texture binding with otherwise identical rendering.
4. **A/B/B/A randomized blocks.** GPU heat, external scheduling, and CPU-side updates are potential confounders. Log order and model type.
5. **Multiple GPU workload scales.** Fit `t(N) = a + bN` with at least 6 values of N. Fixed costs are intercepts; issue/processing cost is slope **only when the curve is linear**.
6. **Independent signal.** Confirm a bottleneck from two different observables (time plus hotspot counters; changed pixels plus depth-test count; transfer time plus destination hash).

## Timing separation

Measure separately:

- Host prep and command-generation time (CPU timestamp around preparation only).
- Submit-to-complete queue duration (`C3D_GetDrawingTime`, or measured completion events), explicitly including P3D/PPF work as appropriate.
- Per-engine GSP `GetPerfLog` durations/deltas, after enabling its mode and discarding first-frame invalid samples.
- Hotspot profiler counter distribution in a *separate*, calibrated run. Counters are **not** elapsed cycles per stage.

Never include SD writes, framebuffer downloads, or console printing in the timed region. The phase-1 `perf` path measures queue duration after the preceding frame has completed; this is a coarse first test. Add the GSP perflog backend next.

## Accurate readback and state

- Read *the render target*, never the current LCD screen. LCD scanout orientation is not the same thing as GPU raster layout.
- Choose a supported offscreen size aligned to display-transfer requirements (256², RGBA8). Verify the GX flags and byte layout with color sentinels.
- Draw, flush PICA framebuffer cache, wait for P3D completion, issue a tiled-to-linear display transfer, wait for PPF, invalidate CPU cache, then read. Citro3D handles several of these, but verify with self-tests before treating the path as exact.
- At each test boundary explicitly set culling, depth, alpha test, blending, TEV state, viewport, shader, buffers, and fixed attributes. On changing framebuffer or shaders, respect flush/invalidate ordering.
- For shader *floating-point* research, an RGBA8 image is not a bit-exact float24 oracle. The current harness measures quantized downstream effects. Build a dedicated encoding/calibration mechanism before claiming exact intermediate values.
- Some library behavior differs across versions. Record toolchain and library commits, preserve disassembly and command lists, and replicate interesting findings with the smallest possible raw PICA command list.

## Thermal, VBlank and execution-state controls

- Don't benchmark at 60 FPS and infer a GPU limit from frame time. An LCD swap or VBlank wait can dominate.
- Run an empty draw and a full GPU busy saturation case; compare their durations. Measure CPU overhead separately.
- Repeat cold boot and warm GPU runs. Alternate test order.
- Do not compare Old and New 3DS without logging system mode and CPU clocks; this program targets the *Old* model.
- Saturating CPU or SD IO during measurements must be deliberate and recorded as an experimental variable.
- The PICA200 runs with concurrent GPU engines. Separate P3D, PPF, PSC, and display scanout effects when possible.

## Artifacts and reporting

Every test case should produce:

```json
{
  "schema": "pica-lab-result/1",
  "case": "raster.subpixel-edge",
  "parameter": {"offset_num": 5, "offset_den": 64},
  "environment": {"console_model": "Old3DS", "firmware": "fill manually", "build_sha256": "fill automatically in future"},
  "control": "verified flat clear and duplicate shape",
  "capture": {"format": "RGBA8", "width": 256, "height": 256, "path": "raster_0005.rgba", "hash": "from log"},
  "measurement": {"queue_ms": null, "hotspot_counters": null},
  "inference": {"supports": [], "rejects": [], "unresolved": []}
}
```

Keep raw data separate from interpretation. Publish the raw inputs, command-list bytes, shader binary, original outputs, source hash, and scripts. A prose claim without an executable reproducer should not be considered established.

## Acceptance levels

- **G0:** host parser passes synthetic tests.
- **G1:** builds, launches, and exits safely on Old 3DS.
- **G2:** constant-image clear and duplicate-render outputs match across cold boots.
- **G3:** parameterized accuracy cases show deterministic outcomes under reversed order.
- **G4:** timing effects exceed noise and survive ABBA/randomization and scale changes.
- **G5:** independent raw-command reproducer yields the same result without dependence on Citro3D implementation details.
- **G6:** an emulator changes behavior and passes a hardware-reference corpus without regressing other tests.

No higher-grade conclusion may cite a result that failed G2.

## Sources

- Project references and prior art: [sources and knowns](04-sources-and-knowns.md).
- Measured results so far: [state log](../STATE-LOG-2026-11-13.md).
