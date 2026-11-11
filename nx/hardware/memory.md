# NX Memory

The Switch has one pool of fast memory shared by everything: the main processor, the graphics, the system and the game. There is 4 GB in total, and how much of it your game may use depends on how it was launched. Applets (small system programs such as the Album, from which homebrew can also be launched) get a much smaller budget, while a full game — or homebrew that replaces a full game — gets roughly 3 GB to work with according to port reports. Graphics textures, sound, code and display pictures all come out of that same pool.

## Where it fits

This shared temporary workspace (called **[memory](../../glossary.md#memory)**) sits behind one memory controller on the **[Tegra X1](../glossary.md#tegra-x1)** **[system-on-a-chip](../../glossary.md#soc)** and is read by both the **[CPU](cpu.md)** and the **[GPU](gpu.md)**. The **[kernel](../../glossary.md#kernel)** of [NX OS](../software/os.md) carves it into per-program private regions (each running program is a **[process](../../glossary.md#process)**) plus reserved areas for the display pictures (see [input and display](input-and-display.md)).

## How it works

Each game sees private virtual addresses translated and protected by the processor's address-translation hardware (the **[MMU](../../glossary.md#mmu)**), with start positions shuffled per boot for security. Most game memory comes from one large heap region plus mapped aliases for shared buffers; asking the system for memory (a **[syscall](../../glossary.md#syscall)**) returns pages the game can then use for code, data, textures or sound. When the game talks to a system service across process boundaries (called **[IPC](../../glossary.md#ipc)**), buffers are shared or copied through handles rather than by reaching into another program's memory. Running out of the applet budget looks like failed allocations, not a crash of the whole console — the fix is to run as a full-app replacement.

## Technical details

| Item | Value | Evidence |
| ---- | ----- | -------- |
| Total physical RAM | 4 GB shared CPU/GPU: two 2 GB LPDDR4 chips (LPDDR4X on Mariko boards), 64-bit bus | [documented] (SwitchBrew Hardware, oldid 15339: two 2 GB DRAM chips on every model; SwitchBrew Tegra X1) |
| Bandwidth, theoretical | ~25.6 GB/s (X1, LPDDR4-1600) / ~31.8 GB/s (X1+, LPDDR4X-2133) | [documented] (SwitchBrew Tegra X1) |
| Bandwidth at handheld stock memory clock | ~21.3 GB/s (1331.2 MHz × 2 × 8 bytes) | arithmetic from [reported] clock (`nfsmw-nx` `README.md`) |
| Applet budget | Much smaller than an application's; no figure saved | Unknown (see below) |
| Application budget under title override | About 3,185 of 3,189 MB reported as "used" at start by `svcGetInfo(InfoType_UsedMemorySize)` in one port, i.e. a heap of about 3 GB; the same note warns that `UsedMemorySize` counts the whole heap from the start and is not a free-memory figure | [reported] (`nfsmw-nx` `docs/platform-notes.md` "In short", commit `20f8dc1`) |

> Unknown: the applet and application memory budgets per firmware version and applet kind. SwitchBrew's applet memory pages were not among the saved sources and Atmosphere does not document the budgets. A hardware test that prints the heap size from `svcGetInfo` under each launch mode would settle it.

Address-space and layout (brief; full maps live with the OS page):

| Item | Value | Evidence |
| ---- | ----- | -------- |
| Userspace address space | 32 or 36 bits; 38-bit support added in 2.0.0 | [reverse-engineered] (SwitchBrew Memory layout, "Userspace", oldid 12866) |
| Regions randomised by the kernel | ReservedHeapRegion and ReservedMapRegion, each with random upper bits and 2 MB alignment; from 2.0.0 also NewReservedMapRegion and TlsIoRegion; read with `svcGetInfo` | [reverse-engineered] (same page) |

### Address-space widths

Pages on this wiki used to alternate between 38-bit and 39-bit without saying why. The two figures come from different sources describing different things:

- **SwitchBrew (38 bit).** Its Memory layout page says the userspace virtual address space "can be either 32 or 36 bits. [2.0.0+] introduced support for 38 bit address spaces", and that on 2.0.0 and later the main binary region has random bits 37-21 and an NRO base address has random bits 37-12 `[reverse-engineered]`.
- **Atmosphere (39 bit).** The process-creation flags defined in its headers name four address-space types: 32-bit, 36-bit, 39-bit and 42-bit (`CreateProcessParameterFlag_AddressSpace64Bit36`, `...64Bit39`, `...64Bit42`, `...32Bit`), and the homebrew menu launched under title override defaults to a 39-bit address space unless `override_address_space` says otherwise `[documented]` (`Atmosphere` `libraries/libvapours/include/vapours/svc/svc_types_common.hpp`; `docs/changelog.md` lines 738-743; `config_templates/override_config.ini`).

> Unknown: whether SwitchBrew's "38 bit" and Atmosphere's "39 bit" are the same layout under two naming conventions (for example, counting only the user half of the table) or different configurations. Reading `svcGetInfo(InfoType_AddressSpaceSize)` from a running homebrew program on a console would answer it. Do not hard-code either width; query it.

Do not hard-code Heap or Map base addresses — they move per boot and per firmware; query them through the kernel / libnx at runtime.

## Homebrew notes

Launch homebrew by overriding a full game title (title override; see [Atmosphere and running](../homebrew/atmosphere-and-running.md)) to get the large budget; ports report applet-mode launches running out of memory on real asset sets [reported] (`srb2-switch` `README.md`). Allocate GPU-visible and audio/DSP-visible buffers through libnx / deko3d allocators so address translation, alignment and cache maintenance stay correct.

## Sources

- SwitchBrew [Memory layout](https://switchbrew.org/wiki/Memory_layout) (oldid 12866, edited 2024-07-31) — userspace address-space widths and region randomisation.
- SwitchBrew [Hardware](https://switchbrew.org/wiki/Hardware) (oldid 15339) and [Tegra X1](https://switchbrew.org/wiki/Tegra_X1) (oldid 14745) — 4 GB of LPDDR4/LPDDR4X, bus width, theoretical bandwidth.
- [Atmosphere](https://github.com/Atmosphere-NX/Atmosphere) at `4e3f37038d9e82dde7054897213ab32b2ecf651f` — address-space types, `override_address_space`, 39-bit default.
- [nfsmw-nx](https://github.com/StevensND/nfsmw-nx) at `20f8dc1` — stock memory clocks and heap size under title mode `[reported]`.
- [libnx](https://github.com/switchbrew/libnx) at `b156b47` — `virtmem` and `svc` wrappers for querying regions.
- [deko3d](https://github.com/devkitPro/deko3d) at `373bfd4` — `Primer.md` "Memory Blocks": GPU-visible allocation, flags, cache rules.
