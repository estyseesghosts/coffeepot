# GPU memory ownership and synchronization

This page is for a developer who writes data the GPU will read, or reads data the GPU wrote, and needs to know when that is safe. The short answer: three separate questions decide it (can the reader **see** the data, has the earlier work **finished**, and is anyone still **using** the memory), and the tools that answer them are different. A fence answers the last two; cache flags and cache operations answer the first; neither replaces the other.

## Before you start

- Versions: `deko3d` 0.5.0-1 (installed) and commit `373bfd43d8b2d805fa2eb7d406dbb823032df958` (pinned), see [source versions](source-versions.md). Facts below are `[documented]` from `Primer.md` and `include/deko3d.h` at the pinned commit unless labelled otherwise.
- Verification: the ownership rules are applied in `nx/engineering/examples/deko-renderer` (builds, no warnings). **No GPU hazard on this page has been tested.** Eden 0.2.1 aborts on `deko3d` programs, including unmodified upstream `deko_basic`, so even the renderer has not been run. A hazard that "seems fine" proves nothing: a race is nondeterministic and can pass for months.
- Object names (`DkMemBlock`, `DkQueue`, `DkFence`) are explained on [libnx and deko3d](../homebrew/deko3d-and-libnx.md); the frame loop is on [rendering](rendering.md) and [the renderer walkthrough](deko3d-renderer.md).

## Three questions, three mechanisms

| Question | Failure if ignored | Answered by |
| -------- | ------------------ | ----------- |
| **Visibility**: does the reader see the writer's latest bytes? | Stale or half-written data | Memory flags, `dkMemBlockFlushCpuCache`, `dkQueueSignalFence(..., flush=true)`, `DkInvalidateFlags_*` |
| **Ordering**: has the earlier GPU work finished before the dependent work starts? | Reads of not-yet-written data | `dkCmdBufBarrier`, fences (`dkCmdBufSignalFence`/`dkCmdBufWaitFence`, `dkQueueWaitFence`) |
| **Lifetime**: is anyone still using this memory? | Corruption when overwritten, a GPU error when freed | `dkFenceWait` on the CPU, `dkQueueWaitIdle` at teardown |

## Memory mappings

A `DkMemBlock` is mapped separately for the CPU and the GPU, each as `None`, `Uncached` or `Cached`. Defaults: `CpuUncached | GpuCached`. CPU access modes are about the CPU's data cache; the GPU has its own caches on top.

| Use | Flags | Why |
| --- | ----- | --- |
| Vertex, index, uniform, descriptor data the CPU writes | Default | CPU writes are not held in the CPU cache, so no CPU cache maintenance is needed `[documented]` |
| Data the GPU writes and the CPU reads (counter reports, readback) | Default | Signal the fence with `flush = true`, then `dkFenceWait` |
| Large CPU-written data where uncached writes are slow | `CpuCached`, then `dkMemBlockFlushCpuCache` before the GPU reads | The flush cleans and invalidates the CPU cache; the GPU-side caches must also be invalidated before the GPU reads it `[documented]` |
| Never for GPU-to-CPU transfers | `CpuCached` | Userland cannot invalidate the CPU cache, so the CPU may read stale lines `[documented]` |
| Shader code | `Code` flag | Mapped into the GPU code segment; the last `DK_SHADER_CODE_UNUSABLE_SIZE` (0x400) bytes of a block cannot hold code `[documented]` |
| Images | `Image` flag | Extra mappings for normal and compressed access `[documented]` |

> Unknown: how much slower `CpuUncached` writes are than `CpuCached` plus a flush for large uploads on the A57. Needs a copy benchmark; see [profiling](profiling-and-benchmarks.md).

## What submit, flush and a fence mean

1. The CPU records commands into command memory. Recording is not execution.
2. `dkCmdBufFinishList` closes a list; `dkQueueSubmitCommands` appends it to the queue's pending work.
3. `dkQueueFlush` sends the batch to the GPU. It also runs when the list fills, when `flushThreshold` is reached, and inside `dkQueuePresentImage`.
4. The GPU executes asynchronously. Order is decided by the GPU and can only be constrained with fences and `dkCmdBufBarrier`.
5. A fence signalled by the queue lets the CPU (`dkFenceWait`) or the GPU (`dkQueueWaitFence`) know earlier work is done. Only `dkFenceWait` blocks the CPU.

