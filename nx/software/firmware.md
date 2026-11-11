# NX firmware

Firmware is the low-level software that wakes the Switch up and hands control to the main system ([**firmware**](../../glossary.md#firmware)). It checks every stage of startup before running it, so a damaged or tampered stage stops the boot instead of starting a compromised system. System updates replace firmware and operating system components together, which is why the version number matters for both features and homebrew.

The chain starts in a permanent on-chip program (a **[boot ROM](../../glossary.md#boot-rom)**) inside the main chip (a **[system-on-a-chip](../../glossary.md#soc)**, or **[SoC](../../glossary.md#soc)**; here the **[Tegra X1](../glossary.md#tegra-x1)**), passes through small loading programs (**[bootloaders](../../glossary.md#bootloader)**), and ends by starting [Horizon](../../glossary.md#horizon) — see the [OS page](os.md).

## Behavior

Pressing power runs the chain automatically: the boot ROM loads the next loader, that loader loads the next, and each stage verifies the signature of the next before running it (a **[chain of trust](../../glossary.md#chain-of-trust)**) [reverse-engineered] (SwitchBrew Boot page). Late in the chain the system reads a boot configuration plus a firmware package called package2 from dedicated internal-flash partitions named BCPKG2, then starts Horizon [reverse-engineered] (SwitchBrew Flash Filesystem, oldid 13575; the SwitchBrew Boot page was not saved and was not re-read). Four firmware-package titles exist: `0100000000000819` (normal firmware) and `010000000000081A` (safe-mode firmware), plus exFAT variants `010000000000081B` and `010000000000081C`, which are identical to the first two except for a different filesystem sysmodule [reverse-engineered] (SwitchBrew Title list, oldid 15462).

Versions gate everything. Each firmware release may change bootloaders, package2, sysmodules, and the home-menu behavior together, and the console burns a hardware fuse on some updates so it can refuse to boot older, lower-fuse firmware afterwards. The secure monitor derives a fuse version from bits of the `FUSE_RESERVED_ODM4` fuse register [reverse-engineered] (SwitchBrew Secure Monitor, `FuseVersion`, edited 2026-11-06); Atmosphere's changelog speaks of "anti-downgrade fuse checks" in the stock warm-boot firmware [documented] (`Atmosphere` `docs/changelog.md` line 1366). The SwitchBrew Fuses and System versions pages were not saved and the per-update fuse counts were not re-checked. That mechanism is why downgrading is normally blocked even with a full flash backup. Key material is held under the secure monitor: for example the secure boot key is stored in `FUSE_PRIVATE_KEY` and locked so that it reads as all `FF` once the boot ROM finishes [reverse-engineered] (SwitchBrew Cryptosystem, oldid 13916).

What players and developers touch day to day is not flash partitions but files. Internal flash (a form of **[NAND](../../glossary.md#nand)** flash) is partitioned with a standard GPT table; the SAFE, SYSTEM and USER partitions are **[FAT](../../glossary.md#fat)**32 filesystems encrypted with AES-XTS [reverse-engineered] (SwitchBrew Flash Filesystem, oldid 13575). Games never read raw flash: they ask the filesystem service for files, saves, and SD-card content. Save data for **all non-system applications**, wherever the application is installed, lives in the internal USER partition, with a separate save image per user account [reverse-engineered] (same page, "USER"). The SD card holds a single top-level `Nintendo/` folder with `Album/` (screenshots and recorded video), `Contents/` (installed content, large files split into shards) and `save/`, which contains only two system save images, not per-game saves [reverse-engineered] (SwitchBrew SD Filesystem, oldid 13379).

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

Internal-flash partitions (BIS, GPT-partitioned) [reverse-engineered] (SwitchBrew Flash Filesystem, oldid 13575):

| Partition | Contents |
| --------- | -------- |
| PRODINFO | Raw `CAL0` calibration data for the individual console (encrypted with BIS key 0). Never touched by games. |
| PRODINFOF | FAT12 filesystem holding additional calibration data (BIS key 0). |
| BCPKG2-1 to BCPKG2-6 | Six raw partitions: normal main/sub, safe-mode main/sub and repair main/sub. Each holds a BootConfig (first 0x4000 bytes, usually empty) followed by a package2 image; "sub" is a backup of "main". The normal image comes from title `0100000000000819`, the safe-mode image from `010000000000081A`; the repair pair is installed at the factory and not written afterwards on retail. |
| SAFE | FAT32 filesystem, official name "SafeMode" (BIS key 1). |
| SYSTEM | FAT32: system titles and configuration, `save/` for **system** titles only (BIS key 2). |
| USER | FAT32: `Album/`, `Contents/` (installed titles), `save/` holding the saves of every non-system application, one save image per user account, plus `saveMeta/` and `temp/` (BIS key 3). |

SD-card layout [reverse-engineered] (SwitchBrew SD Filesystem, oldid 13379):

| Path | Contents |
| ---- | -------- |
| `Nintendo/Album/` | Screenshots as `YYYY/MM/DD/<DateTime>-<title ID string>.jpg` (MAC-protected), recorded video in `Extra/`. |
| `Nintendo/Contents/` (`placehld/`, `registered/`) | Installed content in NAX0 archives; content over 4 GB is split into numbered files even on exFAT. |
| `Nintendo/save/` | **System-only** save data: `8000000000000000` (describes the other system saves on the SD card) and `8000000000000124` (describes what content is installed to the SD card). Ordinary game saves are not stored here. |

Homebrew files live outside `Nintendo/`, conventionally under `/switch/` (see [toolchain and workflow](../homebrew/toolchain-and-workflow.md)).

Version and anti-downgrade behavior:

| Mechanism | Effect |
| --------- | ------ |
| Versioned boot + system titles | Old and new components are not mix-and-match; mismatched stages refuse to boot. |
| Fuse burning | Selected updates permanently advance a fuse count; bootloaders compare it against the firmware version and halt on mismatch. |
| exFAT titles (`081B`/`081C`) | Consoles without the exFAT update lack the driver pair; SDXC cards behave accordingly until the update is installed. |

Rows describe the behaviour summarised above; the SwitchBrew Fuses and System versions pages were not saved and were not re-read.

> Unknown: a complete per-firmware table of what changed in each boot stage. SwitchBrew's version-history pages track this; this wiki summarizes the mechanism until a pinned table is researched.

Minimal crypto and packaging facts a developer needs (full formats live on SwitchBrew, not here): retail code ships in signed and encrypted NCA containers holding code modules (NSO) plus metadata (NPDM with ACID/ACI capabilities); homebrew builds NRO files instead and never mints NCAs (SwitchBrew NCA, NSO and NPDM pages; not saved, not re-read). Never read raw BIS partitions from game code; use the filesystem service — see the [services and IPC page](services-and-ipc.md).

## Homebrew notes

Early boot flaws are the most valuable targets because they run before Horizon hardens the system, and fuse burning is why keeping a low firmware matters to homebrew users (an **[exploit](../../glossary.md#exploit)** that works on one version may be closed — and blocked from downgrading — on the next). This page states the mechanism only; specific entry points live on [homebrew pages](../homebrew/toolchain-and-workflow.md), not here.

## Sources

- SwitchBrew [Flash Filesystem](https://switchbrew.org/wiki/Flash_Filesystem) (oldid 13575, edited 2025-07-13) — BIS partitions, BCPKG2 slots, where save images live.
- SwitchBrew [SD Filesystem](https://switchbrew.org/wiki/SD_Filesystem) (oldid 13379, edited 2025-06-07) — `Nintendo/` layout, system-only `save/`.
- SwitchBrew [Title list](https://switchbrew.org/wiki/Title_list) (oldid 15462, edited 2026-11-10) — firmware-package title IDs `0819` to `081C`.
- SwitchBrew [Secure Monitor](https://switchbrew.org/wiki/Secure_Monitor) (edited 2026-11-06) and [Cryptosystem](https://switchbrew.org/wiki/Cryptosystem) (oldid 13916) — fuse version bits and key handling.
- [Atmosphere](https://github.com/Atmosphere-NX/Atmosphere) at `4e3f37038d9e82dde7054897213ab32b2ecf651f` — `docs/changelog.md` (anti-downgrade checks), `README.md`.
- [libnx](https://github.com/switchbrew/libnx) at `b156b4774fb2df7b93402169509266da7547fc4c` — filesystem and SD-card access go through service wrappers (`nx/include/switch/services/fs.h`) [documented].
- SwitchBrew Boot, Fuses, System versions, NCA, NSO and NPDM pages — referenced for the chain-of-trust description and packaging roles; not saved with the research sources and not re-verified here.
