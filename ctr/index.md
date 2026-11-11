# Nintendo 3DS (CTR)

The Nintendo 3DS is a handheld console sold from 2011, the successor to the Nintendo DS. Its signature feature is a glasses-free stereoscopic 3D upper screen, paired with a touchscreen lower screen.

It plays 3DS cartridges and (mostly) DS cartridges, and runs a Nintendo operating system the company calls [Horizon](../glossary.md#horizon). The family has two groups. This wiki treats the **Old 3DS** (3DS, 3DS XL, 2DS) as the default; the faster **New 3DS** group (New 3DS, New 3DS XL, New 2DS XL) is covered in explicit **New 3DS:** notes. This wiki uses **CTR** ([glossary](glossary.md#ctr)) as the platform name for the whole family. Online play ended on 8 April 2024; the eShop closed in March 2023.

Unusually for a console, it contains **three** generations of ARM processor: a dual-core ARM11 that runs games (**New 3DS:** quad-core), an ARM9 that acts as a security and storage processor, and an ARM7 kept only for Game Boy Advance/DS compatibility. A single boot ROM, unchanged across all models, is the root of its security — and, once found flawed, the root of its homebrew scene.

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
- [Peripheral control registers](hardware/peripheral-registers.md) — HID, GPIO, I²C, sleep/wake and legacy-input fields for low-level work.
- [Game cards](hardware/gamecard.md) — the cartridge interface and protocol.
- [GPU shading features](hardware/gpu-shading-features.md) — lighting, shadows, procedural textures and the hardware's quirks.
- [Security hardware](hardware/security-hardware.md) — AES/RSA/SHA engines, OTP and boot ROM key storage.
- [Models and variants](hardware/models.md) — the six models and their parts.
- [ARM11 MPCore internals](hardware/arm11-mpcore.md) — register-level reference for the ARM11 cores: caches, MMU/TLB, interrupts, timers, performance counters, VFP11.
- [GPU internal registers](hardware/gpu-internal-registers.md) — bit-level reference for the PICA200 command-list registers.
- [GPU external registers](hardware/gpu-external-registers.md) — memory fill, transfer engine, profiling and command-list start registers.
- [DSP firmware interface](hardware/dsp.md) — DSP firmware file format, shared audio memory layout, AAC decoder and `dsp::DSP` commands.
- [Screen video capture points](hardware/video-capture.md) — board test points for tapping the lower screen's video signal.
- [Board test points](hardware/tp-nets.md) — measured test pads on the Old 3DS board.

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
- [Kernel objects](software/kernel-objects.md) — field-level layouts of KProcess, KThread, class tokens, debug events and per-core state.
- [SVC reference](software/svc-reference.md) — every system call with availability, KernelSetState/GetSystemInfo details, error codes and Luma3DS additions.
- [Title metadata and tickets](software/title-metadata-and-tickets.md) — TMD and ticket byte layouts, keys and certificates.
- [System title list](software/title-list.md) — system titles by category, with IDs.
- [System update data](software/update-data.md) — titles changed by the early system updates.
- [System flaws](software/system-flaws.md) — documented hardware, boot ROM, ARM9 and ARM11 flaws with fix versions.

## Homebrew

For console modification and setup, use the [3DS Hacks Guide](https://3ds.hacks.guide/). If the console is already prepared, start with the [development workflow](homebrew/development-workflow.md) to build and run homebrew; an emulator is another way to begin. The pages below document how the hardware and software work.

- [History](homebrew/history.md) — thirteen years of hacking, from flashcarts to MSET9.
- [Exploit catalogue](homebrew/exploits.md) — the privilege ladder and the named exploits.
- [boot9strap, sighax and ntrboot](homebrew/boot9strap.md) — the boot-ROM break behind today's standard setup.
- [Custom firmware](homebrew/custom-firmware.md) — the CFW concept, its predecessors and emuNAND.
- [Luma3DS](homebrew/luma3ds.md) — its components, patches, Rosalina, plugins and the GDB stub.
- [libctru](homebrew/libctru.md) — the userland library: heaps, clocks, services, QTM.
- [picasso and citro3d](homebrew/picasso-and-citro3d.md) — the shader assembler and GPU library.
- [Development workflow](homebrew/development-workflow.md) — toolchain versions, how `.3dsx` programs are loaded today, `.3dsx` vs `.cia`, and the example projects.
- [Research index](homebrew/research-index.md) — who is credited with which findings and tools, and where vanished sources survive.
- [3ds-examples catalogue](homebrew/3ds-examples.md) — every devkitPro sample program mapped to the wiki guide that covers it.

## Engineering

Practical guides for writing working, fast homebrew. They build on the reference pages above and label each claim by its evidence (documented, reverse-engineered, measured, hypothesis).

- [Debugging](engineering/debugging.md) — 3dslink logging, error screens and crash dumps, GDB through Rosalina, GPU hangs, emulators.
- [Performance](engineering/performance.md) — measuring CPU and GPU time, finding the bottleneck, frame pacing, using the second core, ARM11 code habits.
- [Memory and cache](engineering/memory-and-cache.md) — how much memory a program gets, heap vs linear vs VRAM, cache flushing rules, buffer lifetime.
- [GPU rendering](engineering/PICA200/gpu-rendering.md) — citro3d frames, draw-call costs, batching, textures, combiner recipes, citro2d, stereo and wide mode.
- [Asset pipeline](engineering/PICA200/asset-pipeline.md) — tex3ds, shaders, fonts, RomFS, audio and compression.
- [Testing on hardware](engineering/testing-on-hardware.md) — test tiers, a fast test loop and the experiment protocol.
- [CPU optimisation](engineering/cpu-optimization.md) — how the ARM11 executes code, expensive constructs, reading generated code, hardware performance counters.
- [Threading](engineering/threading.md) — which cores you can use, core-1 time sharing, priorities, synchronisation, a worker pattern.
- [Frame pacing](engineering/PICA200/frame-pacing.md) — the CPU/GPU/display timing model, locking to 60 or 30, fixed-step simulation, missed frames.
- [Audio](engineering/audio.md) — ndsp setup and the DSP firmware file, formats, buffer sizing, streaming, diagnosing crackles.
- [2D rendering](engineering/PICA200/rendering-2d.md) — end-to-end guide to 2D on the PICA200: setup and frame loop, screens and depth, textures and memory, citro2d beyond sprites, performance decisions and troubleshooting.
- [2D tile renderer](engineering/PICA200/2d-tile-renderer.md) — a compile-checked scrolling tilemap with sprites and cached text, four tile-submission strategies, and a scripted mode that writes per-phase timings to the SD card for the crossover experiments (all unrun).
- [Porting](engineering/porting.md) — planning a port, desktop-to-PICA200 concept mapping, SDL and portlibs, what published ports chose.
- [Application lifecycle](engineering/application-lifecycle.md) — how a program handles HOME, sleep, resume, the software keyboard and exit, with a complete logging example.
- [Files and saves](engineering/filesystem-and-saves.md) — SD card, RomFS, save data and extdata, commit rules, write errors, and a two-copy checksummed save manager.
- [IPC and services](engineering/ipc-and-services.md) — how to write a correct service client: message layout, transport versus service results, buffers, handles, sessions and failure tests, with `cfg:u` GetSystemModel as the worked example.
- [Networking](engineering/networking.md) — starting `soc:U`, non-blocking sockets, bounded-memory HTTP downloads with httpc, and HTTPS with certificate validation left on.
- [PICA200 command pipeline](engineering/PICA200/pica200-command-pipeline.md) — what draw calls write, how command lists are encoded and reach the GPU, a hang/overflow/cache checklist, and a command-list decoder.
- [PICA200 shader programming](engineering/PICA200/pica200-shader-programming.md) — picasso shaders from one triangle to lighting, instruction encoding, float24 behaviour and a debugging tree.
- [Input and sensors](engineering/input-and-sensors.md) — buttons, Circle Pad, touch, motion sensors, C-Stick and ZL/ZR, key repeat and model differences.
- [Cameras and Y2R](engineering/camera-and-y2r.md) — capture sequence, buffers, YUV-to-RGB conversion.
- [Microphone](engineering/microphone.md) — shared-memory ring buffer sampling.
- [NFC and infrared](engineering/nfc-and-infrared.md) — amiibo tag-state machine, the Old 3DS NFC adapter path, and the IR transceiver session.
- [System information and power](engineering/system-info-and-power.md) — model, battery, memory and other system-level services.
- [New 3DS extras: QTM and MVD](engineering/new-3ds-qtm-and-mvd.md) — head tracking and the hardware video decoder.
- [Local wireless](engineering/local-wireless.md) — UDS host, join, send and receive.
- [libctru service groups](engineering/service-groups.md) — init/exit pattern, every remaining service group, utilities, and a probe for which services your build can open.
- [citro3d fixed-function API](engineering/citro3d-fixed-function-api.md) — which citro3d call sets lighting, fog, procedural textures, gas and render state.
- [Renderer architecture](engineering/PICA200/renderer-architecture.md) — decision guide for retained vs immediate rendering, batching, state caching, flush ranges and effect reformulation, with planned (not yet run) GPU benchmarks.
- [Stereo engineering](engineering/PICA200/stereo-engineering.md) — cost of the second eye, camera separation and convergence, the 3D slider, depth plane, HUD depth and comfort testing on an Old 3DS.
- [3D scene optimisation](engineering/PICA200/3d-scene-optimisation.md) — visibility culling, draw ordering and the early-depth evidence, LOD, baked lighting and 2D quads in 3D scenes.
- [Case study: HGSS port](engineering/case-studies/case-study-hgss-port.md) — hardware profiling, renderer-state failures, cache validation, audio and shutdown pitfalls.
- [Case studies: rendering trade-offs](engineering/case-studies/case-studies-rendering-tradeoffs.md) — DS and 2D port pipelines, frame ownership and Smash content budgets.
- [Case studies: commercial 3DS games](engineering/case-studies/case-studies-commercial.md) — developer-stated techniques from five shipped games, labelled as reported, with what hardware tests would verify them.
- [Case studies: large 3DS ports](engineering/case-studies/case-studies-ports.md) — reported techniques and results from three N64 ports (F-Zero X, Super Mario 64, Ocarina of Time), with model and conditions.
- [Source versions and verification](engineering/source-versions.md) — pinned upstream commits, toolchain versions and the verification status of every code example.
- [Benchmark suite](benchmarks/README.md) — buildable programs measuring memory, cache-flush, instruction-cache and draw-call costs (`membench`, `flushbench`, `icachebench`, `drawbench`) and the result-recording format (not yet run on hardware).

## Notes and open questions

- Several areas are documented only partly in the sources: scheduler policy, GPU Primitive Engine, many I/O registers, the byte-level layouts of NCSD/NCCH/ExeFS/RomFS (3dbrew has them; not summarised here).
- Source data differ in places (XDMA channel count, DSP sample rate, 176 vs 178 MB maximum application memory, MCU instruction set naming); each hardware page records the disagreement.
- Not yet covered: individual system applications (Home Menu internals, browser), the StreetPass wire protocol (only the mailbox side is documented), the camera and NFC subsystems in detail, the Teak DSP architecture, gas rendering.
- Kernel11 and Process9 internals beyond what 3dbrew publishes are not in the source set.
- The engineering pages contain no measurements made for this wiki yet: costs such as the whole-linear-heap flush, draw-call overhead and heap-versus-linear write bandwidth are listed as open questions with [benchmarks](benchmarks/README.md) that build but have not yet been run on hardware.

## Sources

- [Rodrigo Copetti, Nintendo 3DS Architecture](https://www.copetti.org/writings/consoles/nintendo-3ds/) — practical overview of the whole console (CC BY 4.0; adapted and condensed here).
- [3dbrew](https://www.3dbrew.org/) — the primary community technical reference for hardware, kernel, formats and exploits.
- [ARM11 MPCore Processor Technical Reference Manual (DDI 0360E)](https://developer.arm.com/documentation/ddi0360/e) — CPU core details.
- [Hacks Guide Wiki: History of 3DS Hacking](https://wiki.hacks.guide/wiki/3DS:History_of_3DS_Hacking) — timeline.
- [WikiTemp: 3DS Homebrew Development](https://wiki.gbatemp.net/wiki/3DS_Homebrew_Development) (revision 65702, 25 February 2017; historical, superseded in places) — homebrew build workflow.
- Citra project wiki (pages "Kernel synchronization", "3DS CRO Information", "3DS CEC (StreetPass) Documentation"; "GPU_LUTINPUT_CP" and "Random PICA200 RE Notes" survive as author gists) — the repository was removed from GitHub and no archived copy of the first three was found; see [Research index](homebrew/research-index.md#looking-for-the-vanished-citra-wiki-pages).
- [4Gamer.net CEDEC 2012 PICA200 report](https://www.4gamer.net/games/017/G001762/20120822007/) — DMP's talk on the GPU.
- Project sources: [Luma3DS](https://github.com/LumaTeam/Luma3DS), [libctru](https://github.com/devkitPro/libctru), [citro3d](https://github.com/devkitPro/citro3d), [citro2d](https://github.com/devkitPro/citro2d), [picasso](https://github.com/devkitPro/picasso), [3ds-examples](https://github.com/devkitPro/3ds-examples), [3ds-hbmenu](https://github.com/devkitPro/3ds-hbmenu), [aemstro](https://github.com/smealum/aemstro).
- [fincs, Random PICA200 RE Notes](https://gist.github.com/fincs/543f7e1375815f495f10020a053f14e9) — procedural textures, shadows, GPU quirks.
- [libctru issue #556](https://github.com/devkitPro/libctru/issues/556) — third-party measurements of DSP cache-flush latency under a core-1 time share.