After every flush, deko3d queues a barrier that invalidates the image, shader, descriptor and L2 caches as the first item of the next batch. That makes the point after a flush the supported moment to rewrite vertex data or descriptors **that the GPU is no longer reading**; it does not make rewriting memory an unfinished frame still reads safe `[documented]`.

`dkQueueSignalFence(queue, fence, flush)`: with `flush = true` the GPU writes dirty cache lines to memory, so the CPU can see GPU writes to `GpuCached` memory `[documented]`. Without it, a fence tells you the work finished, not that the CPU will see its output.

## Which operation for which hazard

Every cell not marked `[documented]` is a gap in the sources, not a recommendation.

| Producer → consumer | Do this | Status |
| ------------------- | ------- | ------ |
| CPU writes, GPU reads (default flags) | Write while the GPU is not using the region; submit afterwards | `[documented]` |
| CPU writes, GPU reads (`CpuCached`) | `dkMemBlockFlushCpuCache`, then make sure GPU caches are invalidated | `[documented]` |
| GPU writes, CPU reads | `dkQueueSignalFence(..., true)`, `dkFenceWait`, read through an uncached mapping | `[documented]` |
| GPU frame N finished with a slot's command, uniform and report memory | CPU waits that slot's fence before rewriting | `[documented]` pattern; the renderer example uses it |
| Render target written, then sampled as a texture | `dkCmdBufBarrier(DkBarrier_Fragments, DkInvalidateFlags_Image)` is what the example code uses | The header does not say this is the minimal sufficient pair `[hypothesis]`; test with a checkerboard (Test C below) |
| Compute writes, graphics reads | `DkBarrier_Primitives` completes earlier primitives and compute jobs; which invalidates are also needed depends on the reader's cache | Partly `[documented]` |
| Tiled-cache pass hand-over | `DkBarrier_Tiles` with `DkInvalidateFlags_Image` between tiled passes (upstream `Example08_DeferredShading.cpp`) | `[documented]` as example usage only |
| Before freeing or resizing a resource the GPU used | Wait the fences of every frame that referenced it, or `dkQueueWaitIdle` | `[documented]` |

## Barriers by the hazard they resolve

`DkBarrier_*` sets **how much earlier work must finish**; `DkInvalidateFlags_*` sets **which caches the next reader must not trust**. Choose them separately.

| Mode | Completes before later work starts |
| ---- | ---------------------------------- |
| `Tiles` | Tile processing order (like a Vulkan subpass dependency) |
| `Fragments` | Fragment processing (like a render pass boundary) |
| `Primitives` | All earlier primitives and compute jobs |
| `Full` | Everything earlier, and disables command-list prefetch |

Invalidate flags: `Image` (texture cache), `Shader` (shader code, data, uniform cache), `Descriptors`, `Zcull`, `L2Cache`. The modes are not ranked by measured cost anywhere in the sources, so "weakest that works" means: list what each pass reads and writes, then name the dependency between consecutive passes.

| Pass | Reads | Writes | Dependency on the previous pass |
| ---- | ----- | ------ | ------------------------------- |
| A: geometry | Vertex buffer, texture, uniforms | Offscreen colour, depth | none |
| B: fullscreen | Offscreen colour (as a texture) | Swapchain image | A's colour writes finished, texture cache refreshed |

## The frame ring

Each slot owns command memory, per-frame uniform memory, report memory and one fence. Rules:

- Wait the slot's fence (`dkFenceWait`) before clearing its command buffer or rewriting its memory. A fence covers work submitted **before the signal**, in that queue; it says nothing about other queues, CPU worker threads, or the display.
- Swapchain image availability is a separate question: `dkQueueAcquireImage` returns an image the display has released; it is not the same event as a slot fence.
- A fence that a recorded list references must keep its address until the list is gone `[documented]`. Keep fences in static or slot storage, never on the stack of the recording function.
- Zero a fence that may be waited on before its first signal `[documented]`.
- `DkCmdBuf` is externally synchronized: one recording thread per command buffer. Workers record separate command buffers; the main thread collects their lists and submits them.

