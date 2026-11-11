# Case study: DaedalusX64-3DS (N64 emulator)

An N64 emulator for 3DS that runs commercial games at reduced speed by recompiling N64 CPU code to ARM and translating N64 graphics commands to the 3DS GPU. The companion page covers [native N64-to-3DS ports](case-studies-ports.md); that material is not repeated here.

## Before you start

- Evidence: facts read in the vendored tree are `[documented]` with the source file. Unverified public claims are `[reported]`. Performance inferences are `[hypothesis]` until measured. See [evidence labels](../../../AGENTS.md).
- Source: [masterfeizz/DaedalusX64-3DS](https://github.com/masterfeizz/DaedalusX64-3DS) commit `31c5e56` (2022-10-17; see [source versions](../source-versions.md)).
- Old 3DS is the default. The tree calls `osSetSpeedupEnable(true)` unconditionally and uses `APT_CheckNew3DS` only to switch on the 800-pixel wide mode (`SysCTR/main.cpp` lines 94-101) [documented]; faster-hardware-only items are marked **New 3DS:**.

Jargon: the **[ARM11](../../glossary.md#arm11)** is the 3DS CPU; **[PICA200](../../glossary.md#pica200)** is the GPU; a **[display list](../../glossary.md#display-list)** is the N64 drawing-command stream; **[FCRAM](../../glossary.md#fcram)** is main memory and **[VRAM](../../glossary.md#vram)** is fast GPU memory; a **[draw call](../../glossary.md#draw-call)** is one request to draw triangles; **[V-blank](../../../glossary.md#vblank)** is the pause between screen refreshes.

## What it is

DaedalusX64-3DS is the 3DS backend of DaedalusX64, also ported to PSP and Vita. The README describes a release-plus-data install with ROMs in `sdmc:/3ds/DaedalusX64/Roms/` [reported]. The gap: a 93.75 MHz MIPS R4300i with vector coprocessors, emulated on a 268 MHz dual-core ARM11 with a fixed-function GPU and 128 MB of FCRAM.

## CPU: interpreter bootstraps a dynarec

Cold N64 code runs on a plain interpreter [documented]. After 10 passes a loop trace becomes hot and compiles to ARM (`Dynamo.cpp`, `gHotTraceThreshold`) [documented]. Fragments live in a cache capped at 9,216 entries (8,192 + 1,024) [documented]. The 3DS backend emits ARM through `SysCTR/DynaRec/arm/CodeGeneratorARM.cpp` with a register cache [documented]. The code buffer is 8 MB, page-aligned, marked executable; when it fills, a reset rewinds the pointer and discards everything (`CodeBufferManagerARM.cpp`) [documented].

## RSP: task switch instead of vector emulation

The N64 signal processor runs graphics, audio, JPEG and boot tasks. DaedalusX64 reads the task header in shared memory and dispatches by type instead of emulating the vector unit (`Source/Core/RSP_HLE.cpp`) [documented]. Graphics tasks go to the display-list parser, audio tasks to the audio microcode handler; JPEG and CIC boot handlers are borrowed from Mupen64plus (per a code comment) [documented]. The fast path completes synchronously and raises the SP interrupt [documented].

## RDP to PICA200: combiner trees over texture env

The backend parses the display list (`DLParser.cpp`), tracks RDP state (`RDPStateManager.cpp`), and renders via picaGL (`RendererCTR.cpp`) [documented]. The N64 colour combiner's two-cycle mux equations have no direct PICA200 equivalent, so each mux compiles into a tree (`Combiner/CombinerTree.cpp`), cached by mux key (`mBlendStatesMap`); multi-pass states save and restore the vertex buffer (`mVtx_Save`) [documented]. Blend coverage is a per-game table keyed on RDP blender bits (Mario Golf, Paper Mario, Conker, Banjo entries visible) [documented]. Vertices accumulate in 30,000-entry linear-memory buffers and flush per batch with `glDrawArrays` [documented].

## Audio, pacing, input and saves

The video-interface register `VI_ORIGIN` marks finished frames. `UpdateScreen` fires on origin changes, derives the sync ratio, and presents or skips per the frameskip setting (`GraphicsPluginCTR.cpp`) [documented]. Audio pacing derives from the same ratio (`gSoundSync`, `gVISyncRate`) [documented]. Decoded audio microcode feeds six 512-sample stereo buffers through ndsp (`AudioOutput.cpp`) [documented]. **New 3DS:** audio microcode runs on a second-core worker; on Old 3DS it runs synchronously on the calling thread (`AudioPluginCTR.cpp`) [documented].

Input polls once per frame, halves the circle-pad range into N64 stick units, and maps buttons through a mask/AND/OR tree (`InputManagerCTR.cpp`) [documented]. **New 3DS:** extra mappings use the added controls [documented]. Saves cover EEPROM, SRAM, FlashRAM and mempak (`Core/Save.cpp`, PIF `0x04/0x05`) [documented]; ROM selector, in-game menu and `SaveGames/`/`SaveStates/` live under `SysCTR/UI/` [documented].

## How it pushes 3DS hardware

CPU is the ceiling: recompiled N64 code, display-list parsing, combiner evaluation and audio microcode share two 268 MHz cores with the OS owning part of the second. The tree requests the higher application clock at boot, unconditionally [documented]. GPU rasterisation is cheap by comparison — the ports page reports G-Diffuser at 3.6 ms GPU-busy inside a 96 ms frame [reported] — and two-cycle combiners become multi-pass PICA200 work, multiplying CPU vertex replays and command traffic `[hypothesis]`. Memory is comfortable but split: 8 MB dynarec buffer, ~1 MB vertex staging, converted textures, audio buffers and the RDRAM image compete for contiguous [linear heap](../../glossary.md#linear-heap) blocks `[hypothesis]`.

Right: the threshold-10 ladder, the RSP task switch, mux-keyed combiner caching, and one VI-origin gate for video, audio and frameskip. Wrong: multi-pass fallback replays whole vertex buffers per pass; blend coverage is an ever-growing per-game table; Old 3DS audio shares the emulation thread; frameskip hides rather than shrinks per-frame work.

## Lessons for future 3DS homebrew

| Symptom | Likely cause in this tree | Rule to try first |
| ------- | ------------------------- | ----------------- |
| Periodic stutter, then smooth | Dynarec buffer reset discarding hot traces | Size the code buffer to the workload; count resets before touching codegen [documented] |
| Slow in textured scenes only | Per-draw texture conversion, cache misses | Cache converted textures by tile descriptor; batch one-texture draws `[hypothesis]` |
| Wrong translucency in one game | Unmapped blender mux falling through | Add the mux explicitly with a screenshot test [documented] |
| Crackling that tracks frame rate | Emulation-thread audio starved by slow frames | Double-buffer PCM behind the DSP callback; pace from VI origin, as the **New 3DS:** worker does [documented] |
| Judder with vsync on | 1:1 presents against a 60 Hz panel | One limiter, skip drawing but keep logic ticks (AUTO1/AUTO2/manual) [documented] |
| Coarse stick edges | Circle-pad range halved by integer division | Scale after mapping with a deadzone; verify on hardware [documented] |

## Technical details

- Dynarec: hot threshold 10, fragment cache ~9,216 (`Dynamo.cpp`); 8 MB executable buffer, permissions 7 at init [documented].
- Graphics: picaGL `pglInitEx(0x080000, 0x040000)`, 400x240 target, 30,000-vertex linear staging [documented]. **New 3DS:** `gfxSetWide(true)` 800-pixel mode [documented].
- Audio: 44.1 kHz stereo ndsp, six 512-sample buffers, DSP cache flushed per refill [documented]. **New 3DS:** 8 KB worker at priority `0x18` on core 2 [documented].
- Pacing: origin-change gate, auto-skip when sync below 0.965, VI-sync clamp 1,500–4,000 [documented].

## Open questions

- Per-stage frame cost on Old 3DS (dynarec vs display-list parsing vs audio)? The tree ships a profiler but this page records no measurement.
- Does the 8 MB code buffer fill in practice? A reset counter answers in one session.
- Combiner-state cache hit rate per game? The map exists but publishes no statistics.
## Sources

[masterfeizz/DaedalusX64-3DS](https://github.com/masterfeizz/DaedalusX64-3DS) commit `31c5e56` (2022-10-17), files under `Source/`: `SysCTR/main.cpp` (model check, clock, `pglInitEx`, wide mode); `Core/Dynamo.cpp` (hot threshold 10 at line 63, fragment cache 8,192 + 1,024 at line 61); `SysCTR/DynaRec/arm/CodeBufferManagerARM.cpp` (8 MB code buffer at line 32); `Core/RSP_HLE.cpp`; `SysCTR/HLEGraphics/` (`RendererCTR.cpp`, `GraphicsPluginCTR.cpp`: pacing thresholds); `SysCTR/Graphics/GraphicsContextCTR.cpp` (30,000 vertices at line 95); `SysCTR/HLEAudio/` (`AudioOutput.cpp`: six 512-sample buffers; `AudioPluginCTR.cpp`: New 3DS worker at priority `0x18` on core 2); `SysCTR/Input/InputManagerCTR.cpp`; `Core/Save.cpp`, `PIF.cpp`.

- [Case studies: three large 3DS ports](case-studies-ports.md) — companion page; the G-Diffuser GPU-busy figure cited above is from there `[reported]`.
