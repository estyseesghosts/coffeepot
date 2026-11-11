# Case study: SRB2 3DS port (Doom-Legacy software renderer)

A 3DS port of Sonic Robo Blast 2 v2.1.25, built on the Doom Legacy codebase. It keeps the original CPU software renderer on the 3DS CPU and presents through SDL. The README states it needs a New 3DS for acceptable speed. Nothing below was measured by Coffeepot; claims are `[reported]` by the port README or `[documented]` in its `Makefile` and `src/` tree.

## Before you start

- Evidence: model and feature claims are `[reported]` by the [port README](https://github.com/derrek/SRB2-3DS); build facts are `[documented]` in the `Makefile`.
- Assets are separate SD downloads to `/3ds/srb2_3ds`; DSP firmware must be dumped or there is no sound.
- Playable target is the New 3DS: **New 3DS:** required for acceptable speed `[reported]`; Old 3DS is "extremely laggy" `[reported]`. Old 3DS remains the default for unmarked general statements.
- Companion pages: [porting](../porting.md), [CPU optimization](../cpu-optimization.md), [memory and cache](../memory-and-cache.md), [audio](../audio.md).

Jargon: the **[ARM11](../../glossary.md#arm11)** is the 3DS CPU; a **[cache](../../../glossary.md#cache)** is fast memory in front of main memory; **[FCRAM](../../glossary.md#fcram)** is 3DS main memory; a **[framebuffer](../../../glossary.md#framebuffer)** holds one image; the **[DSP](../../../glossary.md#dsp)** is the audio processor.

## What makes it tick

**Engine.** SRB2 is a Doom-derivative floor-plan engine: BSP sorting (`r_bsp.c`), column-drawn walls (`r_segs.c`, `r_draw.c`), planes (`r_plane.c`), sprites (`r_things.c`), fixed-point 35 Hz logic (`p_tick.c`, `m_fixed.c`). The port compiles this tree largely as-is (`[documented]` in the `Makefile` object list).

**Software renderer on the ARM11.** Wall columns and floor spans are composited by CPU loops into a low-resolution framebuffer that SDL presents. The tree also builds `src/hardware` (`hw_main.o`, `hw_draw.o`) behind `-DHWRENDER`, but the described runtime path is the software rasteriser (`[documented]` in `Makefile` `SOURCES`/`DEFINES`).

**Fixed timestep and audio.** Logic ticks at 35 Hz regardless of draw speed, so slow drawing means stuttering presentation, not slow motion. Audio goes through SDL_mixer (`-lSDL_mixer -lmikmod -lmad -lvorbisidec`, `[documented]`), with no MIDI support (`[reported]`).

## How it pushes 3DS hardware

**Resolution scaling.** The renderer draws into a low-resolution buffer (Doom-family 320x200-class) and upscales to the 400x240 screen, so fill cost follows the small buffer. Exact shipped dimensions are unstated. > Unknown: internal resolution and upscale filter.

**CPU and cache ceiling.** Per-pixel wall, plane, and sprite loops get no PICA200 help on the shipped path. The README blames both clock and cache: the New 3DS has "higher CPU clock frequencies and another layer of cache" (`[reported]`). **New 3DS:** faster clock mode and extra cache change the ceiling; the port targets that model only.

**Memory, assets, cuts.** WAD and `.pk3` add-on lumps (deflate-compressed, hence the `3ds-zlib` dep) stream from SD via `w_wad.c`; `config.cfg` and saves sit beside the game in `/3ds/srb2_3ds` (`[reported]`). Hold the working set in FCRAM; treat SD as load-time only. Netplay (`-DNONET -DNOHS`), splitscreen, and post-processing (`-DNOPOSTPROCESSING -DNOSPLITSCREEN`) are compiled out (`[documented]`); the lower screen is unused (`[reported]`). No separate render or audio core assignment is documented — assume single-core rasterisation.

## Right and wrong

Right: pinning upstream v2.1.25, fixing the asset path, and cutting netplay, splitscreen, and post passes left one bottleneck — raw rasterisation. Right again: zlib support so `.pk3` add-ons load directly. Wrong for Old 3DS owners: no reduced-geometry or lower-resolution fallback shipped, so the README declares the Old 3DS unsupported rather than scaling down. The custom citro3d-fork build dep also raises the build bar (`[documented]`).

## Technical details

- Files read: `README.md`, `Makefile` (ARCH, DEFINES, objects, LIBS), `src/` listing.
- ARCH `-march=armv6k -mtune=mpcore -mfloat-abi=hard -O3`; DEFS include `-D__3DS__ -D_NDS -DNONET -DNOPOSTPROCESSING -DNOSPLITSCREEN` (`[documented]`).
- Renderer objects linked: `r_bsp.o r_data.o r_draw.o r_main.o r_plane.o r_segs.o r_sky.o r_splats.o r_things.o` plus `v_video.o screen.o` (`[documented]`).
- Present path runs through `src/sdl/` (`i_video.c`) and `v_video.c`/`screen.c`; copy-vs-present mechanics not read for this page.

## Lesson table: 2D and 2.5D software-engine rules

Each rule is a `[hypothesis]` until measured on your workload.

| # | Symptom on Old 3DS | Fix |
| - | ------------------ | --- |
| 1 | Rasteriser misses budget at panel resolution | Render at half resolution or below; budget fill in source pixels |
| 2 | Upscale shimmers and costs a filter pass | Integer ratio with nearest-neighbour; filter offline or not at all |
| 3 | Level loads stall on compressed lumps | Decompress per lump at load, never per frame |
| 4 | Audio stutters as frames drop | Mix at native rate, fixed channels; no per-frame resampling |
| 5 | Gameplay hitches on storage | SD for loads and saves only; working set stays in FCRAM |
| 6 | Port too big or too slow to tune | Cut netplay, splitscreen, post passes before touching the renderer |
| 7 | Builds irreproducible | Pin upstream version, library commits, and asset pack versions |

## Open questions

- Shipped internal resolution and upscale filter? Not stated; `sdl/i_video.c` and `v_video.c` would settle it.
- How much of the Old 3DS gap is clock vs cache? A clock-locked New 3DS run would separate them.
- Does the `src/hardware` path run? It compiles (`[documented]`) but is not the described runtime.

## Sources

- [SRB2 3DS README](https://github.com/derrek/SRB2-3DS) — model requirement, asset install, build deps, known issues (read November 2026).
- Port `Makefile` and `src/` listing — flags, disabled features, renderer and WAD objects.
