# How a draw reaches the screen on the Switch's Maxwell GPU

This page is for a developer who has a `deko3d` renderer running and wants a model of what each step does, so they can tell whether a slow frame is limited by the CPU, the GPU's arithmetic or memory traffic. It follows one textured, depth-tested triangle from the CPU to the display and says, at each step, what can become the limit and how to find out which. It does not repeat the hardware tables on [NX GPU](../hardware/gpu.md).

## Before you start

- Sources: the `deko3d` `Primer.md` and `deko3d.h` at the pinned commit ([source versions](source-versions.md)). The stage list is the Primer's own "graphics pipeline" section; its mapping to physical hardware is **not** specified there. Where this page goes beyond the Primer it says so.
- Nothing here has been measured on a console by this wiki. SwitchBrew's GPU pages (memory mapping, command submission, classes, shader format) were not among the saved sources, so low-level register and channel claims are left out rather than guessed.
- Desktop GM204 (Maxwell 2) documentation explains architectural ideas, but the Switch's Tegra X1 GPU is smaller and differently configured; do not carry desktop throughput, cache sizes or feature availability over `[hypothesis]`.
- Order of reading: [the renderer](deko3d-renderer.md) for the code, [GPU memory and synchronization](gpu-memory-and-synchronization.md) for ownership, [profiling](profiling-and-benchmarks.md) for measurement.

## One draw, step by step

| Step | What happens | Can limit the frame when | How to tell |
| ---- | ------------ | ------------------------ | ----------- |
| 1. CPU preparation (A57) | Game logic runs; geometry, uniforms and GPU-visible memory are prepared. The CPU does not shade pixels | Simulation or render preparation is long | CPU timers; GPU idle |
| 2. Recording and submission (`deko3d`) | Commands (state, resource addresses, draws) are written into command memory; lists are submitted; the queue flushes | Many draws or state changes; flush and present are kernel round trips | Time `dkCmdBuf*` and submit calls separately from GPU time |
| 3. Vertex fetch and shading | The GPU reads vertex and index data using the bound attribute and buffer state; the vertex shader transforms positions | Large meshes, many vertices | `DkCounter_InputVertices`, `VertexShaderInvocations` |
| 4. Assembly, clip, cull, rasterize | Vertices become primitives; clipping, face culling and the viewport transform decide which sample locations are covered | Tiny triangles; heavy overdraw begins here | `DkCounter_InputPrimitives`, `ClipperOutputPrimitives` |
| 5. Early tests | Depth work that can reject fragments before shading (Zcull, early depth) | Scenes with heavy occlusion benefit; ordering matters | `DkCounter_ZcullStats`, `SamplesPassed` |
| 6. Fragment shading | The fragment shader runs, sampling textures | Pixel-heavy shaders, many texture fetches, high resolution | `FragmentShaderInvocations`; vary resolution |
| 7. Sample processing and output | Depth, stencil and blending; results are written to the render target | Overdraw, blending, bandwidth | Compare colour-only, colour + depth, an extra target |
| 8. Presentation | The finished image is handed to the display through the swapchain | The display still holds all buffers | Time `dkQueueAcquireImage`; compare GPU completion with the present interval |

Several stages overlap in time and do not run strictly one after another; the Primer lists supported stages and API operations, not a schedule `[documented]`. GPU completion and the image becoming visible are related but different events.

## Texture and render-target layout

A CPU programmer pictures an image as rows of pixels. The GPU stores images in **block-linear** layout built from 64-byte by 8-row blocks (**[GOB](../glossary.md#gob)**s), arranged for its access patterns `[documented]` (`tegra_swizzle` `tiling.md`). Consequences:

- The allocation can exceed width × height × bytes per pixel: take size and alignment from `dkImageLayoutGetSize` and `dkImageLayoutGetAlignment` instead of reproducing alignment rules.
- Data uploaded with the wrong layout looks scrambled. Upload through `dkCmdBufCopyBufferToImage` (the GPU does the swizzle) or convert at build time; `DkImageFlags_PitchLinear` makes a linear image you fill linearly.
- Mipmaps, compression and alignment all change the footprint.

A good example would show one texture through a correct block-linear upload, a correct pitch-linear image and a deliberately wrong layout; not written yet.

## Zcull, compression and the tiled cache

Three different mechanisms; none is a free speed-up.

| Mechanism | What it is | API |
| --------- | ---------- | --- |
| Zcull | Hardware depth culling; on by default for a queue | `DkQueueFlags_EnableZcull` / `DisableZcull`, `DkInvalidateFlags_Zcull`, `DkCounter_ZcullStats` |
| Render-target compression | Reduces some memory traffic; a target is decompressed before presentation; not the same as texture-file compression | `DkImageFlags_HwCompression` |
| Tiled cache | Keeps render-pass data on chip for some pass structures; its use depends on how passes use attachments and on correct barriers | `dkCmdBufTiledCacheOp` |

`[documented]` as API (`deko3d.h`, `README.md`). Whether each helps your workload is `[hypothesis]` until a same-scene comparison shows it. Show a benefit only on a workload that can exercise it (a heavily occluded scene for Zcull, for example), and record cases where it adds work. SwitchWakerHD leaves the tiled cache and depth compression off because it could not verify them `[reported]`.

## Throughput versus memory bandwidth

CPU and GPU share external memory, but a memory clock's peak is not the bandwidth any one operation gets: access patterns, caches, compression and contention all matter. At 1920×1080 an RGBA8 image is 8,294,400 bytes before layout overhead. That is a footprint, not traffic: multiplying it by the frame rate does not give memory traffic. To learn what a pass costs, compare a colour-only target, colour + depth, an extra offscreen target, an extra fullscreen pass, and different internal resolutions, with the footprint calculated and the GPU time measured.

## FP16, compute and occupancy

Whether Maxwell supports FP16 is not the question; whether the compiler emits instructions that use it is. Chips and Cheese found NVIDIA's own compiler on the Switch did not pack FP16 automatically `[reported]`; whether `uam` does is untested `[hypothesis]`. For compute, workgroup size, shared memory, barriers and dispatch overhead matter, and NVIDIA's CUDA Maxwell guidance is architectural background only; confirm with `uam` output and measurements.

## Pitfalls

- **Concluding "GPU-bound" from a fence wait.** Use timestamps and counters.
- **Estimating allocation from width × height.** Ask the layout.
- **Crediting Zcull or compression without a workload that uses them.**
- **Carrying desktop GM204 numbers over to the Switch.**

## Open questions

- Which stage limits a typical homebrew scene at handheld clocks? Needs counters and the experiments in [profiling](profiling-and-benchmarks.md).
- What do SwitchBrew's GPU documents add about command submission and the channel model? Not read for this page.
- Does `uam` pack FP16? Needs a SASS comparison of the same shader with and without packed types.

## Sources

- [deko3d](https://github.com/devkitPro/deko3d/blob/373bfd43d8b2d805fa2eb7d406dbb823032df958/Primer.md) at `373bfd43d8b2d805fa2eb7d406dbb823032df958` — `Primer.md` "The graphics pipeline", "Counters"; `include/deko3d.h`; `README.md`.
- [tegra_swizzle](https://github.com/ScanMountGoat/tegra_swizzle) at `d7ed318ce233a4d557d4a2f207ed5dc04ed813c0` — `tiling.md`.
- Chips and Cheese, [Nintendo Switch's iGPU: Maxwell Nerfed Edition](https://chipsandcheese.com/) (2023-12-23) `[reported]`; [SwitchWakerHD](https://github.com/centollOS/SwitchWakerHD) at `9aec4e3` `[reported]`.
