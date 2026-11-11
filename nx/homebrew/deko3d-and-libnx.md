# libnx and deko3d

This page is for a game developer who can already build an `.nro` and wants to know which library does what on the Nintendo Switch family (**NX**). It applies on any **[firmware](../../glossary.md#firmware)** that runs current `libnx` / `deko3d`, and the outcome is knowing where to put each line of game code: `libnx` for talking to the system, `deko3d` (or SDL2/OpenGL) for drawing.

For installing the toolchain and the build-run loop, see [toolchain and workflow](toolchain-and-workflow.md). For launching the result with full memory, see [Atmosphere and running](atmosphere-and-running.md).

## What you need

- Working toolchain from [toolchain and workflow](toolchain-and-workflow.md) (`switch-dev` group installed).
- Headers: `#include <switch.h>` pulls in all of `libnx`; `#include <deko3d.h>` pulls in the GPU API. Minimal examples compile with no other setup.
- The `switch-examples` checkout for copy-paste starters (graphics, input, audio, files, network folders).
- Conceptually: your program runs as an ordinary unprivileged program (in **[userland](../../glossary.md#userland)**, inside its own isolated running program (**[process](../../glossary.md#process)**)). It cannot touch hardware directly; every request goes through the privileged core (the **[kernel](../../glossary.md#kernel)**) or a message to another program (**[inter-process communication](../../glossary.md#ipc)**).

## How it works

`libnx` is the operating-system layer. It starts your program, wraps kernel requests (threads, memory, sync), and provides a client for each system service (input, audio, files, display, network). A minimal game initialises the services it uses, runs a frame loop, and shuts them down.

`deko3d` is the graphics layer. It is the homebrew alternative to Nintendo's NVN API: an explicit, Vulkan-like API over the NVIDIA Maxwell GPU in the Tegra chip (the main chip combining **[CPU](../../glossary.md#cpu)** and **[GPU](../../glossary.md#gpu)** — a **system-on-a-chip**, or **[SoC](../../glossary.md#soc)** — see **[Tegra X1](../glossary.md#tegra-x1)** and **[Maxwell](../glossary.md#maxwell)**). You create device → memory → shaders/images → command buffers → queue → swapchain, record work explicitly each frame, and submit it. There is almost no hidden state.

SDL2 (from `portlibs`) sits on top of both: it uses `libnx` services internally and gives you a simpler 2D / framebuffer-style API. Use SDL2 for ports and 2D games; use `deko3d` when you need explicit 3D control or maximum GPU headroom.

## Technical details

### What `libnx` covers [documented] (readable in the `libnx` tree)

| Area | Headers / source dirs | Game use |
| ---- | --------------------- | -------- |
| Startup/runtime | `runtime/crt0.s`, `runtime/romfs.c` | Entry point, argument passing, read-only asset pack (`romfs`) mounting. |
| Kernel wrappers | `kernel/` (`thread`, `mutex`, `event`, `virtmem`, `jit`, `svc`) | Threads, locks, events, virtual-memory reservation. Thin wrappers over privileged requests (**[system calls](../../glossary.md#syscall)**). |
| Message framing | `sf/` (CMIF marshalling) | Packs service requests. You rarely touch it; service headers call it for you. |
| Services | `services/` (~100 clients) | `hid` (pads/touch/motion), `audren`/`audout`/`audin`/`hwopus` (audio), `fs`/`romfs`/`ncm` (files), `nv`/`nvidia`/`display`/`grc` (display/GPU plumbing), `applet`/`ldr`/`pm`/`sm`/`set` (lifecycle), `bsd`/`nifm`/`ldn` (network), `pcv`/`psm` (clocks/power). |
| Video plumbing | `services/vi`, `services/nv` | Display handles consumed directly or handed to `deko3d` swapchain creation. |

Typical game bring-up order (each is one `*Initialize()` call; check `switch-examples` for the exact sequence):

1. `hid` — input. Poll each frame (`hidScanInput`, read buttons/sticks/touch).
2. `fs` / `romfs` — files. `romfsInit` mounts bundled assets; `fs` opens SD-card and save data.
3. Audio — `audren` mixer init for PCM or `hwopus` for compressed streams.
4. Display — either `vi` framebuffer path (simple 2D) or `deko3d` swapchain (3D / high-performance 2D).
5. Network (optional) — `nifm` + `bsd` sockets, or `curl` from portlibs.

```c
#include <switch.h>
// Verification: builds with switch-dev libnx; not run on hardware as a fragment.
int main(int argc, char** argv) {
    romfsInit();
    hidInitialize();
    // ... frame loop: hidScanInput(); u64 k = hidKeysHeld(CONTROLLER_P1_AUTO);
    hidExit();
    romfsExit();
    return 0;
}
```

### What `deko3d` covers [documented] (readable in `deko3d` headers and `dekotools` docs)

Pipeline objects, in creation order:

`DkDevice` → `DkMemBlock` → `DkShader` / `DkImage` → `DkCmdBuf` → `DkQueue` → `DkSwapchain`

- Explicit like Vulkan: you allocate GPU-visible memory blocks, bind images and buffers yourself, record command buffers, and submit to a queue that presents to the swapchain. No global bind-to-modify state machine.
- Debug vs. release: link `-ldeko3dd` for validation layers during development, `-ldeko3d` for release. The debug layer catches missing barriers and bad descriptors at the cost of CPU time.
- Contrast with OpenGL (`switch-examples/opengl/`): OpenGL keeps hidden per-target state. `deko3d` forces that state into descriptors and command records, which is more code but fewer "why did the last draw leak into this one" bugs.

Shaders [documented] (readable in `dekotools` `uam` / `dksh` tool docs):

- Author in GLSL, compile offline with the `uam` tool to `.dksh`.
- Layouts are explicit: `layout(binding=N)` for uniform/storage blocks, samplers, and images.
- Per-stage limits to design around: 16 uniform + storage blocks, 32 samplers, 8 images per shader stage. Exceeding them fails at pipeline creation, not at draw time.

### `switch-examples` map [documented] (directory listing)

| Folder | What to copy |
| ------ | ------------ |
| `graphics/deko3d/` | Ordered progression: device/queue setup → textured quad → depth, culling, uniform buffers → multi-pass. Start at hello-triangle. |
| `graphics/opengl/` | Stateful OpenGL path. Prefer for GL ports, not new 3D. |
| `hid/` | `read-controls`, `touch`, `vibration`, `sixaxis` (motion). |
| `audio/` | `playtone` / `audren-simple`, `hwopus` (compressed decode). |
| `filesystem/` | `romfs`, `sdmc`, `save`. |
| `applet/` | Lifecycle, callbacks when the Home button suspends the game. |
| `network/` | `curl`, `nxlink`, `ldn`. |

### When to use SDL2 vs. `deko3d`

- SDL2 (portlibs): fastest route for 2D, ports, and jam games. Handles window, input, and audio setup for you. Cost: less control over GPU memory and sync; heavy 3D will fight the abstraction [reported] (port READMEs; conditions as stated by authors, not reproduced here).
- `deko3d`: choose when draw-call structure, render targets, or bandwidth matter, or when targeting 60 fps 3D with explicit memory control. Cost: you own barriers, layouts, and swapchain resizing.
- OpenGL: choose only to port existing GL code. New code should prefer `deko3d`.

Code examples on this page state their check level; none claim hardware-tested status.

## Limits and risks

- `libnx` service names and request IDs track system firmware. Update `libnx` with the toolchain after a major firmware release or new service calls fail.
- `deko3d` exposes the real GPU; a bad descriptor or out-of-bounds image can hang the GPU and abort the app. Develop against `-ldeko3dd` and only strip validation at the end.
- Applet-mode memory limits apply no matter which graphics API you pick. A `deko3d` swapchain that fits under title override can still fail under Album hbmenu. See [Atmosphere and running](atmosphere-and-running.md).
- Homebrew cannot use Nintendo's NVN SDK or official middleware. GPU profiling is limited to homebrew timers and counters, not vendor tools.

## Sources

- [libnx](https://github.com/switchbrew/libnx) — what `kernel/`, `services/`, `sf/`, `runtime/` cover (startup, kernel wrappers, ~100 service clients, CMIF framing, `romfs`).
- [deko3d](https://github.com/switchbrew/deko3d) — what the device/memory/shader/command/queue/swapchain API covers and debug (`-ldeko3dd`) vs. release (`-ldeko3d`) split.
- [dekotools / uam](https://github.com/switchbrew/dekotools) — what offline GLSL→`.dksh` compilation and `layout(binding=N)` limits cover.
- [switch-examples](https://github.com/switchbrew/switch-examples) — what each demo folder covers (`deko3d`, `opengl`, `hid`, `audio`, `filesystem`, `applet`, `network`).
- [SwitchBrew Homebrew](https://switchbrew.org/wiki/Homebrew) — what `.nro` vs. system-service expectations cover (background reference).
