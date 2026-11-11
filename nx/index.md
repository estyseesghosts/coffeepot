# Nintendo Switch (NX)

The Nintendo Switch is a hybrid console sold from 2017 that works both handheld and docked to a TV. It is built on NVIDIA's Tegra X1 chip and runs a Nintendo operating system the company also calls [Horizon](../glossary.md#horizon) — a distinct implementation from the 3DS one.

The family includes the original Switch, the Switch Lite (handheld-only), and the OLED model. This wiki uses **NX** ([glossary](glossary.md#nx)) as the platform name for the whole family. Unlike a hardware catalogue, this section is written for someone writing a homebrew game: what the hardware gives you, which libraries to use, and which performance habits pay off.

## At a glance

| Area | Summary |
| ---- | ------- |
| CPU | [Four ARM Cortex-A57 cores; ports report three usable by a game](hardware/cpu.md) |
| GPU / video | [NVIDIA Maxwell, 256 cores, lower clock handheld than docked, shared memory](hardware/gpu.md) |
| Memory | [4 GB shared CPU/GPU; large budget when launched in place of a full game](hardware/memory.md) |
| Storage / media | [Boot chain, internal-flash partitions, SD layout](software/firmware.md) |
| Operating software | [Horizon: small kernel plus sysmodules, CMIF/TIPC services](software/os.md) |
| Homebrew status | Runs through a custom firmware entry point and Atmosphere: [toolchain](homebrew/toolchain-and-workflow.md), [libnx + deko3d](homebrew/deko3d-and-libnx.md), [running](homebrew/atmosphere-and-running.md) |


## Hardware

- [CPU](hardware/cpu.md) — the A57 cluster, caches, clocks, and the three-core game budget.
- [GPU](hardware/gpu.md) — Maxwell configuration, handheld/docked clocks, tiling and coherence rules.
- [Memory](hardware/memory.md) — 4 GB shared pool, applet vs. application budgets, address-space widths.
- [Input and display](hardware/input-and-display.md) — Joy-Con hardware, HID shared memory, handheld/docked display.
- [Models and revisions](hardware/models.md) — Erista vs. Mariko, Lite/OLED boards, storage and memory.

## Software

- [Firmware](software/firmware.md) — boot chain, package2/BCPKG2, internal-flash and SD layout, fuses and version gating.
- [OS](software/os.md) — Horizon process, capability, and applet model.
- [Services and IPC](software/services-and-ipc.md) — CMIF/TIPC, handle discipline, and which services games actually use.

## Homebrew

- [Toolchain and workflow](homebrew/toolchain-and-workflow.md) — devkitA64, NRO builds, hbmenu, nxlink debug loop.
- [libnx and deko3d](homebrew/deko3d-and-libnx.md) — system library vs. GPU library, SDL2 choice, example map.
- [Atmosphere and running](homebrew/atmosphere-and-running.md) — CFW parts, title override, emuMMC, ban/brick risks.

For console modification and setup, follow [switch.homebrew.guide](https://switch.homebrew.guide/); this wiki covers writing, running, debugging, and tuning homebrew once the console is ready.

## Engineering

Practical guides for writing working, fast homebrew. They build on the reference pages above and label each claim by its evidence (documented, reverse-engineered, measured, reported, hypothesis). No result on these pages is a Coffeepot measurement yet, and no code has been run on a console.

- [A first Switch game](engineering/first-game.md) — one complete, compile-checked program: input, drawing, audio, saves, logging, crash dump, frame timing.
- [Debugging](engineering/debugging.md) — `nxlink` logging, fatal screens and `addr2line`, crash dumps, Atmosphere crash reports, hangs, GPU errors.
- [Rendering](engineering/rendering.md) — a correct `deko3d` frame loop, per-slot fences, static upload, command capture, image layout, barriers, port lessons.
- [Performance and porting](engineering/performance-and-porting.md) — measurement hygiene, reported speed-ups with their conditions, threads, lifecycle, saves, audio, porting checklist.
- [Source versions](engineering/source-versions.md) — pinned commits, installed toolchain, and what each code example has been checked against.

### Case studies by purpose

Each page opens with *When is this useful?* so you can skip the ones that do not match your project. Figures are `[reported]` from the projects' own trees unless a page says otherwise.

- **Native `deko3d` and recompiled ports:** [SA2](engineering/case-studies/case-study-sa2.md), [USF4 and NFSMW](engineering/case-studies/case-studies-demanding-ports.md), [Wind Waker HD](engineering/case-studies/case-study-wind-waker-hd.md), and the overlapping [deko3d stack summary](engineering/case-studies/case-study-deko3d-gpu-stack.md).
- **OpenGL and SDL engine ports (no frame rates stated in their trees):** [open-engine ports](engineering/case-studies/case-studies-open-engines.md), [dhewm3](engineering/case-studies/case-study-dhewm3.md), [Serious Engine](engineering/case-studies/case-study-serious-engine.md), [nxquake2](engineering/case-studies/case-study-nxquake2.md), and the overlapping [Godot and STK page](engineering/case-studies/case-study-godot-stk-engines.md).
- **Emulation, rehosting and measurement:** [melonDS](engineering/case-studies/case-study-melonds.md), [Max Payne](engineering/case-studies/case-study-max-nx.md), [Eidos (cautionary)](engineering/case-studies/case-study-eidos-ouroboros.md).
- **OS, services and custom firmware (overlap the reference pages):** [libnx OS interface](engineering/case-studies/case-study-libnx-os-interface.md), [Atmosphere CFW](engineering/case-studies/case-study-atmosphere-cfw.md).

## Notes and open questions

- Clock figures are port reports (`nfsmw-nx`), because the SwitchBrew Clocks page was not available; per-model thermal throttling is unmeasured.
- Applet and application memory budgets are not pinned per firmware; see [memory](hardware/memory.md).
- Engineering code fragments compile against devkitA64 r30-1 / libnx 4.12.0-1 / deko3d 0.5.0-1 with no warnings but have not been run on hardware; benchmarks that settle the draw-call, bandwidth and threading questions are open work (see the Open questions of each engineering page).
- Erista and Mariko share one programming model; power and thermal differences are noted on the [models page](hardware/models.md).
- Pages that cite SwitchBrew pages that were not among the saved sources (Boot, Fuses, System versions, NCA, NSO, NPDM, Applets, HID, Clocks) say so inline.

## Sources

- [SwitchBrew](https://switchbrew.org/) — primary community technical reference for Tegra X1, kernel, IPC/SVC, services, filesystems. Revisions read are on [source versions](engineering/source-versions.md).
- [libnx](https://github.com/switchbrew/libnx), [deko3d](https://github.com/devkitPro/deko3d), [uam](https://github.com/devkitPro/uam), [switch-examples](https://github.com/switchbrew/switch-examples) — toolchain, libraries, and demo progression, at pinned commits.
- [Atmosphere](https://github.com/Atmosphere-NX/Atmosphere), [Hekate](https://github.com/CTCaer/hekate) — CFW and bootloader behavior.
- [devkitPro](https://devkitpro.org/wiki/Getting_Started) — toolchain install.
- [NVIDIA Tegra X1](https://developer.nvidia.com/tegra-x1) — SoC background; the saved SwitchBrew Tegra X1 page is the cited source for specifications.
- [switch.homebrew.guide](https://switch.homebrew.guide/) — install and setup procedures (linked, not duplicated).
