# Case study: deko3d GPU stack on Tegra X1

deko3d is the community homebrew low-level 3D API for the Nintendo Switch: an explicit, Vulkan-like layer over the second-generation Maxwell GPU (GM20B) in the **[Tegra X1](../../glossary.md#tegra-x1)**, built from a year-and-a-half of reverse engineering NVIDIA's NVN. This page summarises the project's own tree — Primer.md, README.md, `deko3d.h`, the uam compiler, tegra_swizzle, and switch-examples 01–09. Nothing was measured by Coffeepot; header facts `[documented]`, silicon and port costs `[reported]`.

## Before you start

- Evidence: API behaviour `[documented]` from `private_please_gitignore/switch/deko3d/`; clocks, bandwidth, occupancy, bind costs `[reported]`.
- Clocks: ~307.2 MHz handheld, 384 MHz higher-handheld, 768 MHz docked; 25.6 GB/s shared peak docked, lower handheld `[reported]`.
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
| Cores | 256 CUDA in 2 SMMs, 16 texture units, 16 ROPs `[reported]` (Chips & Cheese). |
| Clocks, bus | 307.2 / 384 / 768 MHz; 25.6 GB/s shared peak docked, lower handheld `[reported]`. |
| Layout | Block-linear GOB 64×8; tegra_swizzle implements tile/untiling `[documented]`. |
| Occupancy | 64 warps/SM, 255 regs/thread, 64 KiB shared/SM; FP64 div/sqrt approximated `[reported]` (Tuning Guide). |
| Precision | FP16 at reduced cost, never autopacked; variable int div/mod decays to float `[documented]`. |

## What it does well

| Strength | Why it matters |
| -------- | -------------- |
| Explicit control | No hidden allocator or driver thread; queues, barriers, layouts, slots are yours. |
| Validation layer | `-ldeko3dd` catches bad state on dev machines; ship on `-ldeko3d`. |
| No runtime compiler | Precompiled DKSH, ~500 KiB apps vs ~8 MB switch-mesa, no first-run stutter `[reported]`. |
| Maxwell features | Zcull, tiled cache, compressed targets nouveau had not picked up `[reported]`. |

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
| 4 | G-buffer saturation on the shared 25.6 GB/s bus `[reported]` — keep passes in the tiled cache (Example 08). |
| 5 | Depth cost on 16 ROPs `[reported]` — keep Zcull on, discard rather than clear overwritten attachments. |
| 6 | Shader stutter and NRO bloat — compile to DKSH offline within caps, ship `.dksh`, dev on `-ldeko3dd`. |

## Open questions

- Whether the OS kill-after-GPU-crash behaviour can be disabled is unknown.
- Sustained shared-bus bandwidth per clock profile is unquantified.
- One special Zcull mode plus unmapped "color reduction"/"stencil cull" knobs.
- Captured words may not survive version changes; invalidation is the app's job.

## Sources

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
