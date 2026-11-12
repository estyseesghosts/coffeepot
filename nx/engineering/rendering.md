# Rendering on NX with deko3d

This page is for a homebrew developer who is drawing with `deko3d`, the community graphics library for the Switch, and wants the frame loop to be correct first and fast second. The short answer: build the frame the way the upstream `deko_basic` example does, give every in-flight frame its own command memory and a **[fence](../glossary.md#fence)**, wait for that fence on the CPU before reusing the slot, and treat every performance trick below as something to measure on your own scene before you keep it.

Every API call on this page was compiled against the installed headers (see [source versions](source-versions.md)). None of it has been run on a console. Where a recommendation rests on a design opinion instead of a measurement or a header, it is labelled `[hypothesis]` and says what would settle it.

## Before you start

- Toolchain: devkitA64 r30-1, `libnx` 4.12.0-1, `deko3d` 0.5.0-1, `uam` 1.1.0-2, built with `-Wall` and no warnings. Source commits read are pinned on [source versions](source-versions.md).
- Applies to the Nintendo Switch family running homebrew through `deko3d` on the NVIDIA **[Tegra X1](../glossary.md#tegra-x1)**. Model and clock differences are on the [GPU](../hardware/gpu.md) and [models](../hardware/models.md) pages.
- Read [libnx and deko3d](../homebrew/deko3d-and-libnx.md) first for the object model, [a native renderer, step by step](deko3d-renderer.md) for a complete program, [GPU memory and synchronization](gpu-memory-and-synchronization.md) for ownership rules, and [profiling and benchmarks](profiling-and-benchmarks.md) for how to measure.
- All code here is C and uses only the public `deko3d.h` API. The upstream examples use the C++ wrapper (`deko3d.hpp`); both call the same functions.

## Start from the upstream frame loop

What to do: copy `graphics/deko3d/deko_basic` from `switch-examples` and change it. Do not write the setup from memory. The sequence it uses is the one the rest of this page assumes: create a device, a framebuffer image layout, a memory block holding the swapchain images, a swapchain, a code memory block for shaders, a command memory block and command buffer, then one queue. Each frame it calls `dkQueueAcquireImage`, submits the command lists, and calls `dkQueuePresentImage` `[documented]` (`switch-examples` `graphics/deko3d/deko_basic/source/main.c`, lines 68-188).

The shader files are built by the example's Makefile: each `*.glsl` becomes a `.dksh` file in `romfs/shaders/` through `uam`, and `romfsInit` makes them readable at `romfs:/shaders/` `[documented]` (same file, lines 117-119; `switch-examples` `graphics/deko3d/deko_basic/Makefile`).

- Expected result: a coloured triangle on a cleared background, and `Plus` returns to the homebrew menu.
- How it fails: a black screen or an abort almost always means a missing `romfs/shaders/*.dksh`, a memory block missing a required flag (`DkMemBlockFlags_Code` for shaders, `DkMemBlockFlags_Image` for swapchain images), or a size that is not a multiple of `DK_MEMBLOCK_ALIGNMENT` (`0x1000`) `[documented]` (`deko3d.h`). Link `-ldeko3dd` while developing so the validation layer reports the bad call.

## Reuse a frame slot only after the GPU is done with it

What to do: keep two (or three) frame slots. Each slot owns one command buffer with its own command memory and one fence. Before recording into a slot, wait on its fence from the CPU with `dkFenceWait`. After submitting, tell the queue to signal the fence with `dkQueueSignalFence`.

```c
#define SLOTS 2
typedef struct {
    DkCmdBuf cmdbuf;   // command memory added with dkCmdBufAddMemory, one region per slot
    DkFence  done;     // signalled by the GPU when this slot's work has finished
} FrameSlot;
static FrameSlot g_slots[SLOTS];   // static storage: fences start zeroed
static unsigned  g_frameNo;

static void frame(DkQueue queue, DkSwapchain swapchain, const DkCmdList bindTarget[])
{
    FrameSlot* s = &g_slots[g_frameNo++ % SLOTS];

    dkFenceWait(&s->done, -1);          // CPU blocks until the GPU finished this slot's last frame
    dkCmdBufClear(s->cmdbuf);           // safe now: the old command list is no longer in use
    recordScene(s->cmdbuf, g_frameNo);  // your recording code
    DkCmdList scene = dkCmdBufFinishList(s->cmdbuf);

    int image = dkQueueAcquireImage(queue, swapchain);
    dkQueueSubmitCommands(queue, bindTarget[image]);
    dkQueueSubmitCommands(queue, scene);
    dkQueueSignalFence(queue, &s->done, false);
    dkQueuePresentImage(queue, swapchain, image);   // also flushes the queue
}
```

Verification: compiles in `api-check`, `-Wall`, no warnings (a signature check: the fragment lacks the shaders and render targets a real frame needs); not run on hardware. The complete working version is in `nx/engineering/examples/deko-renderer`. The `bindTarget` lists are built once at setup as in `deko_basic` (lines 134-141).

- Expected result: the CPU records frame N+1 while the GPU draws frame N, and the CPU only blocks when the GPU is more than `SLOTS` frames behind.
- Why this is the right wait: `dkQueueWaitFence` makes the **GPU** wait for a fence; it never blocks the CPU. `dkFenceWait` is the CPU-side wait `[documented]` (`Primer.md` "Fences" and "Queues": "Fences can be waited on by the GPU or CPU (using the `dkFenceWait` function)"; `dkQueueWaitFence`: "The GPU can be instructed to wait on a fence"). A fence that may be waited on before its first signal must be zero-initialised, and it must not move in memory while any command list that references it exists `[documented]` (`Primer.md` "Fences").
- How it fails: if frame time equals CPU time plus GPU time instead of the larger of the two, something is waiting too early, usually `dkQueueWaitIdle` inside the frame. `dkQueueWaitIdle` is shorthand for signalling a temporary fence, flushing and waiting, and the Primer says to use it sparingly; use it once, before destroying resources `[documented]` (`Primer.md` "Queues"; `deko_basic` `graphicsExit`).

> Unknown: whether two or three slots is better for a given scene. It trades latency against how much slack the CPU has before it blocks. A sweep of 1 to 4 frames against measured input-to-display latency would settle it.

## Submission is lazy; flushing is a window to update memory

What to do: remember that `dkQueueSubmitCommands` does not start the GPU. Work reaches the GPU when the queue is flushed (`dkQueueFlush`, which `dkQueuePresentImage` calls), when the internal list fills, or when the flush threshold is reached `[documented]` (`Primer.md` "Queues"). After each flush deko3d inserts a barrier that invalidates the image, shader, descriptor and L2 caches. That makes the point after a flush the supported moment to rewrite vertex, uniform or descriptor memory from the CPU, as long as the GPU is not still reading it `[documented]` (same section).

- Expected result: you can overwrite a buffer in slot A's memory after slot A's fence has been waited on, without an extra manual cache invalidate.
- How it fails: writing into memory that an unfinished frame still reads gives corrupted geometry for one frame. Per-slot memory regions, not one shared region, are the fix.

## Upload static data once; send small changes per frame

What to do: allocate a `DkMemBlock` for data that never changes, copy into it once at load time through its CPU address, and refer to it by GPU address from then on. Per frame, change only small uniform blocks, either by writing into a per-slot uniform region or by recording `dkCmdBufPushConstants` into the command stream.

```c
// Load time: create a block, copy once.
DkMemBlockMaker maker;
uint32_t size = (bytes + DK_MEMBLOCK_ALIGNMENT - 1) & ~(DK_MEMBLOCK_ALIGNMENT - 1);
dkMemBlockMakerDefaults(&maker, device, size);          // CpuUncached | GpuCached
DkMemBlock block = dkMemBlockCreate(&maker);
memcpy(dkMemBlockGetCpuAddr(block), verts, bytes);      // once

// Per frame: bind a uniform buffer, then change its contents inside the command stream.
dkCmdBufBindUniformBuffer(cmdbuf, DkStage_Vertex, 0,
    dkMemBlockGetGpuAddr(uniformBlock) + offset, 0x100); // buffer address aligned to DK_UNIFORM_BUF_ALIGNMENT
dkCmdBufPushConstants(cmdbuf, dkMemBlockGetGpuAddr(uniformBlock), 0x100, 0, 16 * sizeof(float), mvp);
```

Verification: compiles, `-Wall`, no warnings; not run on hardware. `dkMemBlockMakerDefaults` sets `CpuUncached | GpuCached` `[documented]` (`deko3d.h`), the same flags `deko_basic` uses for command memory. `dkCmdBufBindUniformBuffer` takes `(cmdbuf, stage, id, address, size)` and wraps `dkCmdBufBindUniformBuffers` `[documented]` (`deko3d.h`). `dkCmdBufPushConstants` writes into a uniform buffer from within the command stream; all uniform buffers have push-constant semantics and may be up to 64 KiB `[documented]` (`Primer.md` "Inline data updates").

CPU-cached memory (`DkMemBlockFlags_CpuCached`) needs `dkMemBlockFlushCpuCache` before the GPU reads, and the Primer advises against CPU-cached memory for GPU-to-CPU transfers because the CPU cache cannot be invalidated from userland `[documented]` (`Primer.md` "Memory Blocks").

- Expected result: no per-frame copies of geometry; only the small uniform data changes.
- How it fails: a profile that shows a copy of the same vertex data every frame means the buffer belongs in load-time memory. `[hypothesis]` that this matters on Switch more than on a desktop because the CPU and GPU share one memory bus; confirm by timing a frame with and without the per-frame upload.

## Cut CPU draw cost before you cut GPU cost

What to do: measure first (see [performance and porting](performance-and-porting.md)). If the render thread is the limit and the GPU is not busy, reduce the number of separate draws and state changes. Common shapes are merging static geometry into larger buffers grouped by material, packing many small images into one texture, and recording unchanging state once.

- Evidence, honestly stated: two ports report the same pattern, not a general law. USF4's `deko3d` renderer used 2.8 ms of render-thread time per frame against 18.6 ms for its Vulkan-on-NVK renderer, in one docked Training-fight A/B at CPU 1887 MHz, GPU 844.8 MHz, memory 1862.4 MHz, 720p `[reported]` (`usf4-nx` `docs/deko3d-renderer-usf4.md` section 1, commit `30ffae0`). The expensive part there was the Vulkan driver's per-bind and per-draw work on the A57, which `deko3d` does not have `[reported]` (`sa2-nx` `docs/deko3d-renderer-guide.md`, measured by banjo-nx: about 3 µs per pipeline bind and 2.4-2.9 µs per draw on NVK). Those numbers are not `deko3d` costs; do not use them as a `deko3d` draw budget.
- `[hypothesis]` Merging small draws helps a `deko3d` scene that is bound by the CPU recording and submitting them. What would settle it: a benchmark that sweeps draw count from 100 to 10,000 at fixed total vertex count, on retail clocks, recording CPU time and GPU time separately. This benchmark has not been written.
- `[hypothesis]` Specific tricks often recommended for tile maps (16×16-tile chunks, one texture atlas per material, cutout by `discard` instead of blending) come from unreviewed tutorial walkthroughs in the research folder, not from measurements. They are reasonable defaults for a tile game. They are not requirements: a game with a few hundred draws may gain nothing from them, and whether `discard` costs more than blending on this GPU has not been measured here.

Repeated state can be baked once with command capture and replayed, which avoids re-encoding structs on every use:

```c
static uint32_t words[256];                          // generous: overrunning it is fatal
dkCmdBufBeginCaptureCmds(cmdbuf, words, 256);
/* bind shaders, rasterizer state, colour state, vertex state ... */
uint32_t n = dkCmdBufEndCaptureCmds(cmdbuf);
dkCmdBufReplayCmds(cmdbuf, words, n);                // once per use
```

Verification: compiles, `-Wall`, no warnings; not run on hardware. Capture cannot contain `dkCmdBufAddMemory`, `dkCmdBufFinishList`, `dkCmdBufClear`, compute commands, fence commands, indirect draws, `DkBarrier_Full` or `dkCmdBufCallList`, and running out of capture storage is an unrecoverable error. Replayed words may be incompatible across `deko3d` versions, so capture at run time rather than saving them to disk `[documented]` (`Primer.md` "Command capture and replay").

## Image layout and shaders

What to do: ask `deko3d` for an image's size and alignment instead of computing it. The default is **[block-linear](../glossary.md#gob)** layout, which is what the GPU samples efficiently; pitch-linear is an explicit opt-in.

```c
DkImageLayoutMaker maker;
dkImageLayoutMakerDefaults(&maker, device);
maker.format = DkImageFormat_RGBA8_Unorm;            // flags default to block-linear
maker.dimensions[0] = w;
maker.dimensions[1] = h;
DkImageLayout layout;
dkImageLayoutInitialize(&layout, &maker);
uint64_t bytes = dkImageLayoutGetSize(&layout);
uint32_t align = dkImageLayoutGetAlignment(&layout);
```

Verification: compiles, `-Wall`, no warnings; not run on hardware. This is the sequence in `deko_basic` (lines 76-90) and in the textured-cube example `Example04_TexturedCube.cpp` `[documented]`. Block-linear data is stored in 64-byte by 8-row blocks (GOBs); `tegra_swizzle` implements the byte layout for converting images at build time `[documented]` (`tegra_swizzle` `tiling.md`, commit `d7ed318`).

Shaders are compiled offline. `deko3d` accepts only native Maxwell code in a **[DKSH](../glossary.md#dksh)** file; GLSL and SPIR-V cannot be used at run time, and `uam` is a PC tool that produces DKSH `[documented]` (`deko3d` `README.md` "Can I use the shader compiler inside my program?"; `Primer.md` "Shaders"). This is by design, so a shader that is missing at run time is not compiled on the fly: the load simply fails. Hitches the ports report on first use come from programs that deliberately compile shaders on the console with their own copy of `uam` (SA2, USF4, Wind Waker HD; see the [case studies](case-studies/case-study-sa2.md)). Plain homebrew that ships prebuilt DKSH does not have that problem.

Binding limits per stage: 16 uniform buffers, 16 storage buffers, 32 texture bindings and 8 image bindings `[documented]` (`deko3d.h`: `DK_NUM_UNIFORM_BUFS`, `DK_NUM_STORAGE_BUFS`, `DK_NUM_TEXTURE_BINDINGS`, `DK_NUM_IMAGE_BINDINGS`). `deko3d` has no pipeline objects, so these are binding-slot counts, not limits checked at pipeline creation.

- Expected result: images created from the queried layout sample correctly; shaders load from `romfs`.
- How it fails: assuming a pitch for a block-linear image gives a scrambled texture. Passing `DkImageFlags_PitchLinear` gives a linear image that you must then fill linearly.
- Mipmaps and block compression reduce memory traffic in principle; `[hypothesis]` that they matter for your scene before you have measured it. What would settle it: the same scene with and without mipmaps and compressed formats, with the memory-clock profile fixed (handheld versus docked).

## Barriers, the tiled cache, compression and Zcull

What to do: name the hazard before choosing the barrier. A barrier has two independent parts: the **execution scope** (`DkBarrier_*`: what earlier work must finish) and the **cache invalidations** (`DkInvalidateFlags_*`: which caches the next reader must not trust). The header names four modes: `Tiles` (orders tile processing, like a Vulkan subpass), `Fragments` (orders fragment processing, like a render pass), `Primitives` (completes earlier primitives and compute jobs) and `Full` (completes everything and disables command-list prefetch) `[documented]` (`deko3d.h` `DkBarrier`). No source ranks them by measured cost, so do not pick one because it "seems cheapest"; list what each pass reads and writes and what the next pass reads, as in the table on [GPU memory and synchronization](gpu-memory-and-synchronization.md#barriers-by-the-hazard-they-resolve). Rendering to an image and then sampling it needs both the writes finished and the texture cache refreshed. The tiled cache, render-target compression and image layout also affect what is required. The upstream examples show each use: `DkBarrier_Fragments` before discarding a depth buffer (`Example03_Cube.cpp` line 299), `DkBarrier_Tiles` with `DkInvalidateFlags_Image` between tiled passes (`Example08_DeferredShading.cpp` lines 353-398) and a primitives barrier between compute work (`Example09_SimpleCompute.cpp` line 9).

```c
dkCmdBufBarrier(cmdbuf, DkBarrier_Fragments, 0);
dkCmdBufBarrier(cmdbuf, DkBarrier_Tiles, DkInvalidateFlags_Image);
dkCmdBufTiledCacheOp(cmdbuf, DkTiledCacheOp_Enable);
```

Verification: compiles, `-Wall`, no warnings; not run on hardware.

Hardware compression for render targets is requested with `DkImageFlags_HwCompression`, as `deko_basic` does for its framebuffers (line 78). `deko3d` handles the decompression needed for presentation `[documented]` (`deko3d` `README.md`: "compressed render targets (decompressed prior to presentation)"). `DkImageFlags_Z16EnableZbc` applies to `DkImageFormat_Z16` images only `[documented]` (`deko3d.h`).

Zcull (hardware depth culling) is **on** by default: `dkQueueMakerDefaults` sets `DkQueueFlags_EnableZcull`, and `DkQueueFlags_DisableZcull` turns it off. `DkInvalidateFlags_Zcull` invalidates its state, and `DkCounter_ZcullStats` reports how it performed `[documented]` (`deko3d.h` lines 231, 269-270, 288, 349).

> Unknown: exactly when `dkCmdBufTiledCacheOp(Flush)` must be issued relative to sampling or presenting a tiled target. The header names the operations and the Primer lists them without semantics. Example08 enables the tiled cache around its deferred pass; an experiment that omits the flush and reads the target back would show whether it is required.

- Expected result: no `Full` barrier where a narrower dependency would do (the saving was not measured here).
- How it fails: a `Full` barrier between every pass serialises the GPU; too weak a barrier gives one-frame-old or half-written data in the next pass. The debug library (`-ldeko3dd`) adds "parameter and state checking" and warning/error messages `[documented]` (`deko3d` `README.md`); whether it detects a missing barrier is not stated there.

## Do not read the frame back to answer a gameplay question

What to do: answer questions about the picture (is the top dark, did the screen flash) from state your game already has, or from a small summary computed on the GPU, instead of copying the finished frame to the CPU every frame.

- Evidence: the Eidos port's first attempt spent 22.5 ms of a 61.2 ms frame copying two GPU-composed screens (about 400 KiB) to the CPU through uncached mappings; a fence change did not alter that, so the cost was CPU copy time `[reported]` (Eidos Switch engineering guide, scene "Mistralton field", measured 2026-11-03; private document, not independently auditable; see the [Eidos case study](case-studies/case-study-eidos-ouroboros.md)).
- Expected result: no full-frame CPU copy on the steady path.
- How it fails: if a "GPU slow" profile is really time spent in submit or readback, the timing buckets are mislabelled. Time CPU recording and GPU completion separately and keep the buckets exclusive.

## Resolution: pick one strategy and keep the scale an integer

What to do: either render at a small fixed size and scale up by a whole number, or render at the real output size. Handheld output is 1280×720 and docked output is up to 1920×1080 `[reported]` (`deko_basic` defines `FB_WIDTH`/`FB_HEIGHT` for 720p with a commented 1080p alternative; `sa2-nx` `README.md` states 1280×720 handheld and 1920×1080 docked from 1.0.3.1).

```c
int scale = destWidth / renderWidth;   // 320x180 to 1280x720 is 4, to 1920x1080 is 6
```

- Expected result: crisp pixels at a predictable fill cost.
- How it fails: a fractional scale blurs unevenly. If one axis shimmers and the other is crisp, the factor is not an integer on that axis.

## Memory and bandwidth arithmetic

The arithmetic below is exact; the conclusions about speed are not.

| Surface set (RGBA8 colour ×2, 32-bit depth, RGBA8 resolve) | Bytes | Megabytes (10^6) |
| ---------------------------------------------------------- | ----- | ---------------- |
| 1280×720: 921,600 pixels × 4 bytes × 4 surfaces | 14,745,600 | 14.7 |
| 1920×1080: 2,073,600 pixels × 4 bytes × 4 surfaces | 33,177,600 | 33.2 |

These are sizes before alignment and padding (query the real size with `dkImageLayoutGetSize`). They are **memory footprints**, not per-frame traffic. Per-frame traffic is the footprint times how often each surface is read and written, which depends on overdraw, blending, compression and caches, and this wiki has not measured it.

For scale: the theoretical docked memory bandwidth is 25.6 GB/s (1600 MHz memory, 64-bit bus), which is about 427 MB per frame at 60 fps; the stock handheld memory clock of 1331.2 MHz gives about 21.3 GB/s, or about 355 MB per frame `[reported]` clocks (`nfsmw-nx` `README.md` stock-clock table), arithmetic ours. The CPU shares that bus. Any claim that a given resolution "fits" the budget is a `[hypothesis]` until measured with real counters.

## Lessons the ports paid for

Each item names the port that hit it. All are `[reported]` unless a header or Primer line is cited, and none was reproduced by Coffeepot; keep the conditions with the number. Case-study links give the full context.

**A smaller depth target can clip the whole picture.** USF4 left a 320×180 depth target bound while drawing full-screen 1280×736 passes; on the console only the top-left corner drew, because Maxwell discards everything outside the smallest bound target, and the PC emulator never showed it. The fix was not to bind a smaller depth target for draws and clears that neither test nor write depth (`usf4-nx` `docs/deko3d-renderer-usf4.md` section 5). SA2 notes it never bound such a pair in a 10-minute run but other games might (`sa2-nx` docs). Check that every draw's colour and depth targets match in size, or bind none.

**Composite on the GPU; never read the frame back.** melonDS draws both DS screens as textured quads and waits on a per-buffer "ready" fence before drawing and signals a "presented" fence after, with no full-screen CPU read `[documented]` (`melonDS` `src/frontend/switch/main.cpp` lines 702-714, [case study](case-studies/case-study-melonds.md)). Compare the Eidos readback cost above.

**Barrier and invalidate hygiene is worth auditing in traces.** SwitchWakerHD's plan records, from its own traces, `Fragments` barriers falling from 47 to 20 per frame when issued only for real read-after-write and write-after-read hazards, Zcull invalidates from 12 to 5 by keeping Zcull across the same depth target and layer, and a 1280×720 depth blit plus two `Full` barriers avoided by sampling depth in place for a draw that reads but does not write it. The GPU-time gains in that table (0.3-0.8 ms, up to ~1 ms, 0.3-0.6 ms) are the plan's estimates, flagged by its author as uncertain; the same plan leaves the tiled cache and depth compression switched off because they were not verified (`SwitchWakerHD` `docs/deko3d-plan.md` lines 85-92, [case study](case-studies/case-study-wind-waker-hd.md)).

**Shader hitches come from compiling on the console; prebuild the list.** `deko3d` itself never compiles at run time (see above), but SA2, USF4, NFSMW and Wind Waker HD each run `uam` or the driver's compiler on the console. Their remedies match: save the list of shaders or pipelines a play session used, ship that list, build the entries on a background core during menus, and `fsync` the cache file so closing the game does not lose records. USF4 measured the empty-cache first fight at about 90 shaders at 84 ms each, a 7.6 s frame; NFSMW measured 112 pipelines at 59-82 ms each, fixed by a shipped list that readied 113 of 113 in 0.3 s (`usf4-nx` `docs/deko3d-renderer-usf4.md`; `nfsmw-nx` `docs/native-renderer.md`).

**Code and block limits.** Shader code blocks live in a code segment of up to 4 GiB of GPU address space that `deko3d` cannot reuse after a block is freed, and the last `DK_SHADER_CODE_UNUSABLE_SIZE` (0x400) bytes of any code block are unusable because of a hardware bug, so reuse code blocks and leave that tail free. Every memory block uses limited operating-system bookkeeping regardless of size, so allocate big blocks and sub-allocate `[documented]` (`Primer.md` "Memory Blocks"; `deko3d.h` line 154).

**A GPU error is fatal to the queue and to the process.** After a GPU error the queue is in an error state, which `dkQueueIsInErrorState` reports, and the only legal call is `dkQueueDestroy`. The Primer warns that the system appears to kill processes that crash the GPU a few seconds later, so save progress promptly `[documented]` (`Primer.md` "Queues"):

```c
if (dkQueueIsInErrorState(queue)) { /* save now; then dkQueueDestroy and exit */ }
```

Verification: compiles in the `api-check` program, `-Wall`, no warnings; not run on hardware.

**Prove each side of an A/B actually draws.** USF4's first renderer comparison ran the native renderer without its shader package beside the NRO; it issued 0 draws, presented black screenshots, and "won" at 60 fps. The rule it adopted is a screenshot plus a draw count for every presented frame before any comparison (`usf4-nx` `docs/deko3d-renderer-usf4.md`).

**Make the renderer selectable at run time while you are learning.** nxquake2 ships four renderers behind one cvar so each path can be timed on the console against the same assets `[documented]` (`nxquake2` `doc/990_switch.md`, `Makefile`; [case study](case-studies/case-study-nxquake2.md)). Cheap to build in early, expensive to retrofit.

**If you stay on OpenGL (switch-mesa).** Ports that do so state no frame rates in their trees, so these are inferences, not results `[hypothesis]`: per-light and per-surface draws multiply state changes (dhewm3), stencil shadow volumes cost CPU build time and fill (dhewm3), unsorted translucency and per-surface state switches are the likeliest driver overhead (Serious Engine), and window resolution is the first fill lever; docked 1920×1080 has 2.25 times the pixels of handheld 1280×720 by arithmetic (Max Payne, nxquake2). Taisei sets `MESA_NO_ERROR=1` in release builds to disable GL error checking `[documented]` (`taisei` `src/arch_switch.c` line 81); its effect was not measured. Godot and SuperTuxKart set neither that nor any Zcull, tiling or compression control, because GL does not expose them ([open-engine ports](case-studies/case-studies-open-engines.md)).

## Technical details

- Submission: lazy; `dkQueueFlush`, a full internal list or the flush threshold submit work; `dkQueuePresentImage` flushes `[documented]` (`Primer.md` "Queues").
- Fence rules: a fence struct must stay at the same address while any command list that references it exists; zero-initialise a fence that may be waited on before it is first signalled `[documented]` (`Primer.md` "Fences").
- `dkQueueSignalFence(queue, fence, flush)`: the `flush` argument writes dirty GPU cache lines to memory so the CPU can see GPU writes to `GpuCached` memory `[documented]` (`Primer.md` "Queues").
- Code memory: a shader code block is allocated with `DkMemBlockFlags_Code`, shader offsets are aligned to `DK_SHADER_CODE_ALIGNMENT` (`0x100`) `[documented]` (`deko3d.h`; `deko_basic` `loadShader`).
- Uniform buffers: alignment `DK_UNIFORM_BUF_ALIGNMENT` (`0x100`), up to 64 KiB, push-constant semantics `[documented]` (`deko3d.h`; `Primer.md`).
- Maxwell compute guidance from NVIDIA's CUDA tuning guide (coalescing, occupancy, 255 registers per thread, 48 KB shared memory per block with at most 32 KB recommended) describes CUDA kernels on desktop Maxwell. It does not describe `uam`-compiled graphics shaders on the Switch, and no measurement here shows it transfers `[hypothesis]` (NVIDIA Maxwell Tuning Guide). The Switch GPU has 64 KB shared memory per multiprocessor and runs at 768 MHz docked `[reported]` (Chips and Cheese, 2023-12-23).
- FP16: Chips and Cheese found that NVIDIA's own compiler on the Switch did not pack FP16 operations automatically. That says nothing about `uam`, whose FP16 packing behaviour this wiki has not tested `[hypothesis]`. What would settle it: compile the same half-precision shader with and without explicit packed types and compare the SASS and GPU time.

## Pitfalls

- **Many tiny draws with the GPU mostly idle.** Symptom: frame time follows draw count while GPU busy time stays low. Fix: measure CPU recording time separately, then merge or cull; see the hypotheses above.
- **`dkQueueWaitFence` used to wait on the CPU.** Symptom: the CPU overwrites command memory that the GPU is still reading and you see one-frame corruption. Fix: `dkFenceWait`.
- **`dkQueueWaitIdle` every frame.** Symptom: frame time is CPU plus GPU instead of the larger of the two. Fix: per-slot fences.
- **Reusing one command memory region for every slot.** Symptom: random corruption under load. Fix: one region per slot, cleared only after that slot's fence.
- **Always-on GPU timers.** Symptom: the measured pass shrinks when timers are removed. Fix: time one pass at a time, briefly.
- **Runtime texture conversion (PNG at load).** Symptom: long loads. Fix: convert at build time; see [performance and porting](performance-and-porting.md).

## Open questions

- At what draw count does a plain `deko3d` scene on retail clocks become CPU-bound? Needs the draw-count sweep benchmark described above.
- Does atlas size (512, 1024, 2048) change bandwidth or cache behaviour on handheld clocks? Needs a same-scene A/B with the memory profile fixed.
- Is `dkCmdBufTiledCacheOp(Flush)` required before sampling a tiled target? Needs the readback experiment above.
- Does `uam` pack FP16 automatically? Needs the SASS comparison above.
- What is the per-frame memory traffic of a typical 720p and 1080p scene? Needs GPU counters or a bandwidth benchmark.

## Sources

- [deko3d](https://github.com/devkitPro/deko3d) at `373bfd43d8b2d805fa2eb7d406dbb823032df958` — `include/deko3d.h` (function signatures, flags, barriers, alignment constants), `Primer.md` (queues, fences, memory blocks, capture and replay, shaders), `README.md` (offline shader compiler, validation layer, compressed render targets).
- [switch-examples](https://github.com/switchbrew/switch-examples) at `669786898205b7beb25ff1731e72982e6d0397d3` — `graphics/deko3d/deko_basic/source/main.c`, `graphics/deko3d/deko_examples/source/Example03_Cube.cpp`, `Example04_TexturedCube.cpp`, `Example08_DeferredShading.cpp`, `Example09_SimpleCompute.cpp`.
- [uam](https://github.com/devkitPro/uam) at `5a5afc2bae8b55409ab36ba45be63fcb73f68993` — the offline compiler (derived in part from Mesa/Nouveau code).
- [tegra_swizzle](https://github.com/ScanMountGoat/tegra_swizzle) at `d7ed318ce233a4d557d4a2f207ed5dc04ed813c0` — `tiling.md`, 64×8-byte GOB tiling.
- [usf4-nx](https://github.com/Thorhax/usf4-nx) at `30ffae0` and [sa2-nx](https://github.com/ChanseyIsTheBest/sa2-nx) at `baf523b` — reported A/B numbers and NVK per-call costs `[reported]`.
- [nfsmw-nx](https://github.com/StevensND/nfsmw-nx) at `20f8dc1` — stock-clock table, pipeline-list result `[reported]`.
- [SwitchWakerHD](https://github.com/centollOS/SwitchWakerHD) at `9aec4e3`, [melonDS](https://github.com/RSDuck/melonDS) at `6c6f1e8`, [nxquake2](https://github.com/fgsfdsfgs/nxquake2) at `a8fdda02`, [taisei](https://github.com/taisei-project/taisei) at `baf029b` — barrier audit, GPU compositing, renderer selection, `MESA_NO_ERROR`; see the linked case studies.
- [Chips and Cheese: Nintendo Switch's iGPU](https://chipsandcheese.com/) (2023-12-23) — shared memory size, docked clock, FP16 behaviour of NVIDIA's compiler `[reported]`.
- [NVIDIA Maxwell Tuning Guide](https://docs.nvidia.com/cuda/maxwell-tuning-guide/) (CUDA documentation 13.5) — desktop Maxwell compute guidance; see the caveat above.
- Eidos Switch engineering guide (private research file `EIDOS_SWITCH_ENGINEERING_GUIDE.md`, section 1) — readback numbers `[reported]`; not publicly checkable.
