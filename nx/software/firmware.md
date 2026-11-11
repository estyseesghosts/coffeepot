# NX firmware

Firmware is the low-level software that wakes the Switch up and hands control to the main system ([**firmware**](../../glossary.md#firmware)). It checks every stage of startup before running it, so a damaged or tampered stage stops the boot instead of starting a compromised system. System updates replace firmware and operating system components together, which is why the version number matters for both features and homebrew.

The chain starts in a permanent on-chip program (a **[boot ROM](../../glossary.md#boot-rom)**) inside the main chip (a **[system-on-a-chip](../../glossary.md#soc)**, or **[SoC](../../glossary.md#soc)**; here the **[Tegra X1](../glossary.md#tegra-x1)**), passes through small loading programs (**[bootloaders](../../glossary.md#bootloader)**), and ends by starting [Horizon](../../glossary.md#horizon) — see the [OS page](os.md).

## Behavior

Pressing power runs the chain automatically: the boot ROM loads the next loader, that loader loads the next, and each stage verifies the signature of the next before running it (a **[chain of trust](../../glossary.md#chain-of-trust)**) [reverse-engineered] (SwitchBrew Boot page). Late in the chain the system reads a boot configuration plus a firmware package called package2 from dedicated internal-flash partitions named BCPKG2, then starts Horizon [reverse-engineered] (SwitchBrew Boot / BCPKG2 pages). Two package2 titles exist per slot: `0100000000000819` / `010000000000081A`, plus an exFAT-driver pair `010000000000081B` / `010000000000081C` on consoles whose firmware bundles exFAT support [reverse-engineered] (SwitchBrew title-list pages).

Versions gate everything. Each firmware release may change bootloaders, package2, sysmodules, and the home-menu behavior together, and the console burns a hardware fuse on some updates so it can refuse to boot older, lower-fuse firmware afterwards [reverse-engineered] (SwitchBrew Fuses / System versions pages). That fuse mechanism is why downgrading is normally blocked even with a full flash backup: the old software no longer matches the fuse count. Keys needed to decrypt later stages live under TrustZone and are never exposed to games [reverse-engineered] (SwitchBrew Boot notes).

What players and developers touch day to day is not flash partitions but files. Internal flash (a form of **[NAND](../../glossary.md#nand)** flash) is partitioned with a standard table; the readable system and user areas use a common file layout (the **[FAT](../../glossary.md#fat)** family, FAT32) encrypted with AES-XTS [reverse-engineered] (SwitchBrew NAND / BIS partitions pages). Games never read raw flash: they ask the filesystem service for files, saves, and SD-card content. The SD card holds a `Nintendo/` folder with per-title content (large files split into shards) and an `Album/` folder with screenshots and video; save data lives in its own per-title directory [reverse-engineered] (SwitchBrew SD card / Filesystem pages).

## Technical details

Boot chain (simplified; stage names follow SwitchBrew):

| Order | Stage | What it does |
| ----- | ----- | ------------ |
| 1 | Boot ROM (Tegra X1 on-chip ROM) | Loads the first bootloader from flash; cannot be updated, so flaws here are permanent. |
| 2 | Early bootloaders | Initialize memory and load later stages; each checks the next signature. |
| 3 | Secure Monitor / exosphere (EL3) | Holds keys and the most privileged transitions; starts before Horizon. |
| 4 | BootConfig + package2 from BCPKG2 partitions | Provides kernel plus early sysmodules (`0819`/`081A`; exFAT pair `081B`/`081C` when present). |
| 5 | Horizon OS | Starts remaining sysmodules, applets, then the home menu or game. See the [OS page](os.md). |

All rows [reverse-engineered] (SwitchBrew Boot page). Exact loader names and offsets differ by SoC revision and firmware; this table states the order, not binary layouts.

Internal-flash partitions (BIS, GPT-partitioned) [reverse-engineered] (SwitchBrew NAND page):

| Partition | Contents |
| --------- | -------- |
| PRODINFO | Per-device calibration, keys, and identity. Never touched by games. |
| BCPKG2 (several slots) | BootConfig plus package2 images described above. |
| SAFE | System data kept across factory resets. |
| SYSTEM | System titles and configuration (FAT32, AES-XTS). |
| USER | Installed titles, saves metadata, user data (FAT32, AES-XTS). |

SD-card layout (plain language; canonical detail stays on SwitchBrew):

| Path | Contents |
| ---- | -------- |
| `Nintendo/Contents/` | Installed content, large files split into shards (NAX0). |
| `Nintendo/save/` (per-title save dirs) | Save data reached through the filesystem service, not direct file paths. |
| `Album/` | Screenshots and captured video. |

Rows [reverse-engineered] (SwitchBrew SD card pages).

Version and anti-downgrade behavior:

| Mechanism | Effect |
| --------- | ------ |
| Versioned boot + system titles | Old and new components are not mix-and-match; mismatched stages refuse to boot. |
| Fuse burning | Selected updates permanently advance a fuse count; bootloaders compare it against the firmware version and halt on mismatch. |
| exFAT titles (`081B`/`081C`) | Consoles without the exFAT update lack the driver pair; SDXC cards behave accordingly until the update is installed. |

All rows [reverse-engineered] (SwitchBrew Fuses / System versions pages).

> Unknown: a complete per-firmware table of what changed in each boot stage. SwitchBrew's version-history pages track this; this wiki summarizes the mechanism until a pinned table is researched.

Minimal crypto and packaging facts a developer needs (full formats live on SwitchBrew, not here): retail code ships in signed and encrypted NCA containers holding code modules (NSO) plus metadata (NPDM with ACID/ACI capabilities); homebrew builds NRO files instead and never mints NCAs [reverse-engineered] (SwitchBrew NCA, NSO, NPDM pages). Never read raw BIS partitions from game code; use the filesystem service — see the [services and IPC page](services-and-ipc.md).

## Homebrew notes

Early boot flaws are the most valuable targets because they run before Horizon hardens the system, and fuse burning is why keeping a low firmware matters to homebrew users (an **[exploit](../../glossary.md#exploit)** that works on one version may be closed — and blocked from downgrading — on the next). This page states the mechanism only; specific entry points live on [homebrew pages](../homebrew/toolchain-and-workflow.md), not here.

## Sources

- [SwitchBrew wiki](https://switchbrew.org/) — Boot, BCPKG2, NAND / BIS partitions, Fuses, System versions, NCA, NSO, NPDM, SD card pages for chain order, partition names, package2 title IDs, fuse behavior, and packaging roles.
- [libnx](https://github.com/switchbrew/libnx) — filesystem and SD-card access go through service wrappers (`include/switch/services/fs.h` and friends) [documented]; boot and fuse access are not exposed to normal titles by design.
