# Case studies: open-source engine ports on Switch (reported results)

SDL plus EGL and GLES ports get 2D and modest 3D running fast with little Switch-specific code; demanding 3D engines boot and play but inherit the cost of the abstraction. None of the four ports below uses native `deko3d` command control, so none sets Maxwell features (Zcull, tiling, compression) directly. They split into three rendering paths: a CPU software renderer shown through SDL2's framebuffer (srb2-switch), an SDL window with a GL renderer on Mesa/Nouveau EGL (taisei, and SuperTuxKart for its gamepad layer), and OpenGL ES through the engine's own EGL context with no SDL (Godot). None of the figures were measured by Coffeepot; all are `[reported]` from the vendored trees and their READMEs, at the commits on [source versions](../source-versions.md).

## When is this useful?

Read this if you are bringing an existing SDL or OpenGL engine to the Switch with little Switch-specific code and want to know what that route does and does not control.

## Before you start

- Projects: srb2-switch (Sonic fangame on Doom Legacy), taisei (bullet-hell shooter), stk-code (SuperTuxKart), godot_switch (Godot 3.x export target).
- Companion pages: [rendering](../rendering.md) and [performance and porting](../performance-and-porting.md) for the deko3d path these ports do not take; [toolchain](../../homebrew/toolchain-and-workflow.md) for title override and nxlink.
- Decision rule up front: keep the renderer you already have until a measurement says otherwise. A 2D or software-rendered game goes through SDL as it is. For a 3D engine, profile first (CPU recording time and GPU time separately, see [performance and porting](../performance-and-porting.md)), and replace the OpenGL path with `deko3d` only if the CPU cost of the GL driver is the measured limit; no tree here records a draw-count or frame-rate threshold at which that happens.

## Per-engine snapshot

