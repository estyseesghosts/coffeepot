# Case study: what GTA III (re3) would need on a 3DS (source read, no port code)

GTA III from fully reversed source (GTAmodding's re3). The repository this page was written from, [wabisuke-dev/re3onN3ds](https://github.com/wabisuke-dev/re3onN3ds) at commit `3233ffe1` (2021-09-03), is named for the 3DS but its saved checkout contains **no 3DS code**: it is upstream re3 at a merge commit, its README is re3's, and a search for libctru, citro3d or `__3DS__` finds nothing. So this page does not describe a 3DS port. It reads re3's streaming and configuration code and asks what a port would have to cut. Every fit claim is a `[hypothesis]`. The New 3DS (804 MHz, extra cores, up to 124 MB for games) is the realistic target, with the Old 3DS as the limit.

## Before you start

- Engine facts are `[documented]` in the named files. All 3DS performance and fit claims are `[hypothesis]`. Coffeepot ran nothing on hardware.
- Old 3DS is the default: **ARM11** at 268 MHz on two cores, ~64 MB for the game, no level-2 cache. **New 3DS:** to 804 MHz, four cores, to 124 MB, plus 2 MB level-2 cache ([CTR CPU](../../hardware/cpu.md), [CTR memory](../../hardware/memory.md)). The saved re3 checkout adds none of this: it has no 3DS code to read.
- Companion pages: [porting](../porting.md), [performance](../performance.md), [memory and cache](../memory-and-cache.md), [threading](../threading.md), [audio](../audio.md).

Jargon: the **[ARM11](../../glossary.md#arm11)** is the 3DS CPU; **[FCRAM](../../glossary.md#fcram)** is main memory (128 MB; **New 3DS:** 256 MB); **[VRAM](../../glossary.md#vram)** is 6 MB of on-chip GPU memory; **[PICA200](../../glossary.md#pica200)** is the 3DS GPU; a **[draw call](../../glossary.md#draw-call)** is one request to draw triangles with the current state.

## What makes it tick: the streaming core

GTA III never loads Liberty City whole. `src/core/Streaming.h` shows the design `[documented]`: a table of entries (one per model plus texture dictionaries) with load states (`NOTLOADED` / `LOADED` / `INQUEUE` / `READING` / `STARTED`), flags (`DONT_REMOVE`, `SCRIPTOWNED`, `DEPENDENCY`, `PRIORITY`) and a position and size on disc. Channels queue a few reads at a time so sectors page in ahead of the camera and out behind it. The README lists "No loading screens between islands (\"map memory usage\" in menu)" as an option `[documented]` (`README.md` line 52); keeping more city resident is something a small-memory port may not be able to afford `[hypothesis]`. `src/core/config.h` pins the working set `[documented]`: 5,500 model-info slots (`MODELINFOSIZE`, 3,150 on PS2), 140 peds, 110 vehicles with 50 loaded at once (`MAXVEHICLESLOADED`, 70 on mobile), 1,250 map objects (`NUM_MAPOBJECTS`), 4,930 path nodes.

## The PICA200 path: RenderWare has to go

Rendering is original RenderWare (Direct3D 8) or the reimplementation librw (D3D9, OpenGL 2.1+, OpenGL ES 2.0+) `[documented]` (README). None runs on a 3DS, so a port needs a new backend against citro3d: per-entity state changes become PICA200 **[command lists](../../glossary.md#command-list)**, lighting becomes fixed-function plus the six-stage **[TEV](../../glossary.md#tev)** combiner. The city draws as thousands of small per-entity draws, so batching static geometry into fewer, larger submissions is the first GPU-side win `[hypothesis]`; see [GPU rendering](../PICA200/gpu-rendering.md).

## ARM11 use: logic, streaming and audio on four cores

City logic (peds, traffic, pathfinding), streaming and audio mixing all want CPU. On Old 3DS the game gets the appcore plus a syscore share at 268 MHz with no level-2 cache, and pointer-chasing list code stalls on main memory `[hypothesis]`. **New 3DS:** logic on the appcore, streaming and file I/O on a second core, audio on a third, present overlapped on a fourth; see [threading](../threading.md).

## Streaming and draw distance on FCRAM budgets

Sectors, textures, collision, path nodes, audio and framebuffers share the application region: ~64 MB on Old 3DS, up to 124 MB on New 3DS `[documented]` ([CTR memory](../../hardware/memory.md)). Since re3's caps were tuned for much bigger machines, a port budgets downwards: fewer resident dictionaries and vehicles, a shorter streaming radius, and a draw distance (far plane plus fog) tied to that radius `[hypothesis]`. The original's sequential IMG archives suit the **[SD card](../../../glossary.md#sd-card)** better than scattered files; keep reads sequential and asynchronous through the `READING`/`STARTED` states. Radio and effects need their own small preload budget, not the model streamer's.

## Audio and saves

Audio mixes through Miles or OpenAL (`sampman_miles.cpp`, `sampman_oal.cpp`) `[documented]` — a 3DS port swaps one for software mixing on an ARM11 core into the **[DSP](../../../glossary.md#dsp)**; see [audio](../audio.md). re3 keeps user files in the game directory; on 3DS those become save data plus SD storage `[hypothesis]`.

## Technical details

- `Streaming.h`: channel struct holds 4 stream IDs with offsets, state, position, size, retries, status `[documented]`.
- `config.h`: `MODELINFOSIZE` 5500 (PS2: 3150), `MAXVEHICLESLOADED` 50 (mobile: 70), `NUMPEDS` 140, `NUMVEHICLES` 110, `NUM_PATHNODES` 4930, `NUM_MAPOBJECTS` 1250 `[documented]`.
- 3DS reference: ARM11 268 MHz, **New 3DS:** to 804 MHz via `osSetSpeedupEnable` behind a model check; ~64/124 MB for games; VRAM 6 MB; L2 **New 3DS** only `[documented]`.

## Lesson table

All rows are `[hypothesis]`.

| Symptom | Likely cause | Fix to try first |
| ------- | ------------ | ---------------- |
| Hitches when driving fast | Sectors page in too slowly | Shrink radius; mark the corridor ahead `PRIORITY` |
| Memory grows after island changes | Textures, models accumulate | Cut caps below defaults; evict by distance |
| Pop-in at the horizon | Draw distance exceeds streamed set | Tie far plane and fog to streaming radius |
| Spikes with crowds, traffic | Thousands of tiny draws | Batch static geometry; cut redundant passes |
| Audio stutters loading sectors | Mixer shares core, file reads | Own core for mixing; preload radio separately |
| Stalls entering a new area | Random small SD reads | Sequential archives; async reads overlapped |

## Open questions

- No 3DS code, frame-rate, memory or streaming figures exist in the saved checkout; every fit claim needs a `[measured]` New 3DS result from an actual port.
- Does the city at a playable draw distance fit 124 MB with audio and framebuffers? A per-island residency census would settle it.
- Is the bottleneck CPU state translation or PICA200 fill? See [frame pacing](../PICA200/frame-pacing.md).

## Sources

- [wabisuke-dev/re3onN3ds](https://github.com/wabisuke-dev/re3onN3ds) commit `3233ffe1` (2021-09-03; see [source versions](../source-versions.md)): `README.md`, `src/core/Streaming.h`, `src/core/config.h`, `src/audio/` — all upstream re3 code. [GTAmodding/re3](https://github.com/GTAmodding/re3) is the upstream project.
- [CTR CPU](../../hardware/cpu.md), [CTR memory](../../hardware/memory.md), [threading](../threading.md), [GPU rendering](../PICA200/gpu-rendering.md).
