# Nintendo Switch (NX)

The Nintendo Switch is a hybrid console sold from 2017 that works both handheld and docked to a TV. It is built on NVIDIA's Tegra X1 chip and runs a Nintendo operating system the company also calls [Horizon](../glossary.md#horizon) — a distinct implementation from the 3DS one.

The family includes the original Switch, the Switch Lite (handheld-only), and the OLED model. This wiki uses **NX** ([glossary](glossary.md#nx)) as the platform name for the whole family. Unlike a hardware catalogue, this section is written for someone writing a homebrew game: what the hardware gives you, which libraries to use, and which performance habits pay off.

## At a glance

| Area | Summary |
| ---- | ------- |
| CPU | [Four ARM Cortex-A57 cores, three for games at 1020 MHz](hardware/cpu.md) |
| GPU / video | [Maxwell GM20B: 307 MHz handheld, 768 MHz docked, shared memory](hardware/gpu.md) |
| Memory | [4 GB shared CPU/GPU; full-app budget only via title override](hardware/memory.md) |
| Storage / media | [Package2 boot chain, BIS partitions, SD layout](software/firmware.md) |
| Operating software | [Horizon: small kernel plus sysmodules, CMIF/TIPC services](software/os.md) |
| Homebrew status | Open via RCM/modchip + Atmosphere: [toolchain](homebrew/toolchain-and-workflow.md), [libnx + deko3d](homebrew/deko3d-and-libnx.md), [running](homebrew/atmosphere-and-running.md) |

Keep numbers out of this table unless they fit in one line. Full specs live on subpages.

## Hardware

- [CPU](hardware/cpu.md) — the A57 cluster, caches, clocks, and the three-core game budget.
- [GPU](hardware/gpu.md) — Maxwell GM20B config, handheld/docked clocks, tiling and coherence rules.
- [Memory](hardware/memory.md) — 4 GB shared pool, applet vs. full-app budgets, address-space notes.
- [Input and display](hardware/input-and-display.md) — Joy-Con hardware, HID shared-memory polling, handheld/docked display.
- [Models and revisions](hardware/models.md) — Erista vs. Mariko, Lite/OLED differences, entry-point effects.

## Software

- [Firmware](software/firmware.md) — boot chain, package2/BCPKG2, fuses and version gating.
- [OS](software/os.md) — Horizon process, capability, and applet model.
- [Services and IPC](software/services-and-ipc.md) — CMIF/TIPC, handle discipline, and which services games actually use.

## Homebrew

- [Toolchain and workflow](homebrew/toolchain-and-workflow.md) — devkitA64, NRO builds, hbmenu, nxlink debug loop.
- [libnx and deko3d](homebrew/deko3d-and-libnx.md) — system library vs. GPU library, SDL2 choice, example map.
- [Atmosphere and running](homebrew/atmosphere-and-running.md) — CFW parts, title override, emuMMC, ban/brick risks.

For console modification and setup, follow [switch.homebrew.guide](https://switch.homebrew.guide/); this wiki covers writing, running, debugging, and tuning homebrew once the console is ready.

## Engineering

Practical guides for writing working, fast homebrew. They build on the reference pages above and label each claim by its evidence (documented, reverse-engineered, reported, hypothesis).

- [Rendering](engineering/rendering.md) — batching, atlases, static upload, command-list rings, tiled textures, resolution choice.
- [Performance and porting](engineering/performance-and-porting.md) — clock locking, payoff order, three-core split, porting checklist.
- [Case study: SA2 port](engineering/case-studies/case-study-sa2.md) — Renut on deko3d, snapshot deltas, store-cards, shader cache.
- [Case studies: demanding ports](engineering/case-studies/case-studies-demanding-ports.md) — USF4's deko3d switch vs NFSMW staying on NVK.
- [Case studies: open-engine ports](engineering/case-studies/case-studies-open-engines.md) — SDL/GLES path vs native deko3d decision rule.
- [Case study: Wind Waker HD port](engineering/case-studies/case-study-wind-waker-hd.md) — sealed shader templates, barrier hygiene, operate-like-a-port discipline.
- [Case study: Eidos on Switch](engineering/case-studies/case-study-eidos-ouroboros.md) — cautionary tale: readback-bound GL attempt, measurement protocol that came out of it.

## Notes and open questions

- Clocks and budgets above are retail stock values from SwitchBrew and port reports; per-model thermal throttling curves are unmeasured.
- Applet memory budgets vary by firmware and applet kind; this wiki does not pin a version-by-version table yet.
- Engineering code fragments are illustrative and not run on hardware; benchmarks proving draw-call and bandwidth limits on retail clocks are open work.
- SoC revisions (Erista vs. Mariko) share the same programming model; power differences are noted on the [models page](hardware/models.md).

## Sources

- [SwitchBrew](https://switchbrew.org/) — primary community technical reference for Tegra X1, kernel, IPC/SVC, services, filesystems.
- [libnx](https://github.com/switchbrew/libnx), [deko3d](https://github.com/switchbrew/deko3d), [switch-examples](https://github.com/switchbrew/switch-examples) — toolchain, libraries, and demo progression.
- [Atmosphere](https://github.com/Atmosphere-NX/Atmosphere), [Hekate](https://github.com/CTCaer/hekate) — CFW and bootloader behavior.
- [devkitPro](https://devkitpro.org/wiki/Getting_Started) — toolchain install.
- [NVIDIA Tegra X1 / Maxwell documentation](https://developer.nvidia.com/tegra-x1) — SoC composition, GPU rates, tuning guidance.
- [switch.homebrew.guide](https://switch.homebrew.guide/) — install and setup procedures (linked, not duplicated).
