# PICA Lab: real-hardware PICA200 research

**Status:** v0.6 research harness. v0.5 has run on an Old 3DS (run-logs/run_012-014). The v0.6 additions (tie-probe windows C1-C6 in `raster_fine`, the redesigned `timing_iso` with a wall-clock cross-check and 0/1-triangle and repeated-draw variants) build cleanly but are **not yet run on hardware**. Host tools pass their self-test, which checks the tools only. See `STATE-LOG-*.md` for what has been measured and what was wrong.

This project is intended to produce **independent, falsifiable hardware measurements**, not benchmark scores detached from explanations. It starts with safe Citro3D-generated command lists and a 256×256 offscreen RGBA8 render target. Each completed test saves a CSV row and, for accuracy tests, the complete raw linear readback. More intrusive testing is specified but **not enabled**.

## Build

Requires devkitPro's 3DS development environment, including `devkitARM`, `libctru`, `citro3d`, `picasso`, and `3ds_rules`. Use devkitPro's standard installed shell or MSYS2 environment on Windows.

```sh
make -j4
# output: pica-lab.3dsx (when correctly built)
```

The Makefile is adapted from devkitPro's official 3DS application template. It has not been compiled here: address any version-specific toolchain errors against the upstream sample before running hardware investigations.

Copy `pica-lab.3dsx` to `sdmc:/3ds/pica-lab/pica-lab.3dsx` and launch it through Homebrew Launcher. The app creates `sdmc:/3ds/pica-lab/` itself.

