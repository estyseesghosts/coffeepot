# A native deko3d renderer, step by step

This page walks through one small native renderer written against the `deko3d` C API: a textured, depth-tested cube drawn into an offscreen image, then a fullscreen pass that samples it into the swapchain, with pipelined frames and GPU timing. It is for a developer who has seen `deko_basic` draw a triangle and needs the next steps (buffers, textures, depth, two passes, frame pacing, teardown) in one place. It is a reference to read and adapt, not an engine. The program is `nx/engineering/examples/deko-renderer/source/main.c`.

## Before you start

- Toolchain: devkitA64 r30-1, `libnx` 4.12.0-1, `deko3d` 0.5.0-1, `uam` 1.1.0-2 ([source versions](source-versions.md)). Read [libnx and deko3d](../homebrew/deko3d-and-libnx.md) for the object model and [GPU memory and synchronization](gpu-memory-and-synchronization.md) for the ownership rules this program follows.
- Verification: builds with no warnings (normal and `-DDKR_SELFTEST` builds). **Never run.** Eden 0.2.1 aborts about 9 s after starting any `deko3d` program on the machine used here, including unmodified upstream `deko_basic` (exit code 3), so the emulator cannot confirm even that it draws. Treat every statement about behaviour at run time as `[hypothesis]` until a console run is recorded.
- Build: `make` in a copy of the directory (it needs `../common` beside it). It links `-ldeko3dd`, the debug library; switch to `-ldeko3d` for a release build and run both.

```sh
make                                  # normal build: prints p50/p95 timings every 120 frames over nxlink
make CFLAGS_EXTRA=-DDKR_SELFTEST      # 240 frames, checks the picture and counters, writes report.csv
make CFLAGS_EXTRA=-DDKR_SLOTS=3       # three frame slots instead of two
```

## The objects and who owns what

| Object | Responsibility | Rule |
| ------ | -------------- | ---- |
| `DkDevice` | Root object; GPU address space | Destroy last |
| `DkMemBlock` | Backing memory for commands, images, buffers, code | Do not free while the GPU may use it; allocate few, large blocks |
| `DkImage` + layout | An image's format, size and placement in a block | Ask the layout for size and alignment; the block must outlive the image |
| `DkShader` | Native shader metadata pointing at code in a `Code` block | The code block must stay alive |
| `DkCmdBuf` → `DkCmdList` | Records commands; a list is the recorded result | Recording is not execution; one recording thread per command buffer |
| `DkQueue` | Submits lists; work is asynchronous | Order only through fences and barriers |
| `DkFence` | Signalled by the queue when earlier work finishes | Stable address; CPU waits with `dkFenceWait` |
| `DkSwapchain` | The images the display shows | Acquire and present are separate calls; destroy before the image memory |

Unlike `libnx`, `deko3d` creation calls do not return error codes. A failure is reported through the device's debug callback in the `-ldeko3dd` library (the callback is expected not to return), and as a fatal system error in the release library `[documented]` (`Primer.md` "Device"). The example installs a callback that logs and aborts.

## Walkthrough, in the order the program does it