| Engine | Build (as vendored) | Renders how on Switch |
| ------ | ------------------- | --------------------- |
| srb2-switch | `make PLATFORM=Switch`; assets via download script; install NRO to SD, launch from hbmenu [reported] | SDL2 software **[framebuffer](../../../glossary.md#framebuffer)**, stretched to display resolution. Default 320×200; ships 800×450 config as perf/visual balance [reported] |
| taisei | devkitA64 plus switch-dev, meson and ninja with a Switch crossfile [reported] | SDL window plus GL renderer over Mesa/Nouveau EGL. Queries display resolution at boot, sets `MESA_NO_ERROR=1` in release [reported] |
| stk-code | CMake Switch build with `NO_SHADERC=on` (`switch/make.sh` line 16), packaged to NRO plus data zip [documented] | GLES2 path through the engine's abstraction (`USE_GLES2`) [reported]. `NO_SHADERC` is the option the top-level `CMakeLists.txt` (lines 273-284) offers to "disable vulkan support" when the Vulkan shader compiler is not installed; it does not by itself say anything about GLES shader compilation [documented] |
| godot_switch | scons `platform=switch`; editor gains a Switch exporter emitting NRO plus package [reported] | Native Switch EGL context (RGBA8 plus D24S8) driving GLES2/GLES3 rasterizers; no SDL in the loop [reported] |

## Threading, input, and audio

| Engine | Threading | Input | Audio |
| ------ | --------- | ----- | ----- |
| srb2 | Engine and SDL threads; keeps simulating while suspended on Home to prevent lag-switch exploits [reported] | SDL gamepad plus USB mouse/keyboard; inline system-keyboard applet hooks [reported] | SDL_mixer stack; Switch output resolves to 48 kHz through the OS mixer [reported] |
| taisei | SDL-threaded loader and audio; applet exit hook quits cleanly, exit locked at startup [reported] | SDL gamepad layer over HID; no custom Joy-Con driver | SDL audio resampled to the 48 kHz Switch path [reported] |
| stk | Heavier threaded engine (physics, audio, network); same model on Switch, less headroom | SDL gamepad and window upstream; same mapping in the NRO | OpenAL and ogg-vorbis stack; 48 kHz output [reported] |
| godot_switch | POSIX threads, visual-server wrapped multithreading; audio thread at priority `0x2B` (`audio_driver_switch.cpp` line 117) [documented] | Custom 8-slot gamepad mapping over HID button state | Native audren: 48 kHz, 24 voices, stereo, 2 mix buffers, page-aligned double-buffered pool [reported] |

## What had to change for Switch

- **Memory mode:** srb2 and taisei both require full-RAM access — hold R on a title (title override), never the Album applet; taisei warns it will crash from the photo applet [reported]. See [title override](../../glossary.md#title-override).
- **Assets:** download script (srb2), sidecar data zip (stk), package beside the NRO (godot, taisei) [reported]. Simple to ship and update; no cleverness needed.
- **Resolution:** srb2 800×450 shipped default; taisei reads the display resolution at boot; godot and STK leave it to the EGL surface plus in-game settings [reported].
- **Not measured:** none of the four trees state frame rates; neither Godot nor STK sets `MESA_NO_ERROR` or touches Zcull, tiling or compression [documented, absence of a call in the files read].
- **Debug:** NXLINK flag (srb2), stdio over nxlink in debug builds (taisei), trace plus `nxlink -p` console log (godot_switch) [reported].

## What the SDL and GL port path gets right

1. **SDL video, audio, and input is the fast path to "it runs."** srb2 and taisei reuse the desktop frame loop nearly unchanged; the Switch delta is a Makefile or crossfile, an applet hook, and key mapping [reported].
2. **Software rasterizers map cleanly to small framebuffers plus upscale.** srb2 at 800×450 is the canonical trick: **[CPU](../../../glossary.md#cpu)** fill cost scales with pixels, SDL stretches the rest [reported].
3. **Fixed-timestep simulation survives downscaling.** Shooter and Doom-tick logic stays correct while only the present resolution drops — reuse, do not redesign [reported].
4. **Pre-converted assets plus sidecar data beat clever packaging** for ports: zip-with-NRO layouts are simple to ship and update [reported].
5. **One env-var perf win is real:** taisei's `MESA_NO_ERROR=1` in release disables GL error checking to save CPU [reported].

## Constraints, observed problems and untested hypotheses

Rows tagged `[documented]` are read from the source; `[reported]` rows are the authors' own statements; `[hypothesis]` rows are inferred from the structure and were not measured.

1. **OpenGL hides Maxwell.** All four go through Mesa/Nouveau EGL or SDL; none sets Zcull, tile modes, or cache policy explicitly. Per-draw overhead and bandwidth waste that deko3d code would attack stay invisible [reported].
2. **No tiled-cache or bandwidth discipline.** Software upscale and full-res GLES both pay full bandwidth; there is no MSAA or compression tradeoff pass in any port tree examined.
3. **Desktop RAM and CPU assumptions break in applet mode.** The single most common failure is launching from Album and running out of memory — hence the repeated title-override warnings [reported].
4. **Audio plumbing "works", not tuned.** Only godot_switch sizes audren pools directly; the SDL ports inherit latency and buffer defaults [reported].
5. **Slow media path ignored.** srb2 notes long boot on slow SD cards with no asset-streaming fix [reported]; STK's multi-hundred-MB data zip has the same exposure.

## Lesson: what this route controls

- A cheap first step for an existing SDL or GL engine is to keep that path, ship a modest resolution (srb2-switch's 800×450 is the one figure in these trees), require title override and pre-convert assets `[hypothesis]`.
- Moving to `deko3d` gives control over barriers, Zcull, tiling and CPU draw submission, and costs owning that code. Whether a given engine needs it depends on a measurement: no tree here shows a draw-count or frame-rate threshold, and nothing shows that 60 fps 3D is impossible through OpenGL on this hardware `[hypothesis]`. Decide with the draw-count sweep proposed on [rendering](../rendering.md).

## Open questions

- Measured fps and frame-time for each port at shipped resolve on stock vs overclocked clocks? None stated in-tree.
- STK playable resolve and fps on the Switch GLES path? Build CI proves it builds, not how it plays.
- godot_switch GLES3 vs GLES2 exporter performance and package load times from SD?
- Audio latency and buffer sizes for the SDL ports at 48 kHz output?
- Any Zcull or tile wins if one scene per engine were re-implemented in deko3d? Experiment proposed, not done.

## Sources

- [srb2-switch](https://github.com/P-AS/srb2-switch) commit `90194061a5da6af7df48be82a097affafbf70fb4` — `README.md`: `make PLATFORM=Switch`, full-RAM warning (line 15), 800x450 default versus 320x200 (lines 26-28), `NXLINK=1` build (line 54).
- [taisei](https://github.com/taisei-project/taisei) commit `baf029b05b625f5543543b8d96ecf38ae8e7f1b1` — `src/arch_switch.c` line 81 (`NX_SETENV("MESA_NO_ERROR", "1")`), meson cross build files.
- [stk-code](https://github.com/supertuxkart/stk-code) commit `b2a9f223326422aa2dd4eea28319b0c5115fde03` — `CMakeLists.txt` (`NO_SHADERC` option at line 275), Switch packaging scripts and CI workflow.
- [godot_switch](https://github.com/redthing1/godot_switch) commit `0918643f62a6b7664a9d9bc6bcfdb280510a1b17` — `platform/switch/audio_driver_switch.cpp` (`AudioRendererOutputRate_48kHz`, `num_voices = 24`, `num_mix_buffers = 2`), scons build and exporter.
