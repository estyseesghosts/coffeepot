# Case study: dhewm3 Switch port (Doom 3 stencil-forward on Tegra X1)

A native Switch homebrew port of dhewm3 (the GPL Doom 3 source port) by fgsfds: the full id Tech 4 engine built to an NRO with `Makefile.nx`, drawing through switch-mesa OpenGL. This page summarises what the vendored tree's docs and code say; no scene was benchmarked, so there are no fps figures. It ends with what to copy first for stencil-shadow forward renderers on a Tegra X1 homebrew build.

## Before you start

- Evidence: tree facts are `[documented]` (file named); unmeasured costs are `[hypothesis]`. Coffeepot ran no benchmark or build.
- Tree: `neo/` engine, `base/` retail data (patch 1.3.1); Switch glue in `neo/sys/nx/`; build in `neo/Makefile.nx` (0.2.0; `dhewm3nx` and `d3xp` targets). Install goes to `/switch/dhewm3/`; nxlink stdio only under `DEBUG`/`NXLINK_DEBUG` [documented] (`main.cpp`, `README.md`).
- Companion pages: [rendering](../rendering.md), [performance and porting](../performance-and-porting.md), [NX GPU](../../hardware/gpu.md).

Jargon used here: the **[CPU](../../../glossary.md#cpu)** runs game logic and builds shadow geometry; the **[GPU](../../../glossary.md#gpu)** draws; a **[framebuffer](../../../glossary.md#framebuffer)** is the pixel target; a **[cache](../../../glossary.md#cache)** is fast memory beside the processor; a **[shader](../../../glossary.md#shader)** is a small GPU program; **[Tegra X1](../../glossary.md#tegra-x1)** is the Switch chip, whose GPU is **[Maxwell](../../glossary.md#maxwell)**; **[libnx](../../glossary.md#libnx)** is the homebrew system library; **[title override](../../glossary.md#title-override)** launches via the full-memory game slot.

## What it is

| Layer | Approach |
| ----- | -------- |
| Engine | Full id Tech 4: game logic (`game/` or `d3xp/`), collision (`cm/`), map/AAS compilers in-tree; runtime AAS serves AI pathing only [documented] (`neo/Makefile.nx`). |
| Lighting | Unified per-light interactions: each light-surface pair gets an ambient+diffuse+specular pass [documented] (`neo/renderer/draw_arb2.cpp`). |
| Shadows | CPU-built stencil volumes (`tr_stencilshadow.cpp`, `tr_turboshadow.cpp`, bounds in `tr_shadowbounds.cpp`); GPL tree uses the depth-fail path, same visuals [documented] (`README.md`). |
| Surfaces/assets | GUI surfaces, render-to-texture subviews, particles as ordinary draws; retail `.pk4` data with the precompiled `interaction.vfp` program pair [documented] (`neo/renderer/`). |
| Platform glue | SDL2 window/input, GL 4.3 compat context asking 24-bit depth + 8-bit stencil with a fallback ladder [documented] (`neo/sys/glimp.cpp`); fgsfds OpenAL-soft fork for audio [documented] (`README.md`). |

## On the cores and the GPU

| Behaviour | Detail |
| --------- | ------ |
| Threads | One `common->Frame()` pump in `appletMainLoop()`; no separate frontend/backend render thread in the Switch glue [documented] (`neo/sys/nx/main.cpp`). |
| Shadow CPU cost | Silhouette/cap emission per lit occluder each frame — the classic A57 hotspot `[hypothesis]` [documented] (`tr_stencilshadow.cpp`). `Simd` ships SSE/AltiVec paths plus generic fallback, no NEON path [documented] (`neo/Makefile.nx`); flags `-mcpu=cortex-a57 -O3 -ffast-math`. |
| Draw pattern | One stencil pass per light plus one interaction pass per pair: draws scale with lights × surfaces, with per-pair state churn [documented] (`draw_arb2.cpp`, `draw_common.cpp`). |
| Fill cost | Stencil writes, stencil-tested fills, then lit fills re-touch the same pixels; cost is fill-proportional, so window size is the biggest lever (port skips Switch fullscreen) [documented] (`neo/sys/glimp.cpp`) `[hypothesis]`. |
| Memory/swap | `Sys_GetSystemRam` reads the real grant via `svcGetInfo` (≈304 MB applet fallback); saves sit beside data in `BUILD_DATADIR` [documented] (`main.cpp`). Swap via `r_swapInterval`, warn-and-continue [documented] (`glimp.cpp`). |
| Relief valve | `nx_overclock` cvar (0–3) sets CPU/GPU/EMC triples up to 1.785 GHz / 768 MHz / 1.6 GHz, restored on exit [documented] (`nx_main.cpp`). |

## What it does well

| Technique | Why it helps on Tegra X1 |
| --------- | ------------------------ |
| Stock GL maps to switch-mesa | No shim or translator: `draw_arb2` issues plain GL on a 4.3 context (`glapi`/`drm_nouveau`) [documented] (`glimp.cpp`, `Makefile.nx`). |
| Scales by resolution | Fill-proportional cost: one window-size change trades pixels for time, no asset work `[hypothesis]`. |
| Precompiled interactions | `interaction.vfp` pair avoids runtime compiles in the hot loop [documented] (`draw_arb2.cpp`). |

## What it does poorly (costs and traps)

| Cost / trap | Detail |
| ----------- | ------ |
| Interaction count | Every light × surface is a draw pair, unbatched and unclustered `[hypothesis]` [documented] (`draw_arb2.cpp`). |
| Shadow double tax | Volumes cost A57 build time plus Maxwell stamp fill, then the lit pass re-touches the pixels `[hypothesis]`. |
| Applet memory | Without title override the ~300 MB grant shares code/heap/images/sound cache; big bump pages evict and hitch on SD reads [documented] (`main.cpp`) `[hypothesis]`. |

## Lesson: what to copy first

| # | Symptom to fix |
| - | -------------- |
| 1 | Draws scale with lights × surfaces — cap per-surface lights, cull by shadow-bounds first [documented] (`tr_shadowbounds.cpp`). |
| 2 | Shadow CPU dominates — cache static volumes, skip off-screen casters, prefer turboshadow for closed meshes [documented] (`tr_turboshadow.cpp`). |
| 3 | Stencil fill saturates the bus — cut window size first, never stencil bits: the renderer needs 8-bit stencil [documented] (`glimp.cpp`). |
| 4 | Generic math on A57 — add a NEON `Simd` path; `-mcpu=cortex-a57` does not fix silhouette algorithms [documented] (`Makefile.nx`). |
| 5 | Applet hitching on pk4 streaming — require title override, compress bump pages, stream sound via the OpenAL cache [documented] (`main.cpp`, `README.md`). |
| 6 | Overclock as the only lever — menu-expose the `nx_overclock` triple with restore-on-exit, log which clock moved the bottleneck [documented] (`nx_main.cpp`). |

## Open questions

- No fps, draw-count, or resolution-vs-time figures exist in-tree; all costs above are mechanism-based.
- Whether switch-mesa depth-fail stencil runs at full rate on Maxwell is unconfirmed in-tree.
- Shipped window size, `BUILD_DATADIR` and `r_swapInterval` defaults live in packaging outside `neo/`.

## Sources

All in `private_please_gitignore/switch/dhewm3/`:

- `README.md` — dhewm3 scope, Switch prerequisites, `make -f Makefile.nx`, stencil depth-fail note.
- `neo/Makefile.nx` — targets, A57 flags, renderer/idlib/game lists, `D3XP` variant.
- `neo/renderer/draw_arb2.cpp`, `draw_common.cpp` — interactions, `interaction.vfp`.
- `neo/renderer/tr_stencilshadow.cpp`, `tr_turboshadow.cpp`, `tr_shadowbounds.cpp` — volumes, bounds.
- `neo/sys/glimp.cpp` — SDL window, GL 4.3 context, depth/stencil ladder, swap interval.
- `neo/sys/nx/nx_main.cpp`, `main.cpp`, `nx_events.cpp` — overclock, paths/RAM/pump, SDL input.
