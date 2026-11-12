# Case study: melonDS Switch port (DS on Tegra X1)

A Nintendo DS emulator running as Switch homebrew: it emulates the DS chipset on the Switch **[CPU](../../../glossary.md#cpu)** and composites both DS screens through the Switch **[GPU](../../../glossary.md#gpu)**. This page summarises the vendored tree ([RSDuck/melonDS](https://github.com/RSDuck/melonDS) commit `6c6f1e8`, 2021-11-04, a historical fork snapshot that contains the Switch frontend; it is not current melonDS, and the current project's NDS core, renderers and frontends have changed since; see [source versions](../source-versions.md)): how the DS maps onto three A57s and **[Maxwell](../../glossary.md#maxwell)**. Tree facts are `[documented]`; outside claims are `[reported]`.

## When is this useful?

Read this when writing an emulator or any program that composites a small framebuffer on the GPU and pulls audio in blocks.

## Before you start

- Evidence: structure is `[documented]` in `src/NDS.cpp`, `src/GPU.cpp`, `src/frontend/switch/main.cpp`, `Platform.cpp`, `Gfx.cpp` and `CMakeLists.txt`. Coffeepot ran no benchmark or ROM.
- Pacing reference: one DS frame is 263 scanlines at ~60 Hz; the frontend loops `NDS::RunFrame()` until 1/60 s of emulated time elapses (`main.cpp:683-698`) `[documented]`.
- Clocks: settings expose 1020–1785 MHz plus downclocks, re-applied on dock, performance-mode and resume hooks (`main.cpp:65-121`) `[documented]`. No in-tree fps table ties a game to a clock, so speed talk stays `[reported]`.
- Companions: [rendering](../rendering.md), [performance and porting](../performance-and-porting.md), [Eidos tale](case-study-eidos-ouroboros.md).

Jargon: the **[CPU](../../../glossary.md#cpu)** runs guest ARM9/ARM7 code; the **[GPU](../../../glossary.md#gpu)** draws via **[Maxwell](../../glossary.md#maxwell)**; a **[framebuffer](../../../glossary.md#framebuffer)** is one screen image; **[cache](../../../glossary.md#cache)** is fast memory beside a core; **[ARM](../../../glossary.md#arm)** names DS and Switch cores alike; **[libnx](../../glossary.md#libnx)** is the homebrew system library; **[title override](../../glossary.md#title-override)** escapes the small applet memory budget.

## What it is

| Layer | Approach |
| ----- | -------- |
| DS CPUs | ARM9 (ARMv5, 67 MHz) plus ARM7 (ARMv4, 33 MHz) with timers, DMA and GXFIFO stalls interleaved to a shared target in `NDS::RunFrame<EnableJIT, ConsoleType>` (`NDS.cpp:947-1069`) `[documented]`. |
| Interpreter vs JIT | Interpreter `Execute()` or A64 JIT `ExecuteJIT()` (`src/ARMJIT_A64/`) chosen by `Config::JIT_Enable`; JIT builds only on x86-64/ARM64, with a NEON 2D path on ARM64 (`CMakeLists.txt:59-90`) `[documented]`. |
| DS video | 2D tile/object engines plus 3D geometry commands, composited Switch-side. Renderer 0 is native (`GPU2D/3D::DekoRenderer`); renderer 1 is OpenGL with fallback (`GPU.cpp:389-426`) `[documented]`. |
| DS audio | SPU mixed in-core, pulled by a thread in 768-sample blocks at 32823 Hz into a 2-slot audren queue (`main.cpp:146-247,304`) `[documented]`. |
| Saves | Cart SRAM flushed after each emulated frame (`NDS.cpp:1042`); BIOS, firmware and SD expected under `/switch/melonds/` (`Platform.cpp:32-48`) `[documented]`. |

## What it gets right

| Technique | Effect | Conditions |
| --------- | ------ | ---------- |
| Interleaved cores | ARM9, ARM7, timers, DMA, 3D stalls advance to one target, keeping timing games stable (`NDS.cpp:958-1022`) `[documented]`. | Core design; read from source. |
| Native deko3d compose | 2D/3D output as textures drawn as quads with ready/presented fences (`main.cpp:702-714`) `[documented]`. | No fullscreen CPU readback. |
| Touch fallbacks | Real touch plus stick-mouse, stick-offset ray and gyro cursors, hold/toggle click (`main.cpp:426-668`) `[documented]`. | DS touch is mandatory input. |
| Save compatibility | Per-frame SRAM flush; layout matches desktop melonDS so saves travel with the ROM `[documented]`. | Pause/stop/reset in `main.cpp:856-871`. |
| Pacing controls | `LimitFramerate`, shoulder fast-forward, dock/handheld switch (1280×720 vs 1920×1080), nearest/linear and integer scaling (`main.cpp:886-930`) `[documented]`. | Dual 256×192 screens upscale ~3×. |
| Tunable JIT | Enable, block size 1–32, branch/literal/fast-memory toggles (`SettingsDialog.cpp:290-310`) `[documented]`. | One build tests parity vs headroom. |

No speed figures are stated in the tree, so none are given here.

## Constraints, observed problems and untested hypotheses

Rows tagged `[documented]` are read from the source; `[reported]` rows are the authors' own statements; `[hypothesis]` rows are inferred from the structure and were not measured.

| Cost / trap | Detail |
| ----------- | ------ |
| Two guests on three A57s | ARM9, ARM7, SPU and OS share three cores; threads round-robin from core 1 at priority 0x30, audio on core 0 (`Platform.cpp:72-83`, `main.cpp:312`) `[documented]`. A JIT cuts per-instruction cost but adds code-cache pressure `[hypothesis]`; sustained heat at the high clock settings is also unmeasured `[hypothesis]`. |
| Renderer confusion | Switch frontend defaults to renderer 0 (Deko/Soft); desktop GL expectations do not transfer (`main.cpp:249-254`) `[documented]`. |
| Compose and filter fill | Two framebuffers upload and draw as textured quads each frame with a nearest/linear sampler choice `[documented]`. Linear at docked 1080p costs **[Maxwell](../../glossary.md#maxwell)** fill for no emulated detail `[hypothesis]`. |
| Audio stretch | Starved `ReadOutput` sleeps 10 µs and pads with the last sample (`main.cpp:216-243`); `SPU::Sync(wait)` is marked unused (`SPU.cpp:795-823`) `[documented]`. Late video thus stretches audio instead of dropping cleanly. |
| Applet memory | Missing BIOS/firmware yields dialogs (`main.cpp:767-808`) `[documented]`. Album-applet launch starves the JIT cache; the tree assumes full-memory homebrew without asserting **[title override](../../glossary.md#title-override)**. |

## Lesson: what to copy first

| # | Symptom to fix |
| - | -------------- |
| 1 | Fullscreen CPU pixel read per frame — composite on the GPU with per-buffer fences (`main.cpp:702-714`); the Eidos port paid ~22.5 ms a frame for that copy `[reported]` (see the [Eidos case study](case-study-eidos-ouroboros.md)). |
| 2 | Guest cores drifting under JIT — interleave ARM9/ARM7/timers/DMA to one target (`NDS.cpp:958-1022`); determinism before pinning `[documented]`. |
| 3 | Crackle blamed on the mixer — pull 768-sample blocks to the video frame, pad explicitly, and replace the unused `SPU::Sync` only with a measured hook `[documented]`. |
| 4 | "Slow GPU" from submission stamps — split emulation, audio-pull and present with the histogram plus `Profiler::Sum`; compare identical save, scene, clocks and launch mode `[documented]`. |
| 5 | Desktop filter defaults — ship nearest plus integer scaling handheld; allow linear only after a docked fill measurement `[hypothesis]`. |
| 6 | Overclock as first fix — re-apply clocks on resume but gate acceptance on stock clocks under title override; a fast wrong frame and a correct slow frame both fail (the Eidos acceptance rule) `[reported]`. |

The lessons are inferences from the mechanisms the code shows. A `[documented]` tag cites the code that shows the mechanism, not a measurement that the fix works; none was tested on a console by Coffeepot.

## Open questions

- Stock-clock JIT-vs-interpreter deltas per game on the A57? No table in-tree `[documented]`.
- Docked linear-vs-nearest fill cost for the two-screen composite on Maxwell? Needs a same-scene frame pair `[hypothesis]`.
- Thermal and audio-underrun behaviour over a 30-minute session at the high clock settings? Needs a p50/p95/p99 plus underrun log.

## Sources

[RSDuck/melonDS](https://github.com/RSDuck/melonDS) commit `6c6f1e8` (line numbers above are from the saved tree and drift between commits):

- `README.md` — identity, BIOS/firmware needs, desktop build matrix.
- `CMakeLists.txt` — JIT/NEON/deko3d/OpenGL flags, Switch frontend switch.
- `src/NDS.cpp`, `src/GPU.cpp`, `src/SPU.cpp` — interleave, renderer select, audio FIFO.
- `src/frontend/switch/main.cpp` — threads, audio, 1/60 s loop, touch, layout, clocks.
- `src/frontend/switch/Platform.cpp`, `Gfx.cpp`, `SettingsDialog.cpp` — paths, core pinning, fences, JIT options.
- [Eidos postmortem](case-study-eidos-ouroboros.md) — what not to copy: GL-on-Nouveau, readback, nested timing.