1. **Device** (`graphicsInitialize`). Flags `DkDeviceFlags_DepthZeroToOne | DkDeviceFlags_OriginLowerLeft` make image row 0 the bottom, like OpenGL, with clip-space z in [0, 1]. The perspective matrix in the example matches (`perspective` is right-handed, z in [0, 1]); a matrix built for [-1, 1] depth clips half the scene.
2. **Image memory.** One block holds the swapchain images, the depth image, two offscreen colour images and the texture. Each image is placed at an offset aligned to `dkImageLayoutGetAlignment`, and its size comes from `dkImageLayoutGetSize`; do not compute it from width × height. The swapchain images and depth use `DkImageFlags_HwCompression`; the offscreen image is sampled and copied, so it is left uncompressed `[hypothesis]` (compression with sampling is supported; this program avoids testing two things at once).
3. **Swapchain.** `DkImageFlags_UsagePresent` images, created from the default window (`nwindowGetDefault`). Handheld is 1280×720; the dock path was not tested here.
4. **Shaders.** GLSL cannot be used at run time. `uam` compiles each `source/*.glsl` to a DKSH file under `romfs/shaders/` at build time (the stage comes from the `_vsh` / `_fsh` suffix), and `romfsInit` makes them readable. `loadShader` copies each file into a `Code` block at a `DK_SHADER_CODE_ALIGNMENT` offset and refuses to use the last `DK_SHADER_CODE_UNUSABLE_SIZE` bytes. A missing file prints its name and stops setup; see [errors](errors-and-failure-modes.md).
5. **Vertex and index data.** 24 vertices (position + UV, stride 20 bytes) and 36 `uint16_t` indices go in the data block through its CPU address, once. `DkVtxAttribState` gives each attribute's offset and format and `DkVtxBufferState` the stride; a wrong stride draws garbage that looks like shredded geometry.
6. **Texture.** A 64×64 checkerboard is written linearly into a staging region and copied with `dkCmdBufCopyBufferToImage`, which writes the block-linear layout the GPU samples (see [GOB](../glossary.md#gob)). A CPU bitmap cannot be treated as a block-linear image; use the copy, or convert at build time with a swizzle tool. The staging memory is reused only after the one `dkQueueWaitIdle` that follows the upload.
7. **Descriptors.** Image and sampler descriptors are written once into the data block while no GPU work exists, then bound with `dkCmdBufBindImageDescriptorSet` / `dkCmdBufBindSamplerDescriptorSet`. A texture handle is `dkMakeTextureHandle(imageIndex, samplerIndex)`; the GLSL `binding` is the index in the bound handle table.
8. **Depth and culling.** Pass A binds colour and depth together, clears both, enables depth test and write (`DkCompareOp_Less`), and keeps the default back-face culling with counter-clockwise front faces. Each cube face is generated counter-clockwise seen from outside; inside-out geometry (wrong winding, or a flipped projection) shows as a missing cube, not a wrong one. Keep colour and depth targets the same size: USF4 lost most of its picture to a smaller bound depth target `[reported]` (see [rendering](rendering.md#lessons-the-ports-paid-for)).
9. **Two passes.** Pass A writes the offscreen image. `dkCmdBufBarrier(DkBarrier_Fragments, DkInvalidateFlags_Image)` finishes its writes and refreshes the texture cache, then pass B draws one oversized triangle (no vertex buffer; `gl_VertexID`) that samples the offscreen image and swaps red and blue, so a picture or pixel check proves pass B ran. The per-image "bind swapchain image" list sits between the two command lists.
10. **Frame ring** (`frame`). Slot `n % DKR_SLOTS` owns a command buffer with its own region of command memory, a uniform region, a report region and one `DkFence`. Per frame: `dkFenceWait` on the slot (only if it was submitted), read its GPU reports, record, `dkQueueAcquireImage`, submit A, bind list, B, `dkQueueSignalFence(queue, &slot->done, true)`, `dkQueuePresentImage`. The uniform matrix is written by the CPU into the slot's region after the fence wait.
11. **GPU timing.** `dkCmdBufReportCounter` writes `DkCounter_TimestampPipelineTop` before pass A, `DkCounter_FragmentShaderInvocations` after the draw and `DkCounter_Timestamp` after pass B. A report is two 64-bit words (counter, timestamp). The GPU time of the frame is `dkTimestampToNs(end − start)`. Counter operations flush the tiled cache, so they change what they measure `[documented]` (`Primer.md` "Counters"): see [profiling](profiling-and-benchmarks.md).
12. **Unfocused.** The loop skips drawing and sleeps while `appletGetFocusState()` is not in focus. Resize, docked/handheld switching and device loss are **not** handled; see [lifecycle](lifecycle-and-threading.md).
13. **Teardown.** `dkQueueWaitIdle` once, then destroy queue, command buffers, memory blocks, swapchain (before the image memory it uses) and device.

## What the self-test checks

With `-DDKR_SELFTEST` the program runs 240 frames, and on frame 100 renders pass B a second time into an uncompressed image, copies it to a CPU-readable buffer after a fence with `flush = true`, and writes `report.csv` and `check.rgba` (raw 1280×720 RGBA) to `sdmc:/switch/dkrenderer/`. Checks:

| Check | Establishes |
| ----- | ----------- |
| `corner_is_swapped_clear_colour` | Pass A cleared and pass B sampled and swapped channels |
| `cube_covers_5_to_60_percent`, `cube_centre_is_not_background` | Geometry and depth produced a picture of plausible size |
| `texture_sampled_two_or_more_colours` | The texture was sampled, not a constant |
| `fragment_counter_covers_picture` | The GPU counted at least as many fragments as pixels drawn |
| `gpu_report_timestamps_ordered` | The reports were readable after the fence |

It does not check hazards (a missing barrier may still pass), the dock path, resize, or suspend. Record the result with the console model, firmware and library revisions ([source versions](source-versions.md#verification-model)).

## Shader toolchain: native homebrew versus ports

`uam` is a PC tool. A native project knows its shaders and compiles them at build time; a missing DKSH is a load failure, not a run-time compile `[documented]` (`deko3d` `README.md`). A port that must handle shaders it did not choose in advance needs its own run-time compiler or a prebuilt cache; that is application work, not a `deko3d` feature (see [shader hitches](rendering.md#lessons-the-ports-paid-for)).

## Pitfalls

- **Black screen.** Missing `romfs/shaders/*.dksh`, a block without the `Code` or `Image` flag, or a size that is not a multiple of `DK_MEMBLOCK_ALIGNMENT`. Run with `-ldeko3dd` and read the callback output over nxlink.
- **Mirrored or upside-down image.** `OriginLowerLeft` and `OriginUpperLeft` flip image rows; clip-space Y orientation is separate (`DkDeviceFlags_YAxis*` exist only in newer headers than the installed 0.5.0).
- **Blocking mid-frame.** `dkQueueWaitIdle` inside the loop makes frame time CPU + GPU instead of the larger of the two.
- **Debug-only validation.** The release library omits parameter and state checks; test both.

## Open questions

- Does it draw, and is `Fragments` + `Image` the right barrier? Needs a console run.
- Do two or three slots give better pacing and latency? See [profiling](profiling-and-benchmarks.md).
- Resize, suspend/resume and handheld/dock transitions are not implemented; they need the sequence in [lifecycle](lifecycle-and-threading.md) and a test that stops submissions, waits all fences, recreates size-dependent images and resumes.

## Sources

- [deko3d](https://github.com/devkitPro/deko3d/blob/373bfd43d8b2d805fa2eb7d406dbb823032df958/Primer.md) at `373bfd43d8b2d805fa2eb7d406dbb823032df958` — `Primer.md`, `include/deko3d.h`, `README.md`.
- [switch-examples](https://github.com/switchbrew/switch-examples) at `669786898205b7beb25ff1731e72982e6d0397d3` — `graphics/deko3d/deko_basic` (setup sequence), `deko_examples` `Example04_TexturedCube.cpp` (vertex state, descriptors).
- [uam](https://github.com/devkitPro/uam) at `5a5afc2bae8b55409ab36ba45be63fcb73f68993` — offline GLSL compiler.
- `nx/engineering/examples/deko-renderer` — the program described.
