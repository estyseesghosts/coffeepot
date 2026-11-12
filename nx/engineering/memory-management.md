# Memory on Switch homebrew: the budget, the heap and GPU blocks

This page is for a developer who runs out of memory on the console but not on a PC, or who needs to plan how much a game can allocate. The short answer: a homebrew process does not own the console's RAM; its budget depends on how it was launched (Album applet or title override), `libnx` reserves one fixed heap at startup, `UsedMemorySize` counts that reservation rather than your live objects, and GPU memory blocks have their own limits. Measure the budget on the console you ship on, in the launch mode you ship in, instead of repeating a figure from a port.

## Before you start

- Toolchain: devkitA64 r30-1, `libnx` 4.12.0-1, `deko3d` 0.5.0-1 ([source versions](source-versions.md)). Facts marked `[documented]` are from `libnx` at `b156b4774fb2df7b93402169509266da7547fc4c` (`nx/source/runtime/init.c`, `kernel/svc.h`) and the `deko3d` `Primer.md`.
- Verification: `nx/engineering/examples/memory-probe` builds with no warnings and **ran in Eden 0.2.1**. Its numbers describe Eden's emulated process (about 3.44 GB total), are not console results, and are quoted below only to show what the program reports. It has not been run on a console in either launch mode.
- Launch modes are explained on [Atmosphere and running](../homebrew/atmosphere-and-running.md#how-it-works); hardware on [NX memory](../hardware/memory.md).

## What the process gets

Two launch modes give different budgets: the Album applet (small) and title override (the program runs as an application with application limits). Several ports require title override and fail with out-of-memory aborts under the Album `[reported]` (see [performance and porting](performance-and-porting.md#port-checklist-things-ports-reported-tripping-over)). The only figure on this wiki, about 3,189 MB of heap at start under title override, is one port's report `[reported]` (`nfsmw-nx`), not a platform constant.

## The heap `libnx` creates

At startup `libnx` sets up the newlib heap once, in `__libnx_initheap` `[documented]` (`init.c`):

1. If the loader provides a heap override, that segment is used.
2. Otherwise, with `__nx_heap_size` left at 0, the size is `TotalMemorySize − UsedMemorySize − 2 MiB`, rounded down to a multiple of 2 MiB (a 512 MiB fallback applies if that is zero), then `svcSetHeapSize`.
3. A program can set `__nx_heap_size`, or override `__libnx_initheap`, to choose its own.

So `malloc` draws from a fixed segment. Anything outside it (GPU blocks, thread stacks allocated by the kernel, the application's code) is accounted separately. max_nx reserves a 512 MB newlib heap and gives the rest to the mapped game, and states that applet mode cannot work because nothing remains `[reported]` (`max_nx` `source/config.h`).

## What the kernel reports

`svcGetInfo(&value, InfoType_X, CUR_PROCESS_HANDLE, 0)` returns, among others (`svc.h`):

| Info | Meaning | Availability |
| ---- | ------- | ------------ |
| `CoreMask`, `PriorityMask` | Allowed cores and priorities | |
| `HeapRegionSize` | Size of the heap region | |
| `TotalMemorySize` | Memory available to the process | |
| `UsedMemorySize` | Memory currently used by the process | |
| `SystemResourceSizeTotal` / `Used` | Memory for process management | 3.0.0+ |
| `TotalNonSystemMemorySize` / `UsedNonSystemMemorySize` | The same, excluding process management | 6.0.0+ |

Handle the failure of each call. Two warnings: `UsedMemorySize` is the memory the process has reserved, **not** the number of live objects, and `Total − Used` is not what one allocation can obtain. In Eden the probe reported `UsedMemorySize` almost equal to `TotalMemorySize`, and unchanged after freeing 3.4 GB of `malloc` chunks, because the heap was reserved at startup `[hypothesis]` that the same holds on hardware.

## Measure the budget

`nx/engineering/examples/memory-probe` writes `report.csv` to `sdmc:/switch/memoryprobe/`: the applet type, firmware, every info value above (or why it failed), the newlib heap size, and how many 16 MiB `malloc` chunks it could obtain and verify page by page before freeing them all. Run it:

1. Under the Album applet (applet type 2) and under title override (type 0), on each model you ship.
2. Again with `deko3d` memory blocks, which have extra mapping and bookkeeping limits (not included in the example).

Record the console, firmware and launch mode with every number ([source versions](source-versions.md#verification-model)).

## Plan by lifetime

| Lifetime | Typical content | Strategy |
| -------- | --------------- | -------- |
| Application | Permanent state, shared services | Allocate at startup |
| Level / scene | Static geometry, textures, map | Bulk allocate; release on a scene change |
| Streaming | Assets between storage and memory | Bounded staging and upload pools |
| Frame | Commands, transient geometry, uniforms | Per-slot ring guarded by fences ([synchronization](gpu-memory-and-synchronization.md)) |
| Temporary | Decompression, parsing | A reusable scratch arena |

A large heap does not justify unbounded temporary allocation. Keep a high-water mark per category; regressions are far easier to find in a log than in a crash.

## GPU memory

- Each `DkMemBlock` consumes limited OS bookkeeping whatever its size, so allocate large blocks and sub-allocate. A mesh-per-block design is a poor fit.
- Shader code lives in a code segment of up to 4 GiB of GPU address space; freed mappings are not reused, so reuse code blocks, and keep the last `DK_SHADER_CODE_UNUSABLE_SIZE` (0x400) bytes of each free `[documented]` (`Primer.md`).
- A simple suballocator needs: per-type alignment (`DK_UNIFORM_BUF_ALIGNMENT` 0x100, `DK_IMAGE_DESCRIPTOR_ALIGNMENT` 0x20, image alignment from the layout), bump or free-list allocation, and **deferred frees** until every frame that used the region has completed. Not implemented on the wiki yet.
- Staging memory for an upload must not be recycled because the CPU submitted the copy; recycle it after the copy's fence.

## Asset loading

`Read → decode/convert → stage → upload → make resident → release staging`, each stage with an owner and a completion condition, behind a bounded job queue so a large asset set cannot create unbounded allocations. Convert at build time where the format is known (texture layout, packed geometry, shader packages). Test on fast and slow SD cards and report load-time distributions and hitches. See [storage and saves](storage-and-saves.md).

## Pitfalls

- **Works under title override, aborts under the Album.** The budgets differ; log the launch mode.
- **Reading `UsedMemorySize` as live data.** It is a reservation.
- **A block per resource.** Exhausts OS bookkeeping.
- **Freeing GPU memory early.** The GPU may still read it; wait the fences.
- **Quoting a port's memory figure as your budget.** Measure.

## Open questions

- What are `TotalMemorySize`, the heap and the allocatable total under each launch mode on Erista and Mariko? Needs the probe on consoles.
- How do `deko3d` block limits show up (count and size)? Needs a probe that allocates blocks until failure.
- Does `UsedMemorySize` behave as in Eden on hardware?

## Sources

- [libnx](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/source/runtime/init.c) at `b156b4774fb2df7b93402169509266da7547fc4c` — `nx/source/runtime/init.c` (`__libnx_initheap`), `nx/include/switch/kernel/svc.h` (`InfoType_*`).
- [deko3d](https://github.com/devkitPro/deko3d/blob/373bfd43d8b2d805fa2eb7d406dbb823032df958/Primer.md) at `373bfd43d8b2d805fa2eb7d406dbb823032df958` — `Primer.md` "Memory Blocks".
- [nfsmw-nx](https://github.com/StevensND/nfsmw-nx) at `20f8dc1`, [max_nx](https://github.com/fgsfdsfgs/max_nx) at `264a0cc` — budget reports `[reported]`.
- `nx/engineering/examples/memory-probe` — the program and its report.
