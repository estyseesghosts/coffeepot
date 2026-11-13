# Porting games and engines to the Nintendo 3DS

Porting a game is not the same as recompiling it. An existing engine may expect desktop OpenGL, a programmable pixel shader, hundreds of megabytes of memory, blocking file reads, or several fast CPU threads. The 3DS provides a much smaller ARM11 CPU budget, a **[PICA200](../glossary.md#pica200)** graphics processor with vertex shaders but *no programmable fragment shader*, and an operating system whose application must yield for HOME, sleep, and applets. A successful port changes the engine's work to fit those constraints while preserving the game's behaviour.

This is the **implementation guide**: start with a source tree, decide what is feasible, construct a native 3DS application, replace incompatible subsystems, bring up one representative scene, and validate it on hardware. It deliberately links to Coffeepot's detailed API and hardware pages instead of reprinting them. The most useful companions are [development workflow](../homebrew/development-workflow.md), [2D rendering](PICA200/rendering-2d.md), [GPU rendering](PICA200/gpu-rendering.md), [memory and cache](memory-and-cache.md), [performance](performance.md), [debugging](debugging.md), and [testing on hardware](testing-on-hardware.md).

**Evidence and verification.** This revision is an engineering procedure assembled from Coffeepot's source-checked pages as they stood on **10 November 2026**. API behaviour cited below follows the versions in [source versions](source-versions.md): devkitARM r68, libctru 2.7.1, citro3d 1.7.1, citro2d 1.7.0, picasso 2.7.2, and tex3ds 2.3.0. The architecture examples and pseudocode on this page **have not been compiled or run**; they are instructions to adapt, not drop-in programs. No performance numbers here are measurements made by Coffeepot. Measurements and decisions for a particular port must be recorded separately.

## 1. The route through a port

A workable order is:

1. **Inventory** the original engine, dependencies, game data, frame model, rendering paths, and memory needs.
2. **Set a hardware and fidelity contract.** Decide Old 3DS versus New 3DS, target frame rate, display layout, and what must remain exact.
3. **Build an empty native application** with the real devkitPro toolchain. Confirm startup, input, exit, error reporting, and hardware deployment.
4. **Make the original game's simulation run without its old renderer.** Keep a desktop reference build for comparison.
5. **Implement a small native renderer** for one representative scene. Do not carry an unbounded software-rendering fallback into every scene.
6. **Integrate assets, audio, input, saves, and lifecycle** as explicit platform backends.
7. **Instrument and profile** on the slowest supported physical console. Change one cause at a time.
8. **Expand content coverage,** regression-test effects and exceptional scenes, then package and release.

The first playable scene is not the first success criterion. Earlier successes are a cross-compiled executable, a game-state step that matches the reference, a frame whose CPU/GPU costs are known, and an asset budget that is bounded. They prevent months of making a broken or slow architecture more elaborate.

### Which approach fits the source?

| Original game or engine | Sensible starting point | Main risk |
| --- | --- | --- |
| Simple SDL or software 2D game with little per-pixel work | Reuse input/audio/files where useful; render sprites with **citro2d** | Software output copied every frame becomes the bottleneck |
| Tile/sprite game from a DS-like system | Preserve game logic and graphic state; translate tiles, maps, palettes, sprites and effects into PICA200 work | Exact priority, windows, blending and mid-scanline changes do not map automatically |
| Older fixed-function OpenGL 3D game | Replace the GL backend with **citro3d**; rewrite material states and data submission | Too many draws, texture changes, or CPU-transformed vertices |
| Modern GL/ES/Vulkan/D3D engine | Write a feature-by-feature feasibility analysis before touching its renderer | Fragment shaders, large textures, deferred rendering, compute, instancing, and memory assumptions |
| CPU renderer or emulated machine | First distinguish CPU-visible *emulation* work from display composition | GPU use cannot accelerate inherently serial CPU emulation; naive pixel uploads can dominate |
| Large engine with plugins, scripts, or dynamically loaded libraries | Isolate a small playable feature slice; review each dependency | Runtime memory, executable format, unavailable platform services |

A native citro3d/citro2d path is usually the target for a performance-sensitive Old 3DS game. A software path can be useful as a **correctness oracle** or for a small exceptional effect; its existence is not proof that it can sustain the intended frame rate. See [rendering strategies](PICA200/gpu-rendering.md) and [2D rendering](PICA200/rendering-2d.md).

### Lessons reported by one large port

Three public N64 ports report their techniques and results in [case studies: large 3DS ports](case-studies/case-studies-ports.md). They are `[reported]` evidence only, taken on mixed models (Old 3DS hardware, New 3DS, simulated Old 3DS speed, emulator), and not reproduced here. Choose a port strategy from them with these cautions:

- The one source with Old 3DS hardware numbers (G-Diffuser's F-Zero X native renderer) reports its largest gains from replacing a generic display-list interpreter with specialised draw paths, not from GPU tuning.
- A port that has only emulator or simulated-speed numbers (the Ocarina of Time research) has not shown an Old 3DS result; do not choose a design from it alone.
- Techniques that need core 2 (for example a render thread) are New 3DS techniques; audio on core 1 is available on Old 3DS; see the case-study techniques table for which are which.

## 2. Audit the source before porting it

Do this on a **desktop copy of the original project**. Do not begin by trying to make its entire build system target `arm-none-eabi`.

### 2.1 Establish a reference executable

Build the original, identify a reproducible scene, and save the exact revision and build options. Collect:

- A screenshot or frame capture at a known game state, ideally including translucent sprites, text, scrolling and depth/lighting.
- A deterministic input recording or scripted sequence that reaches that state.
- A gameplay-state checksum after a fixed number of updates (positions, animation state, script flags, random seed).
- The time consumed by update, rendering, file loading, and audio on the original machine, **with the host machine named**. Desktop timing does not predict 3DS timing, but it locates the original expensive subsystems.
- Peak and steady memory by class: code, world state, decoded resources, graphics resources, framebuffers, audio, transient scratch.

Keep the reference running after the platform port begins. When a sprite is drawn incorrectly on the 3DS, the original provides the expected visual; when a script fails, a deterministic state comparison distinguishes engine changes from platform changes.

### 2.2 Map platform dependencies

Search the source, then inspect the actual call sites. For a C/C++ repository, useful first passes are:

```sh
rg -n 'SDL_|gl[A-Z]|glfw|glew|vulkan|vk[A-Z]|D3D|DirectX|Metal' src include
rg -n 'pthread_|std::thread|std::async|atomic|mutex|condition_variable' src include
rg -n 'fopen|fread|fwrite|mmap|stat\(|opendir|rename\(|fsync' src include
rg -n 'malloc|calloc|realloc|new\b|memcpy|memmove' src include
rg -n 'double|sin\(|cos\(|sqrt\(|/\s*[a-zA-Z_]' src include
rg -n 'gl_FragColor|fragment|sampler|shader|framebuffer|RenderTarget' .
```

These are **search hints**, not exhaustive detection; adapt directories and names to the project. Trace the control flow from entry point through update, render, audio, event handling, and asset loading. Also find where the original decides resolution and time step, not merely where it calls a window API.

Create a `PORT_AUDIT.md` in the port's repository:

| Area | Original implementation | 3DS replacement | Coupling / risk | First test |
| --- | --- | --- | --- | --- |
| Entry / event loop | `main`, platform events | `aptMainLoop`, HID | High if rendering drives simulation | Quit, HOME, suspend |
| Rendering | SDL/OpenGL/custom | citro2d/citro3d | Usually high | Reference-scene screenshot |
| Textures | Arbitrary image uploads | Offline `.t3x`, tiled textures | High if content is generated at runtime | One atlas |
| Sound | Desktop mixer | `ndsp` + decode queue | Medium | Loop without underruns |
| Storage | Host filesystem | `romfs:/`, `sdmc:/` | Medium | Load and recover save |
| Threads | Desktop workers | Small 3DS worker set | Medium/high | Run on Old 3DS |
| Time | Host clocks, sleep | System ticks, frame pacing | High if coupled to draw calls | Logic tick rate |
| Memory | Host allocator | Ordinary, linear, VRAM | High | Peak scene |
| Network / services | Desktop sockets/HTTP | libctru services | Feature-dependent | Timeout and suspend |

For every library, determine whether it is (a) buildable for ARM11 and already supplied by devkitPro, (b) small enough to port, (c) replaceable by a 3DS library, or (d) inappropriate on this hardware. The [devkitPro portlibs](../homebrew/development-workflow.md) include codecs, compressors, fonts, physics and other packages; their existence does not mean a desktop integration is free.

**Audit literal hardware addresses as well as SDK macros.** Redirecting an SDK register macro to emulated memory does not redirect a source file's direct pointer literal. HGSS found crashes from DS I/O and VRAM addresses that bypassed the port's arrays. Scan the port-preprocessed sources for pointer literals, parse and range-check numeric values (including forms without leading zeroes), then inspect the resulting binary and test late-loading scenes. Treat this as a targeted audit, not proof that every indirect address has been found. [reported: project S15, November 2026; [case study](case-studies/case-study-hgss-port.md)].

### 2.3 List non-negotiable behaviour

Separate **correctness** from **presentation**. For example, a strategy game might require identical move order, script events, RNG, collision and save behaviour, but allow a different antialiasing mode, menu layout, or lighting approximation. A console-game port may require exact tick cadence, object ordering and pixel-effect masks. Write those rules down *before* replacing the renderer.

For proprietary games, verify that you may redistribute the source, engine, assets, or conversion tools. Usually the cleanest arrangement is to distribute the port and let each user supply their own legally obtained game data; do not embed commercial ROM content or signing keys.

## 3. Design against the real machine

### 3.1 Pick the lowest target you will actually test

| Constraint | Old 3DS family | New 3DS family | Consequence |
| --- | --- | --- | --- |
| ARM11 application CPU | 268 MHz application core; no L2 | Fast mode up to 804 MHz, with L2 | A New 3DS pass cannot establish Old 3DS performance |
| Application memory region | Normally 64 MiB | Normally 124 MiB | Both include executable and heap allocations; not all bytes are available to your game |
| Parallel CPU work | Core 0 plus a limited share of system core 1 | Core 0, core 2 where allowed, and limited core 1 | Do not infer desktop-thread throughput |
| GPU | PICA200 | PICA200 | Same rendering model; do not assume GPU work becomes 3× faster |
| VRAM | 6 MiB, two 3 MiB banks | Same | Colour/depth targets compete with textures |
| Displays | Top 400×240 per eye, bottom 320×240 | Same logical sizes | Stereo costs a second top-screen view; wide mode is separate |

Sources: [CPU](../hardware/cpu.md), [memory](../hardware/memory.md), [models](../hardware/models.md), [display](../hardware/display.md), [threading](threading.md). These are **capacities, not a promise that the loader grants all of them**.

A `.3dsx` normally inherits the memory mode of the title that launches it. A `.cia` may request a different application memory mode via its executable metadata; this changes startup/system behaviour and should not be treated as a free switch. On New 3DS, a `.3dsx` may need `osSetSpeedupEnable(true)` to request fast CPU/L2 mode. Read [memory mode](memory-and-cache.md), [New 3DS speed-up](performance.md), and [3DSX versus CIA](../homebrew/development-workflow.md) before depending on either.

### 3.2 Write budgets, including peaks

At **60 Hz**, the frame period is approximately **16.7 ms** (about 4.47 million Old 3DS core-0 cycles before other work); at **30 Hz**, **33.3 ms** (about 8.94 million). These are frame deadlines, **not** CPU processing allowances. CPU simulation, CPU draw recording, GPU commands, transfers and display waits can overlap; measure the critical path rather than summing unrelated timers. See [performance](performance.md) and [frame pacing](PICA200/frame-pacing.md).

Use a planning table like this; replace `?` with measurements and make the sum fit the smallest memory mode:

| Bucket | Persistent bytes | Worst transient bytes | Allocator | How measured |
| --- | ---: | ---: | --- | --- |
| Executable, static data, runtime | ? | ? | Application memory | Map / build output |
| Game state and script VM | ? | ? | Normal heap | Heap counters |
| Decoded map and asset data | ? | ? | Normal heap | Loader metrics |
| Vertex/index buffers and texture staging | ? | ? | Linear heap | Allocation ledger |
| Resident textures | ? | ? | Linear heap or VRAM | Texture ledger |
| Audio queues | ? | ? | Linear heap | `ndsp` ledger |
| Screen colour/depth, cached layers | ? | ? | VRAM | Exact dimensions/formats |
| File/decompression scratch | ? | ? | Appropriate heap | Cold-load peak |
| Margin for fragmentation / future content | ? | ? | All pools | Deliberate reservation |

**Useful arithmetic:** 400×240 RGBA8 = 384,000 bytes; a 400×240 depth24/stencil8 buffer is another 384,000 bytes. A 512² RGBA8 texture is 1 MiB; RGB565 is 512 KiB; ETC1 is 128 KiB (before file/container overhead). Stereo may require both top-eye targets and extra depth/colour; count each actual allocation. A 256² RGBA8 texture with full mipmaps takes about 341 KiB, not 256 KiB. The VRAM bank layout can prevent a large allocation even when the total free count seems sufficient. [Memory and cache](memory-and-cache.md), [GPU render-target budget](PICA200/gpu-rendering.md).

Do not call the full 64 MiB an “available heap.” Read `osGetMemRegionSize(MEMREGION_APPLICATION)`, `linearSpaceFree()`, and `vramSpaceFree()` on your running build; normal-heap availability also depends on the executable and configured heap split. Default libctru heap allocation, `__ctru_heap_size`, and `__ctru_linear_heap_size` are described in [memory and cache](memory-and-cache.md).

### 3.3 Decide screen layout early

The **logical** top screen is 400×240; the bottom touch screen is 320×240. citro2d uses top-left origin, x right and y down. Raw citro3d render targets for the physical LCD are commonly **240×400** and **240×320** because the panels are rotated; do not confuse those two coordinate conventions. [Display](../hardware/display.md), [2D rendering](PICA200/rendering-2d.md).

Choose and document one option for every view: native-resolution centred, aspect-preserving fit, expanded field of view, UI re-layout, or a separate bottom-screen interface. For an original 256×192 DS picture, scaling to 400×240 is **non-integer** (different horizontal and vertical factors); nearest filtering can create uneven pixel widths and bilinear filtering can blur. A 1:1 centred image is simpler and exact but leaves borders. **Never silently stretch menus, touch hitboxes and world cameras together.** Test text legibility at physical size.

Treat stereoscopic 3D and 800×240 wide mode as *optional presentation features*, not free enhancements: stereo requires two top-eye views; wide mode doubles horizontal fill and is mutually exclusive with stereo. Hardware/model restrictions are in [GPU rendering](PICA200/gpu-rendering.md) and [display](../hardware/display.md). Begin with one 2D top-eye target and one bottom target, or just the screen your game actually uses.

## 4. Bring up a native build before moving game code

Follow [Nintendo 3DS homebrew setup and development workflow](../homebrew/development-workflow.md) for **Windows devkitPro MSYS2, macOS, Linux/WSL2 and Docker**. The point is to use the maintained compiler, `libctru` startup, link script, and packaging rules rather than inventing your own.

With `3ds-dev` installed and `$DEVKITARM` set:

```sh
git clone https://github.com/devkitPro/3ds-examples.git
cp -R 3ds-examples/templates/application ctr-bootstrap
cd ctr-bootstrap
make
```

For a host using the official Docker image, the build command from the directory containing the Makefile is:

```sh
docker run --rm -v "$PWD:/work" -w /work devkitpro/devkitarm:latest make
```

**Windows PowerShell Docker syntax** is different; use the command in [development workflow](../homebrew/development-workflow.md) instead. For repeatable builds, pin and record a tested image tag rather than `latest`.

Build and run the unmodified template first. Keep the resulting `.elf` (symbolized debugging) alongside `.3dsx`. Start the latter in an emulator for quick logic checks, then on hardware through the Homebrew Menu or wireless netloader:

```sh
3dslink -s ctr-bootstrap.3dsx
```

In Homebrew Menu, press **Y** to enable netloader. `3dslink` may need `-a <console-ip>` if discovery fails; the console and PC must share a network. [Testing on hardware](testing-on-hardware.md), [debugging](debugging.md).

Before moving any game code, prove that the bootstrap can **open both needed screens, read keys, survive several minutes, exit cleanly, resume after HOME/lid close, and print or persist a fault**. A project that launches only in Azahar has not completed this step.

### Suggested source boundaries

The exact directory names are optional; the dependency direction is not:

```text
src/
  game/                     # simulation, entities, scripts, RNG; no libctru calls
  render/
    scene.h                 # semantic render commands, materials, draw order
    common/                 # optional platform-independent culling / batches
    ctr/                    # citro2d/citro3d renderer
    reference/              # existing desktop renderer, if retained
  platform/
    platform.h              # clock, input, files, audio, lifecycle contracts
    ctr/                    # libctru backends
    desktop/                # correctness reference and automated tests
  assets/                   # source art and reproducible converters
  tests/                    # replay inputs, expected hashes, visual fixtures
```

The simulation should produce **renderable scene data**, not arbitrary OpenGL calls. The renderer consumes that data. Platform input should be converted into game actions, not exposed as `hidKeysHeld()` throughout gameplay. File loading should request logical resources, not embed `romfs:/...` in game rules. This boundary permits desktop-versus-3DS comparison and makes renderer rewrites affordable.

**Do not translate every old API call one-for-one.** That approach preserves desktop batching, draw order, and upload behaviour even when all three are wrong for the PICA200. Rebuild the *work units*—visible tile spans, sprite runs, mesh groups, material passes, cached backgrounds—around the destination hardware.

## 5. Integrate the 3DS application lifecycle

The entry loop must cooperate with **APT**, the operating-system application manager. `aptMainLoop()` processes HOME, sleep, applets and termination; it may block while another application is in control. Other threads do **not** automatically stop when it blocks. [Application lifecycle](application-lifecycle.md).

A minimal **structure**, not a tested stand-alone program:

```c
/* Illustrative structure: implement the application_* and renderer_* functions. */
gfxInitDefault();
/* Initialise citro3d/citro2d, if using them; create targets. */
application_init();

while (aptMainLoop()) {
    hidScanInput();              /* once per outer iteration */
    application_read_input();    /* turn hardware state into game actions */
    application_step_due_ticks();/* fixed game time, not frame count */
    application_service_audio(); /* queue/refill without blocking on GPU */
    renderer_draw_if_ready();    /* record and submit when resources are ready */
}

application_shutdown();
/* Free app graphics/audio and render targets before library teardown. */
/* C2D_Fini(); C3D_Fini(); gfxExit(); where used. */
```

- Register `aptHook()` and handle `ONSUSPEND`, `ONRESTORE`, `ONSLEEP`, and `ONWAKEUP`. Hooks should set state flags and park workers, **not** do slow file I/O or GPU work. Redraw after restore. See the [complete lifecycle example](application-lifecycle.md).
- Treat a long suspend as a **clock discontinuity**. Reset the fixed-step accumulator or cap catch-up; do not simulate minutes of elapsed wall time on wake.
- Shutdown after `aptMainLoop()` returns false: stop worker production, finish or cancel I/O, save while storage services remain available, empty/stop audio queues, wait for GPU use to finish, free resources in dependency order. Do not depend on an `ONEXIT` hook to write saves.
- For an installed `.cia`, verify changes in HOME, applets and memory mode separately from `.3dsx` tests.

### Input mapping

Use `hidScanInput()` once before reading `hidKeysDown()` (edge), `hidKeysHeld()` (level), or `hidKeysUp()` (release). `hidTouchRead()` returns bottom-screen coordinates in 320×240 space; only use them as an active touch when `KEY_TOUCH` is held. Read Circle Pad through `hidCircleRead()`. [Input and sensors](input-and-sensors.md).

Define game actions such as `MOVE_X`, `CONFIRM`, `BACK`, `CAMERA_X`, `OPEN_MAP`, `TOUCH_SELECT` and map them in one module. Give menus, camera, and gameplay their own input contexts. Keep the New 3DS C-Stick and ZL/ZR as **enhancements**; Old 3DS has no equivalents. Explicitly define the fallback. For an original game that relied on mouse, keyboard or a second analog stick, design the replacement interaction rather than quietly dropping it.

### Files and saves

Use **`romfs:/`** for read-only game assets (after `romfsInit()`), **`sdmc:/3ds/<your-app>/`** for writable files in an ordinary `.3dsx`, and title-specific save archives if building an installed title that needs them. Bundle or derive a stable content manifest and validate filenames at start-up. Do not assume host filesystem case rules, absolute paths, or atomic rename semantics. [Filesystem and saves](filesystem-and-saves.md).

For a robust save system, include magic, version, length, generation and checksum; keep two generations and select the newest valid one. Check *every* write, flush, sync and close. An installed title's save archive requires an explicit commit (`archiveCommitSaveData()`), which normal `sdmc:/` files do not. Test full SD, interrupted write, corrupt file and old save version—not just a successful save/load.

### Audio is a deadline, not a decorative worker

Use libctru's **`ndsp`** to queue PCM wave buffers in *linear memory*. The DSP consumes audio independently of video; one audio buffer must not be reused until its status is `NDSP_WBUF_DONE` or `NDSP_WBUF_FREE`. Flush CPU-written data before queueing it. See [audio programming](audio.md).

The DSP output rate is about **32,728 Hz**, and `ndsp` offers 24 channels. Your source content can have a different rate: `ndspChnSetRate()` configures resampling. Start with a looped PCM tone or short sound effect before bringing in music codecs. For music, decode Vorbis/Opus/tracker data ahead into a small rotating PCM queue. `ndspInit()` can fail if DSP firmware is not available; provide the documented `/3ds/dspfirm.cdc` remedy rather than crashing.

**Audit questions:** What is the longest delay between refills? How much queue time is available? Does the decoder touch SD during a critical frame? Can it block behind a texture loader? Are audio ticks or music events coupled to rendered frames? Count `ndspGetDroppedFrames()` and record the longest refill gap. A visually smooth port with constant audio crackles is not performant.

### Threads: prove benefit before adding them

On Old 3DS, the reliable application core is core **0**. Core **1** is shared with the OS and requires `APT_SetAppCpuTimeLimit(percent)` before creating the allowed worker; inspect the granted value with `APT_GetAppCpuTimeLimit()`. On New 3DS, core **2** can be used where title permissions allow. Core 3 is not an ordinary application core. [Threading](threading.md).

Start single-threaded except where audio requires predictable service. Add a worker only for separable throughput work, e.g. decompression or building next-scene data. Its queue must have bounded capacity and ownership. Never hold a global renderer lock across an SD read or wait for a core-1 task that may be time-sliced for tens of milliseconds. A larger core-1 share can slow GSP/DSP/FS services, so higher utilization is not necessarily higher frame rate [documented mechanism; actual impact must be measured].

### Network, peripherals and uncommon services

If the original needs networking, sockets need `socInit()` with a **page-aligned** work buffer; that buffer becomes inaccessible until `socExit()`. Use nonblocking I/O and explicit timeouts. Do not stall gameplay on DNS, TLS, matchmaking or an HTTP request. See [networking](networking.md) and [IPC and services](ipc-and-services.md).

Camera, microphone, infrared, NFC and local wireless are separate service integrations with permission and model restrictions, not desktop-style plug-ins. Read [input and sensors](input-and-sensors.md), [camera and Y2R](camera-and-y2r.md), [microphone](microphone.md), [local wireless](local-wireless.md), and [NFC and infrared](nfc-and-infrared.md). If a required service is unavailable in an ordinary homebrew process, document that limitation before making gameplay depend on it.

## 6. Replace the renderer: decide what the GPU will actually draw

On the 3DS, the PICA200 executes command lists submitted through the **GSP** system service. **citro3d** is the maintained high-level interface for 3D buffers, render targets, shaders, textures and fixed-function pixel state; **citro2d** builds sprite, image, text and shape batching on top of it. [GPU command pipeline](PICA200/pica200-command-pipeline.md), [GPU rendering](PICA200/gpu-rendering.md), [2D rendering](PICA200/rendering-2d.md).

Before writing a new backend, capture the **meaning** of each source draw: geometry, material, transform, clip/window, blend mode, priority, target and visibility. A raw list of old GL calls often obscures these semantics. The aim is to reproduce the resulting image using a small number of *PICA-compatible jobs*, not to emulate a desktop graphics driver call by call.

### 6.1 An operational graphics translation table

| Original concept | 3DS mechanism | What the implementer must change |
| --- | --- | --- |
| GLSL vertex shader | `picasso` `.v.pica` assembled to `.shbin` | Rewrite instructions and uniform mapping; PICA vertex arithmetic uses 24-bit floats |
| GLSL fragment shader | Six **TEV** colour-combiner stages, fragment lighting LUTs, fog, texture units and blending | Reduce/rewrite the effect; there is no general fragment program |
| GL uniforms, program binding | `shaderProgram`, `shaderInstanceGetUniformLocation`, `C3D_FVUnif*` | Cache locations; update only when values change |
| `glVertexAttribPointer` / VBO | `C3D_AttrInfo` + `C3D_BufInfo` | Choose compact interleaved vertex layouts in GPU-addressable memory |
| `glDrawArrays` | `C3D_DrawArrays` | Combine submissions that share state and order |
| `glDrawElements` | `C3D_DrawElements` | 8- or 16-bit indices only; split oversized meshes |
| `glTexImage2D` | `tex3ds` + `Tex3DS_TextureImport*`, or tiled runtime upload | Convert offline; use GPU-compatible layout and formats |
| Desktop framebuffer object | `C3D_RenderTargetCreate*` | Budget colour/depth storage in VRAM and avoid feedback loops |
| Blending / stencil / depth / alpha test | `C3D_AlphaBlend`, `C3D_StencilTest`, `C3D_DepthTest`, `C3D_AlphaTest` | Verify exact order, write masks, depth and alpha conventions |
| Scissor | `C3D_SetScissor` | Check physical target orientation and clipping coordinates |
| Multiple render targets / deferred pipeline | Limited explicit passes, smaller cached surfaces or forward shading | Do not assume desktop G-buffer architecture will fit |
| Instancing / compute shaders | No direct equivalent | CPU-build shared batches, reuse geometry; sometimes use PICA geometry shader |
| A window swap | `C3D_FrameBegin`, draw targets, `C3D_FrameEnd` | GPU work runs asynchronously; screens are separate targets |

This is a **design mapping**, not a complete API equivalence. For actual calls, state lifetime and quirks, read [picasso and citro3d](../homebrew/picasso-and-citro3d.md), [shader programming](PICA200/pica200-shader-programming.md) and [fixed-function API](citro3d-fixed-function-api.md).

### 6.2 Bring-up order for any native renderer

1. Clear one screen with a known colour. Verify the right screen and correct orientation on hardware.
2. Draw one hard-coded triangle or one sprite. Do not load game assets yet.
3. Bind one converted texture, with visible top/left markings that expose rotation and V-coordinate mistakes.
4. Add an orthographic or perspective transform. Verify scale and handedness with axes.
5. Add source alpha, cut-out alpha, opaque depth, and a visible clipping rectangle as separate test cases.
6. Add texture/material changes and inspect draw-call count and state transitions.
7. Render the reference scene at real resolution. Record CPU recording and GPU timing independently.
8. Only then turn on scene streaming, large atlases, dynamic geometry, stereo, wide mode, or multi-pass effects.

Use the maintained [3DS graphics examples](../homebrew/3ds-examples.md) as the source of buildable initial programs. The [complete 2D sprite example](PICA200/rendering-2d.md) and [tile-renderer example](PICA200/2d-tile-renderer.md) are additional Coffeepot references; their pages state their build/test status.

### 6.3 Porting ordinary 2D games

For games whose frames consist mostly of sprites, tilemaps, HUD elements and text, start with **citro2d**. It draws quads on the GPU rather than rasterising each sprite on the ARM11. The core operation is to create screen targets once, build/load atlas textures, and draw visible objects between `C3D_FrameBegin`/`C3D_FrameEnd`. [2D rendering setup and complete example](PICA200/rendering-2d.md).

The renderer should receive a small command vocabulary, for example:

```text
BEGIN_SCREEN(top)
DRAW_TILE_LAYER(map_id, camera, palette_state, priority)
DRAW_SPRITE(image_id, x, y, flip, tint, priority)
PUSH_CLIP(rect_or_mask)
DRAW_TEXT(layout_id, x, y, colour)
POP_CLIP()
END_SCREEN
```

This is **an illustrative engine-side data model**, not a citro2d API. It keeps the original game's rules from manipulating GPU state directly. The backend can then cull, reuse cached layers, batch compatible draws and expose a debug description of exactly what was submitted.

**Batching discipline:** citro2d groups objects into runs; a change of texture, tint/fade, view transform, or another relevant state can end a run. It has a finite object buffer (`C2D_Init` capacity; default 4096). Draw sprites from the same atlas together **only inside sections where doing so preserves layer order, clip, blend and priority**. `C2D_Draw*` returning false may mean the object limit has been reached—track failures. See [citro2d batching](PICA200/gpu-rendering.md) and [2D performance decisions](PICA200/rendering-2d.md).

**Visible-tile culling:** for `TILE`-sized map cells and a camera at `(camX, camY)`, compute the first and last visible columns/rows rather than looping through the entire map. Example for a 400×240 top-screen view:

```text
tx0 = floor(camX / TILE)
ty0 = floor(camY / TILE)
tx1 = floor((camX + 400 - 1) / TILE)
ty1 = floor((camY + 240 - 1) / TILE)
```

Clamp the resulting indices to the map. With 16×16 tiles, a visible map window contains about 375 tiles when aligned, or up to 416 when offset; a 128×96 world contains 12,288 tiles. Reducing submitted work to what is visible matters. The full [2D tile renderer](PICA200/2d-tile-renderer.md) demonstrates camera clamping, visible-cell iteration, animation, sprite sorting, cached layers, and an on-screen performance overlay.

**Text:** pre-parse strings that change rarely rather than re-laying out every glyph each frame; use `C2D_Text`, `C2D_TextBuf` and a font from the system or a converted `.bcfnt`. Text is not free when interleaved with sprite atlases. Keep caches keyed to content, font, size and layout parameters. [2D rendering](PICA200/rendering-2d.md), [asset pipeline](PICA200/asset-pipeline.md).

**Draw-order trap:** translucent sprite pixels and depth writes may interact differently from the original engine. Use an explicit ordered sequence for overlapping alpha-blended sprites. Treat cut-out sprites, additive effects, colour-math and clipping as separate material families and write image fixtures for each.

### 6.4 Porting a DS-style tile and palette renderer

A Nintendo DS graphics engine does more than draw pre-coloured sprites. Its 2D output can depend on character/tile data, maps, palette banks, scrolling registers, sprite attributes, layer priorities, windows, blending/brightness and scanline-specific register changes. The 3DS has **no matching DS 2D hardware pipeline** in native 3DS mode. It must reconstruct the result with the PICA200. [2D rendering: porting a tile- and palette-based original](PICA200/rendering-2d.md).

Do not replace a CPU DS renderer with “upload the whole 256×192 framebuffer every frame” and expect a fast port. That can be a correct reference implementation, but it still rasterises the original scene on the slow CPU and pays conversion/transfer costs. Instead, identify **what changes** between frames and keep the unchanged work resident on the GPU.

| Original feature | Native-renderer representation | Invalidation and correctness test |
| --- | --- | --- |
| 4bpp/8bpp tiles + palette bank | Expand a tile or tile page into a direct-colour texture; cache by **tile bytes and effective palette state** | Change one palette entry and verify every affected cached tile changes |
| Background tilemap | Visible quads from an atlas, or a cached texture for a map window | Scroll one pixel: no stale edge column, no whole-map rebuild |
| Animated tile | Switch atlas frame or update only its tile/page | All frames, correct period and palette |
| Character sprite | Atlas quad with position, flip, priority and tint | Overlap two sprites; test transparent corners |
| Background/sprite priority | Explicit layer runs, potentially depth/stencil for suitable cases | Equal-priority and priority-boundary fixtures |
| Rectangular window | Scissor or a suitable mask pass | Inside/outside pixels and edge positions |
| Irregular/object window | Stencil mask or another targeted composition pass | Shape boundary and masking under movement |
| Blend/brightness registers | Material/blend/TEV state or a separate composition pass | Compare each coefficient, target mask and alpha corner |
| Mosaic | Quantised coordinates or preprocessed source | Exact grouping and origin |
| H-blank/scanline scroll effects | Group identical spans into a small number of strips | Compare every boundary; avoid 192 tiny draws where possible |
| Capture/bitmap/unusual modes | Dedicated tested path or full-screen fallback | Same-state reference screenshot |

**Use dirty tracking, not wishful caching.** The cache key must reflect the *meaning* of its content (tile/palette/mode/bank mapping). Detect writes from every valid path, including DMA or bank remapping if the original machine is being emulated. If you cannot track a particular writer, invalidate conservatively. Do not cache by a pointer alone when memory contents can change behind it.

**Fallback contract:** keep a correct software/reference renderer available while translating effects. Every GPU fast path should report the precise reason for rejection: `unsupported_window`, `untracked_palette`, `capture_mode`, `too_many_spans`, `unsupported_blend`, etc. A path taken on only 5% of frames will not rescue performance. **Count acceptance rate by scene** as well as speed. A hybrid that composes CPU and GPU layers must reproduce final priority, alpha and windows; otherwise fallback for the whole affected surface. This set of techniques is a [hypothesis] for any particular port until correctness and performance are compared on hardware.

**Three independently useful comparisons:** (1) captured-state offline fixtures against the reference renderer, (2) same-state replay with both backends in the same build, and (3) old/new physical 3DS tests. They catch different failures. The emulator is additional debugging coverage, not a substitute for the third. [Rendering-2D](PICA200/rendering-2d.md), [testing on hardware](testing-on-hardware.md).

**Cached pictures require coherent inputs.** A signature must describe the exact snapshot used for rendering; a hash taken before or after a draw is not sufficient if a writer can mutate inputs during it. Separate volatile execution/timing state from effective visual state without discarding interrupt-driven effects. The [visual-state signature plan](PICA200/rendering-2d.md#cache-effective-visual-state-not-raw-register-blocks) details dependency and race checks.

**Caching decoded geometry requires its dependencies.** SM64DS Enhanced describes memoizing DS display-list decoding while preserving dynamic matrix operations [reported: project patch, revision and test model unspecified; [case study](case-studies/case-studies-rendering-tradeoffs.md#ds-geometry-translated-to-native-draws)]. List address alone is not a sufficient key if bytes or inherited graphics state can change. Track those inputs, invalidate when they change, and compare cached execution with the uncached reference on identical captured state. Count hits, invalidations and mismatches before relying on the cache.

### 6.5 Porting 3D geometry and material pipelines

For a fixed-function or early-shader-era 3D engine, translate mesh/material submissions into **sorted render passes**:

1. Transform/cull objects or batches on the CPU as needed; do not generate triangles for invisible objects.
2. Group opaque objects by compatible pipeline state and texture while keeping required depth semantics.
3. Submit compact vertex/index buffers from GPU-addressable memory.
4. Supply model/view/projection and other uniforms to a `picasso` vertex program.
5. Configure texture sampling, TEV stages, lighting/fog and blend/depth state.
6. Draw translucent objects in an order that preserves their correct composition.
7. Draw UI last, preferably with a separate 2D path.

**Vertex streams.** `C3D_AttrInfo` describes attributes; `C3D_BufInfo` describes buffer address and stride. Do not assume that a desktop vertex struct with `float` positions/normals/UVs is the cheapest on ARM11. Where precision permits, pack colours, UVs and normals to smaller formats; verify that attribute offsets and shader input declarations match byte-for-byte. The GPU supports unsigned-byte and unsigned-short indices, not 32-bit indices; partition large meshes. Indexing only helps when vertices are *actually shared*—otherwise conversion and indirection can cost more. [GPU rendering](PICA200/gpu-rendering.md), [GPU command pipeline](PICA200/pica200-command-pipeline.md).

**Vertex versus pixel work.** Use the vertex shader for transformations and interpolated quantities; the fragment side uses six TEV stages and specialized fixed-function units, **not GLSL**. Typical material replacements:

| Desktop effect | Practical PICA200 direction | Caveat |
| --- | --- | --- |
| Textured material × vertex tint | TEV texture × primary colour | A straightforward one-stage operation |
| Unlit or simple Gouraud colour | Interpolated vertex colour | Check precision and colour format |
| Two-texture blend | Two texture units + TEV interpolate | Validate alpha and blend order |
| Baked lightmap | UV/multitexture + TEV multiply | Prefer offline baking if original lights are static |
| Standard fragment lighting | PICA fragment lighting with LUTs | Requires correct vertex outputs and setup; not arbitrary BRDF code |
| Fog | Fixed-function fog | Match original range only where possible |
| Screen-space pixel shader | Bake, approximate with small passes/TEV, or remove | Not directly portable |
| Shadow map | Texture/render-to-texture approach where feasible | Costs render target, depth pass and bandwidth |
| Per-pixel palette-index lookup | Offline/CPU expansion into GPU-colour textures | No general shader-based texture lookup indirection |

TEV configuration uses `C3D_GetTexEnv`, `C3D_TexEnvSrc`, `C3D_TexEnvFunc`, and other texenv calls; lighting/fog/stencil use the separate groups documented in [citro3d fixed-function API](citro3d-fixed-function-api.md). Test a single material in isolation before building an automatic translator. **Generate a lookup table of supported material combinations**; unsupported combinations should fail visibly or select an explicitly documented approximation.

**Shader implementation.** Assemble `.v.pica` and, when genuinely helpful, `.g.pica` with `picasso`, load the `.shbin` via `DVLB_ParseFile`, and bind a `shaderProgram`. A 3DS vertex program has different register and output semantics from GLSL, including 24-bit floating-point values. Keep world coordinates near the camera/object origin to reduce precision loss. If you attempt geometry-shader expansion of sprites or particles, measure it against CPU-batched quads; the existence of a geometry shader is not evidence of a speedup. [Shader programming](PICA200/pica200-shader-programming.md), [picasso](../homebrew/picasso-and-citro3d.md).

**Batch within state boundaries.** `C3D_Draw*` records commands and the dirty state that precedes them. Too many small draws waste ARM11 recording time and GPU command-list space. Conversely, merging objects across texture, stencil, clip or blend changes can corrupt the image. Batch runs of compatible material/state and preserve required order. [GPU command pipeline](PICA200/pica200-command-pipeline.md).

### 6.6 Screen targets, depth, stereo and cached passes

`C3D_RenderTargetCreate` allocates colour/depth storage; use the actual target format and dimensions to account for every byte. `C3D_RenderTargetSetOutput` associates a target with an LCD screen; off-screen targets can be used for pre-rendered scenery or additional passes. The GPU's tiled target format and display-transfer stage are not interchangeable with ordinary row-major CPU buffers. [GPU rendering: frame anatomy and render targets](PICA200/gpu-rendering.md).

For optional stereo, prepare the camera for left and right eyes and render the top scene per eye. Read `osGet3DSliderState()` or the documented stereo-enable state, and **skip the second eye when stereo is off**. A static bottom screen need not be redrawn every frame. For a cached background, render it only when content or effect state changes, then sample it in a **different** pass—never sample a texture while drawing into the same texture.

A render-to-texture cache is useful only if its update frequency, resolution and memory cost beat redrawing the original primitives. A 512² RGBA8 colour surface is already 1 MiB and may need a depth surface too; a second full-screen pass can also cost more than expected. Test both paths.

## 7. Build an asset pipeline for the destination, not for the desktop

The main rule is to perform predictable, repeatable conversion **on the development machine**, then make the console load resources already suited to its GPU and memory. See [asset pipeline](PICA200/asset-pipeline.md) for the precise conversion options and asset types.

### 7.1 Inventory and classify every asset

Create a manifest that records: source path, logical resource ID, source dimensions/format, chosen 3DS format, converted byte size, expected in-memory size, atlas membership, load group, and whether the asset is persistent or streamed. Separate original data retained for gameplay from *render-ready assets*. Do not make the game discover assets by scanning arbitrary host directories at runtime.

A practical tree is:

```text
assets-src/
  sprites/             # authoritative art / source data
  textures/
  music/
  maps/
assets-build/
  graphics/            # generated .t3x and headers
  audio/               # generated compressed music or PCM
  packs/               # generated indexed resource packs
romfs/
  graphics/
  maps/
  audio/
```

The names are a suggestion. Whatever you choose, make conversion deterministic and rerunnable: committing a human-edited binary cache without its source image or options makes later optimization unnecessarily difficult.

### 7.2 Convert texture data deliberately

The PICA200 samples **tiled** textures in 8×8 blocks (Morton order within a tile); it does not accept a normal row-major RGBA array as an ordinary GPU texture. The supported dimensions are powers of two, **8–1024** per side. Use `tex3ds` for source art instead of runtime swizzling whenever possible. [GPU rendering](PICA200/gpu-rendering.md), [asset pipeline](PICA200/asset-pipeline.md).

Example `sprites.t3s` input file:

```text
--atlas -f rgba5551 -z auto --border edge
player_idle.png
player_walk.png
ui_cursor.png
```

Example converter invocation from an asset-build rule:

```sh
tex3ds -i sprites.t3s -H sprites.h -d sprites.d -o sprites.t3x
```

This is the documented **tex3ds 2.3.0 command shape**; filenames depend on your project, and the images must exist relative to the input file as the converter expects. The official template can wire `.t3s` conversion into Make; see [asset pipeline](PICA200/asset-pipeline.md). For this version, the wiki records a Windows build rejecting some short option aliases, so prefer `--atlas` and `--border` in scripts.

Choose memory formats by visual requirement:

| Asset | Starting candidate | Why / trade-off |
| --- | --- | --- |
| Opaque pixel art | `rgb565` | 2 bytes/pixel, no alpha |
| Hard-edged sprites | `rgba5551` | 2 bytes/pixel with 1-bit transparency |
| Smooth alpha UI | `rgba4` | 2 bytes/pixel; test banding |
| High-quality alpha | `rgba8888` | 4 bytes/pixel, expensive |
| Painted/photographic 3D textures | `etc1` or `etc1a4` | Hardware compression, but lossy and unsuitable for crisp text/pixel art |
| Glyph masks / grayscale | `a8`, `l8`, `la88` as needed | Small and directly sampled |

Atlas together what is **drawn together**, not everything in the game. A single huge atlas can force unrelated screens to stay resident, cause allocation failure, or increase reload time. Use load groups such as `common UI`, `current map`, `battle`, and `next scene`. With linear filtering, pad atlas borders to prevent neighbouring art bleeding; with nearest sampling, align geometry to whole pixels where pixel accuracy matters.

### 7.3 Runtime-generated and frequently changing textures

Some games really do create images at runtime: user drawings, palette-expanded tiles, dynamic signs, map caches, video frames, procedurally generated textures. For those, establish an explicit conversion/upload path:

```text
source bytes (normal heap)
      -> convert/expand only dirty region
      -> GPU-compatible tiled staging data (linear heap)
      -> clean/flush written cache range
      -> submit upload or use valid GPU texture memory
      -> retain texture until GPU work using it completes
```

**Never let a CPU writer overwrite a buffer still in use by the GPU.** Use two buffers or defer writes until the previous GPU queue finishes. Every upload should report original bytes, converted bytes, conversion time, flush time and submission time. The [memory and cache](memory-and-cache.md) page gives the exact clean/invalidate semantics and texture-upload choices.

### 7.4 Avoid transition-time memory spikes

Count peak residency of **old scene + next scene + decompression scratch + textures + audio**, not merely the stable state after a load. When memory is tight, unload a safe group before loading another and use a loading screen that still services audio and APT. Prefer a bounded working set and prefetch only assets a transition predicts. Record cold load time, warm load time, cache hit/miss counts, evictions and failures. [Asset pipeline: streaming and hitches](PICA200/asset-pipeline.md).

Compressed files save SD space and reads but cost CPU decompression; do not assume one codec is best. Test file reading, decompression, format conversion, swizzling, flush and upload as **separate stages**. A cache with a nominally high hit rate can still stall badly when a nearly full atlas allocator has to evict or rebuild many entries.

## 8. Make memory ownership and CPU/GPU synchronization explicit

The most important division is between three memory pools:

- **Ordinary heap (`malloc`, `new`)**: game state, CPU-only arrays, scripts, decoded content before upload.
- **Linear heap (`linearAlloc`, `linearMemAlign`)**: GPU-visible vertex/index data, CPU-written textures, command buffers and DSP wave buffers.
- **VRAM (`vramAlloc`, citro3d render targets, optional `C3D_TexInitVRAM`)**: GPU-local textures and render surfaces. It is limited to 6 MiB in two banks; do not assume the CPU can write it like RAM.

See [memory-and-cache](memory-and-cache.md) for actual allocator behaviour and [hardware memory](../hardware/memory.md) for the physical layout.

### 8.1 Treat every shared buffer as an object with states

A practical state machine:

```text
FREE -> CPU_WRITING -> READY_TO_SUBMIT -> GPU_OR_DSP_IN_FLIGHT
                                             |
                          FREE <- COMPLETED -+
```

Ownership changes at a known point. For standard citro3d frames, `C3D_FrameBegin` waits for the prior submitted frame to finish; its return is a useful ownership boundary. Data generated **before** that return must not overwrite storage still being consumed by the previous frame. A buffer queue can hold “being rendered”, “ready next”, and “being produced”; it must never allow the producer to reuse the in-flight slot. [Frame pacing](PICA200/frame-pacing.md), [memory and cache](memory-and-cache.md).

### 8.2 Cache coherency: the minimum required rules

The ARM11 CPU has a write-back cache; GPU/DSP engines do not automatically see modified cache lines.

- **CPU wrote, device reads:** clean/flush the written range before submitting device work.
- **Device wrote, CPU reads:** wait for completion, then invalidate the relevant CPU cache range before reading.
- **Do not invalidate dirty CPU data** you still need; doing so discards changes.
- Align shared buffers and record their owner, size and last fence/completion point.

`svcStoreProcessDataCache`, `svcFlushProcessDataCache`, `svcInvalidateProcessDataCache`, and the GSP/DSP helpers have different semantics and costs; do not interchange them blindly. By default, citro3d's `C3D_FrameEnd(0)` flushes the **whole linear heap** in the source version checked here. It is a safe default during bring-up. An advanced optimization is to flush only tracked dirty ranges and call `C3D_FrameEnd(GX_CMDLIST_FLUSH)`; this is unsafe unless **every CPU-written GPU input** is accounted for. Keep a debug switch to compare against the safe default. [Memory and cache: C3D_FrameEnd flush](memory-and-cache.md).

### 8.3 Design allocations so failures remain understandable

Create an allocation ledger with resource ID, pool, bytes, lifetime and owner. Include every shader binary, texture, atlas, render target, mesh buffer, audio queue, compression scratch and applet/network work buffer. Reject content loads with a clear error rather than silently using partially initialized textures or invalid pointers.

Use arenas or pools for transient CPU work only after identifying actual allocation pressure. A compact allocator cannot fix an oversized working set. Exercise repeated loading/unloading and long play to expose fragmentation and lifetime leaks, not only the peak scene after fresh launch.

## 9. Separate simulation, rendering, presentation and sound clocks

A 30 fps renderer does **not** imply the original game should advance at 30 ticks/s. Preserve the simulation's intended rate; display the most recent completed state on an independently paced output path. [Frame pacing](PICA200/frame-pacing.md).

For an engine originally tied to 60 Hz, an abstract loop is:

```text
on each active outer loop:
    poll platform input ONCE
    elapsed = clock_now - previous_clock
    if resumed from HOME/sleep: reset previous_clock and accumulator
    else: add elapsed to accumulator, with a documented catch-up bound
    while accumulator >= fixed_simulation_step:
        advance gameplay exactly one step using queued input actions
        accumulator -= fixed_simulation_step
    service audio queue and pending resource completions
    if graphics can accept a new frame:
        submit rendering of the current game state
```

This is **pseudocode**. Use `svcGetSystemTick()` for high-resolution measurement; on the pinned libctru source its counter uses the base ARM11 clock even in New 3DS fast mode. `C3D_FrameRate(30.0f)` with `C3D_FRAME_SYNCDRAW` is one way to present at steady 30 fps; do not use that setting as a simulation-clock replacement. Read the [frame-pacing page](PICA200/frame-pacing.md) for real API rules and a fixed-step example.

A port of an existing console game may have a native rate slightly different from exactly 60 Hz (e.g. the DS refresh is about 59.83 Hz). Decide what must be maintained: gameplay logic timing, music sync, animation frames or display refresh. Document the compromise and test accumulated drift over minutes, not only the first second.

### Four counters, not one “FPS”

Track at least:

1. **Simulation ticks processed per second** — whether the game keeps its intended speed.
2. **Scenes or render commands produced per second** — whether the CPU keeps up with building frames.
3. **GPU frames completed per second** — whether the PICA200 finishes submitted work.
4. **Distinct frames actually presented per second** — whether the player sees fresh pictures.

Also record **age of the displayed game state**, in logic ticks, to expose queues that add latency while FPS looks good. If a render queue falls behind, prefer dropping an *unstarted old rendering job* over dropping simulation or audio. Never overwrite a queued GPU buffer to implement that policy.

## 10. Instrument before optimizing

For a port, a single average FPS number is almost useless. It cannot distinguish a CPU-bound engine, expensive draw submission, GPU fill, a cache flush, file I/O, late audio refill or frame pacing. Collect the following telemetry in development builds and retain a low-overhead subset in release diagnostics. [Performance engineering](performance.md), [debugging](debugging.md).

| Metric | Capture point | What it answers |
| --- | --- | --- |
| Simulation ticks and CPU update duration | Before/after each update block | Is the game too expensive on ARM11? |
| Scene build duration, visible objects, culled objects | Renderer input generation | Is the CPU generating too much work? |
| Draw calls, material/texture changes, vertex/index bytes | Citro3d submission | Is the batching model reasonable? |
| `C3D_GetProcessingTime()` | After `C3D_FrameEnd` | Elapsed frame processing through the end hook and command split; excludes later flush and queue start |
| `C3D_GetDrawingTime()` | After the most recent GPU frame completes | How long is GPU work plus transfers and completion latency? |
| Wait time around `C3D_FrameBegin` | Before/after call | Is the CPU blocked waiting for prior GPU work/vblank? |
| GSP P3D / PPF / PSC counters | Optional GSP performance log | Are drawing, transfers or clears dominating? |
| Whole-frame interval histogram / worst 1% | Successive frames | Are there spikes hidden by the average? |
| Texture cache hit/miss, upload bytes, rebuild count | Asset/render cache | Do caches actually help? |
| Linear/VRAM free and peak allocated | Allocator wrappers | Is a scene close to failure? |
| SD read/decode/decompress/swizzle/flush durations | Loading pipeline | What causes transition hitches? |
| Audio queue depth / refill delay / dropped frames | `ndsp` service loop | Is playback at risk? |
| Renderer fallback reason and acceptance rate | CPU/GPU path decision | Is the optimized path used frequently? |
| APT events, thread stalls, invalid resources | Event/failure ring buffer | Why did it fail after HOME or long play? |

For raw CPU sections, measure ticks surrounding a named stage and aggregate in memory. **Do not print or send one log packet per draw or tile.** Text formatting and network service traffic can change the timing you are trying to measure. Export one summary per second and keep an in-memory ring of worst-frame details for later inspection.

On the pinned libraries, `C3D_GetProcessingTime()` is elapsed time after the `FrameBegin` wait through the FrameEnd hook and command split, including possible preemption; `C3D_GetDrawingTime()` includes queued GPU work and screen transfers, and refers to a **completed** frame. The default whole-linear-heap flush may not appear in either timer, so time `C3D_FrameEnd` separately when investigating it. `GSPGPU_SetPerfLogMode(true)`/`GSPGPU_GetPerfLog()` can separate P3D drawing, PPF transfer, PSC clears; follow the cautions in [performance](performance.md).

### The diagnostic decision table

| Measured symptom | More likely cause | First controlled experiment |
| --- | --- | --- |
| Update CPU dominates, GPU mostly idle | Simulation/decoder/VM/CPU rasterizer | Disable one update subsystem; use tick measurements and CPU performance counters |
| CPU render recording dominates | Too many API calls, vertex writes, batching misses | Compare equivalent scene with fewer grouped draws; record correctness and CPU time |
| GPU P3D dominates | Fill, overdraw, expensive materials, too many triangles | Disable stereo, reduce target size, reduce overdraw or material complexity one at a time |
| PPF/PSC or waits dominate | Transfers/clears/synchronization | Skip unchanged bottom target; remove unnecessary off-screen pass; inspect frame queue |
| Game smooth except transition | SD read, decompression, swizzle, allocation or cache pressure | Log each loading stage, compare cold and warm |
| Black/stale or corrupted textures | Tiling, invalid address, missing flush, premature reuse | Compare safe whole-heap flush; inspect allocation and ownership |
| Slow only after many minutes | Leak, fragmentation, growing queues or caches | Capture allocation ledger and queue lengths over long play |
| Audio glitches with normal graphics | Refill scheduling, buffer reuse, long service calls | Increase queue slack for diagnosis; measure worst refill interval |
| New 3DS fine, Old 3DS terrible | CPU, memory or worker assumption | Run exact same scene on Old 3DS; disable N3DS-only features |
| Fast path rarely selected | Missing scene coverage / invalidation logic | Count fallback reasons and fix the frequent cases |

Do not take the result “GPU is at 8 ms” as proof that the whole frame fits 16.7 ms. CPU and GPU work can overlap, but waiting, clears, uploads and presentation can still miss the deadline. Collect frame-interval distributions alongside per-stage clocks.

### Profiling discipline

**One change per experiment:** save the scene identifier, test duration, build commit, target model, system version, Luma version, CPU speed mode, memory mode, diagnostics enabled/disabled, cache state and output. Keep an untouched baseline build. Compare on the **same hardware** before changing another variable. Repeat until the results are stable; reject an optimization that gains speed by changing gameplay or losing visual features.

Coffeepot has buildable microbenchmarks—[membench, flushbench, drawbench and icachebench](../benchmarks/README.md)—for memory/cache, flushing, draw-call and instruction-cache questions, and they have been run on an Old 3DS (system 11.17.0-50U, Luma3DS 13.0.2) and a New 2DS XL at its default 268 MHz clock (Luma3DS 13.3.3). The numbers that matter for a port: a separate citro3d draw costs about 10 µs of CPU ([GPU rendering](PICA200/gpu-rendering.md#what-a-draw-call-costs)); the default whole-heap flush at `C3D_FrameEnd` costs about 90 µs per frame ([memory and cache](memory-and-cache.md#measured-costs-old-3ds)); and a hot loop whose code exceeds 16 KiB runs up to 5.6 times slower per instruction ([CPU optimisation](cpu-optimization.md#icachebench-on-an-old-3ds)). The New 2DS XL at 268 MHz gave the same draw cost, the same I-cache cliff with an 11% dearer miss, and 15-30% dearer cache flushes; **nothing is measured at 804 MHz with L2.** Do not quote these figures for other workloads or models. Record your own results in the benchmark's specified format.

## 11. Debug failures without guessing

A correct debugging setup is part of the port, not an afterthought. Keep the **exact symbolized `.elf`** for every test build. Use [3dslink log streaming](debugging.md), [Luma3DS Rosalina GDB](debugging.md), crash dumps and emulator captures for complementary evidence.

**Crash or memory fault:** save the last scene/resource ID, read the exception and registers, attach GDB to identify the faulting thread and stack, and check allocations/invalid frees. On hardware, Rosalina can expose an extended-remote GDB server; the [debugging page](debugging.md) gives the exact enable and connection sequence.

**GPU freeze with CPU still alive:** log command submission and the last completed frame. Bisect the draw list, not the entire program. Investigate wrong attribute stride or shader output, command-buffer overflow, misaligned/invalid transfers, GPU-inaccessible buffers, and cache coherency. `C3D_FrameBegin(C3D_FRAME_NONBLOCK)` can prevent the diagnostic loop from waiting forever and give you a place to report the stalled GPU. See [GPU hangs](debugging.md) and [PICA command pipeline](PICA200/pica200-command-pipeline.md).

**Wrong image:** render diagnostic checkerboard textures with marked corners, a constant-colour quad, a cut-out sprite, and two overlapping transparent sprites; then verify UV orientation, colour channel order, texture tiling, depth ordering, alpha/blend state, target orientation and atlas borders. The [2D rendering troubleshooting guide](PICA200/rendering-2d.md) lists specific symptoms and tests.

**Only breaks on HOME, sleep or load:** log the APT transition; verify worker pause/restore, service lifetime, GPU resource validity, data paths, missed audio refills, and any stale timing accumulator. Read [application lifecycle](application-lifecycle.md), not a generic desktop suspend guide.

**Only breaks on physical console:** retain the emulator trace, but prioritize actual allocation boundaries, memory modes, cache flushes, non-coherent device buffers and command-list constraints. Emulator timing and permissive behavior cannot prove hardware correctness.

## 12. Concrete porting examples

These are *design walkthroughs*, not claims that a particular engine has already implemented them successfully.

### Example A: DS-era 2.5D RPG, 256×192 world, sprites and 3D geometry

**Original:** 2D maps, animated 4/8bpp tiles, palettes and layered sprites, with a small 3D overworld/battle view and script-driven events. Two screens, touch menus and a distinct simulation tick rate.

**Implementation sequence:**

1. Keep map/script/battle state and the original timer/RNG model on the desktop. Record a walk-through of a scrolling area and one battle, including camera, event and sprite outputs.
2. Represent the original PPU/graphics state explicitly—tile/map bytes, palette banks, scroll, window, blend and priority—rather than having the simulation draw into an RGBA buffer directly.
3. Start a citro2d top-screen renderer with an **opaque tile layer only**. Build a cache of palette-expanded tile pages; retain GPU atlas residency across camera movement. Check the rendered scene against the reference at 1:1 pixel scale.
4. Add characters and effects in exact layer order. Separate opaque, cut-out and alpha-blended runs; record where a given effect cannot yet be reproduced by native passes.
5. Implement windows, brightness and scanline effects as separate tested materials/passes. For any unsupported case, use the reference renderer as an explicit whole-surface fallback with a visible reason code—not an unnoticed full-frame CPU fallback.
6. Move the original 3D scene geometry into a citro3d mesh/material path. Prefer static meshes in packed buffers, cull non-visible elements, and retain any texture used across scene frames.
7. Re-layout top and bottom screens and update touch hitboxes in their native coordinate systems. Decide whether preserving 256×192 exactly is more important than filling the 400×240 top screen.
8. Add audio and ROM data loading with distinct queues and cache budgets; validate long transitions and repeated map changes on Old 3DS.

**Specific failure modes to test:** a palette animation that changes without tile bytes changing, a sprite crossing two layer priorities, a sprite window or fade, per-scanline scroll, a battle with many unique textures, and a map transition while audio continues. **Success** means reference-state and image fixtures match (or every difference is documented), the native path covers ordinary gameplay, fallback frequency is measured, and the simulation rate remains correct at the selected presentation rate. [Tile port techniques](PICA200/rendering-2d.md), [2D tile renderer](PICA200/2d-tile-renderer.md).

### Example B: older desktop 3D engine with GL materials

**Original:** desktop GL renderer with static meshes, per-object matrix uniforms, several textures, alpha particles, simple lighting, postprocessing and a UI drawn after the world.

**Implementation sequence:**

1. Strip the renderer behind a `submit_mesh(mesh_id, material_id, transform, order)`-style interface. Capture a single camera angle and list every material actually present in the test scene.
2. Convert mesh data into compact GPU-visible vertex streams and 16-bit-indexed chunks. Confirm attribute layout with a single coloured mesh.
3. Replace the GLSL vertex code with a `picasso` shader. Verify clip-space position and perspective using a simple moving camera.
4. Classify materials: textured/tinted, cut-out, alpha, two-texture, lit, fogged and special. For each, write one PICA TEV/lighting configuration and a reference screenshot test; mark unsupported desktop fragment operations explicitly.
5. Build opaque batches from compatible state, then translucency passes preserving order. Instrument draw count and material changes per frame.
6. Convert art with `tex3ds`: use compressed formats for suitable large painted textures, 16-bit formats for simple art, and mipmaps where minification needs them. Avoid runtime image conversion.
7. Replace or simplify postprocessing deliberately. Compare the cost and fidelity of baking an effect, using fixed-function state or adding a small render-to-texture pass.
8. Tune geometry and fill separately on Old 3DS. Do not assume a scene with few polygons is cheap if it draws many full-screen translucent passes.

**Success** is a scene that matches all required geometry, ordering and material features, has known per-stage cost, and has no unexpectedly retained desktop GL dependencies. [GPU rendering](PICA200/gpu-rendering.md), [shader programming](PICA200/pica200-shader-programming.md), [fixed-function API](citro3d-fixed-function-api.md).

### Example C: SDL software-rendered 2D game

**Original:** a game update creates CPU pixel buffers, SDL uploads/presents them, and SDL also supplies input, filesystem and audio interfaces.

**Implementation sequence:**

1. Compile the source against a suitable SDL port only to establish a reference playable path and discover unsupported platform assumptions. This does **not** validate rendering performance.
2. Profile the CPU renderer, texture conversion and screen upload separately on an Old 3DS.
3. Identify image-level operations that can be expressed as GPU quads (sprite draw, backgrounds, text, fades). Move those to citro2d while keeping the SDL event and audio glue if useful.
4. Put rare complex CPU effects in a bounded dirty surface; update only when its content changes. Do not keep composing a full-resolution framebuffer every frame once the GPU sprite renderer exists.
5. Re-check input, audio, file paths, suspend and threaded workload. SDL's documented 3DS limitations—including software rendering and unusual thread scheduling—must be reviewed against the actual SDL version used.

For SDL2/SDL3, the upstream Nintendo 3DS README documents **software-only SDL rendering** and cooperative scheduling caveats. Confirm the current README for your exact SDL release, rather than assuming an SDL2 desktop GL backend translates to the PICA200. References: [SDL2 3DS README](https://wiki.libsdl.org/SDL2/README-n3ds), [SDL3 3DS README](https://wiki.libsdl.org/SDL3/README-n3ds), [Coffeepot development workflow](../homebrew/development-workflow.md).

See the [rendering trade-off case studies](case-studies/case-studies-rendering-tradeoffs.md) for real port designs, fidelity compromises and suggested hardware comparisons.

### Choose a bring-up path by source renderer

- **Software-rendered 2D:** capture a reference frame and draw order, identify sprite/tile and palette operations, then introduce GPU layers incrementally with a whole-surface fallback where blending or priority cannot be reproduced. Use the [worked ordering example](PICA200/rendering-2d.md#worked-scene-order-first-batch-second).
- **DS-style 2D:** model tiles, effective palettes, object state and per-line register effects before caching. Validate invalidation, windows, scanline changes and fallback coverage with matching captured state; see [the invalidation and scanline plan](PICA200/rendering-2d.md#worked-invalidation-and-scanline-plan).
- **Hybrid 2.5D:** establish coordinate systems and sprite pivots, then add terrain, cut-outs, characters, translucent work and UI while testing occlusion in both directions. See [the hybrid scene contract](PICA200/3d-scene-optimisation.md#a-hybrid-scene-coordinate-depth-and-pass-contracts).
- **Desktop 3D:** inventory meshes and materials, convert coordinate and vertex formats, replace fragment programs with supported fixed-function combinations or baked assets, and validate each material and visibility boundary. See [model conversion](PICA200/asset-pipeline.md#plan-a-3d-model-conversion-pipeline) and [material state contracts](citro3d-fixed-function-api.md#material-state-contracts).
- **New game:** begin with a small scene and explicit resource and frame ownership, not a generic abstraction for features you do not yet need. Add audio, storage and lifecycle paths incrementally.

For each route, define pass/fail conditions first: deterministic scene, expected image or accepted difference, no missing objects, bounded memory, correct gameplay time, and successful HOME/sleep/exit and audio checks on the target. This is a bring-up map; the wiki does not yet contain a complete runnable implementation for every route.

### Lessons from eleven source-read ports

These come from reading the source of eleven homebrew projects (links are the case studies). None was run by Coffeepot, and none of their trees states frame rates, so every item is a design pattern to try, not a result. The case studies hold the files and line references.

- **Detect the model once, then size things from it.** Vril reads `APT_CheckNew3DS` once and picks a 20 MB hunk on Old 3DS and 64 MB on New 3DS, and turns on wide mode only on a New 3DS that is not a 2DS ([Vril](case-studies/case-study-vril-engine.md)). Daedalus enables wide mode only on a New 3DS ([Daedalus](case-studies/case-study-daedalus-3ds.md)). State the minimum model in the README, as SRB2's does ([SRB2](case-studies/case-study-srb2-3ds.md)).
- **Raise the stack for engines that use it.** libctru's default `__stacksize__` is 32 KB `[documented]`. DXX sets 8 MB after the stack blew on entering a level, Quake2CTR sets 3 MB and Vril 256 KB `[documented]`. DXX's README reports that its CIA build still crashes with the same setting and suspects the stack; the cause is unconfirmed ([DXX](case-studies/case-study-dxx-3ds.md), [Quake2CTR](case-studies/case-study-quake2ctr.md)).
- **If you port through an OpenGL wrapper (picaGL),** Quake2CTR, Vril, Daedalus and Xash3DS all do. They each call `pglInitEx` once with two buffer sizes (`0x040000, 0x100000` in Quake2CTR and Vril; `0x080000, 0x040000` in Daedalus), fix the viewport at 400x240, and give up what the wrapper lacks: Xash3DS declares an OpenGL 1.1 subset, disables VBOs, shaders and non-power-of-two textures and caps textures at 256 px; Vril's screenshot code works around the missing `glReadPixels` by finishing the GPU and reading back the top-screen framebuffer `[documented]`. This wiki has not read picaGL's own header, so what the two sizes mean is unverified.
- **No `dlopen`: link statically and keep a name table.** Xash3DS builds with `-DSINGLE_BINARY` and fakes `dlopen`/`dlsym` with a registry of export tables ([Xash3DS](case-studies/case-study-xash3ds.md)); Quake2CTR links its game and renderer modules into the binary ([Quake2CTR](case-studies/case-study-quake2ctr.md)).
- **Convert at build time, not at run time.** Xash3DS converts `.t3s` to `.t3x` and `.v.pica` to `.shbin` in its Makefile; OpenLara ships precompiled PICA shaders; the Sonic CD FAQ answers slow video with pre-scaled video files rather than a faster decoder ([Sonic CD](case-studies/case-study-sonic-cd-3ds.md), [OpenLara](case-studies/case-study-openlara-3ds.md)).
- **Cut a crashing feature before it blocks the port, and record it.** DXX skips the Descent 1 end-of-level sequence and disables Descent 2 movies because they crashed; Xash3DS compiles its autosave out on 3DS, with a source comment calling it slow and crash-prone on the Old 3DS `[documented]`. Both left manual alternatives. Keep a list of cut features so they can be restored.
- **Decide the render path by model.** The Sonic CD port ships a hardware build (citro2d) for Old 3DS and a software build for New 3DS, and says the software build needs a New 3DS for full speed `[reported]` (FAQ); SRB2 requires a New 3DS and uses a GPU renderer ([SRB2](case-studies/case-study-srb2-3ds.md)). Maintaining two backends doubled testing in the Sonic CD port, by its author's account.
- **Streaming open-world engines (hypothesis).** re3's configuration caps (5,500 model-info slots, 140 peds, 110 vehicles, 4,930 path nodes) were sized for much larger machines than a 3DS; a port starts by cutting them. The saved re3 checkout has no 3DS code, so this is design reading only ([re3](case-studies/case-study-re3-n3ds.md)).
- **Port the whole hardware access class, then grep for stragglers.** The kohjoh DS port converted the DS geometry FIFO stores in its SDK layer but not the direct command-register stores written beside them. Nothing failed loudly: the writes landed in plain memory and a skinned model's weighted vertices used zero matrices (41 all-zero clip matrices per frame) and collapsed. After porting an access class, scan the whole tree for every other way to reach the same hardware. Do not patch the symptom: dropping or redrawing triangles with w ≤ 0 is what produced the earlier port's stretched model [reported: kohjoh project notes, 2026-11-13, private repository; `docs/troubleshooting.md`; checked against melonDS and in Azahar].
- **Render at one publish point.** The same project removed a class of drawer/game races by running game logic and rendering on one thread and drawing at a single coherent point in the loop (no locks, no torn frames), where the earlier port had used a drawer thread and spent weeks on races [reported: same source, `docs/lessons.md`]. See [threading](threading.md) before adding a render thread.
- **Keep the source machine's matrix and lighting semantics on the CPU, send object-space vertices to the GPU.** The DS port kept DS matrix-stack, lighting and test behaviour on the CPU and gave the PICA200 vertices in object space with a matrix palette; 3D worked on the first attempt, and widescreen needed one fixup matrix [reported: same source]. The trade-off (CPU lighting cost) is measured by the project as about 11% of samples on the title screen in Azahar.
- **Match the source depth mode.** A DS title screen used W-buffer mode with near = 0, so every vertex had z/w = 1 and failed a greater-than depth test. Enabling the PICA200's W-buffer mode (`DEPTHMAP_ENABLE` = 0, see [citro3d API](citro3d-fixed-function-api.md)), writing depth as w/4096, testing less-than and clearing depth to 0xFFFFFF fixed it [reported: same source; checked in Azahar against melonDS frames].
- **Submit command streams in one transfer.** Writing the DS geometry command buffer to the GPU in several pieces split 65% of command words mid-command. Submitting it in one transfer removed that and made the display-list cache hit 74% of words (Azahar) [reported: same source].
- **Emulators lag the console on memory.** sm64ds-enhanced's source notes that its emulator gives a `.3dsx` 96 MB on an Old 3DS profile, so the port caps itself to the retail 64 MB when testing there ([SM64 DS](case-studies/case-study-sm64ds-enhanced.md)). See [testing on hardware](testing-on-hardware.md).

## 13. Release acceptance: what a complete port must prove

Do not call a port “done” merely because the title screen works. Use **feature and hardware gates** with the evidence attached. If your release intentionally supports only New 3DS, label that honestly; never imply Old 3DS support from an emulator or a slow-clock New 3DS run.

| Gate | Required evidence |
| --- | --- |
| Boot and reproducible build | Build commands, toolchain versions, source revision, `.elf` retained, `.3dsx` launches on target |
| Correct simulation | Deterministic input replay, state checks, original timing and RNG verified for representative cases |
| Rendering correctness | Reference screenshots for opaque, cut-out, translucent, depth, clipping, text, animation and target screens |
| Renderer coverage | Per-scene GPU fast-path acceptance and fallback reasons; no unintended full-frame CPU rasterization |
| Frame timing | Physical-console distributions for update, recording, GPU/transfers, frame intervals and presented frames |
| Memory | Peak ordinary/linear/VRAM use in the largest and longest-running scenes; bounded transition peaks |
| Audio | No buffer reuse, starvation, dropped-frame bursts or crackles during gameplay, loading, HOME and sleep |
| Assets | Reproducible converters, all resources accounted for, corrupt/missing asset response tested |
| Saves | Two-generation or equivalent robust scheme; invalid version, full storage and interrupted write tests |
| Lifecycle | HOME, lid, wake, user quit, applets, low-battery conditions and teardown behave properly |
| Models | Old 3DS/2DS and New 3DS where supported; stereo-off and special controls explicitly covered |
| Release packaging | Correct `romfs:/` content, save path, license/asset handling, launch instructions and known limitations |

### A minimal real-hardware test matrix

| Scenario | Why it is in the matrix |
| --- | --- |
| Fresh boot, first asset load | Cold SD and texture-cache costs |
| Repeated camera pan across map boundaries | Cache invalidation, batching and seams |
| Worst normal scene and worst effect-heavy scene | Different CPU/GPU bottlenecks |
| Thirty minutes of navigation and scene switching | Fragmentation, leaks and queue growth |
| HOME during world update / GPU use / save / audio | Lifecycle consistency |
| Lid close during loading and reopen | Worker synchronization and clock reset |
| Last-available-memory pressure | Allocation failure and safe fallback |
| Full, slow or corrupt SD content | Error paths and save integrity |
| 3D slider on/off; wide mode where supported | Target counts, state, stereo view and fill |
| New 3DS speed mode on/off, Old 3DS baseline | Separates CPU assumptions from GPU assumptions |

Record every test's hardware model, firmware/CFW, exact build, settings, scenario and pass/fail. The [testing-on-hardware page](testing-on-hardware.md) explains how to structure reproducible experiments. Emulator runs remain valuable for quick logic/debugging, but **emulator performance is not real-hardware evidence**.

## 14. Porting handoff checklist

A human implementer joining the project should be able to find these artifacts immediately:

- [ ] `PORT_AUDIT.md`: original features, dependencies, missing platform APIs, strict-correctness requirements, target models.
- [ ] `PORT_BUDGET.md`: target scene/frame budget; ordinary, linear and VRAM peaks, with measured headroom.
- [ ] Desktop/reference build plus reproducible input replay and expected-state/image fixtures.
- [ ] Minimal `.3dsx` project and documented devkitPro/Docker build command; exact package versions pinned.
- [ ] Clear platform abstractions for lifecycle, input, clock, storage, audio and networking.
- [ ] Native renderer contract for scene commands; a list of supported and unsupported material/effect types.
- [ ] Conversion scripts and manifest for textures, maps, fonts, compressed data and audio.
- [ ] Frame diagnostic counters, allocation ledger, renderer fallback counters and crash-log method.
- [ ] Two real-hardware baseline runs: diagnostics enabled and disabled, with cold/warm cases.
- [ ] Bug tracker grouped by **correctness**, **performance**, **resource lifetime**, **system integration** and **unimplemented feature** rather than “FPS is low”.
- [ ] Release test matrix with pass/fail evidence and explicit limits of model support.

**Avoid:** giant platform-dependent render functions; pervasive `#ifdef __3DS__` in game logic; full-frame allocations in the draw loop; CPU-side pixel rendering for scenes the GPU could batch; late texture swizzling; unbounded caches; draw sorting that breaks transparency or clipping; “fixed” FPS achieved by slowing simulation; assuming a core-1 worker is free CPU time; measuring only in an emulator; and merging multiple optimizations without isolated before/after evidence.

## 15. Technical references inside Coffeepot

Start with a task, then follow the page that has the actual API and source evidence:

| If you are implementing… | Read |
| --- | --- |
| Compiler install, Makefile, Docker, 3DSX and deployment | [Development workflow](../homebrew/development-workflow.md), [3DS examples](../homebrew/3ds-examples.md) |
| libctru services, allocators and process setup | [libctru](../homebrew/libctru.md), [application lifecycle](application-lifecycle.md) |
| CPU limits, cache, instruction tuning | [ARM11 MPCore](../hardware/arm11-mpcore.md), [CPU optimization](cpu-optimization.md) |
| 2D sprites, text, camera, tiles, caching, pixel accuracy | [2D rendering](PICA200/rendering-2d.md), [complete tile-renderer example](PICA200/2d-tile-renderer.md) |
| citro3d setup, textures, batching, stereo, targets | [GPU rendering](PICA200/gpu-rendering.md), [command pipeline](PICA200/pica200-command-pipeline.md) |
| Vertex/geometry shader assembly, uniforms and output | [Picasso and citro3d](../homebrew/picasso-and-citro3d.md), [PICA shader programming](PICA200/pica200-shader-programming.md) |
| Lighting, fog, TEV, procedural textures, blending and stencil | [Fixed-function API](citro3d-fixed-function-api.md), [GPU shading hardware](../hardware/gpu-shading-features.md) |
| Texture formats, atlases, RomFS, codecs, streaming | [Asset pipeline](PICA200/asset-pipeline.md) |
| Heap/VRAM sizes, cache and buffer ownership | [Memory and cache](memory-and-cache.md), [hardware memory](../hardware/memory.md) |
| Game clock, presentation and CPU/GPU overlap | [Frame pacing](PICA200/frame-pacing.md) |
| Audio decoding, DSP queues and deadlines | [Audio programming](audio.md), [DSP hardware](../hardware/dsp.md) |
| Inputs, touch, C-Stick, sensors | [Input and sensors](input-and-sensors.md) |
| SD/RomFS, robust saves and file failure handling | [Filesystem and saves](filesystem-and-saves.md) |
| Work queues, core scheduling and APT CPU share | [Threading](threading.md) |
| Network services, IPC and service-specific buffer rules | [Networking](networking.md), [IPC and services](ipc-and-services.md) |
| Logs, GDB, exception dumps, GPU hangs | [Debugging](debugging.md) |
| Instrumentation, counters and bottleneck analysis | [Performance](performance.md), [benchmark suite](../benchmarks/README.md) |
| Test environments, real-console methodology | [Testing on hardware](testing-on-hardware.md), [model differences](../hardware/models.md) |
| Versioned source citations and verification status | [Source versions](source-versions.md) |

## Retail developer accounts

What shipped 3DS games' developers have said about renderer rewrites, VRAM-driven art, 60 FPS targets and stereo cost is collected, with the claim-by-claim evidence, on [commercial case studies](case-studies/case-studies-commercial.md). All of it is `[reported]` press material, not measurement.

## Open questions and limitations

- The Coffeepot [benchmark suite](../benchmarks/README.md) builds, but the wiki does **not yet contain physical-console measurements** for its draw, flush and memory benchmarks. Any numerical crossover (e.g. number of sprites where citro2d beats software composition) is presently a test question, not a known constant.
- The best compromise between CPU rendering and native GPU composition depends on each game's scene and effects. Capture the same input/state and compare image correctness, memory, 99th-percentile frame interval and load hitches on the **same Old 3DS**.
- Source-derived details—especially library flush behaviour, core-1 scheduling, `.3dsx` loader memory mode and helper defaults—are specific to the versions on [source versions](source-versions.md). Recheck when upgrading SDK components.
- GPU/CPU renderer parity for DS-specific scanline effects, windows and colour math cannot be asserted without exact fixtures. Treat any shortcut as a **[hypothesis]** until the specific effect is validated.

## Sources

- kohjoh project notes (private repository, no public revision yet; `docs/lessons.md`, `docs/troubleshooting.md`, read 2026-11-13) — access-class porting, single publish point, W-buffer, command-stream transfers.
Primary sources for the APIs and hardware facts on this page, as already pinned and discussed in Coffeepot:

- [devkitPro 3ds-examples](https://github.com/devkitPro/3ds-examples) — application template, shader/texture build rules, graphics/input/audio examples.
- [devkitPro libctru](https://github.com/devkitPro/libctru) — system loop, memory, HID, NDSP, GSP, service and debug interfaces; pinned revision on [source versions](source-versions.md).
- [devkitPro citro3d](https://github.com/devkitPro/citro3d) — render queue, draw submission, texture handling, combiner state and timers; pinned source notes on [GPU rendering](PICA200/gpu-rendering.md).
- [devkitPro citro2d](https://github.com/devkitPro/citro2d) — 2D objects, sprite sheets, batching and text; implementation evidence and verification in [2D rendering](PICA200/rendering-2d.md).
- [devkitPro picasso](https://github.com/devkitPro/picasso) and [tex3ds](https://github.com/devkitPro/tex3ds) — PICA shader assembly and offline image conversion.
- [3dbrew](https://www.3dbrew.org/wiki/Main_Page) — hardware registers, system services and memory-mode background; see the linked hardware/software pages for individual references.
- [SDL2 Nintendo 3DS README](https://wiki.libsdl.org/SDL2/README-n3ds), [SDL3 Nintendo 3DS README](https://wiki.libsdl.org/SDL3/README-n3ds) — SDL 3DS backend limitations and build guidance.

The examples in this guide are **implementation plans and illustrative pseudocode**. They have not been compiled against the pinned toolchain or exercised on hardware as part of writing this page. Refer to the linked, versioned Coffeepot examples for code-specific verification status.
