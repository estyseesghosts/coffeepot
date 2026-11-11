# Case study: deko3d GPU stack on Tegra X1

deko3d is the community homebrew low-level 3D API for the Nintendo Switch: an explicit, Vulkan-like layer over the second-generation Maxwell GPU in the **[Tegra X1](../../glossary.md#tegra-x1)**, built from a year-and-a-half of reverse engineering NVIDIA's NVN. This page summarises the project's own tree — Primer.md, README.md, `deko3d.h`, the uam compiler, tegra_swizzle, and switch-examples 01–09. Nothing was measured by Coffeepot; header facts `[documented]`, silicon and port costs `[reported]`.

## When is this useful?

This page overlaps [libnx and deko3d](../../homebrew/deko3d-and-libnx.md) and [rendering](../rendering.md), which are canonical. Use it as a one-page recap of the `deko3d` object model and its limits.

## Before you start

- Evidence: API behaviour `[documented]` from [devkitPro/deko3d](https://github.com/devkitPro/deko3d) at `373bfd4`; clocks and bind costs `[reported]`. This page duplicates parts of [libnx and deko3d](../../homebrew/deko3d-and-libnx.md) and [rendering](../rendering.md), which are canonical.
- Scope: user-space rendering through **[deko3d](../../glossary.md#deko3d)** plus **[libnx](../../glossary.md#libnx)** on **[NX](../../glossary.md#nx)**. Kernel internals and NVN are background.

Jargon: the **[CPU](../../../glossary.md#cpu)** records command words the GPU executes; both share one **[system-on-a-chip](../../../glossary.md#soc)** and bus; a **[shader](../../../glossary.md#shader)** is a GPU program; each swapchain **[framebuffer](../../../glossary.md#framebuffer)** is one displayable image; **[cache](../../../glossary.md#cache)** is the CPU/image/descriptor/L2 caches barriers invalidate; **[texture tiling](../../../glossary.md#tiling)** is the block-linear GOB layout images must use; **[Maxwell](../../glossary.md#maxwell)** is the GPU architecture generation.

## What makes deko3d tick

Creation order, each object borrowing earlier ones `[documented]`:

| Step | Object | Notes |
| ---- | ------ | ----- |
| 1 | `DkDevice` | Root object, private GPU address space; origin/clip flags fixed at creation. |
| 2 | `DkMemBlock` | Wholesale store, size multiple of 0x1000; `Code` blocks enter a 4 GiB code segment, `Image` blocks add compressed mappings. |
| 3 | `DkImage`, `DkShader`, `DkCmdBuf` | Layouts, DKSH shaders, command buffers carved from blocks. |
| 4 | `DkQueue` | Graphics/Compute/priority/Zcull flags; ring ≥ 0x10000, threshold size/8; per-queue state. |
| 5 | `DkSwapchain` | Caller images plus window; acquire/present by slot, one fence per slot. |

| Mechanism | Behaviour |
| --------- | --------- |
| Lazy flush | Items batch until flush, present-flush, ring wrap, or threshold; each flush invalidates image/shader/descriptor/L2 caches. |
| Barriers | Tiers `None`/`Tiles`/`Fragments`/`Primitives`/`Full` plus image/shader/descriptor/Zcull/L2 invalidate bits. |
| Tiled cache | `Enable`/`Disable`/`Flush`/`FlushAlt` plus tile size; Example 08 keeps deferred G-buffers in tile memory. |
| Clears, Zcull | Clear/discard color and depth-stencil, depth resolve; compressed targets decompressed pre-present; Zcull default-on, save/restore TODO. |
| Capture/replay | Record raw GPU words once, replay at zero CPU cost; excludes compute, fences, indirect draws, `Full`; version-matched only. |
| DKSH pipeline | Native SM53 SASS only, no runtime compiler; uam compiles GLSL offline; mandatory `layout(binding=N)`, per-stage caps 16/16/32/8 UBO/SSBO/sampler/image (compute UBO 6–15 emulated). |
| Debug/release | `-ldeko3dd` validates via debug callback; `-ldeko3d` drops checks, errors become Fatal. |

## How it meets the A57 and the GPU

CPU records into `GpuCached|CpuUncached` memory, submitted as one batch at flush; each flush, acquire, and present is a kernel round trip `[documented]`. CPU batching is the bottleneck: the SA2 port reports ~3 µs binds, ~2.5 µs draws, ~1.5 µs descriptor updates on the A57 — its reason for dropping NVK `[reported]`. deko3d answers with 64 KiB push-style uniform updates, capture/replay, and threshold tuning `[documented]`.

| GPU fact | Value |
| -------- | ----- |
| Cores | 256 CUDA cores (SwitchBrew Tegra X1) in 2 multiprocessors (arithmetic, Chips and Cheese) `[reported]`. Texture-unit and ROP counts are Unknown: no saved source states them. |
| Clocks, bus | 307.2 / 460.8 / 768 MHz stock per `nfsmw-nx`; 25.6 GB/s theoretical shared peak, 21.3 GB/s at the handheld memory clock (arithmetic) `[reported]`. See [NX GPU](../../hardware/gpu.md). |
| Layout | Block-linear GOB 64×8; tegra_swizzle implements tile/untiling `[documented]`. |
| Occupancy | The CUDA Maxwell Tuning Guide's occupancy numbers describe desktop CUDA kernels; whether they apply to uam graphics shaders is a `[hypothesis]`. Shared memory is 64 KB per multiprocessor `[reported]` (Chips and Cheese). |
| Precision | uam: integer division and modulo by non-constants decay to floating point with a warning; 64-bit float divide and square root are approximated `[documented]` (`uam` `README.md`). NVIDIA's own compiler on the Switch did not pack FP16 automatically `[reported]` (Chips and Cheese); uam's behaviour is untested. |

## What it does well

| Strength | Why it matters |
| -------- | -------------- |
| Explicit control | No hidden allocator or driver thread; queues, barriers, layouts, slots are yours. |
| Validation layer | `-ldeko3dd` catches bad state on dev machines; ship on `-ldeko3d`. |
| No runtime compiler | Precompiled DKSH, ~500 KiB apps vs ~8 MB switch-mesa, no first-run stutter `[reported]`. |
| Maxwell features | Zcull, tiled cache, compressed targets that, per the deko3d README, nouveau had not picked up `[documented]` (README's claim). |

## What it does poorly

| Limit | Detail |
| ----- | ------ |
| You own coherence | Barriers, invalidates, layouts, resize, fence lifetimes are yours; a miss is silent corruption. |
| Code-segment cap | 4 GiB space never reused after free; last 0x400 bytes of each code block unusable — reuse blocks. |
| Fence stability | Fence commands keep a raw pointer to your struct; moving it in flight is undefined behaviour. |
| Block-count limits | Every block costs OS bookkeeping whatever its size — buy wholesale, suballocate. |
| Crash recovery | A GPU error toasts the queue; the OS kills GPU-crashing processes seconds later. |
| Single-vendor gaps | Maxwell-only by design; transform feedback, multidraw, conditional render, events, sparse textures missing. |

## Lessons: what to copy first

| # | Symptom to fix |
| - | -------------- |
| 1 | Bind-scale CPU cost `[reported]` — capture repeated state once, replay it. |
| 2 | Uniform-update stalls — 64 KiB push-style updates between flushes. |
| 3 | Ring-wrap stalls — grow the ring, tune the threshold (size/8) so the GPU drains first. |
| 4 | G-buffer traffic on the shared memory bus — Example 08 keeps deferred G-buffers in tile memory; the benefit is `[hypothesis]` until measured. |
| 5 | Depth cost — Zcull is on by default; discarding attachments whose contents are not needed is an option the API provides `[documented]`; the gain is `[hypothesis]`. |
| 6 | Shader stutter and NRO bloat — compile to DKSH offline within caps, ship `.dksh`, dev on `-ldeko3dd`. |

## Open questions

- Whether the OS kill-after-GPU-crash behaviour can be disabled is unknown.
- Sustained shared-bus bandwidth per clock profile is unquantified.
- One special Zcull mode plus unmapped "color reduction"/"stencil cull" knobs.
- Captured words may not survive version changes; invalidation is the app's job.

## Sources

Repositories at pinned commits (see [source versions](../source-versions.md)): [devkitPro/deko3d](https://github.com/devkitPro/deko3d) `373bfd43d8b2d805fa2eb7d406dbb823032df958`, [devkitPro/uam](https://github.com/devkitPro/uam) `5a5afc2bae8b55409ab36ba45be63fcb73f68993`, [ScanMountGoat/tegra_swizzle](https://github.com/ScanMountGoat/tegra_swizzle) `d7ed318ce233a4d557d4a2f207ed5dc04ed813c0`, [switchbrew/switch-examples](https://github.com/switchbrew/switch-examples) `669786898205b7beb25ff1731e72982e6d0397d3`, [ChanseyIsTheBest/sa2-nx](https://github.com/ChanseyIsTheBest/sa2-nx) `baf523b8e93a1b02d8b73d13aebf86780f8cc0c6`. Files:


- deko3d `Primer.md` — objects, queues, fences, barriers, DKSH, uam limits, capture/replay, debug vs release.
- deko3d `README.md` — goals, NVN relation, mesa comparison, gap list.
- deko3d `include/deko3d.h` — flags, limits, defaults (`DK_*`, barrier/invalidate enums, swapchain maker).
- uam `README.md`, `source/` — offline GLSL→Maxwell compiler, binding rule, 16/16/32/8 caps, warnings.
- tegra_swizzle `tiling.md`, `src/` — GOB tiling/untiling for baking assets.
- switch-examples `graphics/deko3d` — 01 setup, 02 triangle, 03 cube, 04 texture, 05 tessellation, 06 multisampling, 07 lighting, 08 deferred+tiled, 09 compute.
- Maxwell Tuning Guide HTML — occupancy and register background `[reported]`.
- Chips & Cheese Tegra X1 HTML — cores, clocks, bandwidth background `[reported]`.
- open-gpu-doc tree — published class background only.
- sa2-nx port tree — A57 bind/draw/descriptor costs `[reported]`.
