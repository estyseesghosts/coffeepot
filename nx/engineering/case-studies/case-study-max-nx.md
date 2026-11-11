# Case study: Max Payne Switch port (max_nx, binary-wrapper port)

A Switch homebrew port of Max Payne Mobile (Android 1.7, `arm64-v8a`) running the original `libMaxPayne.so` natively. A launcher maps the shared object into memory, relocates it, resolves imports against Switch libraries, patches OS/graphics/audio entry points, and jumps into the game's main loop. Coffeepot has run none of this code.

## When is this useful?

Read this if you want to run an existing AArch64 Android game binary by loading it and hooking a few entry points instead of rewriting the engine.

## Before you start

- Evidence: every behaviour is `[documented]` in the vendored tree (`README.md`, `Makefile`, `source/`, `source/hooks/`). Source: [fgsfdsfgs/max_nx](https://github.com/fgsfdsfgs/max_nx) commit `264a0cc` (2025-11-27; see [source versions](../source-versions.md)).
- No performance figures: the tree states no frame rates, frame times, or clocks. Expectations below are inference with conditions unstated, never measured results.
- Baseline: GLES2 on `switch-mesa` / `switch-libdrm_nouveau`, RGBA8 with 24-bit depth + 8-bit stencil [documented]. 1280x720 handheld, 1920x1080 docked, overridable in `config.txt` [documented].
- Companion pages: [rendering](../rendering.md), [performance and porting](../performance-and-porting.md), [NX GPU](../../hardware/gpu.md).

Jargon: the **[CPU](../../../glossary.md#cpu)** runs logic and graphics calls; the **[GPU](../../../glossary.md#gpu)** draws; a **framebuffer** holds the pixels being rendered (**[framebuffer](../../../glossary.md#framebuffer)**); a **cache** is fast memory beside a core (**[cache](../../../glossary.md#cache)**). The SoC is the **[Tegra X1](../../glossary.md#tegra-x1)**, its GPU family **[Maxwell](../../glossary.md#maxwell)**, the homebrew user library **[libnx](../../glossary.md#libnx)**, full-memory launch via a game **[title override](../../glossary.md#title-override)**.

## What it is

| Layer | Approach |
| ----- | -------- |
| Engine | Stock Android binary executed in place; no source port [documented]. |
| Loader | Custom ELF loader: splits heap in `__libnx_initheap`, maps code via code-memory syscalls, relocates, resolves symbols, flushes caches, runs init array [documented]. |
| OS shim | Fake-Android layer: device spoofing, archive-path redirect, stubbed movie/network/vibration/JNI, libc shims [documented]. |
| Graphics | Four EGL functions hooked (init, make/unmake current, swap); GLES2 stream passes to Mesa untouched [documented]. |
| Audio | ~130 OpenAL symbols re-pointed at Switch openal-soft; the hook overrides a 22,050 Hz request with 44,100 Hz (`source/hooks/openal.c` line 28) [documented]. |
| Assets | Ships none; user extracts APK assets, the `arm64-v8a` `.so`, and OBB archives [documented]. |
| Simulation | Variable-step mobile loop preserved; `config.txt` scales decals, debris, shadows, LOD instead [documented]. |

## What makes it tick

The launcher reserves a 512 MB newlib heap, gives the rest to the mapped `.so`, then loads, patches, forces `UseRGBA8 = 1`, and calls the game's init and main entry [documented]. The game thinks it is on a Tegra phone: spoofed device IDs keep bloom off by default, and reported screen size matches the window size, so it renders at window resolution [documented]. Detail knobs hook the engine's detail getters, not shaders [documented].

On the ARM cores this is the easy case: already-AArch64 code needs no interpreter, only branch-trampoline hooks [documented]. Threading is the game's own, passed to plain thread creation with a faked handle; nothing pins threads, so game threads contend on title-override cores [documented]. One trap is explicit: the game's stack guard reads thread-pointer state that returns zero under Horizon, so the port installs a static buffer there (flagged `POSSIBLY VERY BAD` in-tree) [documented].

On Maxwell the path is GLES2 via Nouveau Mesa [documented]. Batch discipline is the 2012 mobile title's own; the port trims work with content scalers, not reordered batches [documented]. Swap is EGL-default with no frame-pacing or dynamic resolution, so docked 1080p costs ~2.25x handheld pixel fill on the same call stream [documented].

## What it gets right

| Technique | Why it works |
| --------- | ------------ |
| Native-arch reuse | arm64 binary needs no emulation; thousands of lines, not an engine port [documented]. |
| Tiny graphics seam | Four hooked EGL functions keep driver behaviour standard Mesa [documented]. |
| Content scalers as knobs | Bloom, mipmaps, shadows, LOD, decal/debris multipliers map onto fill-rate cost [documented]. |
| Loud failure mode | Startup checks data, `.so` symptoms, and syscalls, each with an on-screen error [documented]. |
| Memory honesty | Refuses applet mode (heap plus image cannot fit); requires title override [documented]. |

## What it gets wrong (costs and traps)

| Cost / trap | Detail |
| ----------- | ------ |
| No measured baseline | No fps, splits, or clock guidance; every knob is set blind [documented]. |
| Privileged syscalls | Needs code-mapping syscalls; stock firmware cannot run it [documented]. |
| Unpinned threading | Background threads can preempt render submission; no mitigation [documented]. |
| Full-rate 1080p docked | Same draws at 2.25x pixels, no dynamic resolution [documented]. |
| Fragile coupling | Only the 1.7 `arm64-v8a` `.so` resolves; names and offsets baked in [documented]. |
| Stubbed features | Movies, touch, vibration are stubs/TODOs; silent no-ops [documented]. |

## Lesson: what to copy first

| # | Symptom to fix |
| - | -------------- |
| 1 | Applet-mode OOM crash — split newlib heap from image memory; require title override [documented]. |
| 2 | `Could not find symbol` — pin the exact APK/ABI pair; print the missing symbol [documented]. |
| 3 | RGB565 banding — force RGBA8 targets when memory allows [documented]. |
| 4 | 1080p fill collapse — default bloom off via device spoof; live decal/shadow/LOD multipliers [documented]. |
| 5 | Silent missing-data boot — stat every archive first; name the missing file [documented]. |
| 6 | Exit leaks — capture device/context in hooks; tear down with EGL and unload [documented]. |

The lessons are inferences from the mechanisms the code shows. A `[documented]` tag cites the code that shows the mechanism, not a measurement that the fix works; none was tested on a console by Coffeepot.

## Open questions

- Held frame rate/split at 720p vs 1080p, at which clocks? Tree is silent; needs a measured run.
- Does 44.1 kHz resampling cost CPU vs 22,050 Hz passthrough? Unstated.
- Is the static-buffer thread-register hack safe under the full thread count? Flagged possibly very bad by the author.

## Sources

- `README.md` — scope, assets, dependencies, applet limit, syscalls. `Makefile` — CPU flags, Mesa/Nouveau/SDL2/openal linkage.
- `source/main.c` — heap split, checks, screen policy, load sequence. `source/so_util.c` — mapping, hooks, cache flush.
- `source/imports.c` — libc/pthread/time shims. `source/hooks/opengl.c` — EGL config, hooked functions.
- `source/hooks/openal.c` — OpenAL re-pointing, 44.1 kHz override. `source/hooks/game.c` — spoofing, input, getters, exit, thread hack.
- `source/config.c` / `config.h` — knob defaults and format.
