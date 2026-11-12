# Case study: Serious Engine Switch port (inferred from source)

A homebrew port of Serious Engine 1.10 (Serious Sam: The Second Encounter) to Switch via devkitA64, SDL2, and OpenGL on Nouveau/Mesa. This page summarises what the port's own tree says about the engine and what that implies on Switch hardware. Tree facts are `[documented]`; the "implication" columns and lessons are the author's inferences from engine structure, labelled `[hypothesis]`, not reported measurements. The port's tree states no frame rates. Coffeepot measured nothing here.

## When is this useful?

Read this if your engine is portal- or sector-culled and runs through OpenGL on switch-mesa; its claims are inferences, not results.

## Before you start

- Evidence: source is [fgsfdsfgs/Serious-Engine](https://github.com/fgsfdsfgs/Serious-Engine) commit `13c4a1a` (2020-05-12; see [source versions](../source-versions.md)). `[documented]` means readable in that tree (`Sources/build-switch.sh`, `Sources/CMakeLists.txt`, `Sources/Engine/`, `Sources/GameMP/`, `Sources/EntitiesMP/`). `[hypothesis]` means an inference from the engine's structure; no measurement exists in the tree.
- Build: host-built `ecc` first, then `./build-switch.sh` (`-DTFE=TRUE` for First Encounter); NRO lands in `Sources/switch-build/` [documented]. Deps include `switch-mesa switch-libdrm_nouveau switch-sdl2 switch-zlib libnx` [documented].
- Renderer: `Gfx_OpenGL.cpp` on `switch-mesa` via `Gfx_wrapper.cpp` and `SDL/SDLOpenGL.cpp`; no deko3d/NVN path in-tree [documented].
- Companion pages: [rendering](../rendering.md), [performance and porting](../performance-and-porting.md), [NX GPU](../../hardware/gpu.md).

Jargon used here: the **[CPU](../../../glossary.md#cpu)** ticks entities and walks visibility; the **[GPU](../../../glossary.md#gpu)** draws the visible set into a **[framebuffer](../../../glossary.md#framebuffer)**; a **[cache](../../../glossary.md#cache)** is fast memory beside each core; **[Tegra X1](../../glossary.md#tegra-x1)** is the Switch SoC; **[Maxwell](../../glossary.md#maxwell)** is its GPU; **[libnx](../../glossary.md#libnx)** is the homebrew library; **[title-override](../../glossary.md#title-override)** gives full game memory.

## What it is

| Layer | Approach |
| ----- | -------- |
| Worlds | Brush/CSG static geometry: `Engine/Brushes/` sectors and polygons compiled into `Engine/World/` with collision grids and ray casting [documented]. |
| Visibility | Sector/portal walk (`Rendering/Render.cpp`, `RenderBrushes.cpp`, `RendClip.cpp`, `RenCache.cpp`) fixes the visible set before drawing [documented]. |
| Entities | `.es` scripts compiled by host `ecc` to C++, statically linked via `FakeExports.cpp` (no dynamic loader on Switch) [documented]. |
| Surfaces | Texture/lightmap pipeline: `Graphics/Texture.cpp`, `TextureEffects.cpp`, `Light/LayerMaker.cpp`, `Light/LayerMixer.cpp`, `ShadowMap.cpp` [documented]. |
| Platform | `PLATFORM_SWITCH` selects Unix filesystem/input paths, SDL input/events, touch keyboard; x86 ASM off (`USE_PORTABLE_C`), `-mcpu=cortex-a57` [documented]. |
| Input/audio/saves | SDL2 input/events with Switch cases, OGG Vorbis audio, saves via Unix filesystem layer plus shell variables [documented]. |

## How it meets the Cortex-A57

| Load | Implication |
| ---- | ----------- |
| Entity ticks: hundreds of enemies, projectiles, particles tick per frame from generated state machines [documented]. | Pointer-chasing bursts stress the Cortex-A57's caches and memory latency (the A57 is an out-of-order core, but dependent loads still wait on memory); arena fights are the CPU spike [hypothesis]. |
| Visibility traversal: portal walk plus collision-grid queries prune before drawing [documented]. | Cheap next to fill, but deep open views stay branchy and serial [hypothesis]. |
| Threading: Unix synchronisation with optional `SINGLE_THREADED`; largely serial pipeline [documented]. | Few cores stay busy; keep game tick and render prep apart rather than over-threading [hypothesis]. |
| Portable C: x86 ASM paths dropped for `-O3` portable C [documented]. | Correct but slower than the 2001 fast paths; A57 tuning only partly closes it [hypothesis]. |

## How it meets the Maxwell GPU

| Load | Implication |
| ---- | ----------- |
| Open scenes draw far once portals open [documented]. | Culling is the gift to a weak GPU: unseen sectors cost no fill [hypothesis]. |
| Layered translucency and particles stack overdraw [documented]. | Fill-heavy docked; resolution and effect density are the first dials [hypothesis]. |
| Textures/lightmaps stream from GRO archives [documented]. | SD faults hitch; keep the working set resident via title-override memory [hypothesis]. |
| SDL swap chain, no dynamic-resolution logic in-tree [documented]. | Missed vsync halves rate; fixed low handheld resolution is the safe fallback [hypothesis]. |
| Frequent texture/blend/fog switches per batch [documented]. | Mesa absorbs this poorly; sort opaque front-to-back, batch by state [hypothesis]. |

## What it gets right

| Strength | Effect on Tegra X1 |
| -------- | ------------------ |
| Portal/sector culling | Trades cheap traversal for expensive fill [hypothesis]. |
| Static brushes, precomputed collision/shadow layers | No per-frame world rebuilds [documented]. |
| Shared shader/texture-effect library | Amortises surface variety instead of per-polygon state [documented]. |
| Single static NRO | No runtime plugin loading where `dlopen` is absent [documented]. |

## Constraints, observed problems and untested hypotheses

Rows tagged `[documented]` are read from the source; `[reported]` rows are the authors' own statements; `[hypothesis]` rows are inferred from the structure and were not measured.

| Cost / trap | Detail |
| ----------- | ------ |
| Entity/physics spikes | Mass encounters tick hundreds of entities plus collision in one frame [hypothesis]. |
| State churn on old GL | Per-surface switches multiply Mesa overhead; unsorted transparents worsen it [hypothesis]. |
| Streaming on SD | GRO/level loads via Unix file I/O stall on slow cards [hypothesis]. |
| No dynamic scaling | No in-tree scaler or shader prewarm; docked fill overload lands on the user [documented]. |

## Lesson: what to copy first

| # | Symptom to fix |
| - | -------------- |
| 1 | Arena tick spikes — cap tickers by distance, stagger AI across frames, pool projectiles [hypothesis]. |
| 2 | Costly traversal, idle GPU — tighten portals and sector splits; a smaller visible set beats a faster culler [hypothesis]. |
| 3 | Fill-bound docked frames — ship handheld-first resolution, sort opaque front-to-back, gate translucency [hypothesis]. |
| 4 | Driver overhead per draw — sort by state, merge static brush runs, keep render-cache hits high [hypothesis]. |
| 5 | SD hitches — preload the working set under title-override memory, prefetch GRO blocks, never fault textures from the render path [hypothesis]. |
| 6 | First-view stutter — warm the visible set with a load-time camera sweep, hold vsync so one slow frame does not cascade [hypothesis]. |

The lessons are inferences, not results; each needs a console profile before it is adopted.

## Open questions

- Handheld/docked resolution, clocks, and fps per level are unstated in-tree; only console profiling settles them.
- Mesa/Nouveau batch and vsync costs for this GL stream are unquantified.
- GRO and save placement (SD versus NAND) for the NRO build is unpinned in the docs.

## Sources

[fgsfdsfgs/Serious-Engine](https://github.com/fgsfdsfgs/Serious-Engine) commit `13c4a1a`:

- `README.md` — contents, Switch pacman deps, build flow, NRO output.
- `Sources/build-switch.sh`, `Sources/CMakeLists.txt` — host `ecc`, `SWITCH_LIBNX` flags, `-mcpu=cortex-a57`, static link, `elf2nro` packaging.
- `Sources/Engine/{World,Brushes,Rendering}/` — `World.cpp`, `WorldCollision*.cpp`, `Brush*.cpp`, `Render*.cpp`, `RenCache.cpp`.
- `Sources/Engine/{Graphics,Light,Sound}/`, `Base/{SDL,Unix,NX}/`, `GameMP/`, `EntitiesMP/` — GL path, textures, mixer, Switch input/filesystem cases, `.es` entities.
