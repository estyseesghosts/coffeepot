# libnx and deko3d

This page is for a game developer who can already build an `.nro` and wants to know which library does what on the Nintendo Switch family (**NX**). The outcome is knowing where to put each line of game code: `libnx` for talking to the system, `deko3d` (or SDL2/OpenGL) for drawing. It applies to the library versions listed on [source versions](../engineering/source-versions.md); service names and request IDs track system **[firmware](../../glossary.md#firmware)**, so an old `libnx` can fail on a new system version.

For installing the toolchain and the build-run loop, see [toolchain and workflow](toolchain-and-workflow.md). For launching the result with full memory, see [Atmosphere and running](atmosphere-and-running.md). For writing a correct `deko3d` frame loop, see [rendering](../engineering/rendering.md).

## What you need

- Working toolchain from [toolchain and workflow](toolchain-and-workflow.md) (`switch-dev` group installed).
- Headers: `#include <switch.h>` pulls in `libnx`; `#include <deko3d.h>` pulls in the GPU API. The upstream `deko_basic` example compiles with no other setup `[documented]` (`switch-examples` `graphics/deko3d/deko_basic/source/main.c`).
- A `switch-examples` checkout for starters (see the map below).
- Conceptually: your program runs as an ordinary unprivileged program (in **[userland](../../glossary.md#userland)**, inside its own isolated running program, a **[process](../../glossary.md#process)**). It cannot touch hardware directly; every request goes through the privileged core (the **[kernel](../../glossary.md#kernel)**) or a message to another program (**[inter-process communication](../../glossary.md#ipc)**).

## How it works

`libnx` is the operating-system layer. It starts your program, wraps kernel requests (threads, memory, sync), and provides a client for each system service (input, audio, files, display, network). A minimal game initialises the services it uses, runs a frame loop gated on `appletMainLoop()`, and shuts them down.

`deko3d` is the graphics layer. It is the homebrew alternative to Nintendo's NVN API: an explicit, Vulkan-like API over the NVIDIA Maxwell GPU in the Tegra chip (the main chip combining **[CPU](../../glossary.md#cpu)** and **[GPU](../../glossary.md#gpu)** — a **system-on-a-chip**, or **[SoC](../../glossary.md#soc)** — see **[Tegra X1](../glossary.md#tegra-x1)** and **[Maxwell](../glossary.md#maxwell)**). You create a device, memory blocks, shaders and images, a command buffer, a queue and a swapchain; you record work explicitly each frame and submit it.

SDL2 (from `portlibs`) sits on top of the system libraries and gives you a simpler 2D / framebuffer-style API. SDL2 ports that draw through OpenGL use the Mesa/Nouveau stack, not `deko3d` (see the [open-engine case studies](../engineering/case-studies/case-studies-open-engines.md)).

## Technical details

### What `libnx` covers `[documented]`

Layout read from the `libnx` tree at the pinned commit: `nx/source/` and `nx/include/switch/` contain `applets/`, `arm/`, `audio/`, `crypto/`, `display/`, `kernel/` (threads, mutexes, events, virtual memory, JIT, `svc`), `nvidia/`, `runtime/` (startup, `romfs`, `pad`, `nxlink`), `sf/` (service-framework message marshalling) and `services/` (one header per service client).

| Area | Directories | Game use |
| ---- | ----------- | -------- |
| Startup and runtime | `runtime/` | Entry point, `romfs` mounting, the `PadState` input API (`pad.h`), `nxlink` stdio redirection (`nxlink.h`). |
| Kernel wrappers | `kernel/` | Threads, locks, events, virtual-memory reservation. Thin wrappers over privileged requests (**[system calls](../../glossary.md#syscall)**). |
| Message framing | `sf/` | Packs service requests. You rarely touch it; service headers call it for you. |
| Services | `services/` | `hid` (pads, touch, motion), `audren`/`audout`/`audin`/`hwopus` (audio), `fs`/`ncm` (files), `nv`/`vi` (display and GPU plumbing), `applet` (lifecycle), `bsd`/`nifm`/`ldn` (network), `pcv`/`psm` (clocks, power). |

Typical bring-up order for a game:

1. `romfsInit()` — mounts bundled assets at `romfs:/`.
2. Input — `padConfigureInput(1, HidNpadStyleSet_NpadStandard)` and `padInitializeDefault(&pad)` once; `padUpdate(&pad)` once per frame; read with `padGetButtons` (held) and `padGetButtonsDown` (pressed this frame).
3. Audio — `audren` for mixed PCM, `hwopus` for Opus streams.
4. Display — either a software framebuffer path (simple 2D), `deko3d` swapchain (3D or high-performance 2D), or EGL/OpenGL.
5. Network (optional) — `socketInitializeDefault()`, or `curl` from portlibs.

```c
#include <switch.h>

int main(int argc, char** argv)
{
    romfsInit();
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);   // one player, standard controllers
    PadState pad;
    padInitializeDefault(&pad);                            // handheld plus first connected pad

    while (appletMainLoop())                               // false once the system asks the app to exit
    {
        padUpdate(&pad);                                   // once per frame
        u64 down = padGetButtonsDown(&pad);
        if (down & HidNpadButton_Plus)
            break;                                         // return to hbmenu
        /* update, draw */
    }

    romfsExit();
    return 0;
}
```

Verification: compiles inside the wiki's compile-check program with devkitA64 r30-1 / `libnx` 4.12.0-1, `-Wall`, no warnings; not run on hardware. The input calls match `switch-examples` `templates/application/source/main.c` `[documented]`.

The old HID polling API (`hidScanInput`, `hidKeysHeld`, `hidKeysDown`, `CONTROLLER_P1_AUTO`) was deprecated in `libnx` 4.0.0 and removed in 4.1.3 `[documented]` (`libnx` `Changelog.md`, "Version 4.0.0" and "Version 4.1.3"). The pad API has `padGetButtons` for held buttons and `padGetButtonsDown`/`padGetButtonsUp` for edges; there is no `padGetButtonsHeld` `[documented]` (`libnx` `nx/include/switch/runtime/pad.h`). Code copied from older tutorials will not compile against current `libnx`.

### What `deko3d` covers `[documented]`

Objects, in the order a program creates them:

`DkDevice` → `DkMemBlock` → `DkShader` / `DkImage` → `DkCmdBuf` → `DkQueue` → `DkSwapchain`

The full object list is in `Primer.md`; it also has `DkFence`, `DkVariable` and `DkSampler`.

- Explicit like Vulkan: you allocate memory blocks, bind images and buffers yourself, record command buffers into command lists, and submit them to a queue that presents to the swapchain.
- Two library builds exist: link `-ldeko3dd` while developing for parameter and state checking and warning/error messages; link `-ldeko3d` for release, which omits validation `[documented]` (`deko3d` `README.md`; `Primer.md` "Linking to deko3d").
- Compared with OpenGL: `deko3d` has no hidden global state machine; state lives in structs you bind. It is considerably less code-size heavy than the Mesa OpenGL driver: the `deko3d` README gives about 500 KB for a typical `deko3d` application against 8 MB or more with `switch-mesa` `[reported]` (`deko3d` `README.md`, "Why and when should I use deko3d instead of switch-mesa's OpenGL driver?"; the README's own estimate, not measured here).
- What `deko3d` is not: it is **not** a wrapper around Nouveau. It is its own low-level implementation, written from the reverse-engineered behaviour of NVIDIA's NVN API. Its shader compiler `uam` is derived in part from Mesa/Nouveau source; `uam` is a separate project and a PC tool `[documented]` (`deko3d` `README.md`: "it is a *main design goal*" that the compiler runs at compile time; "the shader compiler (which *is* based off mesa/nouveau sources)"; `uam` `README.md`).

Shaders `[documented]` (`Primer.md` "Shaders"; `uam` `README.md`):

- Author in GLSL and compile on the PC with `uam` into a **[DKSH](../glossary.md#dksh)** file. The upstream examples' Makefiles do this for every `*.glsl` in `source/` and place the result in `romfs/shaders/`.
- `deko3d` accepts only native Maxwell shader code; GLSL and SPIR-V cannot be compiled at run time.
- Bindings must be explicit in the GLSL (`layout (binding = N)`) and map one-to-one to `deko3d` bindings; omitting one is a compile error in `uam` `[documented]` (`uam` `README.md`, "Differences with standard GL and mesa/nouveau").
- Binding limits per shader stage: 16 uniform buffers, 16 storage buffers, 32 texture bindings, 8 image bindings, numbered from zero (`uam` `README.md`; `deko3d.h`: `DK_NUM_UNIFORM_BUFS`, `DK_NUM_STORAGE_BUFS`, `DK_NUM_TEXTURE_BINDINGS`, `DK_NUM_IMAGE_BINDINGS`). `deko3d` has no pipeline objects.

### `switch-examples` map `[documented]`

Directory names as listed at the pinned commit:

| Folder | What is there |
| ------ | ------------- |
| `templates/` | `application`, `library`, `sysmodule` project templates. Start new projects from `templates/application`. |
| `graphics/deko3d/` | `deko_basic` (one triangle, C), `deko_console` (a text-console example), and `deko_examples/` (C++ wrapper: `Example01_SimpleSetup`, `02_Triangle`, `03_Cube`, `04_TexturedCube`, `05_Tessellation`, `06_Multisampling`, `07_MeshLighting`, `08_DeferredShading`, `09_SimpleCompute`). |
| `graphics/opengl/`, `graphics/sdl2/`, `graphics/printing/`, `graphics/simplegfx/`, `graphics/shared_font/` | OpenGL, SDL2, text console, software framebuffer, and system-font demos. |
| `hid/` | `read-controls`, `abstracted-pad`, `touch-screen`, `sixaxis`, `sevensixaxis`, `vibration`, `colors`, `hdls`, `irsensor`, `notification-led`, `ring-con`. |
| `audio/` | `audren-simple`, `playtone`, `hwopus-decoder`, `echo`, `sdl2-audio`. |
| `fs/` | `romfs`, `sdmc`, `save`. |
| `applet/` | `libapplets` (software keyboard, error, web, parental-control auth), `lockexit`, `recording`, `screenshot-overlay`, `vrmode`, `light-sensor`, `app-playstats`. |
| `network/` | `curl`, `ldn`, `lp2p`, `nxlink_stdio`. |

The upstream `deko3d` examples are under `graphics/deko3d/` (`deko_basic`, `deko_console`) and `graphics/deko3d/deko_examples/` (`ExampleNN_*.cpp`). There is no example that demonstrates the applet hook API; the hook types are in `libnx` `nx/include/switch/services/applet.h`.

### When to use SDL2, `deko3d` or OpenGL

- SDL2 (`portlibs`): the shortest route for 2D games, ports and jam games. It handles window, input and audio set-up. In the open-engine ports reviewed here, SDL video went through GLES/EGL on Mesa/Nouveau; none of those ports touched `deko3d` or Maxwell features `[reported]` (see [open-engine case studies](../engineering/case-studies/case-studies-open-engines.md)).
- `deko3d`: choose it when you want to own memory, synchronisation and draw submission, for example for a 3D renderer. The cost is that you own barriers, memory blocks and the swapchain. The recompilation ports in the [case studies](../engineering/case-studies/case-study-sa2.md) moved from Vulkan-on-NVK to `deko3d` for CPU cost; one measured result is in [rendering](../engineering/rendering.md).
- OpenGL: use it to bring existing GL code over. Whether a new project should use it depends on the tradeoff above; this wiki does not claim `deko3d` is better for every game.

## Limits and risks

- `libnx` service wrappers track system firmware. Update `libnx` with the toolchain after a major firmware release or new service calls can fail `[documented]` (`libnx` `Changelog.md` lists service commands added and removed per firmware).
- `deko3d` exposes the real GPU. A bad descriptor or an out-of-bounds image access can hang the GPU or abort the application; develop against `-ldeko3dd`.
- Applet-mode memory limits apply whichever graphics API you pick. A `deko3d` swapchain that fits under title override can still fail when the homebrew menu is launched from the Album. See [Atmosphere and running](atmosphere-and-running.md).
- Homebrew cannot use Nintendo's NVN SDK or official middleware. GPU profiling uses fences, counters and your own timers rather than vendor tools.

## Sources

- [libnx](https://github.com/switchbrew/libnx) at `b156b4774fb2df7b93402169509266da7547fc4c` — directory layout, `Changelog.md` (HID removal), `nx/include/switch/runtime/pad.h`, `services/applet.h`.
- [deko3d](https://github.com/devkitPro/deko3d) at `373bfd43d8b2d805fa2eb7d406dbb823032df958` — `include/deko3d.h`, `Primer.md`, `README.md`. (The repository previously linked from this wiki, `switchbrew/deko3d`, is not the current upstream.)
- [uam](https://github.com/devkitPro/uam) at `5a5afc2bae8b55409ab36ba45be63fcb73f68993` — offline GLSL-to-DKSH compiler; the Mesa-imported code.
- [dekotools](https://github.com/devkitPro/dekotools) — the `dksh` packaging tool the `deko3d` build uses (named by `deko3d` `README.md`; not read separately).
- [switch-examples](https://github.com/switchbrew/switch-examples) at `669786898205b7beb25ff1731e72982e6d0397d3` — directory map, `templates/application`, `graphics/deko3d/deko_basic`.
