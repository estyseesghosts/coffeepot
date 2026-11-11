# Case study: SRB2 3DS port (Doom Legacy hardware renderer on citro3d)

A 3DS port of Sonic Robo Blast 2 v2.1.25, built on the Doom Legacy codebase. It does not run the engine's CPU software rasteriser. It builds the engine's hardware-renderer interface (`-DHWRENDER`) and implements it on citro3d in `src/nds/r_nds3d.c`, with a small PICA200 vertex shader, a texture cache, stereoscopic and wide modes, and a render worker thread that consumes a ring buffer of draw packets. The README says the port targets the New 3DS and does not run acceptably on an Old 3DS. Nothing below was measured by Coffeepot; tree facts are `[documented]` in the named file and the model statement is `[reported]` by the README.

## Before you start

- Evidence: [derrekr/srb2_3ds](https://github.com/derrekr/srb2_3ds) commit `e8726772` (2026-05-03; see [source versions](../source-versions.md)). The tree states no frame rates, so none are given.
- Model: **New 3DS:** the README says the port is for the New 3DS and 2DS, and that it "does *NOT* run on o3DS devices" because it is "extremely laggy" there `[reported]`. Old 3DS is still this wiki's default for unmarked statements, so every lesson below that is about Old 3DS is a `[hypothesis]`.
- Companion pages: [porting](../porting.md), [GPU rendering](../PICA200/gpu-rendering.md), [stereo engineering](../PICA200/stereo-engineering.md), [threading](../threading.md), [memory and cache](../memory-and-cache.md), [audio](../audio.md).

Jargon: the **[ARM11](../../glossary.md#arm11)** is the 3DS CPU; the **[PICA200](../../glossary.md#pica200)** is the 3DS GPU; the **[linear heap](../../glossary.md#linear-heap)** is contiguous memory the GPU can read; **[VRAM](../../glossary.md#vram)** is the 6 MB of fast GPU memory; a **[draw call](../../glossary.md#draw-call)** is one request to draw triangles; a **[TEV](../../glossary.md#tev)** stage is one fixed-function combiner step; a **[shader](../../../glossary.md#shader)** is a small GPU program.

## What it is

| Layer | Approach |
| ----- | -------- |
| Engine | Doom Legacy / SRB2 2.1.25: BSP levels, 35 Hz fixed-point logic, Lua scripting (`-DHAVE_BLUA`). The engine's `src/hardware/` renderer is compiled in (`-DHWRENDER`)`[documented]` (`Makefile` lines 40, 66). |
| GPU driver | `src/nds/r_nds3d.c` ("NDS 3D API for SRB2") implements the hardware-renderer driver interface on citro3d. Its header comment says that in an ideal world it would share code with the OpenGL driver `r_opengl.c` `[documented]`. |
| Vertex shader | `vshader.v.pica`: multiply position by model-view and projection, pass texture coordinates through, output constant white colour `[documented]`. Lighting and colour are not in the shader. |
| Textures | `r_texcache.c`: 2,048 cache entries with aging (`TEX_AGING`) and a purge call; `i_video.c` creates textures with `C3D_TexInit` or `C3D_TexInitMipmap`, optionally `C3D_TexInitVRAM`, and copies with `C3D_SyncTextureCopy` `[documented]`. |
| Draw submission | `r_queue.c`: a ring of 8,192 packets (draw, blend, clear, fade, flush, transform, suspend, exit) consumed by a worker thread, with events for empty, almost-empty and allocation-ok `[documented]`. |
| Output | `vid.width = 400`, `vid.height = 240`. Render targets are created 240x400 (the panel's native orientation) with a 24-bit depth plus 8-bit stencil buffer, for the left eye, the right eye and a 240x800 wide target `[documented]` (`i_video.c` line 1021; `r_nds3d.c` lines 188-190). |
| Audio | SDL_mixer at 44,100 Hz, 16-bit stereo, 4,096-byte buffer `[documented]` (`i_sound.c`). No MIDI `[reported]`. |
| Build | `-march=armv6k -mtune=mpcore -mfloat-abi=hard -O3`, links `-lcitro3d -lctru` and a fork of citro3d `[documented]` (`Makefile`; README). Netplay, post-processing and splitscreen are compiled out (`-DNONET -DNOPOSTPROCESSING -DNOSPLITSCREEN`). |

An earlier version of this page said the port keeps the software renderer on the CPU. That was wrong: the Makefile defines `HWRENDER`, links citro3d and builds `src/nds/`, and the README requires the citro3d fork.

## How it uses the hardware

**Render worker.** `i_video.c` creates a worker thread, trying priorities upward from `0x32` until `threadCreate` succeeds, then raises the main thread's priority; the worker consumes packets and issues the citro3d calls, so command recording is off the game thread `[documented]` (lines 430-460). On an Old 3DS it also tries to raise the application's CPU time limit (`APT_SetAppCpuTimeLimit` to 30, then 40 percent) and prints the result `[documented]`; whether either call succeeded on a real console is not recorded. **New 3DS:** `i_system.c` calls `osSetSpeedupEnable(true)` and `PTMSYSM_ConfigureNew3DSCPU(3)` ("enable fast clock + L2 cache on new3ds") `[documented]`.

**Stereo and wide mode.** The 3D slider position sets the eye separation and the `gr_3dsfoclen` setting the focal length (`get3DSettings` in `r_nds3d.c`); with the slider at zero, 3D is off. `gr_3dswidemode` selects the 800-pixel mode, and the targets are recreated at the start of a frame, never while the GPU has work in flight `[documented]`. Stereo draws the scene twice, once per eye (see [stereo engineering](../PICA200/stereo-engineering.md)); this tree does not say what it costs.

**Colour format.** The `gr_3dsrtformat` setting chooses RGBA8 or RGB565 render targets. A code comment says RGB565 halves colour-write bandwidth but quantises colour enough to expose tiny mismatches between adjacent regions `[documented]`; the effect on speed is not measured.

**Fog.** Fog uses cached hardware fog lookup tables, one per density step `[documented]` (`fogLUTs` in `r_nds3d.c`).

**Memory and loading.** `gr_3dsprecache` (default on) loads a level's textures at level start so first use does not stall; the code stops precaching when free linear heap falls below a safety margin so that runtime allocations (HUD, colour-translated sprites) still fit `[documented]` (comment in `i_video.c`).

**Smaller choices.** `gr_3dsdisablebottom` powers the bottom backlight off after the first rendered frame; `vid_wait` is the vsync setting `[documented]`. Assets are separate downloads in `/3ds/srb2_3ds`; the DSP firmware must be dumped or there is no sound `[reported]`.

## What it gets right and wrong

Right: the engine already had a hardware-renderer interface, so the port is a driver, not a renderer rewrite; command recording moved off the game thread; the texture cache and level precache avoid first-use stalls; targets are recreated only between frames. Wrong or costly: the README gives up on Old 3DS instead of providing a cheaper mode; the port needs a custom citro3d fork, which raises the build bar `[documented]`.

## Lessons

Each is a `[hypothesis]` drawn from this tree. Confirm with [performance](../performance.md) counters.

| Symptom | Idea from this port |
| ------- | ------------------- |
| Game thread spends its frame recording GPU commands | Put the GPU driver behind a ring of command packets and a worker thread; size the ring and block the producer when it is nearly full |
| First-use texture stalls in a level | Precache at level load, but stop at a linear-heap safety margin |
| Render-target changes crash or glitch | Create or delete targets only at the start of a frame after draining the GPU |
| Colour-write bandwidth looks high | Offer an RGB565 target as a setting and test the quantisation artefacts before defaulting to it |
| Stereo roughly doubles scene cost | Make 3D and wide mode runtime settings; leave them off by default on Old 3DS |
| Port is Old-3DS-hostile | Decide the minimum model up front and say so, as the README does |

## Open questions

- What frame rate does a level hold on a New 3DS and on an Old 3DS, and what does stereo cost? The tree says nothing; an on-hardware run with the settings above toggled would answer it.
- Did the `APT_SetAppCpuTimeLimit` calls succeed, and did they help? The code prints the result but nothing is recorded.
- Is the render worker or the game thread the limit in a busy level? A per-thread CPU-time log would settle it.

## Sources

- [derrekr/srb2_3ds](https://github.com/derrekr/srb2_3ds) commit `e8726772`: `README.md` (model statement, build dependencies, known issues); `Makefile` (flags, `HWRENDER`, `-lcitro3d`); `src/nds/r_nds3d.c`, `r_nds3d.h` (renderer driver, targets, stereo, fog); `src/nds/r_queue.c`, `r_queue.h` (packet ring); `src/nds/r_texcache.c` (texture cache); `src/nds/i_video.c` (settings, worker thread, textures); `src/nds/i_system.c` (clock setup); `src/nds/i_sound.c` (audio); `src/nds/vshader.v.pica` (vertex shader).
