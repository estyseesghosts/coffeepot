# NX GPU

The Switch draws everything — menus, 2D art and 3D scenes — with NVIDIA graphics that sit on the same chip as the main processor. The same game usually renders at a lower resolution and clock speed on the built-in screen, and at a higher resolution and clock speed on a TV. Half-precision math can run at double speed, which helps portable performance. There is no separate video memory; the graphics share the main memory with the processor.

## Where it fits

The drawing hardware (the **[GPU](../../glossary.md#gpu)**) is the **[Maxwell](../glossary.md#maxwell)**-generation block of the NVIDIA **[Tegra X1](../glossary.md#tegra-x1)** **[system-on-a-chip](../../glossary.md#soc)**. The **[CPU](cpu.md)** runs game logic and submits drawing work through a graphics library, and finished pictures (each a **[framebuffer](../../glossary.md#framebuffer)**) go to the built-in screen or through the dock to a TV (see [input and display](input-and-display.md)). It reads and writes the same shared [memory](memory.md) as the CPU.

## How it works

Games describe what to draw with small programs that run on the graphics hardware (called **[shaders](../../glossary.md#shader)**) and lists of drawing commands. Retail games use Nintendo's NVN library; homebrew normally uses the community deko3d library instead, which targets the same hardware through the open Nouveau-based driver stack. Small fast memories next to the compute units (called **[caches](../../glossary.md#cache)**) reward textures and render targets stored in block-linear layout, and the CPU and GPU do not see each other's writes automatically — software must flush, invalidate and fence explicitly.

## Technical details

| Item | Value | Evidence |
| ---- | ----- | -------- |
| Architecture | Maxwell GM20B, 256 CUDA cores in 2 streaming multiprocessors, 16 texture units, 16 render-output units | [documented] (NVIDIA Tegra X1 / GM20B materials) |
| L2 for the GPU | 256 KB | [documented] (NVIDIA) |
| Per-multiprocessor shared memory | 64 KB | [documented] (NVIDIA) |
| Half-precision throughput | 2x single-precision rate | [documented] (NVIDIA Maxwell whitepaper) |

| GPU clock target | Frequency | Single-precision rate | Notes |
| ---- | ----- | ----- | ----- |
| Handheld base | 307.2 MHz | ~157 GFLOPS | [documented] (NVIDIA clock/throughput math; SwitchBrew Clocks) |
| Handheld higher | 384 MHz | — | [reported] (SwitchBrew Clocks page) |
| Docked | 768 MHz | ~393 GFLOPS FP32 / ~786 GFLOPS FP16 | [documented] (NVIDIA Maxwell rates; SwitchBrew Clocks for the 768 MHz value) |
| Memory clock range | 1331–1600 MHz | feeds the ~21.3 GB/s handheld stock figure below | [reported] (SwitchBrew Clocks page) |

| Memory path | Value |
| ---- | ----- |
| Bus to shared LPDDR4 | 64-bit |
| Theoretical peak, Tegra X1 | 25.6 GB/s |
| Theoretical peak, X1+ (Mariko-era I/O) | 31.8 GB/s |
| Typical handheld stock setting | ~21.3 GB/s |

Bandwidth rows are [documented] for the theoretical peaks (NVIDIA memory-controller data) and [reported] for the stock handheld figure (SwitchBrew Clocks).

Texture and render-target layout: block-linear tiling in units of 64 bytes wide by 8 rows (GOB 64B×8 rows) [reverse-engineered] (Nouveau / deko3d tiling code and SwitchBrew GPU notes). Linear data must be tiled before the GPU samples or renders it efficiently.

Coherence: no automatic CPU↔GPU coherence. Software must flush CPU writes before GPU reads, invalidate CPU caches after GPU writes, and insert fences between GPU work and CPU reuse [documented] (deko3d memory/barrier API; Nouveau driver behavior).

## Homebrew notes

Use deko3d for new homebrew rendering; use NVN-style patterns (command buffers, explicit barriers, tiled images) only as concepts, since NVN itself is a retail SDK library. Always allocate GPU-visible memory through the deko3d / libnx allocators so tiling and cache maintenance are correct.

## Sources

- [SwitchBrew — Tegra X1 / GPU clocks](https://switchbrew.org/wiki/Tegra_X1) — handheld/docked GPU and memory clocks.
- [SwitchBrew — Clocks](https://switchbrew.org/wiki/Clocks) — 307.2 / 384 / 768 MHz GPU targets and 1331–1600 MHz memory range.
- [NVIDIA Tegra X1 whitepaper and GM20B docs](https://developer.nvidia.com/tegra-x1) — 256 CUDA cores, 2 multiprocessors, 16 texture / 16 render-output units, 256 KB L2, FP16 2x rate, 25.6 GB/s peak.
- [deko3d — homebrew GPU library](https://github.com/fincs/deko3d) — block-linear GOB layout, explicit flush/invalidate/fence API, NVN-alternative for homebrew.
- [libnx — GPU / display helpers](https://github.com/switchbrew/libnx) — how homebrew submits work and presents framebuffers.
