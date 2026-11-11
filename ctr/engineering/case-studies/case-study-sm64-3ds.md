# Case study: the older SM64 3DS port (sm64_3ds)

The `sm64-port/sm64_3ds` tree kept privately in this repository: a first-generation decompilation port — the PC port's N64 graphics interpreter plus a thin 3DS backend. The "before" picture to [case studies: large ports](case-studies-ports.md), which covers the newer optimized port. Do not mix their figures; the contrasts are:

- **Approach.** Here the **[CPU](../../../glossary.md#cpu)** interprets graphics commands and hands ready triangles to a passthrough vertex **[shader](../../../glossary.md#shader)**. The newer branch moves the whole N64 vertex stage into the **[PICA200](../../glossary.md#pica200)** vertex shader `[reported]` (second-hand, via the OOT research notes).
- **Era.** First-generation direct port: reuse the PC path, add one backend, ship. The newer branch profiles first, then cuts per-draw CPU costs.
- **Results.** The near-60-fps headline belongs to the newer branch `[reported]`. This tree states no timing figures, so its performance is `Unknown` (`[hypothesis]`: slower by construction).

## Before you start

- Claims are `[documented]` in the named file unless labelled; nothing is `[measured]`.
- Read 11 November 2026 from `private_please_gitignore/sm64_3ds/` (not committed). Old 3DS is the default; the faster clock is enabled only after detecting a New 3DS.

The 3DS **[GPU](../../../glossary.md#gpu)** is the **[PICA200](../../glossary.md#pica200)**; its CPU is the **[ARM11](../../glossary.md#arm11)**. A **[display list](../../glossary.md#display-list)** is the N64 command stream; a **[draw call](../../glossary.md#draw-call)** is one request to draw triangles; **[libctru](../../glossary.md#libctru)** is the homebrew GPU wrapper and user library.

## Engine and render approach

`gfx_pc.c` walks each display list on the CPU — transforming vertices, computing lighting, packing a 26-float vertex buffer (256 triangles per flush) — then `gfx_citro3d.c` rewrites each vertex into a 10-float hardware format for `C3D_DrawArrays`. The vertex shader is three `mov` instructions, so the ARM11 does all geometry work every frame. Combiner modes become precomputed PICA200 **[texture-combiner (TEV)](../../glossary.md#tev)** states in a 64-entry pool.

`produce_one_frame` runs graphics, logic and audio back-to-back on a single **[thread](../../../glossary.md#thread)**: `C3D_FRAME_SYNCDRAW` plus a vblank wait when GPU time is under 16.6 ms. No frame skip, no render thread, no profiling — the time function returns `0.0`, `printf` is compiled out. Audio drives the **[DSP](../../../glossary.md#dsp)** via ndsp (32 kHz stereo 16-bit, four DMA buffers), synthesized inline each frame — unlike the newer port's second-core audio (**New 3DS:** core 2).

The game sees a 400x240 screen; the sole **[render target](../../glossary.md#render-target)** is 400x240 RGBA8 plus depth (~0.75 MB of 6 MB **[VRAM](../../glossary.md#vram)** `[hypothesis]`), left eye only. Anti-aliasing and wide mode are compile-time flags. **New 3DS:** speedup unlocks the faster clock when detected; the C-stick is mapped to C-buttons.

## What it does right and wrong

**Right:** ~800 lines of backend buy full visual parity — every combiner mode maps through one ID encoding. Non-power-of-two textures are padded by wrapping edge pixels with per-texture scale factors: no asset rebuild. Sync-draw plus vblank wait cannot tear. Four ndsp buffers with high/low sample-count switching absorb wobbles without threads.

**Wrong:** the passthrough vertex shader wastes the stage that could remove the bottleneck. `gfx_flush` fires on every state change, splintering one display list into many tiny draws. Fog re-emits every triangle in a second pass. Two-colour batches are scanned twice before copying. Each texture upload calls `C3D_TexInit` plus a CPU swizzle — full price on re-upload. With timing stubbed to zero, none of this can be profiled.

## Technical details

| Item | Value |
| ---- | ----- |
| Game pools (`pc_main.c`) | `0x165000` (~1.4 MB) main + `0x4000` effects |
| GPU vertex buffer (`gfx_citro3d.c`) | 2 MB **[linear heap](../../glossary.md#linear-heap)**, 10 floats/vertex |
| Software triangle buffer (`gfx_pc.c`) | 256 tris x 26 floats, flushed per state change |
| Textures (`gfx_citro3d.c`) | 4,096 `C3D_Tex` slots; 64 KB staging buffer; RGBA8; CPU swizzle |
| Audio (`audio_3ds.c`) | 4 x 16 KB linear; 32 kHz stereo PCM16; 1,100-sample target |
| Flags / output (`Makefile`) | `-march=armv6k -mtune=mpcore -mfloat-abi=hard -O2`; `.3dsx` via `3dsxtool`, no CIA target |

## Lesson table

Fixes are `[hypothesis]` until measured.

| # | Symptom | Fix the newer approach teaches |
| - | ------- | ------------------------------ |
| 1 | CPU time scales with triangles, GPU idles | Move vertex work into the vertex shader; CPU fills small streams |
| 2 | Many tiny draws per frame | Batch across state changes; one render-target select per frame |
| 3 | Fogged scenes cost ~2x | Fold fog into the main pass, not a second emission |
| 4 | Stalls on texture-heavy scenes | Upload each texture once; swizzle offline at build time |
| 5 | Audio crackles when frames run long | Synthesize on a second core with a deeper queue (core 1; **New 3DS:** core 2) |
| 6 | No idea which stage is slow | Ship a real tick source; time each stage per frame |
| 7 | Per-vertex CPU writes dominate | Shrink the CPU-written vertex format; write each vertex once |

## Open questions

- What does this tree run at on an Old 3DS, per scene? No in-repo figures exist; needs a `[measured]` run.
- Was any Old 3DS tuning tried here? Not visible in-repo. The newer branch's savings are second-hand `[reported]` figures with unstated scenes — unverifiable without re-running both.
- How large is the real `C3D_TexInit`-per-upload cost? A micro-benchmark would settle lesson 4.

## Sources

- `private_please_gitignore/sm64_3ds/` as read (`gfx_citro3d.c`, `gfx_3ds.c`, `shader.v.pica`, `gfx_pc.c`, `pc_main.c`, `audio_3ds.c`, `Makefile`, `README.md` with clone URL `sm64-port/sm64_3ds`).
- [Case studies: three large 3DS ports](case-studies-ports.md) — the newer SM64 figures contrasted here.
