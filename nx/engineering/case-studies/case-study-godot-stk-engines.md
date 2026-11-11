# Case study: Godot and SuperTuxKart Switch ports (reported results)

A Godot 3.x `switch` target and a SuperTuxKart (STK) CMake Switch build take the GLES path instead of native deko3d. Both ship as homebrew NROs through GLES; neither touches native Maxwell controls. Coffeepot measured nothing; every tree claim is `[reported]` or `[documented]` in the named file.

## When is this useful?

This page overlaps [open-engine ports](case-studies-open-engines.md), which is the canonical summary. Use it only for the Godot audio and input details.

## Before you start

- Trees: [redthing1/godot_switch](https://github.com/redthing1/godot_switch) at `0918643f6` and [supertuxkart/stk-code](https://github.com/supertuxkart/stk-code) at `b2a9f2233` (see [source versions](../source-versions.md)) (`switch/make.sh`, `switch/package.sh`, `.github/workflows/switch.yml`). This page overlaps [open-engine ports](case-studies-open-engines.md); that page is the canonical summary..
- Builds [documented]: Godot `scons platform=switch` (`platform/switch/detect.py`); STK `switch/make.sh` (`aarch64-none-elf-cmake -DNO_SHADERC=on`, `make install`, `switch/package.sh`).
- Launch: NRO from SD. The applet-vs-title memory trap applies to both.
- Companions: [rendering](../rendering.md), [performance and porting](../performance-and-porting.md).

Jargon used here: the **[CPU](../../../glossary.md#cpu)** runs game, physics, and submission threads; the **[GPU](../../../glossary.md#gpu)** draws; a **[framebuffer](../../../glossary.md#framebuffer)** holds the pixels; a **[shader](../../../glossary.md#shader)** is a GPU program; **[cache](../../../glossary.md#cache)** is fast memory beside a core; **[Maxwell](../../glossary.md#maxwell)** is the GPU architecture in the **[Tegra X1](../../glossary.md#tegra-x1)**; **[libnx](../../glossary.md#libnx)** is the homebrew system library; **[title override](../../glossary.md#title-override)** gives homebrew full game RAM.

## Architecture

| Layer | Godot switch target | STK Switch build |
| ----- | ------------------- | ---------------- |
| Scene / servers | Scene tree plus visual, physics, audio servers; POSIX threads with wrapped visual server (`VisualServerWrapMT`) [documented] | Desktop kart engine reused: physics, networked race, audio threads, same model with less headroom [reported] |
| Render path | Native EGL context driving GLES2/GLES3 rasterizers; no SDL in the loop [documented] | GLES abstraction (`USE_GLES2`); SDL gamepad layer (`SDL_INIT_GAMECONTROLLER` in `input_manager.cpp`) [documented] |
| Assets | Exporter writes NRO plus sidecar `.pck`, or fused `game.pck` in romfs via `build_romfs`; test push over nxlink [documented] | `make.sh` moves data to `sdcard/stk-data` and NRO to `sdcard/switch/`, zips both; CI fetches assets separately [documented] |
| OS glue | `godot_switch.cpp` (nxlink/romfs init, applet splash gate); `detect.py` (`-mtune=cortex-a57`, links `EGL GLESv2 glapi drm_nouveau nx`) [documented] | `switch.cmake` / `devkita64.cmake` (aarch64 toolchain); `package.sh` (`nacptool`, `elf2nro`) [documented] |

## ARM Cortex-A57 interaction

Neither tree pins threads to the 3 app cores or ships PGO/LTO [documented]. Godot adds `-flto` only on request with no PGO flags; STK `make.sh` passes neither [documented]. Godot runs main loop, GDScript logic, visual-server threads, an audio thread at priority `0x2B`, and the `JoypadSwitch` HID poll [documented]. GDScript stays interpreted, so script-heavy scenes pay interpreter cost before any draw submits [reported]. STK runs desktop physics, OpenAL audio, and network threads unchanged, submitting through the GLES abstraction and driver thread [reported]. Both serialize draws through one GLES driver thread per context while everything timeshares the same cores [reported].

## Maxwell interaction

Both push work through EGL/GLES, so tiling, Zcull, and bandwidth control stay inside the driver. Godot's surface is fixed [documented]: RGBA8 plus D24S8, window surface on the default window, GLES 3 requested, size stubs return 1280x720, with no resolve negotiation or compression calls. STK is built with `-DNO_SHADERC=on`, which `CMakeLists.txt` (lines 273-284) describes as disabling Vulkan support when the shaderc library is missing; the define `DISABLE_SHADERC` is consumed in `lib/graphics_engine` (the Vulkan engine). The tree does not show when the GLES shaders compile on the console, so first-run stutter is `[hypothesis]`. Neither sets Zcull, tile modes, or compression, so full-res passes pay full bandwidth at driver defaults [reported]. Error checking stays on in both; neither sets `MESA_NO_ERROR=1` [documented].

## What each does well

| Engine | Strength |
| ------ | -------- |
| Godot | Full export loop: scons target, editor preset (GLES2 `etc` / GLES3 `etc2`, `mobile`), NACP stamping, fused-romfs option, nxlink discovery and test push [documented] |
| Godot | Audio sized for hardware: 48 kHz, 24 voices, stereo, 2 mix buffers, 0x1000-aligned double-buffered pool, own thread [documented] |
| Godot | Input without SDL: 8-slot Npad map (slot 0 also claims handheld), 16-button table plus two sticks, per-frame poll [documented] |
| STK | One-script package: cmake, install, NRO, data zip; CI proves it per push (devkitA64 container, asset fetch, cache, release upload) [documented] |
| STK | Abstraction reuse: GLES2 renderer and SDL input shared with other platforms, so the Switch delta is build files plus packaging [documented] |

## What each does poorly

| Trap | Detail |
| ---- | ------ |
| Compile stutter | GLES drivers compile shaders from source at run time, so first-use hitches are plausible in both `[hypothesis]`; no tree here measures it, and `NO_SHADERC` concerns the Vulkan engine, not this. Neither tree has the sealed-cache plus background-compile defence the deko3d ports built `[documented]`. |
| No bandwidth budget | Fixed RGBA8+D24S8 surface and full-res GLES passes with no Zcull/tiling/compression or MSAA tradeoff pass in either tree [documented]. Overdraw is paid in full [reported]. |
| Applet OOM | Godot gates applet types with a splash screen [documented] but neither documents a title-only budget; STK's large `stk-data` sidecar invites the same Album-launch failure behind **[title override](../../glossary.md#title-override)** warnings [reported]. |
| SD streaming | Large data/pck sidecars sit on SD with no prefetch in the Switch scripts; slow cards stall boot and loads [reported]. |
| Audio defaults | Godot sizes buffers from `output_latency x mix_rate` with no Switch tuning note; STK inherits desktop OpenAL at 48 kHz. No measured latency [reported]. |
| Debug left on | Godot enables nxlink stdio and `TRACE` printf paths [documented]. Strip before shipping [reported]. |

## Lesson: when this path suffices vs deko3d

| # | Rule |
| - | ---- |
| 1 | 2D or modest 3D at 30 fps sub-native with a few hundred draws: stay on GLES `[hypothesis]`. Past roughly 1,000 draws/frame, heavy transparency, or full-res shadows: consider deko3d. The threshold is a `[hypothesis]`; the benchmark that would set it is on [rendering](../rendering.md). |
| 2 | Recurring first-draw hitches with no offline store: GLES has no in-tree answer; the deko3d ports' sealed cache plus warm-up is the answer they built [reported]; whether GLES hitches occur here is untested. |
| 3 | 720p RGBA8+D24S8 missing frame on stock clocks: GLES exposes no tile/Zcull/compression knobs; only a native pass rewrite helps [documented]. |
| 4 | Script/physics plus audio plus render sharing 3 cores with no pinning or PGO: pin, profile, cut submission first [documented]. |

## Open questions

- Measured fps and frame times per engine at shipped resolve, stock vs overclocked? Neither tree states them [reported].
- Godot GLES3 vs GLES2 export performance and `.pck` SD load times? Not stated [reported].
- STK playable resolve and fps on the GLES2 path? CI proves build, not playability [reported].
- Audio latency and underruns at 48 kHz? No figures in-tree [reported].
- Would one scene per engine in deko3d show tile/Zcull wins? Proposed, not done [reported].

## Sources

Repositories at pinned commits: [redthing1/godot_switch](https://github.com/redthing1/godot_switch) `0918643f62a6b7664a9d9bc6bcfdb280510a1b17`, [supertuxkart/stk-code](https://github.com/supertuxkart/stk-code) `b2a9f223326422aa2dd4eea28319b0c5115fde03`. Files:


- `godot_switch/platform/switch/` — `context_gl_switch_egl.cpp` (EGL RGBA8+D24S8, GLES 3, 1280x720 stubs), `audio_driver_switch.cpp/.h` (48 kHz, 24 voices, aligned pool, `0x2B` thread), `joypad_switch.cpp` (8-slot Npad map, per-frame poll), `export/export.cpp` (NRO plus `.pck`, fused romfs, nxlink push), `godot_switch.cpp` (applet splash gate), `detect.py` plus `os_switch.cpp` (scons target, cortex-a57 flags, GLES libs, visual-server wrap).
- `stk-code/switch/make.sh`, `switch/package.sh`, `.github/workflows/switch.yml` — aarch64 cmake `NO_SHADERC=on`, NRO plus `stk-data` zip, devkitA64 CI.
- `stk-code/CMakeLists.txt`, `src/graphics/glwrap.cpp`, `src/input/input_manager.cpp` — `USE_GLES2`, `DISABLE_SHADERC`, SDL gamepad path.
