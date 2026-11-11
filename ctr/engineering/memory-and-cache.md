# Memory and cache management for 3DS homebrew

A 3DS program has three kinds of memory to choose from — the ordinary heap, the **[linear heap](../glossary.md#linear-heap)** that the GPU and audio processor can read, and 6 MB of video memory (**[VRAM](../glossary.md#vram)**) — and it must keep the CPU's cache in step with the other processors that read and write the same memory. Getting this wrong produces the classic 3DS homebrew bugs: textures that flicker or show stale pixels, crackling audio, and programs that run out of memory on an Old 3DS but not a New 3DS. This page explains how much memory you actually get, which pool each kind of buffer belongs in, and exactly when to flush or invalidate.

Hardware facts (physical map, region sizes, VRAM banks) live on [memory](../hardware/memory.md); this page is about using them. Performance aspects of flushing are also on [performance](performance.md).

## Before you start

- Checked against libctru 2.7.1, citro3d 1.7.1 and Luma3DS v13.5 source.
- Terms: *flush* in libctru and citro3d means "clean and invalidate": write dirty cache lines back to memory and drop them from the cache. *Clean* (also called *store*) only writes back. *Invalidate* only drops lines, discarding unwritten changes.

## How much memory you get

The kernel gives the running application one **APPLICATION memory region** whose size depends on the system's memory mode, `APPMEMTYPE` ([memory: FCRAM regions](../hardware/memory.md#fcram-regions-appmemtype)): normally 64 MB on an Old 3DS. **New 3DS:** normally 124 MB. Your code, data and every heap come out of that region; VRAM does not.

**A `.3dsx` inherits the memory mode of whatever was running.** Luma3DS's homebrew loader generates the 3DSX's **[extended header](../glossary.md#exheader)** so that its Old 3DS and New 3DS system modes equal the current `app_memtype` [documented: Luma3DS `hbldr.c`]. In practice:

- A `.3dsx` always runs inside a process that Luma3DS's loader took over: by default the title `000400000D921E00` (named `hblauncher_loader` in the Luma3DS build), or another title selected in Rosalina. Started normally from the Home Menu, that is the default mode — 64 MB on Old 3DS.
- A program that needs more memory on an Old 3DS can be shipped as a `.cia` whose exheader requests a larger mode (e.g. 96 MB with `APPMEMTYPE` 2); the console then reboots into that layout when the title starts, which is why such titles start more slowly and cannot run alongside the Home Menu ([OS](../software/os.md)). Test HOME-button, sleep and applet behaviour in that mode.
- Rosalina's *Switch the hb. title to the current app* makes the next homebrew launch take over the currently running game's process, and with it that game's memory mode [documented: Luma3DS `menus/miscellaneous.c`]. Useful for testing; do not make users depend on it.

Read the actual numbers at start-up instead of assuming them:

```c
printf("APPLICATION region: %lu KiB\n", osGetMemRegionSize(MEMREGION_APPLICATION) / 1024);
printf("linear free: %lu KiB, VRAM free: %lu KiB\n", linearSpaceFree() / 1024, vramSpaceFree() / 1024);
```

### How libctru splits it

At start-up libctru asks the kernel how much the process may still commit, then divides it between the **application heap** (`malloc`, `new`) and the **linear heap** [documented: libctru `source/system/allocateHeaps.c`]:

1. By default, half each (any odd page goes to the application heap).
2. If that would make the application heap larger than 24 MiB, the application heap is set to 24 MiB and the linear heap gets the rest.
3. If *that* would make the linear heap larger than 32 MiB, the linear heap is set to 32 MiB and the application heap gets the rest.
4. The application heap can never exceed the 96 MiB heap address area; any excess goes to the linear heap (libctru 2.7.1).

So on a default Old 3DS launch (64 MB region), the linear heap ends up at 32 MiB and the application heap gets the rest — about 30 MiB minus the size of your program's code and data [derived from the algorithm; check with the snippet above].

**Override the split** by defining either or both global variables in your program; the other side gets the remainder:

```c
u32 __ctru_heap_size        = 40 * 1024 * 1024;  // application heap
u32 __ctru_linear_heap_size = 16 * 1024 * 1024;  // linear heap
```

If the two together exceed what is available, the program stops at start-up with `svcBreak(USERBREAK_PANIC)`. Size them from measured peaks, with margin, and recheck on the smallest memory mode you support.

The virtual address of linear memory is not a universal physical address. Old and newer processes can use different linear windows; let libctru and the relevant service perform address translation, and avoid hard-coding the older window's offset. See [memory-mapping constraints](../software/svc-reference.md#memory-mapping-constraints) and the [memory map](../hardware/memory.md) [reverse-engineered: 3dbrew Memory Management, revision 22529].

## Which pool for which buffer

| Buffer | Pool | Why |
| ------ | ---- | --- |
| Game state, decoded assets the CPU reads, scratch space | Application heap (`malloc`) | Cached normally, no coherency concerns, and every byte here is one the GPU-visible pools keep |
| Vertex and index buffers, textures the CPU writes, citro3d's command buffer, audio wave buffers, buffers passed to camera/Y2R/MVD services | Linear heap (`linearAlloc`, `linearMemAlign`) | Physically contiguous memory, which the GPU, DSP and DMA engines need: they use physical addresses and cannot follow the CPU's page tables |
| Render targets (colour and depth) | VRAM (`vramAlloc`) | citro3d puts them there itself: `C3D_RenderTargetCreate` allocates the colour buffer in VRAM and tries to put the depth buffer in the **other** VRAM bank first [documented: citro3d `renderqueue.c`] |
| Textures that never change and are sampled heavily | VRAM, if it fits (`C3D_TexInitVRAM`) | Keeps texture reads in GPU-local memory; costs a GPU copy to fill (below) |
| Socket service work buffer | Application heap, page-aligned (`memalign(0x1000, …)`) | Required by `socInit` |

Notes:

- `linearAlloc` returns 0x80-byte-aligned blocks [documented: `allocator/linear.cpp`]. Use `linearMemAlign` for stricter alignment.
- VRAM is 6 MiB in two 3 MiB banks; `vramAllocAt(size, VRAM_ALLOC_A | VRAM_ALLOC_B | VRAM_ALLOC_ANY)` picks a bank and no allocation straddles the boundary ([libctru](../homebrew/libctru.md#allocators)). A top-screen RGBA8 colour buffer plus a 24-bit depth/8-bit stencil buffer already take 750 KiB; budget VRAM per screen, per eye.
- The CPU cannot usefully write VRAM in homebrew: Luma3DS maps it **read-only** for 3DSX programs [documented: `hbldr.c` kernel capability "RO static range mapping 0x1F000000"]. Fill VRAM with GPU operations (`GX_TextureCopy`, `GX_DisplayTransfer`, `GX_MemoryFill`) from linear memory.

### Size arithmetic

| Surface | Size |
| ------- | ---- |
| Top screen, one eye, 400×240 RGBA8 | 375 KiB |
| Top screen, one eye, 400×240 RGB565 | 187.5 KiB |
| Bottom screen 320×240 RGBA8 | 300 KiB |
| Depth24+stencil8 for 400×240 | 375 KiB |
| 512×512 texture: RGBA8 / 16-bit formats / ETC1 | 1 MiB / 512 KiB / 128 KiB |
| 256×256 texture, RGBA8, full mipmap chain | ≈ 341 KiB (+⅓) |

Add the LCD framebuffers libctru's `gfx` allocates in linear memory (two per screen for double-buffering; the top screen's right-eye buffers too when stereo is on), citro3d's command buffer (`0x40000` bytes by default, linear), and allocator fragmentation.

### Texture formats and sizes for a 512x512 texture

Bits per pixel are the values citro3d uses to size a texture (`fmtSize` in `source/texture.c` at `9f21cf7` `[documented]`); the sizes are arithmetic (512 x 512 = 262,144 pixels, bytes = pixels x bits / 8), without mipmaps and without any file or container overhead. The "relative" column is against RGBA8.

| Format (`GPU_*`) | Bits per pixel | Bytes for 512x512 | Size | Relative |
| ---------------- | -------------- | ----------------- | ---- | -------- |
| `RGBA8` | 32 | 262,144 x 4 = 1,048,576 | 1 MiB | 100% |
| `RGB8` | 24 | 262,144 x 3 = 786,432 | 768 KiB | 75% |
| `RGBA5551`, `RGB565`, `RGBA4`, `LA8`, `HILO8` | 16 | 262,144 x 2 = 524,288 | 512 KiB | 50% |
| `L8`, `A8`, `LA4`, `ETC1A4` | 8 | 262,144 x 1 = 262,144 | 256 KiB | 25% |
| `L4`, `A4`, `ETC1` | 4 | 262,144 / 2 = 131,072 | 128 KiB | 12.5% |

A full mipmap chain adds about one third (1 + 1/4 + 1/16 + ... approaches 4/3): an RGBA8 512x512 texture with every level is about 1.33 MiB. The trade-off is quality and fit: ETC1 and ETC1A4 are lossy (ETC1A4 stores a 4-bit alpha per pixel next to the colour block) and suit photographic or environment textures, not pixel art; 16-bit formats lose colour depth and alpha precision. `C3D_TexFlush` flushes the whole chain (`C3D_TexCalcTotalSize`, `texture.c` line 239).

### Budget a whole scene transition

A resource moves through unloaded → reading → decoded CPU data → upload staging → resident → no longer referenced → GPU finished → released. Give each state an owner and cancellation rule. A CPU completion event does not establish that an asynchronous transfer or draw has completed; do not overwrite staging data, evict a texture or reuse a vertex buffer until the corresponding consumer has finished. A resource may be logically evicted while its allocation remains alive until that point. See [buffer lifetime](#buffer-lifetime) and [frame pacing](PICA200/frame-pacing.md#overlapping-cpu-and-gpu).

For each scene, account for old resident assets plus compressed incoming bytes, decode scratch, new CPU buffers, GPU targets, texture/geometry allocations, audio queues and allocator overhead. Record startup, steady state and transition peaks separately, including the largest allocatable block; total free space alone does not guarantee a contiguous allocation. If old and new scenes cannot overlap within budget, stage the transition, free only after GPU completion, or use an explicitly bounded loading screen. Define a visible failure path when an allocation or transfer fails.

As **arithmetic**, 400×240 colour at 4 bytes/pixel plus 16-bit depth occupies 576,000 bytes; 320×240 with the same formats occupies 460,800 bytes. Adding an equal-size second-eye top target and a 512×512 RGBA8 colour target plus 16-bit depth gives 3,185,664 bytes (about 3.04 MiB). This assumes separate, unpadded allocations in the same memory pool: real placement, sharing, framebuffer alignment, bank limits and library choices can change the total. It excludes texture assets, scanout buffers, command buffers and overhead; it is not a measured allocation. Produce separate budgets for 2D, hybrid and stereo scenes rather than treating this example as a default.

## Keeping caches coherent

The ARM11 data cache is write-back ([CPU](../hardware/cpu.md#the-mp11-core-from-arms-manual)): a CPU store may sit in the cache for a long time before it reaches FCRAM. The GPU, DSP and DMA engines read and write memory directly and do not see the CPU's cache. Two rules follow:

1. **CPU wrote, other device reads →** clean (or flush) the written range *before* starting the device's work.
2. **Other device wrote, CPU reads →** invalidate the range *after* the device has finished and *before* the CPU reads it. Do not invalidate a range the CPU has dirtied but not written back — those changes are lost.

### The functions

| Function | Operation | How it works |
| -------- | --------- | ------------ |
| `svcStoreProcessDataCache(CUR_PROCESS_HANDLE, va, size)` | Clean | Kernel call |
| `svcFlushProcessDataCache(CUR_PROCESS_HANDLE, va, size)` | Clean + invalidate | Kernel call |
| `svcInvalidateProcessDataCache(CUR_PROCESS_HANDLE, va, size)` | Invalidate | Kernel call |
| `GSPGPU_FlushDataCache(addr, size)` / `GSPGPU_InvalidateDataCache` | Flush / invalidate | IPC request to the `gsp` system module |
| `DSP_FlushDataCache(addr, size)` / `DSP_InvalidateDataCache` | Flush / invalidate | IPC request to the `dsp` system module |
| `GX_FlushCacheRegions(a0, s0, a1, s1, a2, s2)` | Flush up to three ranges | A GX command, executed in order with the other commands in the GX queue |
| `C3D_TexFlush(&tex)` | `GSPGPU_FlushDataCache` over the whole texture (all mip levels); does nothing for VRAM textures | citro3d helper |

[documented: libctru `svc.h`, `services/gspgpu.c`, `services/dsp.h`, `gpu/gx.h`; citro3d `texture.c`]. The `svc*` functions take **virtual** addresses. All of them operate on whole 32-byte cache lines, so a range that shares a line with unrelated data affects that data too: align GPU and DSP buffers to at least 32 bytes and avoid placing CPU-written variables in the same line as a buffer you invalidate.

The kernel calls avoid a round-trip to a system module. That matters when your program has taken core-1 time from the system modules: a third-party report measured `DSP_FlushDataCache` at 1–5 ms with an 80% core-1 limit, versus about 0.1 ms for `svcFlushProcessDataCache` ([performance](threading.md#core-1-borrowing-the-system-core)).

### The C3D_FrameEnd flush

`C3D_FrameEnd(flags)` passes `flags` to `GX_ProcessCommandList`. If `GX_CMDLIST_FLUSH` (bit 1) is **not** set, citro3d first calls `GSPGPU_FlushDataCache` over the **entire linear heap** [documented: citro3d `renderqueue.c`]. With `GX_CMDLIST_FLUSH` set, it skips that, and the GSP module flushes only the command list buffer before running it [documented: libctru `gx.c`, command word 7].

The default is safe: every vertex, index and texture byte the CPU wrote anywhere in linear memory is visible to the GPU. Its cost is small and does not grow with how much of the 32 MiB you use: about 90 µs per frame on an Old 3DS `[measured]`, see [Measured costs](#measured-costs-old-3ds) below. Time it in your own program with the snippet on [performance](performance.md#measuring-time).

### Measured costs (Old 3DS)

`[measured]` Old 3DS (2011 unit), system 11.17.0-50U, Luma3DS 13.0.2, devkitARM r68 and libctru 2.7.1 builds of [`membench`](../benchmarks/README.md#membench) and [`flushbench`](../benchmarks/README.md#flushbench), two sessions that agree within a few percent (medians; full tables and caveats in the [benchmark README](../benchmarks/README.md#hardware-runs)). **New 3DS:** [below the table](#measured-costs-old-3ds).

| Operation | Cost | Notes |
| --------- | ---- | ----- |
| `memset` into the ordinary heap or the linear heap | 635 MiB/s for 16 KiB, about 370 MiB/s for 256 KiB and 1 MiB | Identical for both heaps (within 0.5%): linear memory is cached normally |
| `memcpy` from the heap into linear memory | about 93 MiB/s at 256 KiB and 1 MiB (112 MiB/s at 16 KiB) | Four times slower than writing the same bytes with `memset`; generate data in place when you can |
| `svcFlushProcessDataCache` of a clean range | 22 µs for 1 KiB, 28 µs for 4 KiB, 35 µs for 16 KiB, then 34-36 µs for any size up to 16 MiB | Fixed cost plus a short walk, then flat |
| `svcStoreProcessDataCache` (clean only) | 4-5 µs less than the flush, same shape | Lines stay valid in the cache |
| `GSPGPU_FlushDataCache` of a clean range | 58 µs for 1 KiB, 85-93 µs for 16 KiB to 16 MiB | About 37-57 µs more than the kernel call, the cost of the IPC round trip |
| Flush of a fully dirty, resident 16 KiB range | 65 µs (kernel flush), 53 µs (clean only), 121 µs (GSP) | About 30 µs above the clean figure: about 60 ns per dirty 32-byte line |
| Flush right after writing 16 KiB to 16 MiB | 60-80 µs (kernel), 115-135 µs (GSP) | Only the part still in the 16 KiB L1 cache is dirty at call time |
| Whole 32 MiB linear heap through the GSP | 80-89 µs clean, 135-140 µs with 30 MB just rewritten | |
| `C3D_FrameEnd(0)` against `C3D_FrameEnd(GX_CMDLIST_FLUSH)` on an empty scene | 147 µs against 57 µs | The difference, about 90 µs, is the whole-heap flush |

**New 3DS (a New 2DS XL at the default 268 MHz clock, Luma3DS 13.3.3; three sessions, [details](../benchmarks/README.md#suite-runs-old-3ds-and-new-3ds)):** the shape is the same and the costs are higher. Kernel flush of a clean range 32 µs (1 KiB) and 51 µs (16 KiB to 16 MiB), against 24 and 34 µs on the Old 3DS; GSP flush 79 µs and 102-108 µs, against 61 and 84-85 µs; the whole 32 MiB heap through the GSP 97 µs clean and 158 µs dirtied, against 81 and 135 µs; `C3D_FrameEnd(0)` 174 µs against 149 µs, and `GX_CMDLIST_FLUSH` 61 µs on both, so the avoidable flush is 112 µs per frame (0.7% of a frame). `memset` is 4% faster (388 against 371 MiB/s outside L1) and heap-to-linear `memcpy` 6% slower (12.7 against 12.0 ms per MiB). This says nothing about 804 MHz mode, with the L2 cache on, which has not been run.

Three consequences:

- **The cost of a flush is bounded, not proportional.** Above about 16 KiB the kernel call costs the same whatever the range. The mechanism was not read from the kernel; a switch to a whole-cache operation above some size would produce this shape, and the switch point lies between 16 KiB and 64 KiB `[hypothesis]`. Do not model flush cost as "so many nanoseconds per byte" for large ranges.
- **The default `C3D_FrameEnd(0)` costs about 0.5% of a 16.7 ms frame.** A tracked-flush scheme can save at most about 90 µs per frame (the `C3D_FrameEnd` figures above) and has to pay one kernel flush, 22-35 µs or more, for every range it flushes. From these medians, tracking breaks even at about three ranges per frame and loses beyond that (arithmetic, not a scene measurement). Reach for [flush-range tracking](#flush-range-tracking) only if a profile of your own scene shows `C3D_FrameEnd` is expensive for another reason, or if you have one or two large dynamic buffers.
- **Prefer the kernel call to the GSP call for flushes you issue yourself.** It is 37-57 µs cheaper per call on an idle system and does not depend on the `gsp` module getting core-1 time ([above](#the-functions)).

To switch to targeted flushing:

1. Track every linear-memory range the CPU wrote since the last frame: dynamic vertex/index data, texture uploads, anything else the GPU reads.
2. Flush exactly those ranges (`svcFlushProcessDataCache` or `GSPGPU_FlushDataCache`, or `C3D_TexFlush` for whole textures) **before** `C3D_FrameEnd`.
3. Call `C3D_FrameEnd(GX_CMDLIST_FLUSH)`.

Shader uniforms are not affected: citro3d writes them into the command list itself. A forgotten range shows up as geometry or texels from a previous frame, often only intermittently — keep the default path behind a debug switch to compare against.

### Flush-range tracking

The default `C3D_FrameEnd(0)` flushes the whole linear heap; the alternative is to flush only what the CPU wrote. The exact behaviour of each flag is in the table on [renderer architecture](PICA200/renderer-architecture.md#flush-range-tracking-the-c3d_frameend-flush-and-gx_cmdlist_flush); this section is the design.

1. **Record every write.** At each place the CPU writes memory the GPU will read in this frame (dynamic vertices, indices, texture tiles), call a function that records the address range. Round the range out to 32-byte cache lines first, since flushing operates on whole lines ([the functions](#the-functions)).
2. **Merge.** Keep a small table and merge overlapping or touching ranges; if it overflows, give up for this frame and use the default flush.
3. **Flush before `C3D_FrameEnd`.** Flush each range (`svcFlushProcessDataCache`, virtual addresses), then call `C3D_FrameEnd(GX_CMDLIST_FLUSH)`.
4. **Do not forget the command list.** The command buffer is flushed by the GSP because of `GX_CMDLIST_FLUSH`; your own hand-built command lists are not.

Verification status: builds (devkitARM r68, libctru 2.7.1, citro3d 1.7.1, `-Wall -Wextra -O2 -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft`, linked with `-specs=3dsx.specs -lcitro3d -lctru` inside a stub `main`); Old 3DS: not run; New 3DS: not run. Not exercised on a console, so whether it is faster is `[hypothesis]` and whether it is correct for your engine depends on recording every write.

```c
#include <3ds.h>
#include <citro3d.h>

#define MAX_DIRTY 64
typedef struct { u32 begin, end; } DirtyRange;   /* [begin, end) virtual addresses */
static DirtyRange dirty[MAX_DIRTY];
static int dirtyCount;

/* Call after every CPU write to memory the GPU will read this frame. */
static void dirtyAdd(const void *p, size_t size)
{
	if (dirtyCount > MAX_DIRTY) return;           /* already overflowed: full flush this frame */
	u32 b = (u32)p & ~31u;                        /* round out to 32-byte cache lines */
	u32 e = ((u32)p + size + 31u) & ~31u;
	for (int i = 0; i < dirtyCount; i++) {        /* merge with any overlapping or touching range */
		if (b <= dirty[i].end && e >= dirty[i].begin) {
			if (b < dirty[i].begin) dirty[i].begin = b;
			if (e > dirty[i].end)   dirty[i].end = e;
			return;
		}
	}
	if (dirtyCount < MAX_DIRTY) {
		dirty[dirtyCount].begin = b;
		dirty[dirtyCount].end = e;
		dirtyCount++;
	} else {                                      /* table full: fall back to the safe path */
		dirtyCount = MAX_DIRTY + 1;
	}
}

/* Replaces C3D_FrameEnd(0). Returns false if it had to use the full flush. */
static bool frameEndTracked(void)
{
	bool tracked = dirtyCount <= MAX_DIRTY;
	if (tracked) {
		for (int i = 0; i < dirtyCount; i++)
			svcFlushProcessDataCache(CUR_PROCESS_HANDLE, dirty[i].begin, dirty[i].end - dirty[i].begin);
		C3D_FrameEnd(GX_CMDLIST_FLUSH);
	} else {
		C3D_FrameEnd(0);                          /* citro3d flushes the whole linear heap */
	}
	dirtyCount = 0;
	return tracked;
}
```

Rules: call `dirtyAdd` for every CPU write to GPU-visible linear memory (including texture uploads that `memcpy` into a linear texture; a VRAM texture upload is a `GX_TextureCopy` from a source buffer that must itself be flushed); keep the default `C3D_FrameEnd(0)` path available behind a debug switch and compare frames. The saving is unmeasured: [`flushbench`](../benchmarks/README.md#flushbench) is the planned measurement of the full-heap flush, and a tracked-versus-full comparison is experiment G6 in [renderer architecture](PICA200/renderer-architecture.md#planned-gpu-workload-characterisation-benchmarks). `[reported]` G-Diffuser's `onesubmit_narrow` setting flushes only dirty ranges recorded by its own module at frame end instead of the whole heap; its repository document states about 0.3 ms saved on Old 3DS and 0.4 ms of GX CPU on New 3DS for that game, not a wiki measurement ([`docs/NATIVE-RENDERER.md`](https://github.com/cruxxxxxx/gdx-3ds/blob/main/docs/NATIVE-RENDERER.md), downloaded 11 November 2026).

### Texture uploads

- `C3D_TexLoadImage` / `C3D_TexUpload` expect data already in the GPU's tiled layout (what `tex3ds` produces — see [asset pipeline](PICA200/asset-pipeline.md)). For a texture in linear memory they simply `memcpy`; you must then flush the texture before the GPU samples it. For a VRAM texture they issue a `GX_TextureCopy` from your source buffer, which therefore must itself be in linear memory and flushed [documented: citro3d `texture.c`].
- For dynamically changing textures, update only the 8×8 tiles that changed (16-bit formats: 128 bytes per tile; RGBA8: 256 bytes) and flush only those ranges.
- Converting a row-major image to tiled layout on the CPU is avoidable: the transfer engine can do it (`GX_DisplayTransfer` with `GX_TRANSFER_OUT_TILED(1)`) from a linear-memory source, subject to its size and alignment rules ([display](../hardware/display.md)).

### Upload cost versus VRAM placement

Putting a texture in VRAM costs something first: the CPU cannot write VRAM in homebrew, so the data goes into a flushed linear buffer and a GPU copy (`GX_TextureCopy`, via `C3D_TexUpload`/`C3D_TexLoadImage` on a VRAM texture) moves it ([texture uploads](#texture-uploads)). The possible gain is GPU texture reads from GPU-local memory. Whether sampling from VRAM is measurably faster than from linear memory is not established by any source used here `[hypothesis]`, and the benefit depends on how often the texture is sampled, how often it changes, and how much of the 6 MiB is left after render targets (a stereo top target alone takes about 1.5 MB with 24-bit depth, [GPU rendering](PICA200/gpu-rendering.md#render-target-memory-budget)). Treat "VRAM is faster" as something to test per texture, not a rule; a texture that is re-uploaded every few frames pays the copy every time. The planned measurement is G4 in [renderer architecture](PICA200/renderer-architecture.md#planned-gpu-workload-characterisation-benchmarks).

### Reading GPU output on the CPU

For screenshots, readback of a render-to-texture, or software post-processing: wait until the GPU work and any transfer that writes the buffer have completed (`C3D_FrameBegin` returning, or the PPF/P3D event), invalidate the range, then read. Reading a render target while the GPU is still drawing gives a mix of frames. VRAM buffers must first be copied into linear memory with a GPU transfer.

### DSP buffers

`ndsp` wave buffers live in linear memory. Write the samples, flush (`DSP_FlushDataCache`, or `svcFlushProcessDataCache` under heavy core-1 use), then queue with `ndspChnWaveBufAdd`; do not touch the buffer again until its `status` is `NDSP_WBUF_DONE` (or `NDSP_WBUF_FREE` if never queued) [documented: `3ds-examples/audio/streaming`, libctru `ndsp/ndsp.h`].

### A cache key also needs a coherent snapshot

A key can cover every relevant input and still race a writer between hashing and rendering. For shared emulated video memory, decide whether the consumer takes a full snapshot, synchronizes with writers, or uses a correctly synchronized generation/retry scheme that can detect writes across the entire read. Hashing or checking a generation once does not by itself make unsynchronized reads coherent. The signature must describe the same snapshot consumed by the renderer and remain associated with that output; validate writes during hashing, rendering and asynchronous GPU use, including A → B → A. A pre-draw hash avoids associating an old picture with a newer post-draw hash but does not stop mid-draw mutation. See [visual-state signatures](PICA200/rendering-2d.md#cache-effective-visual-state-not-raw-register-blocks). Include the cost and ownership of the chosen scheme in measurements. The HGSS team identified live VRAM reads as an open race in its [case study](case-studies/case-study-hgss-port.md#pitfalls-and-open-questions); occurrence and a remedy remain unverified.

## Buffer lifetime

Most corruption bugs are a buffer reused while another processor still reads it. Give each GPU- or DSP-visible buffer an explicit owner and state, and only move forward:

```text
FREE → CPU writing → flushed → queued for GPU/DSP → GPU/DSP finished → FREE
```

With citro3d's standard frame loop, "GPU finished" for everything drawn in frame *N* is the moment `C3D_FrameBegin` returns for frame *N+1* ([performance](PICA200/frame-pacing.md#overlapping-cpu-and-gpu)). Data written for the GPU *before* that point needs a second copy. When deleting textures or buffers, wait for the GPU first (`C3D_FrameBegin` returning, or `C3D_Fini`), or the GPU may read freed and reused memory.

## Lessons from ported games

- **Stack.** libctru's default stack is 32 KB (`__stacksize__` in `libctru/source/system/stack_adjust.s`) `[documented]`; the stack sits in the heap you also allocate from, so a large `__stacksize__` takes heap. Engines that needed more: DXX 8 MB, Quake2CTR 3 MB ([DXX](case-studies/case-study-dxx-3ds.md), [Quake2CTR](case-studies/case-study-quake2ctr.md)).
- **Heap and linear heap are a split you can choose.** Quake2CTR gives the linear heap one sixth of the remaining memory, capped at 24 MB; sm64ds-enhanced installs its own split because libctru's default 24/24 MB leaves too little heap on an Old 3DS, with a fixed-size linear heap for GPU buffers and textures (about 10-11 MB used by its comment) `[documented]`; Vril sizes one hunk from the model ([SM64 DS](case-studies/case-study-sm64ds-enhanced.md), [Vril](case-studies/case-study-vril-engine.md)).
- **Free per scene.** sm64ds-enhanced frees caches, decoded textures and waves at each scene change; its comment says otherwise 1-2 MB leaks per scene `[reported]` (comment, not measured by Coffeepot).
- **Precache with a margin.** SRB2 3DS loads a level's textures at level start and stops when free linear heap drops below a safety margin, so later runtime allocations still fit `[documented]` ([SRB2](case-studies/case-study-srb2-3ds.md)).
- **A JIT's code buffer is memory you can lose.** Daedalus has an 8 MB executable buffer that, when full, rewinds and discards everything; whether that happens in play is unmeasured, so count resets before changing code generation ([Daedalus](case-studies/case-study-daedalus-3ds.md)).
- **Hardware-read buffers come from the linear heap and are flushed.** Quake2CTR, Xash3DS, Vril and sm64_3ds all allocate their DSP sound buffers with `linearAlloc` and flush the data cache when they hand a refilled buffer over; Vril invalidates before the DSP reads its music buffers `[documented]` (see [audio](audio.md)).

## Technical details

- `__ctru_heap_size` and `__ctru_linear_heap_size` are weak `u32` symbols in libctru; 0 means "automatic". Caps: `HEAP_SPLIT_SIZE_CAP` 24 MiB, `LINEAR_HEAP_SIZE_CAP` 32 MiB, heap area `OS_HEAP_AREA_BEGIN`–`OS_HEAP_AREA_END` (96 MiB).
- The commit limit comes from `svcGetResourceLimitLimitValues(…, RESLIMIT_COMMIT)`; for an application it equals APPMEMALLOC.
- `GX_ProcessCommandList(buf, size, flags)`: flag bit 0 (`GX_CMDLIST_UPDATE_GAS_ACC`) is passed to the GSP module state; bit 1 (`GX_CMDLIST_FLUSH`) makes the GSP module flush the command-list buffer.
- 3DSX memory map permissions granted by Luma3DS's loader: DSP RAM `0x1FF00000–0x1FF80000` read/write, VRAM `0x1F000000–0x1F600000` read-only, ARM11 I/O `0x1EC00000–0x1F000000` read/write.
- Cache line: 32 bytes; L1 data cache 16 KB per core ([CPU](../hardware/cpu.md#clocks-and-core-counts)).

## Pitfalls

- **Passing a `malloc` pointer to the GPU or DSP.** It is not physically contiguous and not where the device thinks it is. Garbage or a hang.
- **Flushing before writing.** The flush must come after the last CPU write and before the device starts.
- **Rewriting a vertex buffer the GPU is still reading** (written before `C3D_FrameBegin` without double-buffering): flickering or torn geometry.
- **Allocating in the frame loop.** `linearAlloc`/`linearFree` every frame fragments the linear heap; allocate pools at start-up.
- **Assuming the New 3DS's 124 MB (a New 3DS-only figure).** A program developed on a New 3DS may not even start on an Old 3DS. Test the smallest mode.

## Open questions

- Whether tracked flush ranges beat the full-heap flush in a real scene. The microbenchmarks bound the saving at about 90 µs per frame and put the break-even near three ranges ([measured costs](#measured-costs-old-3ds)); a scene benchmark with a tracker has not been run. **New 3DS:** not measured.
- Whether VRAM-resident textures sample measurably faster than linear-memory textures, and the break-even number of samples per upload.
- The cost of the full linear-heap flush in `C3D_FrameEnd` is measured on an Old 3DS (about 90 µs, [above](#measured-costs-old-3ds)) and on a New 2DS XL at 268 MHz (about 112 µs). **New 3DS at 804 MHz with L2:** not measured; the extra 2 MB L2 may change the figures.
- Whether the kernel switches to a whole-cache clean above some range size is not documented in the sources used. The measured cost is flat above 16 KiB on both an Old 3DS and a New 3DS, which is consistent with it; a flush sweep at 16-64 KiB and the kernel source would settle where.
- CPU write bandwidth to the application heap versus linear heap is measured on both models (identical, [above](#measured-costs-old-3ds)); read bandwidth is not, and neither is anything at 804 MHz.

## Sources

- [libctru](https://github.com/devkitPro/libctru) — `source/system/allocateHeaps.c` (heap split), `source/allocator/linear.cpp`, `allocator/vram.h`, `svc.h`, `services/gspgpu.c`, `services/dsp.h`, `gpu/gx.h`, `source/gpu/gx.c`, `os.h`.
- [citro3d](https://github.com/devkitPro/citro3d) — `source/renderqueue.c` (render target allocation, `C3D_FrameEnd` flush), `source/texture.c` (upload and flush), `source/base.c` (command buffer).
- [Luma3DS](https://github.com/LumaTeam/Luma3DS) — `sysmodules/loader/source/hbldr.c` (3DSX system mode and memory mappings), `sysmodules/rosalina/source/menus/miscellaneous.c` (hb title switch), `Makefile` (`HBLDR_DEFAULT_3DSX_TID`).
- [3ds-examples](https://github.com/devkitPro/3ds-examples) — `audio/streaming` (wave buffer flush and queueing), `network/sockets` (socket buffer).
- [G-Diffuser postmortem](https://hypnotize.works/gdx-3ds/postmortem/native.html) — `[reported]` narrow-flush setting; fetched November 2026.
- [Renderer architecture](PICA200/renderer-architecture.md) — what each flush flag does, decision table.
- [3dbrew: Memory layout](https://www.3dbrew.org/wiki/Memory_layout) and [Memory Management](https://www.3dbrew.org/wiki/Memory_Management) — regions and APPMEMTYPE.
