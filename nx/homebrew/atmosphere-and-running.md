# Atmosphere and running homebrew

This page is for anyone who has a compiled homebrew file and wants to run it on the Nintendo Switch family (**NX**). It works on consoles that can load modified system software (**[custom firmware](../../glossary.md#cfw)**, or **CFW**) — unpatched units via the RCM bug, or patched / OLED units with a modchip — and the outcome is booting Atmosphere, opening the homebrew menu with full game-class memory, and knowing what can get the console banned or bricked.

This page explains *what* Atmosphere does and *how to run* under it. It does not duplicate install steps: follow [switch.homebrew.guide](https://switch.homebrew.guide/) for wiring, payloads, and SD layout.

## What you need

- A compatible console: unpatched (RCM-exploitable, roughly June 2017–mid 2018 serials) needs only a USB jig/payload sender; patched and OLED units need a modchip [documented] (fusee-gelee write-up and Hekate/Atmosphere docs list the RCM bug as fixed in later hardware).
- Current Atmosphere + matching Hekate (or other bootloader) release on the SD card, per the guide linked above.
- One SD card reserved for CFW use; ideally FAT32 or exFAT as the guide recommends for your setup.
- No Nintendo account sign-in required for offline homebrew; linking accounts and going online increases ban exposure (see Limits and risks).

## How it works

Atmosphere is a set of cooperating parts that patch Nintendo's startup and system services in memory so homebrew can run alongside (or instead of) normal games.

At a high level: a bootloader payload runs first, sets up a safe environment, then loads patched system modules that lift restrictions (allow unsigned code, redirect loader paths, emulate the internal flash when requested). After boot the console looks normal, but holding R while launching a title hands the full game slot to the homebrew menu instead of the game.

Components [documented] (readable in the Atmosphere repository tree):

| Part | Role |
| ---- | ---- |
| Fusee | Primary payload / bootloader. Loads the rest, applies early patches. |
| Exosphere | Secure-monitor layer. Owns fuses, power, and TrustZone-level configuration. |
| Thermosphere | Hypervisor support layer. Isolates privileged worlds. |
| Stratosphere | System-module patches: `loader` (allow `.nro` / unsigned titles, title override with full memory), `sm` (service access), `pm`, `fs.mitm` / `ns.mitm` (redirection, LayeredFS mods). |
| Troposphere | Application-level patches: homebrew menu integration, `dns.mitm` (network filtering), cheats, sysmodule helpers. |

The two running modes from a game developer's view:

- Applet hbmenu (Album): fast to open, small memory budget. Fine for utilities.
- Title override (hold R on a title launch): hbmenu inherits the game's memory, process priority, and GPU access. Required for real game testing. This is the full-app-memory-via-override behaviour patched in by Atmosphere's `loader` [documented] (Atmosphere `loader` source and release notes).

emuMMC (emulated internal flash on SD) optionally redirects all internal-flash writes to an SD image so experiments never touch the real system partition. Use it when testing installs, system mods, or anything that writes outside `/switch/`.

## Technical details

Boot paths differ by hardware [documented] (Atmosphere/Hekate docs; fusee-gelee write-up for the RCM bug as a **[reverse-engineered](../../glossary.md#chain-of-trust)** boot-ROM flaw):

| Hardware | Entry | Persistence |
| -------- | ----- | ----------- |
| Unpatched RCM unit | USB RCM payload (Fusee/Hekate) via jig + injector, no hardware mod. Coldboot returns to stock unless AutoRCM is set. | Tethered or AutoRCM; no flash rewrite needed. |
| Patched / OLED + modchip | Chip glitches boot to load Hekate/Atmosphere. | Chip remains; stock boot possible via bootloader menu. |

What `loader` patching changes for developers [documented] (Atmosphere `stratosphere/loader` source):

- Title override launches get application-class heap and thread counts; applet launches do not.
- 39-bit virtual address space is available to forwarders / large homebrew under override, easing large contiguous mappings.
- `fs.mitm` LayeredFS overlays SD files over base-game content for mods without repacking.
- `dns.mitm` + `exosphere` blanking options filter Nintendo telemetry hosts; they reduce but do not eliminate ban risk.

Cheats and overclock sysmodules attach to the same service surface games use. They are convenient for testing but leave traces (error logs, abnormal play telemetry) that matter for bans.

This page keeps no local copy of bootloader wiring, fuse counts, or SD file trees; those change per release and live in the guide and release notes linked below.

## Limits and risks

Be blunt — any of these can cost a console, an account, or a save:

- Bans. Piracy, unsigned package (`.nsp`) installs, CDN downloads, and online cheating cause fast bans [reported] (community ban reports, ongoing; console models and firmware versions vary as stated by reporters; not reproduced by this wiki). Heavily modded online play and abnormal telemetry cause delayed bans even without piracy. Offline `.nro` homebrew is the lowest-risk shape, not zero-risk.
- Fuses and downgrades. Each official boot burns anti-downgrade fuses; downgrading below the fuse count without bypasses bricks boot [documented] (Hekate/Atmosphere docs on fuse checks). Do not downgrade to dodge a ban.
- Data loss. Misconfiguration, forced shutdowns during internal-flash writes, and cheap/fake SD cards corrupt NAND or the SD filesystem. Keep a NAND backup and keep emuMMC images on reliable cards.
- Overclocking damage. Sustained overclock + overvolt shortens chip life and can corrupt SD/NAND on bad power [reported] (sys-clk docs and user reports; conditions as stated, not reproduced here). Profile at stock clocks first.
- Updates undo CFW behaviour. A system update can change services faster than Atmosphere patches; after updating, wait for a matching Atmosphere release before booting CFW.

Going online with any CFW traces present (error-report logs, title-activity records) is the common ban vector. If the console must stay clean for online play, keep a stock sysNAND separated from the emuMMC used for homebrew.

## Sources

- [Atmosphere](https://github.com/Atmosphere-NX/Atmosphere) — what Fusee / Exosphere / Thermosphere / Stratosphere / Troposphere cover (boot, patches, `loader` override, `dns.mitm`, cheats).
- [Hekate](https://github.com/CTCaer/hekate) — what bootloader menus, boot entries, and NAND/emuMMC tooling cover.
- [switch.homebrew.guide](https://switch.homebrew.guide/) — what install, SD layout, and update procedures cover (linked, not duplicated).
- [fusee-gelee write-up](https://fail0verflow.com/blog/2018/shofel2/) — what the unpatched-RCM boot-ROM flaw covers and which hardware it applies to.
- [SwitchBrew](https://switchbrew.org/wiki/Main_Page) — what system-service and firmware-version background covers.
