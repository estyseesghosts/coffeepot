# PICA Lab: real-hardware PICA200 research

**Status:** v0.7 research harness. v0.3 to v0.6 have run on one Old 3DS (firmware `023A0000`, runs 003-017; v0.6 is runs 015-017). v0.7 fixes the timing synchronisation (below) and builds without warnings, but has **not run on hardware**. Host tools pass their self-test, which checks the tools only. See [`STATE-LOG-2026-11-13.md`](STATE-LOG-2026-11-13.md) for what has been measured and what was wrong.

> **Timing results before v0.7 are provisional.** Up to v0.6 the timed frames read the GSP perf log and `C3D_GetDrawingTime()` after `C3D_FrameSync()`. In citro3d that call waits for VBlank counters, not for the GPU; the queue wait is inside `C3D_FrameBegin` (`renderqueue.c` lines 75-85 and 163-176 at the [pinned commit](../../engineering/source-versions.md#pinned-source-commits)). So a timing sample was not guaranteed to belong to the frame it was logged with, v0.6's `timing_wall` rows timed a VBlank wait (16.3-16.6 ms in every variant of runs 015-017), and the "empty" variant submitted no command list at all (P3D 0, drawing time left over from an earlier frame). Image results (raster, edge, TEV, blend) do not depend on this: their readback waits for the queue. v0.7 waits for the GX queue to drain before sampling, times each segment of the frame separately and flags any drawing-time sample that cannot belong to its submission. The P3D anomaly, the triangle/fill estimates and the drawing-time "floor" must be re-measured with v0.7 before they are treated as hardware characteristics.

This project is intended to produce **independent, falsifiable hardware measurements**, not benchmark scores detached from explanations. It starts with safe Citro3D-generated command lists and a 256×256 offscreen RGBA8 render target. Each completed test saves a CSV row and, for accuracy tests, the complete raw linear readback. More intrusive testing is specified but **not enabled**.

## Build

Requires devkitPro's 3DS development environment, including `devkitARM`, `libctru`, `citro3d`, `picasso`, and `3ds_rules`. Use devkitPro's standard installed shell or MSYS2 environment on Windows.

```sh
make -j4
# output: pica-lab.3dsx (when correctly built)
```

The Makefile is adapted from devkitPro's official 3DS application template. It builds without warnings (`-Wall -Wextra`) with the toolchain on [source versions](../../engineering/source-versions.md); on Windows use the devkitPro MSYS2 shell (`C:/devkitPro/msys2/usr/bin/bash.exe -lc 'cd .../pica-lab && make -j4'`).

Copy `pica-lab.3dsx` to `sdmc:/3ds/pica-lab/pica-lab.3dsx` and launch it through Homebrew Launcher. The app creates `sdmc:/3ds/pica-lab/` itself.

**SELECT runs every family in order** (selftest, raster+edge, raster_fine, tev, blend, perf, perf2, timing_iso, probe) and stops on an error or a failed self-test. Single families run from the buttons below (all except A and R need A's self-test first).

Each launch creates a new `sdmc:/3ds/pica-lab/run_NNN/` (never overwrites) holding `run.log` (timestamped), `results.csv`, `manifest.txt` and `probe.txt`, plus **`run_NNN.zip`** beside it. The zip is uncompressed and holds all `.rgba` captures; the text files are added at clean exit (START). Copy the zip off the SD card. If the app crashes the zip may lack the text files and the last entries (`zip -FF` can recover it).

`results.csv` columns (v0.7): `case_id,family,variant,parameter,crc32,nonclear_pixels,center_abgr_u32,gpu_queue_ms,p3d_us,begin_us,frameend_us,drain_us,perflog_us,assoc`. v0.4-v0.6 CSVs stop at `p3d_us`, and v0.6 also writes `timing_wall` rows; the tools read every version.

- `crc32` is the CRC-32 of the 256 KB capture (the value the zip entry stores). It is a corruption check and a quick index, **not proof that two images are equal**: a 32-bit checksum can collide. Establish equality by comparing bytes (`analyze.py compare`) and identify a capture durably by SHA-256 (`analyze.py sha256`). v0.3 CSVs have an `fnv1a32` column instead; it collided on two different images and is not used for anything.
- Timed rows (`perf`, `perf2`, `timing_iso`), v0.7: `gpu_queue_ms` is `C3D_GetDrawingTime()` read after the queue drained; `p3d_us` the GSP perf-log P3D delta across `C3D_FrameEnd` to queue idle; `begin_us` the `C3D_FrameBegin(C3D_FRAME_SYNCDRAW)` call (VBlank wait plus previous-queue wait, never GPU time of this frame); `frameend_us` the `C3D_FrameEnd(0)` call (split, whole-linear-heap flush, queue start); `drain_us` from `C3D_FrameEnd` returning until the GX queue is idle (GPU execution plus completion latency); `perflog_us` the second `GSPGPU_GetPerfLog` request; `assoc` is 0 when the drawing time is longer than `frameend_us + drain_us` and so cannot belong to this submission. Image rows write 0 / -1 in these columns.

## Controls / initial implemented tests

| Button | Family | Current implementation | Signal |
|---|---|---|---|
| A | `selftest` | Clear, then two identical rectangles | Readback coherency + determinism gate |
| B | `raster`, `edge`, `edge_cov` | 1/64-pixel triangle translations, forward then backward; quad diagonals; additive coverage | Coverage transition map, watertight shared edges |
| D-pad down | `raster_fine` | 1/256-pixel offset sweeps (x and y, two geometries), 13-step windows around exact-model transitions, forward and reverse | Vertex quantization vs exact pixel-center arithmetic (`tools/rasterfine.py`) |
| X | `tev` | Constant multiplier on a second TEV stage | 8-bit output changes / stage rounding characterization |
| Y | `blend` | Two layers, source alpha 0/16..16/16 | Blend mapping, rounding and pixel reproducibility |
| L | `perf` | 9x9-px triangles (192-3072), draw-call splits, full-target quads; interleaved reps, `empty` floor (v0.7: one minimal submission; before v0.7 it submitted nothing); timing columns above | Per-triangle and per-pixel cost estimate |
| D-pad right | `perf2` | Triangle size as the variable: fixed-pixel and fixed-count series (3-64 px legs) and quads; `perf2_cov` measures pixels per triangle | Fit `p3d_us` (or `drain_us`) `= c + a*tris + b*pixels` (`tools/perf2.py`) |
| D-pad left | `timing_iso` | Isolates the perf2 P3D anomaly: 1/16/64/256/512-triangle, pitch, and 4x/16x repeated-draw variants, back-to-back (`_blk`) vs after an empty frame (`_gap`); v0.7 logs each frame segment (v0.6 logged `timing_wall` rows, which timed a VBlank wait) | `tools/timing_iso.py`: P3D vs drain vs drawing time per segment, frame-multiple check |
| R | `probe` | Read-only GPU identity/status register attempts; log result codes | Service-access feasibility, not profiler enable |
| START | Exit | Flush outputs and exit | |

### Validation gates (mandatory before interpreting results)

1. The `selftest/clear` image contains only 0x000000FF pixels. If not, investigate format conversion and transfer orientation, initial clear semantics, and framebuffer flush ordering.
2. Both `selftest/rect` captures are identical and have sensible nonclear coverage. The app compares their CRC-32 on the console (a fast gate); on the host, confirm byte equality. If not, there is a coherency or state-control problem.
3. `edge` captures should be compared *pixel-for-pixel*, not solely by hash; gaps/differences can be legitimate clues only after gate 1–2 pass.
4. Repeat test families in separate clean sessions. Reverse order and compare: divergence between the two raster sweep directions invalidates a simple static coverage interpretation.
5. `perf` samples must be treated as **GPU queue time**. They include command processing and state setup and may not isolate rasterization. Inspect medians and spread before declaring a difference. From v0.7, discard any row with `assoc=0` and report how many there were.
6. Confirm readback byte/channel order using contrasting solid-color probes before making channel-specific numeric claims. The host tool assumes 32-bit words packed `0xRRGGBBAA` (`[AA,BB,GG,RR]` in little-endian raw memory) as expected for the GX RGBA8 format. **Orientation:** with `Mtx_OrthoTilt` and this transfer, program x is the raw row and program y the raw column, so the raw word index of program pixel (x, y) is `x*256 + y`. Checked on run 015's `raster` step 0, an asymmetric right triangle: its 96-pixel x leg spans raw rows 80-175 and its 128-pixel y leg spans raw columns 48-174, with the right angle at the lowest row and column (no flip).

### Host analysis

```sh
python tools/selftest.py
python tools/analyze.py summary run_NNN.zip      # or a run folder / results.csv
python tools/interrogate.py run_NNN.zip
python tools/rasterfine.py run_NNN.zip           # raster_fine vs exact model
python tools/perf2.py run_NNN.zip [--exclude a,b] # triangle/pixel cost fit
python tools/timing_iso.py run_NNN.zip           # P3D anomaly isolation (v0.5+ runs; per-segment columns need v0.7)
python tools/analyze.py compare run_A.zip run_B.zip [...]   # byte-compare every capture across runs
python tools/analyze.py sha256 run_NNN.zip       # SHA-256 of every capture
python tools/analyze.py ppm /path/to/raster_0003.rgba case.ppm
python tools/analyze.py diff raster_0005.rgba raster_0069.rgba --out difference.rgba
```

`ppm` produces a PPM image without adding third-party packages. Output bytes preserve the transfer's linear layout; use `--rotate` only for human-oriented inspection. `summary` extracts a `.zip` to `run_NNN.unzipped/` beside it; `rasterfine.py` and `compare` read zips directly. `summary` checks CSV crc32 values (skipped for old CSVs), marked-pixel counts and center pixel data against raw captures. **Different GPU results are not automatically failures**—they become research evidence once harness controls pass.

### Published captures

The device zips are 50-230 MB and stay off the repository, but one complete run is published so the image results can be checked pixel by pixel: [`captures/run_015.zip`](captures/run_015.zip) (589 KB, deflate) holds all 753 captures of v0.6 run 015 plus its `results.csv`, `manifest.txt`, `probe.txt` and `run.log`, and [`captures/run_015.sha256`](captures/run_015.sha256) lists the SHA-256 of every capture. Every tool accepts it directly, for example `python tools/rasterfine.py captures/run_015.zip`. Runs 016 and 017 are byte-identical to it in all 753 captures (`analyze.py compare`), so it stands for all three.

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

- The `.3dsx` is **not prebuilt** in the repository and is **not a definitive timing profiler**. v0.3-v0.6 have run on one Old 3DS only; nothing has run on a New 3DS; v0.7 has not run at all.
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

The timing statements in this and the next two sections were made with the pre-v0.7 synchronisation (see the status note at the top) and are provisional.

Run 007 is a completed v0.4 Old 3DS run; run 008 contains no test cases. In run 007 the 256-triangle size-3 and size-4 perf2 variants report about 50 ms P3D while drawing time is below 1 ms. The cause is unknown. The host perf2 tool now withholds the fit when median P3D exceeds median drawing time by more than 5 ms and exceeds 5 ms; this heuristic detects a gross inconsistency, not all invalid timings. Raw samples remain visible. The application logs the P3D total and last-duration counters before and after individual similarly anomalous frames. Re-run on hardware to investigate; a successful build or host self-test does not validate the timing fix. Empty frames can retain the previous drawing-time value and must not be treated as an independent fresh timing.

The raster_fine tool now fails on missing windows or steps. Its strict-inside model has reproducible exceptions in run 007; those are research observations, not a self-test failure of the GPU.

## Runs 009-011 update (2026-11-14)

All 451 image CRCs agree across runs 009, 010 and 011 (010 holds two complete runs). `raster_fine` rejects the "ties outside" model: a pixel center exactly on an edge is covered when the edge direction (vertex order, program coordinates) has dx>0 (A4 step 128: 96 px; B2 step 128 and B4 step 120: 7 px each) and not covered for (-,+), (0,-), (-,-). `tools/rasterfine.py` now uses a provisional direction-based tie rule that reproduces all three runs with 0 differing steps; it was fitted to the data it explains, so it is not yet a discriminated claim. Directions (-,0), (0,+) and (+,-) have no data; windows R1-R6 (reversed winding) target them. The perf2 anomaly persists (tri_s3_n256, tri_s4_n256: P3D 50-83 ms vs drawing 0.17 ms, values near 3/4/5 x 16.67 ms) and is the subject of `timing_iso`. `perf2.py --exclude` allows a fit that skips named variants and prints the exclusion.

## Runs 012-014 update (2026-11-14)

Three v0.5 runs, identical images (every `.rgba` md5 matches), same firmware 023A0000. **Tie rule:** direction-keyed rules fail; forward and reverse winding of the same edge cover the same pixels. Keying on the edge's *inward normal* (winding-independent) fits runs 009-012 with 0 differing steps and clean (non-mixed) evidence classes: ny>0 covered, ny<0 not covered, (+,0) not covered. No data yet for (-,0), (0,-), (+,+); v0.6 windows C1-C6 target them. Still "reproduced", not "discriminated": the rule was derived from these same data. **Timing:** P3D for s3/s4 n<=256 sits at 3-7 x 16.67 ms (vblank multiples) with drawing time ~0.17 ms, while s3_n512 and s8_n256 give ~55 us with no anomaly, so P3D is not a reliable per-draw GPU time. Drawing time grows smoothly (154 us at n=64, 171 at n=256, 200 at n=512).

## Runs 015-017 update (v0.6, 2026-11-14)

Three complete v0.6 runs (2403 cases each, 0 errors), Old 3DS, firmware 023A0000.

- **Images:** all 753 captures are byte-identical across the three runs, and the 597 cases shared with run 012 have identical CRCs. `raster_fine` matches the inward-normal tie rule with 0 differing steps in all 21 windows, forward and reverse.
- **Tie rule, out-of-sample test passed.** The rule "covered iff ny>0, or ny==0 and nx<0" was fitted to runs 009-012 and *predicted* the three classes that had no data. Windows C1-C6 then measured (-,0) 256 covered / 0 not, (0,-) 0 / 192, (+,+) 128 / 0: all three as predicted, in all three runs. Every one of the eight inward-normal classes now has data and none is mixed.
- **Timing:** `timing_wall` (FrameEnd to FrameSync) read 16.3-16.6 ms in every variant: it timed the VBlank wait, which is how the synchronisation fault was confirmed. The P3D anomaly (P3D 50-184 ms while drawing time read 0.15-0.2 ms) appears in every rep for 3-pixel triangles at n = 1, 16, 64 and 256, but not for 8-pixel triangles, not for any repeated-draw variant (even 64 triangles drawn 16 times), and at n = 512 only twice in 120 reps (run 016, `_gap`). Non-anomalous P3D readings are 28-1284 us. In `run.log`, consecutive anomalous `_blk` cases are about 83-111 ms apart against 67-69 ms for the others, so those frames do take extra wall time, but about 16-44 ms, not the 50-184 ms P3D reports. Where the extra time goes cannot be told from v0.6 data; v0.7 times each segment.
- **perf2** reproduces the runs 009-011 fit (`p3d = 10 + 0.125*tris + 4.95 ns*px`, rms 41 us, the two anomalous variants excluded); provisional for the same reason.

## Sources

- Measurements: `run-logs/` (text files of each run; the capture zips stay off-repo) and `STATE-LOG-*.md`.
- External references: the list under "Starting sources" above, and [sources and knowns](docs/04-sources-and-knowns.md).
- Wiki background: [PICA200 command pipeline](../../engineering/PICA200/pica200-command-pipeline.md), [GPU internal registers](../../hardware/gpu-internal-registers.md), [testing on hardware](../../engineering/testing-on-hardware.md).
