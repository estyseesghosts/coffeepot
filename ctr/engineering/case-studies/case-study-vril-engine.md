# Case study: Vril engine (Nazi Zombies: Portable on the 3DS)

Vril is the engine behind *Nazi Zombies: Portable* (NZ:P), a Call of Duty Zombies remake by the NZ:P Team. It descends from Quake source ports (dQuakePlus, ctrQuake, Quakespasm), reworked for multi-platform support, performance, and NZ:P-specific protocol and gameplay changes (`README.md`). Targets include Linux, PSP, PS2, Vita, Switch, Xbox and the 3DS (`Makefile.*`). This page covers only the CTR backend (`source/platform/ctr/`). All facts are `[documented]` in that tree ([nzp-team/vril-engine](https://github.com/nzp-team/vril-engine) commit `9a26605`, 2026-11-08; see [source versions](../source-versions.md)); nothing was built or run. Its theme is graceful scaling: detect the console once at boot, then size memory, screen mode and controls to match.

## Before you start

- Evidence: every behaviour is `[documented]` in the named file. No `[measured]` figures exist on this page.
- Model: Old 3DS is the default; New 3DS differences are flagged.
- Build: `Makefile.ctr` (devkitARM, `-march=armv6k -mtune=mpcore -mfloat-abi=hard -O3`), picaGL rendering, libmpg123 music, working directory `sdmc:/3ds/nzportable`.

Jargon: the **[ARM11](../../glossary.md#arm11)** is the 3DS CPU; the **[PICA200](../../glossary.md#pica200)** is the 3DS GPU; **[linear heap](../../glossary.md#linear-heap)** is physically contiguous memory for GPU/DSP buffers. The `hunk` is the engine's single large zone allocation for maps and assets.

## Architecture: Quake client/server, one heap, picaGL output

Vril keeps the Quake shape — client, server, QuakeC VM (`source/qcvm/`), menus, tests — in one `nzportable` binary. Startup (`sys_ctr.c`) calls `APT_CheckNew3DS` once, then sizes the hunk: 20 MB on Old 3DS, 64 MB on New 3DS, and calls `osSetSpeedupEnable(true)` (line 440). **New 3DS:** the 64 MB hunk and speedup request apply only there.

Rendering (`gl/`) goes through picaGL: `pglInitEx(0x040000, 0x100000)` reserves command and scratch space, the viewport is 400x240, each frame ends with `Draw_Flush` plus a swap. GL state is Quake-1-shaped: front-face culling, flat shading, nearest filtering, alpha test at 0.666. `VID_SetPalette` keeps the 8-bit software palette path including the 15-bit→8-bit nearest-colour table.

## How it pushes 3DS hardware

- **Memory.** The hunk split (20 vs 64 MB) is the main scaling lever, keyed off one boot flag. GPU/DSP buffers come from the linear heap (`linearAlloc` in `snd_ctr.c`, `music.c`, `gl_hyena.c`).
- **Screen.** `gfxInit(GSP_BGR8_OES, GSP_RGB565_OES, false)` with no bottom-screen double buffering. Wide (800-wide) mode turns on only for a New 3DS that is not a 2DS, and off in `+sys_testmode`. Old 3DS output stays 400x240.
- **Audio.** Two ndsp paths (ndsp drives the audio DSP — a **[DSP](../../../glossary.md#dsp)** is a processor for streaming audio work): effects mix into one looping stereo PCM16 buffer at 22050 Hz (2048 samples), while MP3 music decodes via libmpg123 into `linearAlloc`'d wave buffers invalidated before the DSP reads them.
- **Input.** Buttons map through `Sys_SetKeys`; the gyroscope feeds yaw/pitch, touch drags aim with subpixel smoothing, a bottom overlay covers the rest, and Circle Pad Pro initialises on Old 3DS only (skipped when `new3ds_flag` is set — the New 3DS has its own C-stick).
- **Threading.** Single-threaded: `Host_Frame` runs inside `while (aptMainLoop() && game_running)` on the **[appcore](../../glossary.md#appcore-syscore)** (the core that runs the game). **New 3DS:** extra cores untouched.

## Right and wrong

Right: one boot-time probe drives hunk, wide screen and accessory init; audio/GPU buffers live in linear memory with explicit cache maintenance; the screenshot routine works around picaGL's missing `glReadPixels` by finishing the GPU, transferring the top screen to a readable framebuffer, invalidating the cache, and de-rotating columns into a BMP.

Fragile: the file layer keeps a fixed table of 10 `FILE*` handles that fatals when exhausted; `Sys_MakeCodeWriteable` and `Sys_Sleep` are empty stubs, so the loop busy-spins between frames; errors end in paged fatal dialogs rather than recoverable paths.

## Technical details

| Item | Value | File |
| ---- | ----- | ---- |
| Hunk | 20 MB Old 3DS / 64 MB **New 3DS** | `sys_ctr.c` |
| Screen | 400x240; wide only if New 3DS and not 2DS | `sys_ctr.c` |
| GL init | `pglInitEx(0x040000, 0x100000)`, flat, nearest, alpha 0.666 | `gl/gl_vidctr.c` |
| SFX | ndsp ch0 stereo PCM16 22050 Hz, 2048-sample loop | `snd_ctr.c` |
| Music | MP3 via mpg123, `linearAlloc` buffers, cache invalidate | `music.c` |
| Input extras | gyro, smoothed touch aim, bottom overlay, CPP on Old only | `in_ctr.c`, `touch_ctr.c` |
| Timing | `svcGetSystemTick` / 268123480.0 | `sys_ctr.c` |
| Stack | 256 KB (`__stacksize__`) | `sys_ctr.c` |

## Lesson table

| # | Symptom | Fix (as done here) |
| - | ------- | ------------------ |
| 1 | One binary must fit two consoles' memories | Detect the model once; size the zone allocation to it (20 vs 64 MB) (`sys_ctr.c`) |
| 2 | DSP plays garbage after CPU mixing/decoding | Allocate audio buffers with `linearAlloc`; flush/invalidate around every handoff (`snd_ctr.c`, `music.c`) |
| 3 | Accessory init breaks the model with the control built in | Only `cppInit` the Circle Pad Pro when not on a New 3DS (`sys_ctr.c`, `in_ctr.c`) |
| 4 | Renderer wrapper lacks `glReadPixels` | `glFinish`, transfer the top screen to a readable framebuffer, invalidate, de-rotate in software (`sys_ctr.c`) |
| 5 | Touch aiming feels steppy at low resolution | Average each sample with the previous one so subpixel movement accumulates (`in_ctr.c`) |
| 6 | "Out of handles" fatal on content with many open files | Know the fixed 10-handle table exists; budget concurrent opens or enlarge it (`sys_ctr.c`) |

## Open questions

- What frame rate does a real map sustain on an Old 3DS at the 20 MB hunk — scene traversal, picaGL submission, or texture upload? Nothing stated.
- Does `osSetSpeedupEnable(true)` change clocks without matching exheader bits? Unverified here.
- Is the 10-handle table ever exhausted by stock content, or only mods? Unknown.
- The `MAX_AI_COUNT=18` and `R_FIXED_FAR_CLIP=4096` tunables look like 3DS performance caps; their basis is unrecorded.

## Sources

[nzp-team/vril-engine](https://github.com/nzp-team/vril-engine) commit `9a26605`: `README.md`; `Makefile.ctr` (flags, `MAX_AI_COUNT`, `R_FIXED_FAR_CLIP`); `source/platform/ctr/` — `sys_ctr.c` (hunk sizes at lines 35-36, 10-handle file table at line 57, 256 KB stack at line 39), `in_ctr.c`, `snd_ctr.c` (22,050 Hz, 2,048 samples), `music.c`, `touch_ctr.c`, `gl/gl_vidctr.c` (alpha test 0.666 at line 51), `gl/gl_hyena.c`, `circle_pad_pro.c`.
