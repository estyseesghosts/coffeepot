# CTR OS (Horizon for 3DS)

Horizon is the operating system of the 3DS: it runs games, the Home Menu and background services, and keeps them isolated from each other. Players see the menu, applets and apps; developers see a kernel plus a layer of system services.

Nintendo calls this OS [Horizon](../../glossary.md#horizon), a name it reuses for the Switch's distinct OS — this page is only about the 3DS one. Unusually, the 3DS runs **two operating systems side by side** inside `NATIVE_FIRM`: one on each main processor.

## Behavior

### Two halves

| Half | Processor | Kernel | What it provides |
| ---- | --------- | ------ | ---------------- |
| Security half | [ARM9](../hardware/cpu.md) | Kernel9 + a single userland process, **Process9** | Cryptography (AES, RSA, SHA, ECDSA), storage and filesystem access, game-card access, title verification/installation, and the [PXI](../glossary.md#pxi) link to the ARM11 |
| User half | ARM11 | Kernel11, many processes | Multi-core scheduling, networking (HTTP, SSL, Nintendo online services), the Home Menu, launching applications, and the [services](services.md) that games call |

Both kernels are [microkernels](../../glossary.md#microkernel) providing memory management, scheduling and [inter-process communication](../../glossary.md#ipc). Kernel9 has no real privilege separation from Process9: Process9 may call a system call that runs arbitrary code in kernel mode. Together with ARM9-only hardware ([security hardware](../hardware/security-hardware.md)) that makes the ARM9 the console's "security processor".

The ARM11 kernel resides in 512 KB of [AXI WRAM](../glossary.md#axi-wram); Kernel9 and Process9 sit in ARM9-only RAM (1 MB; **New 3DS:** 1.5 MB).

### What games are allowed

Retail games run as sandboxed ARM11 processes and may use only a subset of the services; those in turn call more privileged services. The kernel hard-codes the roles of the first two cores ([kernel page](kernel.md#scheduling)): the game gets the appcore; the syscore belongs to the system, which lends the game a share of its time when asked (often quoted as about 30%; the exheader sets the maximum, up to 89% — see [kernel](kernel.md#scheduling)). Of the main memory, apps get only part: 64 MB of 128 MB on Old 3DS (**New 3DS:** 124 MB of 256 MB by default); a metadata flag (`APPMEMTYPE`) can claim more by making the console reboot into a layout that skips the Home Menu ([memory](../hardware/memory.md)).

### The shell: Home Menu, NS and applets

The **Home Menu** is a scrollable grid of installed applications; almost every service (camera, sound, eShop, settings) is an app launched from it. Process `PM` launches `NS`, which launches the Home Menu. `NS` also provides routines every app uses, such as the "Back to HOME Menu" overlay when the HOME button is pressed during a game; the game's own code handles the event but `NS` supplies the routine.

Games can also summon **applets** — small system programs — for things like the software keyboard. **New 3DS:** these shell routines still run with the full hardware even when a game runs in compatibility mode, giving unoptimised games a small speed boost.

### DS and GBA modes

No DS/DSi shell exists, only the old BIOS routines inside `TWL_FIRM` and `AGB_FIRM` ([firmware](firmware.md)). The DS Wi-Fi setup screens are the exception: DSi-style settings sync with the 3DS Wi-Fi settings at boot.

### Storage

Boot ROMs live in the SoC; per-console secrets live in OTP; system data in the internal eMMC [NAND](nand-filesystem.md); downloaded software and user data on an [SD card](sd-filesystem.md) (AES-128-CTR encrypted); and games on [cartridges](../hardware/gamecard.md).

## Technical details

| Subject | Where documented |
| ------- | ---------------- |
| Kernel objects, system calls, scheduler | [Kernel](kernel.md) |
| Service list | [Services](services.md) |
| Dynamic modules | [CRO modules](cro-modules.md) |
| StreetPass | [StreetPass (CEC)](streetpass-cec.md) |
| Region data | [Country/region archive](country-region-data.md) |
| Installed software, tickets, metadata | [Titles](titles.md) |
| Containers (NCSD, NCCH, CIA, DISA) | [File formats](file-formats.md) |
| Filesystem layout | [NAND](nand-filesystem.md), [SD](sd-filesystem.md), [saves](savegames.md) |
| Boot chain | [Boot process](boot-process.md), [firmware](firmware.md) |
| Signature and encryption chain | [Security architecture](security-architecture.md) |

### Startup of system processes

On `NATIVE_FIRM` boot the ARM11 kernel starts a fixed set of essential modules bundled in the firmware (`sm`, `fs`, `pm`, `loader`, `pxi`), then `PM` starts `NS`. Other system modules are separate [titles](titles.md) installed in NAND. Early Home Menu startup also launches several system modules manually.

### Online services

The OS integrates SpotPass (`boss`), StreetPass (`cecd`), Download Play (`dlp`), the eShop, and Miiverse-era services as system titles; online play and communication for the 3DS ended on 8 April 2024 (see [history](../homebrew/history.md)).

## Homebrew notes

Homebrew runs either inside a hijacked retail process ([userland exploits](../homebrew/exploits.md)) or, with [custom firmware](../homebrew/custom-firmware.md), as a normal system process with extra patches (signature checks removed, layered file system, in-game menu). The OS's lack of address-space randomisation, lack of downgrade protection and permissive kernel page tables are discussed in [security architecture](security-architecture.md).

## Sources

- [Rodrigo Copetti, Nintendo 3DS Architecture: Operating System](https://www.copetti.org/writings/consoles/nintendo-3ds/) — the two-OS design, roles of Kernel9/Process9/Kernel11, core policy, shell, updates.
- [3dbrew: Kernel](https://www.3dbrew.org/wiki/Kernel) — kernel responsibilities.
- [3dbrew: Services API](https://www.3dbrew.org/wiki/Services_API) — service overview.
- [3dbrew: Bootloader](https://www.3dbrew.org/wiki/Bootloader) — startup of PM/NS and Home Menu.
- [Hacks Guide Wiki: History of 3DS Hacking](https://wiki.hacks.guide/wiki/3DS:History_of_3DS_Hacking) — end of online services.
