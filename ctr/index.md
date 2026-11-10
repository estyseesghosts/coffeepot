# Nintendo 3DS (CTR)

The Nintendo 3DS is a handheld console sold from 2011, the successor to the Nintendo DS. Its signature feature is a glasses-free stereoscopic 3D upper screen, paired with a touchscreen lower screen.

It plays 3DS cartridges and (mostly) DS cartridges, and runs a Nintendo operating system the company calls [Horizon](../glossary.md#horizon). The family includes the 3DS, 3DS XL, 2DS (the "Old 3DS" group) and the faster New 3DS / New 3DS XL / New 2DS XL models (the "New 3DS" group). This wiki uses **CTR** ([glossary](glossary.md#ctr)) as the platform name for the whole family. Online play ended on 8 April 2024; the eShop closed in March 2023.

Unusually for a console, it contains **three** generations of ARM processor: a dual- or quad-core ARM11 that runs games, an ARM9 that acts as a security and storage processor, and an ARM7 kept only for Game Boy Advance/DS compatibility. A single boot ROM, unchanged across all models, is the root of its security — and, once found flawed, the root of its homebrew scene.

## At a glance

| Area | Summary |
| ---- | ------- |
| CPU | [ARM11 MPCore for games, ARM9 for security and storage, ARM7 for GBA/DS](hardware/cpu.md) |
| GPU / video | [DMP PICA200 graphics driving a glasses-free 3D display](hardware/gpu.md) and [two screens](hardware/display.md) |
| Memory | [FCRAM main memory plus small fast on-chip pools](hardware/memory.md) |
| Storage / media | [Internal eMMC](software/nand-filesystem.md), [SD card](software/sd-filesystem.md), [game cards](hardware/gamecard.md) |
| Operating software | [Horizon: a separate kernel on each main processor](software/os.md) |
| Homebrew status | Fully open since 2017 via a boot ROM flaw: [boot9strap](homebrew/boot9strap.md) and [Luma3DS](homebrew/custom-firmware.md) |

## Hardware

- [CPU](hardware/cpu.md) — the ARM11/ARM9/ARM7 processors, clocks, caches and the New 3DS differences.
- [Memory](hardware/memory.md) — FCRAM, VRAM, on-chip RAM, address maps and the application memory budget.
- [GPU](hardware/gpu.md) — the PICA200 pipeline, registers and shader basics.
- [Displays](hardware/display.md) — the parallax-barrier 3D screen, framebuffers and the display controller.
- [Audio](hardware/audio.md) — the DSP and the DS-style CSND block.
- [Input/output and peripherals](hardware/io-and-peripherals.md) — buttons, sensors, cameras, the MCU, buses and the I/O register map.
- [Game cards](hardware/gamecard.md) — the cartridge interface and protocol.
- [GPU shading features](hardware/gpu-shading-features.md) — lighting, shadows, procedural textures and the hardware's quirks.
- [Security hardware](hardware/security-hardware.md) — AES/RSA/SHA engines, OTP and boot ROM key storage.
- [Models and variants](hardware/models.md) — the six models and their parts.

## Software

- [Firmware](software/firmware.md) — the four firmware images and how updates work.
- [Boot process](software/boot-process.md) — from power button to Home Menu, and the boot ROM error screen.
- [OS](software/os.md) — the two-kernel Horizon design, the shell and applets.
- [Kernel and system calls](software/kernel.md) — processes, threads, scheduling, SVCs.
- [Services](software/services.md) — the system services games call.
- [Titles](software/titles.md) — title IDs, metadata, tickets and versions.
- [File formats](software/file-formats.md) — NCSD, NCCH, CIA and friends.
- [NAND filesystem](software/nand-filesystem.md) and [SD filesystem](software/sd-filesystem.md) — storage layout and encryption.
- [Savegames](software/savegames.md) — DISA containers and game-card save protection.
- [CRO dynamic modules](software/cro-modules.md) — loadable code modules and the RO service.
- [StreetPass (CEC)](software/streetpass-cec.md) — mailbox model and file formats.
- [Country/region data](software/country-region-data.md) — the shared archive of country and region tables.
- [Security architecture](software/security-architecture.md) — the chain of trust and its design flaws.
- [Development units](software/dev-unit-software.md) — Nintendo's non-retail hardware and software.

## Homebrew

- [History](homebrew/history.md) — thirteen years of hacking, from flashcarts to MSET9.
- [Exploit catalogue](homebrew/exploits.md) — the privilege ladder and the named exploits.
- [boot9strap, sighax and ntrboot](homebrew/boot9strap.md) — the boot-ROM break behind today's standard setup.
- [Custom firmware](homebrew/custom-firmware.md) — the CFW concept, its predecessors and emuNAND.
- [Luma3DS](homebrew/luma3ds.md) — its components, patches, Rosalina, plugins and the GDB stub.
- [libctru](homebrew/libctru.md) — the userland library: heaps, clocks, services, QTM.
- [picasso and citro3d](homebrew/picasso-and-citro3d.md) — the shader assembler and GPU library.
- [Development workflow](homebrew/development-workflow.md) — toolchain versions, how `.3dsx` programs are loaded today, `.3dsx` vs `.cia`, and the example projects.

## Engineering

Practical guides for writing working, fast homebrew. They build on the reference pages above and label each claim by its evidence (documented, reverse-engineered, measured, hypothesis).

- [Debugging](engineering/debugging.md) — 3dslink logging, error screens and crash dumps, GDB through Rosalina, GPU hangs, emulators.
- [Performance](engineering/performance.md) — measuring CPU and GPU time, finding the bottleneck, frame pacing, using the second core, ARM11 code habits.
- [Memory and cache](engineering/memory-and-cache.md) — how much memory a program gets, heap vs linear vs VRAM, cache flushing rules, buffer lifetime.
- [GPU rendering](engineering/gpu-rendering.md) — citro3d frames, draw-call costs, batching, textures, combiner recipes, citro2d, stereo and wide mode.
- [Asset pipeline](engineering/asset-pipeline.md) — tex3ds, shaders, fonts, RomFS, audio and compression.
- [Testing on hardware](engineering/testing-on-hardware.md) — test tiers, a fast test loop, experiment protocol and a memory/flush benchmark.

## Notes and open questions

- Several areas are documented only partly in the sources: scheduler policy, GPU Primitive Engine, many I/O registers, the byte-level layouts of NCSD/NCCH/ExeFS/RomFS (3dbrew has them; not summarised here).
- Source data differ in places (XDMA channel count, DSP sample rate, 176 vs 178 MB maximum application memory, MCU instruction set naming); each hardware page records the disagreement.
- Not yet covered: individual system applications (Home Menu internals, browser), the StreetPass wire protocol (only the mailbox side is documented), the camera and NFC subsystems in detail, the Teak DSP architecture, gas rendering.
- Kernel11 and Process9 internals beyond what 3dbrew publishes are not in the source set.
- The engineering pages contain no measurements made for this wiki yet: costs such as the whole-linear-heap flush, draw-call overhead and heap-versus-linear write bandwidth are listed as open questions with a [benchmark](engineering/testing-on-hardware.md#membench) to settle them.

## Sources

- [Rodrigo Copetti, Nintendo 3DS Architecture](https://www.copetti.org/writings/consoles/nintendo-3ds/) — practical overview of the whole console (CC BY 4.0; adapted and condensed here).
- [3dbrew](https://www.3dbrew.org/) — the primary community technical reference for hardware, kernel, formats and exploits.
- [ARM11 MPCore Processor Technical Reference Manual (DDI 0360E)](https://developer.arm.com/documentation/ddi0360/e) — CPU core details.
- [Hacks Guide Wiki: History of 3DS Hacking](https://wiki.hacks.guide/wiki/3DS:History_of_3DS_Hacking) — timeline.
- [WikiTemp: 3DS Homebrew Development](https://wiki.gbatemp.net/w/index.php?title=3DS_Homebrew_Development) — homebrew build workflow.
- Citra project wiki (kernel synchronization, CRO, CEC, PICA200 notes) — the repository has been removed from GitHub, so pages are cited by title without links.
- [4Gamer.net CEDEC 2012 PICA200 report](https://www.4gamer.net/games/017/G001762/20120822007/) — DMP's talk on the GPU.
- Project sources: [Luma3DS](https://github.com/LumaTeam/Luma3DS), [libctru](https://github.com/devkitPro/libctru), [citro3d](https://github.com/devkitPro/citro3d), [citro2d](https://github.com/devkitPro/citro2d), [picasso](https://github.com/devkitPro/picasso), [3ds-examples](https://github.com/devkitPro/3ds-examples), [3ds-hbmenu](https://github.com/devkitPro/3ds-hbmenu), [aemstro](https://github.com/smealum/aemstro).
- [fincs, Random PICA200 RE Notes](https://gist.github.com/fincs/543f7e1375815f495f10020a053f14e9) — procedural textures, shadows, GPU quirks.
- [libctru issue #556](https://github.com/devkitPro/libctru/issues/556) — third-party measurements of DSP cache-flush latency under a core-1 time share.
