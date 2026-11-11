# NX GPU

The Switch draws everything — menus, 2D art and 3D scenes — with NVIDIA graphics that sit on the same chip as the main processor. The same game usually renders at a lower resolution and clock speed on the built-in screen, and at a higher resolution and clock speed on a TV. Half-precision math can run at double speed, which helps portable performance. There is no separate video memory; the graphics share the main memory with the processor.

## Where it fits

The drawing hardware (the **[GPU](../../glossary.md#gpu)**) is the **[Maxwell](../glossary.md#maxwell)**-generation block of the NVIDIA **[Tegra X1](../glossary.md#tegra-x1)** **[system-on-a-chip](../../glossary.md#soc)**. The **[CPU](cpu.md)** runs game logic and submits drawing work through a graphics library, and finished pictures (each a **[framebuffer](../../glossary.md#framebuffer)**) go to the built-in screen or through the dock to a TV (see [input and display](input-and-display.md)). It reads and writes the same shared [memory](memory.md) as the CPU.

## How it works

Games describe what to draw with small programs that run on the graphics hardware (called **[shaders](../../glossary.md#shader)**) and lists of drawing commands. Retail games use Nintendo's NVN library; homebrew normally uses the community **[deko3d](../glossary.md#deko3d)** library instead, a separate low-level implementation written from the reverse-engineered behaviour of NVN. It is not built on the open-source Nouveau driver; only its offline shader compiler (**[uam](../glossary.md#uam)**) reuses Mesa and Nouveau code. Homebrew that draws through OpenGL does use Mesa/Nouveau (see [libnx and deko3d](../homebrew/deko3d-and-libnx.md)). Small fast memories next to the compute units (called **[caches](../../glossary.md#cache)**) reward textures and render targets stored in block-linear layout, and the CPU and GPU do not see each other's writes automatically — software must flush, invalidate and fence explicitly.

## Technical details

| Item | Value | Evidence |
| ---- | ----- | -------- |
| Architecture | NVIDIA Maxwell, 256 cores, FP16 support, on the Tegra X1 (20 nm Erista) and X1+ (16 nm Mariko) | [documented] (SwitchBrew Tegra X1, oldid 14745: "256-core Maxwell GPU with support for FP16") |
| Multiprocessors | 2 (256 cores ÷ 128 per multiprocessor; each has four partitions of 32 lanes) | arithmetic from [reported] (Chips and Cheese, 2023-12-23) |
| L2 for the GPU | 256 KB | [reported] (Chips and Cheese) |
| Shared memory per multiprocessor | 64 KB (cut from 96 KB on desktop Maxwell) | [reported] (Chips and Cheese) |
| Half-precision | Hardware FP16 is present; two values can be packed in a 32-bit register for double rate | [reported] (Chips and Cheese; they verified higher FP16 throughput with `vkpeak`) |

> Unknown: texture-unit and render-output-unit counts, and the silicon name of this Maxwell variant. The saved sources do not state them; earlier text on this page gave numbers without a source.

| GPU clock | Frequency | Peak FP32 | Notes |
| --------- | --------- | --------- | ----- |
| Handheld, starting clock | 307.2 MHz | ~157 GFLOPS | [reported] (`nfsmw-nx` `docs/platform-notes.md`: a program starts at 307.2 MHz in handheld mode) |
| Handheld, performance mode | 460.8 MHz | ~236 GFLOPS | [reported] (`nfsmw-nx`: obtained by asking the system for an official performance mode) |
| Handheld, 384 MHz profile | 384 MHz | ~197 GFLOPS | [reported] (`SwitchWakerHD` `docs/perf-results.md`: `switchGpuProfile=384`; Chips and Cheese: "in handheld mode ... the gpu is limited to 384mhz", a reader comment) |
| Docked | 768 MHz | ~393 GFLOPS FP32, ~786 GFLOPS FP16 | [reported] (`nfsmw-nx` `README.md`; Chips and Cheese: "runs the iGPU at 768 MHz") |

Peak rates are arithmetic: 256 cores × 2 operations × clock. They are upper bounds, not game throughput. Every clock figure here is a port report; the SwitchBrew Clocks page that older text cited was not available when this page was checked (see [source versions](../engineering/source-versions.md)).

| Memory path | Value | Evidence |
| ----------- | ----- | -------- |
| Bus to shared LPDDR4 | 64-bit | [documented] (SwitchBrew Tegra X1) |
| Theoretical peak, Tegra X1 (LPDDR4-1600) | ~25.6 GB/s | [documented] (SwitchBrew Tegra X1) |
| Theoretical peak, X1+ | ~31.8 GB/s with LPDDR4X-2133 | [documented] (SwitchBrew Tegra X1) |
| Handheld stock memory clock | 1331.2 MHz, ~21.3 GB/s by arithmetic (1331.2 MHz × 2 transfers × 8 bytes) | clock [reported] (`nfsmw-nx` `README.md`) |
| Docked stock memory clock | 1600 MHz, ~25.6 GB/s | clock [reported] (`nfsmw-nx`) |

The CPU shares this bus, so these are totals, not GPU-only figures.

Texture and render-target layout: block-linear tiling, in blocks (GOBs) of 64 bytes by 8 rows. Linear data must be converted before the GPU samples it in the default layout, or the image must be created pitch-linear [documented] (`tegra_swizzle` `tiling.md`, commit `d7ed318`; `deko3d.h`: `DkImageFlags_BlockLinear` is the default).

Coherence: the CPU and GPU do not see each other's writes automatically. For CPU-cached memory the CPU cache must be flushed before the GPU reads, the CPU cache cannot be invalidated from userland, and GPU writes become visible to the CPU only when a fence is signalled with the flush option [documented] (`deko3d` `Primer.md`, "Memory Blocks" and "Queues").

## Homebrew notes

Use deko3d for new homebrew rendering; use NVN-style patterns (command buffers, explicit barriers, tiled images) only as concepts, since NVN itself is a retail SDK library. Always allocate GPU-visible memory through the deko3d / libnx allocators so tiling and cache maintenance are correct.

## Sources

- SwitchBrew [Tegra X1](https://switchbrew.org/wiki/Tegra_X1) (oldid 14745, edited 2026-05-30) — 256-core Maxwell, FP16, memory interface and bandwidth.
- [Chips and Cheese: Nintendo Switch's iGPU: Maxwell Nerfed Edition](https://chipsandcheese.com/) (2023-12-23) — 768 MHz clock, 256 KB L2, 64 KB shared memory, FP16 behaviour. The 384 MHz handheld remark is in the article's reader comments.
- [nfsmw-nx](https://github.com/StevensND/nfsmw-nx) at `20f8dc1` — stock-clock table and handheld start clock `[reported]`.
- [SwitchWakerHD](https://github.com/centollOS/SwitchWakerHD) at `9aec4e3` — 384 MHz handheld profile `[reported]`.
- [tegra_swizzle](https://github.com/ScanMountGoat/tegra_swizzle) at `d7ed318` — block-linear GOB layout.
- [deko3d](https://github.com/devkitPro/deko3d) at `373bfd4` — `include/deko3d.h`, `Primer.md`, `README.md`: not Nouveau-based; explicit flush, invalidate and fence rules.
- [libnx](https://github.com/switchbrew/libnx) — how homebrew obtains the display and submits work.
