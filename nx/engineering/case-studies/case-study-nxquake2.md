# Case study: nxquake2 Switch port ([documented] tree, [reported] context)

A native Switch port of Yamagi Quake II (enhanced GPLv2 Quake II client on Icculus 3.21) by fgsfds, built with devkitA64, SDL2 and switch-mesa: an NRO plus renderer and `game.so` loaded via libsolder, with retail `baseq2/` assets on SD [documented]. Coffeepot measured none of it; the tree gives no Switch frame figures.

## When is this useful?

Read this for how a Quake II client handles the Switch's applet, display and gyro details around an unmodified GL path, and for its renderer-as-a-cvar approach.

## Before you start

- Source: [fgsfdsfgs/nxquake2](https://github.com/fgsfdsfgs/nxquake2) commit `a8fdda02` (2022-12-19; see [source versions](../source-versions.md)); Coffeepot built and ran none of it.
- Ships `ref_gl3.so` (desktop GL 3.2 on switch-mesa/nouveau) next to the NRO per the install doc [documented] (`doc/990_switch.md`).
- Only upstream performance note: playable on Raspberry Pi with the GL1 renderer, frame rate heavily resolution-dependent [reported] (`CHANGELOG`, no model, clocks, or scene stated).
- Companion pages: [rendering](../rendering.md), [performance and porting](../performance-and-porting.md), [NX GPU](../../hardware/gpu.md).

Jargon: the **[CPU](../../../glossary.md#cpu)** runs game and submission on one thread; the **[GPU](../../../glossary.md#gpu)** draws via switch-mesa into a **[framebuffer](../../../glossary.md#framebuffer)**; the hunk allocator rounds to **[cache](../../../glossary.md#cache)** lines; `r_vsync` waits on **[vblank](../../../glossary.md#vblank)**; the **[Tegra X1](../../glossary.md#tegra-x1)** SoC has **[Maxwell](../../glossary.md#maxwell)** graphics, reached through **[libnx](../../glossary.md#libnx)**; **[title-override](../../glossary.md#title-override)** is the full-memory launch mode.

## What it is

| Layer | Approach |
| ----- | -------- |
| Engine | Classic id split: the client embeds a server for single-player and loads game logic as `game.so` [documented]. |
| Tick model | Fixed 10 Hz server tick (`sv.time` is "always `sv.framenum * 100` msec" [documented] (`server.h:70`)); packet rate (`cl_maxfps`, `cl_async`) decoupled from render rate, `-1` auto-picking a dividing packet rate [documented] (`doc/030_configuration.md`, `common/frame.c`). |
| Renderers | Four libraries behind one cvar: `ref_gl1` (legacy GL 1.4), `ref_gl3` (GL 3.2 shaders), `ref_gles3` (GLES3 variant "for embedded GPUs like the Raspberry PI 4"), `ref_soft` (CPU rasteriser) [documented] (`Makefile`, `CHANGELOG`). Sources shared with desktop; only hunk and backend differ per platform [documented]. |
| Switch GL path | NRO links SDL2 plus `libEGL`/`libglapi`/`libdrm_nouveau`; the `.so` files resolve via libsolder, not real `dlopen` [documented]. |
| Assets | Retail `.pak` archives plus loaders for models, `.pcx`, stb images, `.wal` textures, PVS data; Ogg music in `baseq2/music/` [documented] (`Makefile`, `doc/990_switch.md`). |

## How it uses the CPU and GPU

| Aspect | Detail |
| ------ | ------ |
| Threading | Single-threaded: tick, prediction and GL submission share one loop; no render workers, no affinity calls; audio rides its SDL/OpenAL callback [documented]. Cost lands on one Cortex-A57 core by construction [documented mechanism; Switch cost unmeasured]. |
| SIMD | None — no NEON intrinsics; on AArch64 the spin path just emits `yield` [documented] (`common/frame.c:94`). |
| Memory | Hunk allocator; the Switch backend uses plain `calloc`/`free` instead of the desktop `mmap`/`mremap` reservation [documented] (`backends/nx/shared/hunk.c`). No memory-mode handling — see Open questions. |
| Resolution | Always fullscreen; SDL always reports 1920×1080, so size comes from `appletGetOperationMode`: 1920×1080 docked, 1280×720 handheld [documented] (`vid/glimp_sdl.c`). |
| Swap/AA | `r_vsync` (alias `gl_swapinterval`); `r_msaa_samples` with halving-and-retry on context failure; no dynamic resolution [documented] (`common/cvar.c:82`, `glimp_sdl.c`). |
| Submission | Desktop GL3 call stream (lightmaps, per-surface draws, `gl3_shaders.c`) straight at nouveau; no Switch batching or uniform rework [documented structure; cost unmeasured]. |

## What it does well

| Technique | Why it helps on Switch |
| --------- | ---------------------- |
| Mature visibility | PVS culling plus baked lightmaps keep the draw list small [documented architecture; timing unmeasured]. |
| Renderer modularity | Four `ref_*` libraries behind one cvar; GL3/GLES3/GL1/software A/B testable on-console untouched [documented]. |
| Portable saves | `is_portable` forced true: configs, saves, `crash.log`/`fatal.log` sit next to the NRO on SD [documented] (`backends/unix/main.c:124`). |
| Applet citizenship | `NoSuspend` focus hook auto-pauses single-player; single-player skips server timeouts across sleep [documented] (`nx/system.c`, `sv_main.c:287`). |
| Input completeness | SDL pads plus hand-polled libnx sixaxis gyro (switch-sdl2 2.0.14 has no gyro API) with calibration cvars, swkbd keyboard, Switch default binds [documented]. |

## What it does poorly (costs and limits)

| Cost / trap | Detail |
| ----------- | ------ |
| Unmodified submission | Desktop per-surface state changes straight at nouveau, no batching for Maxwell [documented structure; cost unmeasured]. |
| Single-threaded | One A57 runs tick, client, prediction and GL; a heavy scene stalls everything [documented]. |
| Full-res by default | Docked 1920×1080 through the heaviest (GL3) renderer out of the box, no auto-scaler [documented]. |
| Stock audio | SDL `switch` driver or static OpenAL at upstream buffer defaults; no Switch latency tuning found [documented]. |

## Lessons: what to copy first

| # | Symptom → fix |
| - | ------------- |
| 1 | Cannot separate GPU-bound from driver-bound on first boot — ship the renderer as a cvar-selected module so each path is measurable on-console [documented]. |
| 2 | SDL reports 1920×1080 even handheld — probe `appletGetOperationMode` and force fullscreen [documented]. |
| 3 | Sleep kills timeout state — `NoSuspend` focus hook plus auto-pause; skip single-player timeouts across sleep [documented]. |
| 4 | Swap/sleep hiccups cascade into gameplay — decouple packet rate from render rate (`cl_async`/`cl_maxfps -1`) [documented]. |
| 5 | `mmap` reservation does not port — keep a plain-`calloc` hunk backend and size the set for applet memory unless you require title-override [documented]. |
| 6 | Vendored SDL predates sensor APIs — poll `hidGetSixAxisSensorStates` directly with a calibration pause and archived cvars [documented]. |

The lessons are inferences from the mechanisms the code shows. A `[documented]` tag cites the code that shows the mechanism, not a measurement that the fix works; none was tested on a console by Coffeepot.

A new homebrew FPS should copy the architecture, not the call stream: baked PVS + lightmaps, fixed-tick prediction, reloadable renderer, portable saves with crash logs, focus-hook pausing — then its own batched Maxwell path.

## Open questions

- No Switch frame figures: what does `ref_gl3` hold handheld vs docked at stock clocks; would `ref_gles3` beat it?
- Applet vs title-override: nothing requests a memory mode, so it is unknown whether the game fits applet limits. Plain applet launch would settle it.
- SDL `switch`-driver latency vs static OpenAL is unmeasured; gyro calibration persistence past HEAD is unread.

## Sources

[fgsfdsfgs/nxquake2](https://github.com/fgsfdsfgs/nxquake2) at `a8fdda02`: `doc/990_switch.md`; `CMakeLists.txt`; `Makefile`; `src/backends/nx/system.c` + `nx/shared/hunk.c`; `src/backends/unix/main.c` + `system.c`; `src/client/vid/glimp_sdl.c`; `src/client/input/sdl.c` + `menu/menu.c`; `src/client/sound/sdl.c`; `src/common/frame.c` + `src/server/`; `doc/030_configuration.md`, `doc/040_cvarlist.md`, `CHANGELOG`.
