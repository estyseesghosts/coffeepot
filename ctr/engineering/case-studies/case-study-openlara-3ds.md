# Case study: OpenLara on 3DS (grid rooms on PICA200)

OpenLara is an open-source Tomb Raider engine with a dedicated 3DS backend: citro3d rendering in `src/gapi/c3d.h`, a devkitARM target in `src/platform/3ds`, and hand-written PICA200 vertex shaders in `src/shaders/pica`. It shows how a grid-and-portal engine — rooms joined by doorways — maps onto fixed-function-heavy hardware. All statements describe the Old 3DS unless marked. Code facts are `[documented]` in the named file; guidance without figures is `[hypothesis]`.

## Before you start

- Evidence: [XProger/OpenLara](https://github.com/XProger/OpenLara) commit `3b268ca` (2026-07-26; see [source versions](../source-versions.md)); facts read from the tree on 11 November 2026 (`src/gapi/c3d.h`, `src/platform/3ds/main.cpp`, `Makefile`, `src/shaders/pica/*`, `src/level.h`). Nothing here is `[measured]` by Coffeepot, and the tree states no frame-rate figures — do not quote any.
- **New 3DS:** only the clock check in `main.cpp` differs; everything else is the Old 3DS path.
- Companion pages: [three large 3DS ports](./case-studies-ports.md), [porting](../porting.md), [performance](../performance.md), [GPU rendering](../PICA200/gpu-rendering.md), [audio](../audio.md).
- Jargon: the **[ARM11](../../glossary.md#arm11)** is the 3DS CPU; the **[PICA200](../../glossary.md#pica200)** is the 3DS GPU; **[VRAM](../../glossary.md#vram)** is the 6 MB of fast GPU memory; the **[linear heap](../../glossary.md#linear-heap)** is contiguous memory for GPU/DSP/DMA buffers. A **[draw call](../../glossary.md#draw-call)** is one request to draw triangles; a **[TEV](../../glossary.md#tev)** stage is one fixed-function combiner step; a **[shader](../../../glossary.md#shader)** is a small GPU program.

## Engine architecture: rooms, meshes, textures

Levels are sector grids grouped into rooms joined by portals. `src/level.h` `[documented]` keeps per-room flags (`water`, `visible`), ambient plus dynamic lights, and a packed room atlas; `prepareRooms` clears `visible` each frame so only portal-reached rooms draw. Entities carry a room index, re-resolved by `getSector` for overlaps — integer grid math, cheap on the **[ARM11](../../glossary.md#arm11)**.

Meshes convert once from engine `::Vertex` to a packed PICA200 `Vertex` (short position/UV, byte normal/colour/light) in `Mesh::update` `[documented]`. Static rooms use `mmAlloc` (VRAM first, linear fallback); skinned entities stream through double-buffered linear memory keyed on `frameIndex`. Textures upload swizzled: CPU-side 8x8 tiling with flip and channel swap, halved while over 1024px, mipmaps box-downsampled on the CPU, VRAM copies staged via flush plus `C3D_SyncTextureCopy` `[documented]`.

## Renderer path and hardware budget

Lighting runs per-vertex; the fragment side stays at one or two **[TEV](../../glossary.md#tev)** stages (texture modulated by primary colour, optional underwater/greyscale chain). Each `Shader::init` binds a precompiled `.v.pica` program (room, entity, sprite, mirror, underwater, ambient, shadow, upscale, GUI) — no runtime compilation `[documented]`. Fog uses the hardware unit: `setFog` caches 3 LUTs and calls `C3D_FogGasMode`/`FogColor`/`FogLutBind`. The room vertex shader (`compose_room.v.pica`) computes three point lights with `1-d2` attenuation dotted against the unpacked byte normal, added to the baked byte light `[documented]`.

Output renders portrait and rotates: 240x400 buffers presented as 400x240 via a `rotate90` viewport path, double-buffered, costing about 562 KB colour plus 192 KB for one aliased depth buffer shared through a single depth group `[documented]`. Stereo is not used. The Makefile builds `armv6k`/`mpcore` hard-float at `-Ofast -ffast-math` (no RTTI/exceptions), linking citro3d/ctru/vorbisidec with picasso-assembled `.shbin.o` blobs `[documented]`.

Audio, input and saves are minimal: one stereo NDSP channel at 44.1 kHz fed by a mixer thread double-buffering 2,352 frames per half with a **[cache](../../../glossary.md#cache)** flush per hand-off; per-frame HID polling (Circle Pad dead zone 0.3, touch toggles the bottom backlight); everything under `sdmc:/3ds/OpenLara/` `[documented]`. **New 3DS:** `APT_CheckNew3DS` enables the higher clock, nothing else changes.

Right for this hardware: cull by rooms first; pack vertices small and light them per-vertex; hold the TEV chain to one or two stages; alias one depth buffer; ship shaders precompiled; mix audio on its own thread. Wrong (`[hypothesis]`): drawing all rooms behind the depth buffer, per-material TEV growth, per-frame float re-uploads of static rooms, or sharing one thread between logic and the NDSP feeder.

## Technical details

- Vertex declaration `[documented]`: `short4` coord, `ubyte4` normal, `short4` texcoord, `ubyte4` colour/light; loaders `GPU_SHORT`/`GPU_UNSIGNED_BYTE`.
- Enforced limits `[documented]`: 8px minimum texture size, mipmap cap 3, max 4 samplers, 3-entry fog LUT, one depth-alias group, `OPT_NEAREST` forced on render-target textures.
- Portrait math `[documented]`: `isRotate90` swaps viewport/scissor axes for the default targets only; offscreen targets use the unrotated path.

## Lessons: symptom to fix rules

| Symptom (Old 3DS) | Fix |
| ----------------- | --- |
| Whole level draws, only 2-3 rooms visible | Walk portals from the camera room; set per-room `visible` like `prepareRooms` |
| Vertex upload dominates | Pack short/byte attributes once; VRAM for static rooms, double-buffered linear for entities |
| Fragment stalls as materials grow | Light per-vertex; cap compose at texture-times-colour plus one tint stage |
| Fog/water variants multiply shaders at runtime | Precompile `_u` variants; select by room water flag; use the fog LUT |
| Depth memory fragments | Alias one depth buffer across targets at the largest size |
| Audio gaps during load/render | Dedicated mixer thread, double buffers, flush before each hand-off |
| Stereo halves fps for little gain | Ship mono on Old 3DS; budget stereo as a separate pass |

All rows `[hypothesis]` generalised from this tree; confirm with [performance](../performance.md) counters.

## Open questions

- What frame rate does a real level hold on Old 3DS hardware, and which stage (portal walk, upload, TEV, transfer) dominates? Needs an on-hardware profile.
- Does the VRAM-first allocator spill large room atlases to linear memory, and does the `GX_RequestDma` copy path hide it?
- **New 3DS:** does the higher clock only lift the floor, or change which pass to optimise first?

## Sources

[XProger/OpenLara](https://github.com/XProger/OpenLara) commit `3b268ca` (2026-07-26): `src/gapi/c3d.h` (renderer, `DISPLAY_WIDTH 240`/`DISPLAY_HEIGHT 400`, depth alias group, fog, `mmAlloc`), `src/platform/3ds/main.cpp` (dead zone 0.3 at line 95, audio buffers, New 3DS check at line 206) and `Makefile` (`-Ofast -ffast-math`), `src/shaders/pica/*.v.pica`, `src/level.h`. The tree states no 3DS performance figures.