**SELECT runs every family in order** (selftest, raster+edge, raster_fine, tev, blend, perf, perf2, timing_iso, probe) and stops on an error or a failed self-test. Single families run from the buttons below (all except A and R need A's self-test first).

Each launch creates a new `sdmc:/3ds/pica-lab/run_NNN/` (never overwrites) holding `run.log` (timestamped), `results.csv`, `manifest.txt` and `probe.txt`, plus **`run_NNN.zip`** beside it. The zip is uncompressed and holds all `.rgba` captures; the text files are added at clean exit (START). Copy the zip off the SD card. If the app crashes the zip may lack the text files and the last entries (`zip -FF` can recover it).

`results.csv` columns: `case_id,family,variant,parameter,crc32,nonclear_pixels,center_abgr_u32,gpu_queue_ms,p3d_us`. `crc32` is the CRC-32 of the 256 KB capture (same value as the zip entry) and is the image identity. v0.3 CSVs have an `fnv1a32` column instead; it collided on two different images and must not be used as an identity.

## Controls / initial implemented tests

| Button | Family | Current implementation | Signal |
|---|---|---|---|
| A | `selftest` | Clear, then two identical rectangles | Readback coherency + determinism gate |
| B | `raster`, `edge`, `edge_cov` | 1/64-pixel triangle translations, forward then backward; quad diagonals; additive coverage | Coverage transition map, watertight shared edges |
| D-pad down | `raster_fine` | 1/256-pixel offset sweeps (x and y, two geometries), 13-step windows around exact-model transitions, forward and reverse | Vertex quantization vs exact pixel-center arithmetic (`tools/rasterfine.py`) |
| X | `tev` | Constant multiplier on a second TEV stage | 8-bit output changes / stage rounding characterization |
| Y | `blend` | Two layers, source alpha 0/16..16/16 | Blend mapping, rounding and pixel reproducibility |
| L | `perf` | 9x9-px triangles (192-3072), draw-call splits, full-target quads; interleaved reps, `empty` floor; GSP P3D time in `p3d_us` | Per-triangle and per-pixel cost estimate |
| D-pad right | `perf2` | Triangle size as the variable: fixed-pixel and fixed-count series (3-64 px legs) and quads; `perf2_cov` measures pixels per triangle | Fit `p3d_us = c + a*tris + b*pixels` (`tools/perf2.py`) |
| D-pad left | `timing_iso` | Isolates the perf2 P3D anomaly: 1/16/64/256/512-triangle, pitch, and 4x/16x repeated-draw variants, back-to-back (`_blk`) vs after an empty frame (`_gap`); also logs `timing_wall` rows (svcGetSystemTick) | `tools/timing_iso.py`: P3D vs drawing time vs wall time, frame-multiple check |
| R | `probe` | Read-only GPU identity/status register attempts; log result codes | Service-access feasibility, not profiler enable |
| START | Exit | Flush outputs and exit | |

### Validation gates (mandatory before interpreting results)

1. The `selftest/clear` image contains only 0x000000FF pixels. If not, investigate format conversion and transfer orientation, initial clear semantics, and framebuffer flush ordering.
2. Both `selftest/rect` captures have exactly identical hashes and sensible nonclear coverage. If not, there is a coherency or state-control problem.
3. `edge` captures should be compared *pixel-for-pixel*, not solely by hash; gaps/differences can be legitimate clues only after gate 1–2 pass.
4. Repeat test families in separate clean sessions. Reverse order and compare: divergence between the two raster sweep directions invalidates a simple static coverage interpretation.
5. `perf` samples must be treated as **GPU queue time**. They include command processing and state setup and may not isolate rasterization. Inspect medians and spread before declaring a difference.
6. Confirm readback byte/channel order using contrasting solid-color probes before making channel-specific numeric claims. The host tool assumes 32-bit words packed `0xRRGGBBAA` (`[AA,BB,GG,RR]` in little-endian raw memory) as expected for the GX RGBA8 format.

### Host analysis

```sh
python tools/selftest.py
python tools/analyze.py summary run_NNN.zip      # or a run folder / results.csv
python tools/interrogate.py run_NNN.zip
python tools/rasterfine.py run_NNN.zip           # raster_fine vs exact model
python tools/perf2.py run_NNN.zip [--exclude a,b] # triangle/pixel cost fit
python tools/timing_iso.py run_NNN.zip           # P3D anomaly isolation (v0.5+ runs; wall column needs v0.6)
python tools/analyze.py ppm /path/to/raster_0003.rgba case.ppm
python tools/analyze.py diff raster_0005.rgba raster_0069.rgba --out difference.rgba
```

`ppm` produces a PPM image without adding third-party packages. Output bytes preserve the transfer's linear layout; use `--rotate` only for human-oriented inspection. A `.zip` is extracted to `run_NNN.unzipped/` beside it. The tool checks CSV crc32 values (skipped for old CSVs), marked-pixel counts and center pixel data against raw captures. **Different GPU results are not automatically failures**—they become research evidence once harness controls pass.

## Extending the harness

- `docs/01-method.md`: controls, isolation, execution, experimental design, logging.
- `docs/02-experiment-matrix.md`: test inventory, competing hypotheses, independent variables, predicted outcomes and disproof conditions.
- `docs/03-implementation-roadmap.md`: concrete engineering work to complete the larger suite (shader generators, low-level backend, hardware profiler, GAS, caching).
- `docs/04-sources-and-knowns.md`: known findings, false assumptions, primary references, risk register.
- `docs/05-result-triage.md`: what observations actually justify conclusions, evidence standards, suggested first-ten-session campaign.
- `specs/catalog.json`: machine-readable test catalog, each with independent/dependent variables and a null condition.
- `tools/analyze.py`: raw-image and CSV verifier.
- `tools/interrogate.py`: report forward/reverse raster disagreements, observed transitions, shared-edge pixel differences, color-result tables, and ABBA-block timing contrasts without claiming unsupported architectural conclusions.

A proper future test interface should have `init → calibrate → run one parameter → capture/measure → teardown`; never add direct register writes into the shared runner without an explicit safety review. See the roadmap.

## Strict limitations

- The initial `.3dsx` is **not prebuilt**, **not hardware-tested**, and **not advertised as a definitive timing profiler**.
- Hardware-float exact-value serialization, low-level register sweeps, hotspot sampling, shader instruction timing, actual texture-cache probing, and GAS mode are **specified but not implemented** in phase 1.
- The initial 256×256 mode may differ from top-screen configuration. Later repeat critical findings at 240×400 and other supported sizes.
- Each launch writes a new run folder and zip. Keep the whole zip with model, firmware, libctru/citro3d version, shader binary SHA256, clock mode, test order, ambient conditions, date, and any CFW changes.
- Never run known GPU-freezing configurations in unattended sweeps. Consult `docs/04-sources-and-knowns.md`.

## Starting sources

- https://www.3dbrew.org/wiki/GPU/External_Registers
- https://www.3dbrew.org/wiki/GPU/Internal_Registers
- https://www.3dbrew.org/wiki/GPU/Shader_Instruction_Set
- https://www.3dbrew.org/wiki/GPU/Pitfalls
- https://www.3dbrew.org/wiki/GPU/Programming_Guide
- https://github.com/devkitPro/3ds-examples/tree/master/graphics/gpu/simple_tri
- https://github.com/devkitPro/citro3d
- https://github.com/devkitPro/libctru
- https://github.com/devkitPro/picasso
- https://github.com/azahar-emu/azahar

## Timing investigation update

Run 007 is a completed v0.4 Old 3DS run; run 008 contains no test cases. In run 007 the 256-triangle size-3 and size-4 perf2 variants report about 50 ms P3D while drawing time is below 1 ms. The cause is unknown. The host perf2 tool now withholds the fit when median P3D exceeds median drawing time by more than 5 ms and exceeds 5 ms; this heuristic detects a gross inconsistency, not all invalid timings. Raw samples remain visible. The application logs the P3D total and last-duration counters before and after individual similarly anomalous frames. Re-run on hardware to investigate; a successful build or host self-test does not validate the timing fix. Empty frames can retain the previous drawing-time value and must not be treated as an independent fresh timing.

The raster_fine tool now fails on missing windows or steps. Its strict-inside model has reproducible exceptions in run 007; those are research observations, not a self-test failure of the GPU.

## Runs 009-011 update (2026-11-14)

All 451 image CRCs agree across runs 009, 010 and 011 (010 holds two complete runs). `raster_fine` rejects the "ties outside" model: a pixel center exactly on an edge is covered when the edge direction (vertex order, program coordinates) has dx>0 (A4 step 128: 96 px; B2 step 128 and B4 step 120: 7 px each) and not covered for (-,+), (0,-), (-,-). `tools/rasterfine.py` now uses a provisional direction-based tie rule that reproduces all three runs with 0 differing steps; it was fitted to the data it explains, so it is not yet a discriminated claim. Directions (-,0), (0,+) and (+,-) have no data; windows R1-R6 (reversed winding) target them. The perf2 anomaly persists (tri_s3_n256, tri_s4_n256: P3D 50-83 ms vs drawing 0.17 ms, values near 3/4/5 x 16.67 ms) and is the subject of `timing_iso`. `perf2.py --exclude` allows a fit that skips named variants and prints the exclusion.

## Runs 012-014 update (2026-11-14)

Three v0.5 runs, identical images (every `.rgba` md5 matches), same firmware 023A0000. **Tie rule:** direction-keyed rules fail; forward and reverse winding of the same edge cover the same pixels. Keying on the edge's *inward normal* (winding-independent) fits runs 009-012 with 0 differing steps and clean (non-mixed) evidence classes: ny>0 covered, ny<0 not covered, (+,0) not covered. No data yet for (-,0), (0,-), (+,+); v0.6 windows C1-C6 target them. Still "reproduced", not "discriminated": the rule was derived from these same data. **Timing:** P3D for s3/s4 n<=256 sits at 3-7 x 16.67 ms (vblank multiples) with drawing time ~0.17 ms, while s3_n512 and s8_n256 give ~55 us with no anomaly, so P3D is not a reliable per-draw GPU time. Drawing time grows smoothly (154 us at n=64, 171 at n=256, 200 at n=512).

## Sources

- Measurements: `run-logs/` (text files of each run; the capture zips stay off-repo) and `STATE-LOG-*.md`.
- External references: the list under "Starting sources" above, and [sources and knowns](docs/04-sources-and-knowns.md).
- Wiki background: [PICA200 command pipeline](../../engineering/PICA200/pica200-command-pipeline.md), [GPU internal registers](../../hardware/gpu-internal-registers.md), [testing on hardware](../../engineering/testing-on-hardware.md).
