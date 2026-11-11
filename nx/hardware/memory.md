# NX Memory

The Switch has one pool of fast memory shared by everything: the main processor, the graphics, the system and the game. There is 4 GB in total, and how much of it your game may use depends on how it was launched. Small downloadable-style applets get a much smaller budget, while a full game — or homebrew that replaces a full game — gets roughly 3 GB to work with. Graphics textures, sound, code and display pictures all come out of that same pool.

## Where it fits

This shared temporary workspace (called **[memory](../../glossary.md#memory)**) sits behind one memory controller on the **[Tegra X1](../glossary.md#tegra-x1)** **[system-on-a-chip](../../glossary.md#soc)** and is read by both the **[CPU](cpu.md)** and the **[GPU](gpu.md)**. The **[kernel](../../glossary.md#kernel)** of [NX OS](../software/os.md) carves it into per-program private regions (each running program is a **[process](../../glossary.md#process)**) plus reserved areas for the display pictures (see [input and display](input-and-display.md)).

## How it works

Each game sees private virtual addresses translated and protected by the processor's address-translation hardware (the **[MMU](../../glossary.md#mmu)**), with start positions shuffled per boot for security. Most game memory comes from one large heap region plus mapped aliases for shared buffers; asking the system for memory (a **[syscall](../../glossary.md#syscall)**) returns pages the game can then use for code, data, textures or sound. When the game talks to a system service across process boundaries (called **[IPC](../../glossary.md#ipc)**), buffers are shared or copied through handles rather than by reaching into another program's memory. Running out of the applet budget looks like failed allocations, not a crash of the whole console — the fix is to run as a full-app replacement.

## Technical details

| Item | Value | Evidence |
| ---- | ----- | -------- |
| Total physical RAM | 4 GB shared CPU/GPU, 2×2 GB LPDDR4 chips, 64-bit bus | [documented] (NVIDIA Tegra X1 memory data; SwitchBrew hardware notes) |
| Bandwidth, theoretical | 25.6 GB/s (X1) / 31.8 GB/s (X1+ I/O) | [documented] (NVIDIA) |
| Bandwidth, typical handheld stock | ~21.3 GB/s | [reported] (SwitchBrew Clocks page) |
| Applet budget | Much smaller than full-app; exact size varies by applet kind | [reverse-engineered] (SwitchBrew applet/memory notes) |
| Full-app / title-override budget | ~3 GB-class application region via title override / hbmenu override | [reverse-engineered] (SwitchBrew homebrew memory notes; libnx app-region behavior) |

Address-space and layout (brief; full maps live with the OS page):

| Item | Value |
| ---- | ----- |
| Virtual-address size | 32-bit or 36-bit early; 38-bit since system version 2.0.0 |
| Regions of interest | Heap region, Map region, per-process shuffling of base addresses |

Virtual-address growth to 38 bits and the Heap/Map region split are [reverse-engineered] (SwitchBrew kernel / virtual-memory notes). Do not hard-code Heap or Map base addresses — they move per boot and per firmware; query them through the kernel / libnx at runtime.

## Homebrew notes

Launch homebrew by overriding a full game title (title override / hbmenu override) to get the ~3 GB-class budget; applet-mode homebrew will exhaust memory quickly on textures and audio. Allocate GPU-visible and audio/DSP-visible buffers through libnx / deko3d allocators so address translation, alignment and cache maintenance stay correct.

## Sources

- [SwitchBrew — Memory layout / Application memory](https://switchbrew.org/wiki/Memory_layout) — 4 GB shared pool, applet vs. full-app budgets, Heap/Map regions, 32/36-bit and 38-bit address spaces.
- [SwitchBrew — Homebrew Menu](https://switchbrew.org/wiki/Homebrew_Menu) — why overriding a full title gives the large app region.
- [NVIDIA Tegra X1 memory-controller docs](https://developer.nvidia.com/tegra-x1) — shared LPDDR4, 64-bit bus, theoretical bandwidth.
- [libnx — memory and mapping API](https://github.com/switchbrew/libnx) — how homebrew queries regions and maps shared buffers (`virtmem`, `svc` wrappers).
- [deko3d — GPU memory pools](https://github.com/fincs/deko3d) — GPU-visible allocation, tiling and barrier requirements for shared memory.
