# Porting games and engines to the 3DS

Porting an existing program to the 3DS means fitting it into a machine with a 268 MHz CPU (Old 3DS), as little as 64 MB of memory for everything, and a GPU (the **[PICA200](../glossary.md#pica200)**) whose pixel stage is fixed-function **[TEV](../glossary.md#tev)** combiners rather than programmable shaders. Code that compiles is usually the easy part; the work is in replacing the renderer, budgeting memory and CPU time, and choosing what to cut. This page is a checklist for planning a port, a mapping from desktop graphics concepts to citro3d, the libraries available to shorten the work, and what published ports report about their choices.

## Before you start

- Checked against devkitPro packages as of November 2026 (devkitARM r68, libctru 2.7.1, citro3d 1.7.1), the SDL documentation for its 3DS port, and the README files of the ports cited. Nothing on this page is a measurement made for the wiki.

## Plan the port

1. **Set the target.** Old 3DS or New 3DS only? The difference is roughly 3× CPU speed, an L2 cache, a third usable core and twice the default application memory ([CPU](../hardware/cpu.md), [memory and cache](memory-and-cache.md#how-much-memory-you-get)). Deciding "New 3DS only" early is legitimate; deciding it late is expensive.
2. **Measure the original.** Peak memory, CPU time per frame on a known machine, draw calls and triangles per frame, texture memory, audio voices. Scale CPU expectations down a lot: the ARM11 has no SIMD unit, no hardware divide and small caches ([CPU optimisation](cpu-optimization.md)).
3. **Budget memory first.** Code size, assets resident at once, the linear heap for GPU/audio buffers (32 MiB by default), and 6 MB of VRAM for render targets. Builds for the 3DS often need smaller textures, fewer resident assets and streaming ([asset pipeline](asset-pipeline.md)).
4. **Decide the renderer strategy** (next section) before porting game code; it shapes everything else.
5. **Keep a correctness reference.** A desktop build of the same code, with the same inputs, lets you compare output while you change the renderer and optimise.
6. **Plan the frame rate.** Lock to 30 if 60 is out of reach on the target ([frame pacing](frame-pacing.md)), and keep game logic on a fixed step so the game does not slow down when rendering does.

## Renderer strategies

| Strategy | When it fits | Costs |
| -------- | ------------ | ----- |
| **Software rendering into a framebuffer** (what SDL's 3DS port does — see below) | 2D games at low resolution, quick first port, games whose original renderer is a CPU rasteriser | All pixel work on the slow CPU, plus converting and uploading the image every frame |
| **CPU renderer + GPU composition**: draw layers on the CPU, upload as textures, let the GPU scale, filter and blend them | 2D games with a few layers that change rarely | Upload cost scales with what changes; do partial updates ([memory and cache](memory-and-cache.md#texture-uploads)) |
| **Native citro3d renderer** | 3D games; 2D games with many sprites; anything that should run well on Old 3DS | Rewriting the renderer around the PICA200's model ([GPU rendering](gpu-rendering.md), [2D rendering](rendering-2d.md)) |

## Mapping desktop graphics concepts

| Desktop / OpenGL (ES) concept | 3DS equivalent | Notes |
| ----------------------------- | -------------- | ----- |
| Vertex shader (GLSL) | Vertex shader in picasso assembly (`.v.pica`) | No high-level shader language in devkitPro; 24-bit float precision ([picasso](../homebrew/picasso-and-citro3d.md)) |
| Fragment shader | Six combiner stages (TEV), fragment lighting LUTs, fog, procedural textures | Not programmable; redesign effects ([GPU shading features](../hardware/gpu-shading-features.md)) |
| Geometry shader | PICA200 geometry shader (one of the four shader units) | Point expansion, simple primitive generation |
| `glVertexAttribPointer`, VBOs | `C3D_AttrInfo` (≤ 12 attributes) + `C3D_BufInfo`, buffers in linear memory | Attribute types: byte, unsigned byte, short, float |
| Index buffers | `C3D_DrawElements` with `C3D_UNSIGNED_BYTE` or `C3D_UNSIGNED_SHORT` | No 32-bit indices; split large meshes |
| `glTexImage2D` | `tex3ds` at build time, or `C3D_TexInit` + upload of **tiled** data | Power-of-two sizes 8–1024; convert row-major images (or tile them with `GX_DisplayTransfer`) |
| `glGenerateMipmap` | `C3D_TexGenerateMipmap`, or pre-built mipmaps from `tex3ds -m` | |
| Framebuffer objects | `C3D_RenderTargetCreateFromTex` | |
| `glClear` | `C3D_RenderTargetClear` (hardware fill) | |
| Blend, depth, stencil, alpha test, scissor | `C3D_AlphaBlend`, `C3D_DepthTest`, `C3D_StencilTest`/`C3D_StencilOp`, `C3D_AlphaTest`, `C3D_SetScissor` | Same concepts |
| Multiple texture units | Three ordinary units plus the procedural-texture unit | Combine more textures in several passes or bake them |
| Instancing, compute | None | Batch geometry on the CPU or in the geometry shader |
| Swap interval / vsync | `C3D_FRAME_SYNCDRAW`, `C3D_FrameRate` | |
| Two screens | Two (three with stereo) render targets per frame | The bottom screen can show a map, inventory or touch controls |

## Libraries that shorten a port

- **SDL.** devkitPro's package repository carries **SDL 1.2** (`3ds-sdl`, with `sdl_image`, `sdl_mixer`, `sdl_ttf`, `sdl_gfx`). Upstream **SDL2 (since 2.26) and SDL3** include a Nintendo 3DS port contributed by Pierre Wendling, built with devkitARM and the devkitPro CMake toolchain file. Its documentation states that **only software rendering is supported** and that threads run cooperatively on a single core, yielding only in `SDL_Delay` or blocking waits; `SDL_GetBasePath` returns the RomFS root [documented: SDL `README-n3ds`]. SDL is therefore good for input, audio, files and windowing glue, but a game that needs speed will still want a citro3d renderer.
- **Compression, images, audio, physics, scripting**: `zlib`, `zstd`, `lz4`, `libpng`, `libjpeg-turbo`, `giflib`, `freetype`, `libogg`/`libvorbisidec`, `opusfile`, `flac`, `mpg123`, `libmodplug`, `libxmp`, `box2d`, `bulletphysics`, `lua 5.1`, `physfs`, `curl`, `mbedtls`, `yaml-cpp`, `tinyxml2` and more are packaged as `3ds-*` portlibs (installed with `dkp-pacman`, found through `$(PORTLIBS)` and `pkg-config`).
- **Standard C/C++**: newlib provides the C library; `fopen` works on `sdmc:/` and, after `romfsInit()`, on `romfs:/`. Threads: libctru's own API, or experimental `pthread`/C11/`std::thread` support ([threading](threading.md)).

## What published ports report

These are the porters' own statements, useful as examples of decisions rather than as measurements.

- **Super Mario 64 (3DS port by Gericom, maintained in community forks):** runs its audio thread on core 1 on Old 3DS and core 2 on New 3DS; uses a "naïve frame-skip if frame takes longer than 33.3 ms"; offers stereoscopic 3D, an 800-pixel wide mode and an anti-aliasing toggle; requires Luma3DS 10.1.1 or later for its threading [documented: the port's README].
- **DevilutionX (Diablo engine):** its 3DS guide notes that New 3DS models are "significantly" faster, recommends the original 640×480 resolution for a "significant performance improvement", and warns that the Chinese, Korean and Japanese translations can run an Old 3DS out of memory and crash the game [documented: DevilutionX 3DS documentation].

The common thread: both expose presentation options (resolution, wide mode, anti-aliasing, stereo) rather than one fixed configuration, put audio on a separate core, and treat Old 3DS memory as a hard limit.

## Lessons reported by one large port

An engineering-notes set for a Pokemon HeartGold/SoulSilver 3DS port (local, unpublished; the claims are the project's own and not reproduced here) records what cost it time. They fit the advice above, but treat them as `[hypothesis]` until you measure your own game:

- Moving unrelated work (an audio thread) to Old 3DS core 1 reportedly starved geometry and display work under the application's CPU share limit; measure New 3DS separately ([threading](threading.md)).
- Merging draw batches across texture, stencil or blend changes reportedly produced duplicated sprites and halos; batch only runs where order and state cannot change.
- A fixed 30 Hz cap used as game-speed control halved the game clock; keep presentation cadence separate from simulation ([frame pacing](frame-pacing.md)).
- A fast path that is rejected on most frames saves nothing; count how often it is actually taken before tuning it.
- Use three oracles for a renderer rewrite: fixtures against a reference render, same-state replay of both renderers, and real Old plus New 3DS runs; the emulator is extra coverage only.
- Plan stereo and supersampling as optional: never make them mandatory on Old 3DS ([render-target budget](gpu-rendering.md#render-target-memory-budget)).

## Porting checklist

- [ ] Builds with `-march=armv6k -mtune=mpcore -mfloat-abi=hard`, no warnings about `double` promotion in hot code.
- [ ] Endianness: the 3DS is little-endian like x86 and most ARM targets; alignment of packed file data handled explicitly.
- [ ] 32-bit pointers: code that assumes 64-bit `long` or pointer sizes fixed.
- [ ] Filesystem paths mapped (`sdmc:/3ds/<game>/`, `romfs:/`); writable data on the SD card, not in RomFS.
- [ ] HOME, sleep and exit handled through `aptMainLoop()`; audio and GPU state survive suspend ([testing on hardware](testing-on-hardware.md#what-to-test-beyond-it-runs)).
- [ ] Memory peak measured in the largest scene on an Old 3DS in the default mode.
- [ ] Frame time measured on the slowest target, worst scene, with a breakdown ([performance](performance.md)).

## Technical details

- SDL's 3DS port: CMake builds use `-DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/3DS.cmake`; programs should link SDL's `main` helper so RomFS is set up.
- `C3D_DrawElements(primitive, count, type, indices)`: `type` is `C3D_UNSIGNED_BYTE` or `C3D_UNSIGNED_SHORT`; indices must be in linear memory.

## Pitfalls

- **Porting the renderer last.** It decides memory layout, asset formats and frame structure.
- **Assuming the New 3DS.** Test on Old 3DS early if you intend to support it.
- **Keeping desktop asset sizes.** 2048² textures and uncompressed audio do not fit.
- **Relying on SDL for rendering speed.** Its 3DS port renders in software.

## Open questions

- No measured comparison yet of software-rendered SDL output versus a citro3d renderer for the same 2D game on Old 3DS.
- The state of third-party OpenGL-style wrappers for the PICA200 has not been reviewed for this page.

## Sources

- [SDL: README-n3ds (SDL2)](https://wiki.libsdl.org/SDL2/README-n3ds) and [SDL3](https://wiki.libsdl.org/SDL3/README-n3ds) — the 3DS port's build, limitations and contributor; [SDL forum: N3DS port (squashed)](https://discourse.libsdl.org/t/sdl-n3ds-port-squashed/39254) — origin.
- devkitPro package list (`pacman -Sl`, November 2026) — available `3ds-*` portlibs.
- [citro3d](https://github.com/devkitPro/citro3d) — `include/c3d/*.h` (draw, texture, effect APIs).
- [sm64-port (3DS branch) README](https://github.com/thearst3rd/sm64-port/blob/3ds-port/README.md) — core placement, frame skip, presentation options.
- [DevilutionX 3DS documentation](https://github.com/diasurgical/DevilutionX/blob/master/docs/manual/platforms/3ds.md) — Old/New 3DS performance and memory guidance.
