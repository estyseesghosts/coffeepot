# Case study: Quake2CTR (Quake 2 port)

A Nintendo 3DS port of Quake 2 based on q2dos by MasterFeizz. It renders through the GPU via picaGL and keeps the full client/server split with a `ctr/` backend per subsystem. Nothing below was measured by Coffeepot; all behaviour is `[reported]` by the port README or `[documented]` in `ctr/` sources.

## Before you start

- Evidence: all behaviour is `[reported]` by the [port Readme](https://github.com/MasterFeizz/Quake2CTR) or `[documented]` in a named file. No timing figure is stated in the repo.
- Build: `-march=armv6k -mtune=mpcore -O2`, hard-linked game/refresh, `-lpicaGL -lvorbisidec -logg` (`[documented]`).
- Old 3DS is the default model. `osSetSpeedupEnable(true)` requests the higher clock where allowed; C-Stick-only bindings are the explicit model split.
- Companion pages: [porting](../porting.md), [performance](../performance.md), [GPU rendering](../PICA200/gpu-rendering.md), [threading](../threading.md), [audio](../audio.md), [local wireless](../local-wireless.md).

Jargon used here: the **[ARM11](../../glossary.md#arm11)** is the 3DS CPU; the **[PICA200](../../glossary.md#pica200)** is the 3DS GPU; main memory is **[FCRAM](../../glossary.md#fcram)**; **[VRAM](../../glossary.md#vram)** is the 6 MB of fast GPU memory; physically contiguous GPU-readable memory is the **[linear heap](../../glossary.md#linear-heap)**; a **[draw call](../../glossary.md#draw-call)** is one request to the GPU; sound goes through the **[DSP](../../../glossary.md#dsp)**.

## What makes it tick

**Engine architecture.** Stock Quake 2: `qcommon` core, `server` world, `client` prediction/view/sound, and a `game` module with ~30 monster files — all compiled in per the `Makefile`. Hard-linking removes DLL loading: `GetRefAPI`/`GetGameAPI` link statically (`[documented]`).

**Two renderer paths, one shipped.** The tree carries `ref_gl` (OpenGL via picaGL, presented by `pglSwapBuffersEx`) and `ref_soft` (8-bit CPU rasteriser). The shipped menu offers only picaGL and every mode resolves to 400x240 (`[documented]`). The threaded 8-bit blit (`ctr/swimp_ctr.c`) is kept code, not the shipping path.

**Level/mesh pipeline.** `ref_gl` keeps the original pipeline: BSP walk and PVS culling, lightmapped surfaces, alias meshes — fed as `qgl*` calls translated by picaGL. No mesh precompilation exists; the `r_novis`/`r_nocull` cvars are the culling lever.

**Network kept.** `ctr/net_ctr.c` is a full UDP backend: loopback buffers for single player, non-blocking sockets, open/close per multiplayer flag. IPX is stubbed. `NET_Init` allocates a 1 MB SOC buffer for `socInit` (`[documented]`).

**Assets.** Demo in ROM; full game needs retail `baseq2/` at `sdmc:/3ds/quake2/`, CD audio as OGGs in `baseq2/music/` (`[reported]`). `CDAudio_*` are stubs — music bypasses the CD path.

## How it pushes 3DS hardware

**Resolution discipline.** 400x240 matches the top screen, so no scaler runs (`[documented]`). `main()` widens the display on non-2DS models and sizes picaGL heaps `pglInitEx(0x040000, 0x100000)` (`[documented]`).

**ARM11 vs PICA200 split.** Transform, lighting setup, and BSP traversal stay on the ARM11; rasterisation, texturing, fog, and stencil shadows go to the PICA200. Unused fixed-function entries (accum, bitmaps, evaluators) are commented out of the binding table rather than emulated (`[documented]` in `ctr/qgl_ctr.c`). Shadows are a menu toggle — the first knob when fill-bound.

**Memory budget.** `ctr/heap_ctr.c` splits remaining memory ~5:1 application-to-linear and caps linear at 24 MB (`[documented]`). The hunk allocator then takes one big `malloc` — simple but fragmentation-prone. Main-thread stack is 3 MB.

**Fill and frame pacing.** The main loop spins on `Qcommon_Frame` with 1 ms granularity and no frame skip; each `GLimp_EndFrame` swaps via picaGL. Stereo 3D is not wired up, so no second-eye cost exists.

**Audio.** `ctr/snddma_ctr.c` drives ndsp directly: 44.1 kHz stereo 16-bit, 4096-sample looping buffer in linear memory, cache flushed on submit (`[documented]`). The rate cvar is forced, skipping resampler cost.

**Input.** Circle Pad moves/turns, touch drives a bottom-screen keyboard overlay, D-pad and triggers cover the rest (`[documented]` in `ctr/in_ctr.c`, `ctr/touch_ctr.c`). C-Stick camera help exists where present (**New 3DS:** C-Stick bindings; Old 3DS has no stick).

## What it got right and wrong

Right: hard-linking killed DLL churn; one 400x240 mode removed scaler bugs; the heap split plus SOC buffer makes memory explicit; stubbing CD audio avoided a half-port. Wrong: the unshipped software renderer still ships code with no menu entry; `Sys_Sleep` is an empty stub so the loop busy-waits; `Sys_Error` needs Start with no log file.

## Technical details

- Sources read: `Readme.md`, `Makefile`, `ctr/` backends (`sys`, `vid`, `glimp`, `qgl`, `snddma`, `net`, `heap`, `in`, `touch`, `swimp`, `cd`), `ref_gl/`.
- Qcommon/server/game are stock id logic plus glue; `r_novis`, `r_nocull`, `gl_shadows`, `gl_picmip` are the tuning surface.

## Lesson table: Quake-engine port rules

Each rule is a `[hypothesis]` generalised from this port until measured on your workload.

| # | Symptom on Old 3DS | Fix |
| - | ------------------ | --- |
| 1 | Mode switches or scaler tears on change | Fix one 400x240 mode; delete other entries |
| 2 | Fill-bound scenes with shadows on | Default `gl_shadows` off; re-enable after a timed pass |
| 3 | Long-session allocation failure in one hunk | Split app vs linear heaps with a cap; log high-water marks |
| 4 | Audio crackle after porting a DMA backend | One fixed ndsp rate, looping linear buffer, flush on submit |
| 5 | Multiplayer silent or crashing on boot | Allocate the SOC buffer before any socket call; stub IPX |
| 6 | Unplayable without a keyboard for menus | Ship a bottom-screen touch keyboard plus touchpad-look |

## Open questions

- What is the Old 3DS frame time per scene (BSP vs alias-heavy)? No figure exists; `r_speeds 1` plus a tick log would settle it.
- Does `osSetSpeedupEnable(true)` change Old 3DS results? Unconditional call; a paired run is unrecorded.

## Sources

- [Quake2CTR Readme](https://github.com/MasterFeizz/Quake2CTR) — keybinds, baseq2/music install (read November 2026).
- Port tree `ctr/` (`sys_ctr.c`, `vid_ctr.c`, `glimp_ctr.c`, `qgl_ctr.c`, `snddma_ctr.c`, `net_ctr.c`, `heap_ctr.c`, `in_ctr.c`, `touch_ctr.c`, `swimp_ctr.c`, `cd_ctr.c`), `Makefile`, `ref_gl/` — backend choice, memory, audio, network, input.
