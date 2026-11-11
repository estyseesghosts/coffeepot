# Case study: Sonic CD 3DS port (RSDKv3)

A 3DS port of the 2011 Sonic CD remake, built on the RSDKv3 decompilation. It ships two render backends from one codebase: a hardware path that draws sprites through the GPU, and a software path that renders the original framebuffer on the CPU and uploads it. Nothing below was measured by Coffeepot; all behaviour is `[reported]` by the port README or `[documented]` in its `RSDKv3/3ds/` sources.

## Before you start

- Evidence: every behaviour is `[reported]` by the [port README](https://github.com/SaturnSH2x2/Sonic-CD-11-3DS) or `[documented]` in a named file, at commit `8b30cf6` (2025-02-24; see [source versions](../source-versions.md)). No frame-rate figure is stated.
- The port is deprecated upstream in favour of a Redux fork; the design notes still apply to 2D sprite ports.
- Old 3DS is the default model here. Model-specific claims name their model in the row.
- Companion pages: [porting](../porting.md), [2D rendering](../PICA200/rendering-2d.md), [audio](../audio.md), [files and saves](../filesystem-and-saves.md).

Jargon: the **[ARM11](../../glossary.md#arm11)** is the 3DS CPU; the **[PICA200](../../glossary.md#pica200)** is the 3DS GPU; a **[framebuffer](../../../glossary.md#framebuffer)** holds one image; **[VRAM](../../glossary.md#vram)** is the 6 MB of fast GPU memory; a **[batch](../../glossary.md#batch)** is a run of sprites in one draw call.

## What makes it tick

**Engine.** RSDKv3 is a scripted 2D engine: fixed-timestep logic, tile layers, paletted sprites, script-driven objects (`RSDKv3/Scene.cpp`, `Sprite.cpp`, `Palette.cpp`). The port keeps that core and replaces three seams: rendering (`3ds/render_3ds.cpp`), audio (`3ds/audio_3ds.cpp`), and video (`3ds/video_3ds.cpp`), wired by `Makefile.3ds` (`[documented]`).

**Two renderer paths.** Flag `RETRO_USING_C2D` picks the backend (`[documented]` in `Makefile.3ds`, `[reported]` in the FAQ):

| Backend | How it draws | Full speed where |
| ------- | ------------ | ---------------- |
| HW | citro2d backend; sprites as textured quads on the PICA200 | Old 3DS `[reported]` |
| SW | Original CPU renderer into a framebuffer, one upload per frame | **New 3DS:** full speed `[reported]`; Old 3DS too slow `[reported]` |

**Fixed timestep and audio.** Logic runs at a fixed rate with frame limiting added by the port, so missed draws repeat a frame rather than slowing the game (`[reported]`). Audio goes through SDL_mixer (`-lSDL_mixer -lmikmod -lmad -lvorbisidec`, `[documented]` in `Makefile.3ds`), feeding the 3DS path that needs dumped DSP firmware. Build flags are `-march=armv6k -mtune=mpcore -O2` (`[documented]`).

## How it pushes 3DS hardware

**Fill cost decides the backend.** Software blending, scanline deformation, and palette swaps burn ARM11 cycles per pixel, and the README states the Old 3DS CPU cannot finish a frame — hence the New 3DS requirement for SW (`[reported]`). **New 3DS:** higher clock plus extra cache closes the gap. The HW build moves per-pixel work to the PICA200, leaving the CPU with setup and logic, which fits the Old 3DS (`[reported]`).

**Batching and resolution.** The HW header sizes its layer cache for the panel (`MAX_TILES_PER_LAYER`, commented "400x240 -> 25x15 tiles onscreen", `[documented]` in `render_3ds.hpp`). Sheets sharing one [atlas](../../glossary.md#atlas) go out in few batches; each texture or tint change ends one. The 400x240 target matches the top screen 1:1, so neither backend pays a scaler.

**Stereo, VRAM, saves.** HW builds offer full stereoscopic 3D — each sprite drawn twice with an offset, roughly doubling its draw cost (`[reported]`); SW has no stereo path. VRAM budgets are unstated. > Unknown: peak texture use per zone. Settings live in `settings.ini`, mods and the dev menu (SELECT) load from SD, and `-DFORCE_CASE_INSENSITIVE` fixes Windows-authored mod paths (`[reported]`/`[documented]`).

## Right and wrong

Right: one engine with two backends shipped on Old 3DS without forking game code; fixed 400x240 output avoided scaler cost; pre-scaled videos (linked in the FAQ) turned a decode problem into an asset problem. Wrong, by the author's account: two backends doubled testing, and HW dropped effects SW kept (realtime palettes unsupported, `[reported]`). Full-size video stayed slow — the offered fix is smaller assets, not a faster decoder.

## Technical details

- Files read: `README.md`, `Makefile.3ds`, `RSDKv3/3ds/render_3ds.hpp`, `video_3ds.cpp` (`SCREEN_WIDTH 400`, `SCREEN_HEIGHT 240`).
- Deps: SDL, SDL_mixer, `3ds-libvorbisidec`, `3ds-libtheora`, `3ds-mikmod`, `3ds-libmad` (`[documented]` in README).
- Even the SW path initialises citro2d to present (`C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT)` in `video_3ds.cpp`): GPU present, CPU compose.

## Lesson table: 2D retro-engine rules

Each rule is a `[hypothesis]` until measured on your workload.

| # | Symptom on Old 3DS | Fix |
| - | ------------------ | --- |
| 1 | CPU blending misses the frame budget | Move sprites to GPU quads; keep CPU for setup and logic |
| 2 | Base layer scaled every frame | Render at 400x240 exactly; never scale at runtime |
| 3 | Shimmer on scroll | Integer placement; snap tile origins to whole pixels |
| 4 | Zone-load hitches | Pre-convert sheets to tiled textures at build; load per zone |
| 5 | Music stutters as frames drop | Mix at native rate; fix channels and lock scope first |
| 6 | Mod assets load on PC, fail on device | Case-insensitive lookup; log the resolved path |
| 7 | Cutscenes drop frames | Ship pre-scaled video; never decode full-size on CPU |

## Open questions

- Measured HW vs SW frame time on one stage? No figure stated; a limiter-off paired run would settle it.
- Which effects beyond realtime palettes differ between backends? Unrecorded.
- VRAM high-water mark per zone? Unstated.

## Sources

[SaturnSH2x2/Sonic-CD-11-3DS](https://github.com/SaturnSH2x2/Sonic-CD-11-3DS) commit `8b30cf6`: `README.md` (deprecation notice; features including stereoscopic 3D in the HW build only; the FAQ on the HW and SW builds at lines 30-37; build flag `RETRO_USING_C2D`), `Makefile.3ds`, `RSDKv3/3ds/render_3ds.cpp` (tile caches), `render_3ds.hpp`, `video_3ds.cpp` (`SCREEN_WIDTH 400`).