Two or three slots: more slots add latency and absorb CPU spikes; fewer reduce latency and make the CPU wait sooner. No measurement on this wiki settles it; see [profiling](profiling-and-benchmarks.md).

## Tests that would make this credible

None is written yet. Each should be a broken program paired with a corrected one, run on a console, with the picture checked rather than "did it complete".

| Test | Broken | Corrected | Check |
| ---- | ------ | --------- | ----- |
| A: command-memory reuse | Overwrite a slot's commands before its fence | Wait the fence | Every frame's picture matches; failures are nondeterministic, so run long |
| B: GPU to CPU | Read a compute output after the work finished but without `flush = true` | Fence with flush | Compare every word |
| C: render to texture | Omit or weaken the barrier | The barrier above | Checkerboard comparison of the final image |
| D: resize | Destroy old targets without waiting | Wait every frame's fence | Resize repeatedly while rendering |

The renderer example (`nx/engineering/examples/deko-renderer`) already reads a render target back (`dkCmdBufCopyImageToBuffer`, then fence with flush) and checks the pixels; that is Test B's mechanism with the corrected sequence only.

## Technical details

- Fence: `dkFenceWait(fence, timeout_ns)`; `-1` waits forever. `dkQueueWaitFence` is a GPU-side wait.
- `dkQueueWaitIdle` signals a temporary fence, flushes and waits; use it at teardown, not per frame.
- Queue error state: after a GPU error the only legal call is `dkQueueDestroy`; see [errors and failure modes](errors-and-failure-modes.md).
- Constants: `DK_MEMBLOCK_ALIGNMENT` 0x1000, `DK_UNIFORM_BUF_ALIGNMENT` 0x100, `DK_UNIFORM_BUF_MAX_SIZE` 0x10000, `DK_IMAGE_DESCRIPTOR_ALIGNMENT` 0x20, `DK_SHADER_CODE_ALIGNMENT` 0x100.
- Each memory block consumes limited OS bookkeeping regardless of size: allocate large blocks and sub-allocate. The code segment is 4 GiB of GPU address space and freed code mappings are not reused, so reuse code blocks.

## Pitfalls

- **`dkQueueWaitFence` to wait on the CPU.** It only makes the GPU wait; use `dkFenceWait`.
- **Fence without `flush = true` before a CPU read.** Symptom: old values in a counter report or readback. Fix: signal with `flush = true`.
- **`CpuCached` memory for results.** Symptom: stale reads. Fix: default (uncached) mapping.
- **A fence on the stack.** Symptom: undefined behaviour at submit. Fix: stable storage.
- **One command region for all slots.** Symptom: random corruption under load. Fix: one region per slot.
- **Concluding "it works".** A missing barrier can render correctly for one driver state and fail on another model or clock.

## Open questions

- Is `Fragments` plus `Image` sufficient and minimal for render-to-texture? Needs Test C on Erista and Mariko.
- When must `dkCmdBufTiledCacheOp(Flush)` be issued relative to sampling a tiled target? The Primer lists it without semantics.
- Does the debug library (`-ldeko3dd`) detect a missing barrier? The README says it checks parameters and state, not hazards.
- How do `CpuUncached` writes compare with `CpuCached` plus a flush for large uploads?

## Sources

- [deko3d](https://github.com/devkitPro/deko3d/blob/373bfd43d8b2d805fa2eb7d406dbb823032df958/Primer.md) at `373bfd43d8b2d805fa2eb7d406dbb823032df958` — `Primer.md` ("Memory Blocks", "Fences", "Queues", "Barriers and synchronization") and `include/deko3d.h` (flags, barrier modes, invalidate flags, alignment constants).
- [switch-examples](https://github.com/switchbrew/switch-examples) at `669786898205b7beb25ff1731e72982e6d0397d3` — `Example03_Cube.cpp`, `Example08_DeferredShading.cpp` barrier usage.
- `nx/engineering/examples/deko-renderer` — the frame ring, counter reports and readback used here.
