# Atmosphere and running homebrew

This page is for anyone who has a compiled homebrew file and wants to run it on the Nintendo Switch family (**NX**). It works on consoles that can load modified system software (**[custom firmware](../../glossary.md#cfw)**, or **CFW**) — unpatched units via the RCM bug, or patched / OLED units with a modchip — and the outcome is booting Atmosphere, opening the homebrew menu with full game-class memory, and knowing what can get the console banned or bricked.

This page explains *what* Atmosphere does and *how to run* under it. It does not duplicate install steps: follow [switch.homebrew.guide](https://switch.homebrew.guide/) for wiring, payloads, and SD layout.

## What you need

- A compatible console. Early Erista units can be started through the RCM USB mode with a payload and no hardware modification; later Erista units and all Mariko units, including the Switch Lite and OLED, need a modchip or another entry point. The exact serial-number ranges are on the guide linked above and were not re-checked here (see [models](../hardware/models.md)). `[reported]` (old Homebrew Guide, `gettingstarted/checkingrcm.rst`, 2022; the fail0verflow write-up named in Sources was not saved).
- Current Atmosphere + matching Hekate (or other bootloader) release on the SD card, per the guide linked above.
- One SD card reserved for CFW use; ideally FAT32 or exFAT as the guide recommends for your setup.
- No Nintendo account sign-in required for offline homebrew; linking accounts and going online increases ban exposure (see Limits and risks).

## How it works

Atmosphere is a set of cooperating parts that patch Nintendo's startup and system services in memory so homebrew can run alongside (or instead of) normal games.

At a high level: a bootloader payload runs first, sets up a safe environment, then loads patched system modules that lift restrictions (allow unsigned code, redirect loader paths, emulate the internal flash when requested). After boot the console looks normal, but holding R while launching a title hands the full game slot to the homebrew menu instead of the game.

Components `[documented]` (`Atmosphere` `README.md`, "Components", commit `4e3f370`; directory names from the repository tree):

| Part | Role |
| ---- | ---- |
| Fusée | First-stage loader. Loads and validates the custom secure monitor and package2 (kernel and boot sysmodules) and patches them; replaces the stock first-stage loaders. |
| Exosphère | Customised secure monitor (TrustZone). |
| Thermosphère | An EL2 hypervisor. The README describes its role as EmuNAND support, "backing up and using virtualized/redirected NAND images". |
| Stratosphère | Custom sysmodules: those that extend the kernel and those that reimplement or hook system actions. Includes `loader`, `sm`, `pm`, `fs`, `ncm`, `dmnt` (cheats and debugging) and `ams_mitm` (see below). |
| Troposphère | "Application-level Horizon OS patches". In the repository it holds small homebrew applications (`daybreak`, `haze`, `reboot_to_payload`). |
| emuMMC | Separate project that emulates the internal flash from files on the SD card (`emummc/` in the repository; `docs/components/emummc.md`). |

`ams_mitm` contains the man-in-the-middle service patches `fs_mitm` (file redirection, used for LayeredFS overlays), `ns_mitm`, `set_mitm`, `bpc_mitm` and `dns_mitm` (`stratosphere/ams_mitm`). Earlier text on this page placed `dns.mitm` and cheats under Troposphère; they are Stratosphère components.

The two running modes from a game developer's view:

- Applet hbmenu (Album): fast to open, small memory budget. Fine for utilities.
- Title override (hold R on a title launch): the homebrew menu is launched in place of the game you started, so it runs as an application instead of as the Album applet. Required for real game testing. The key is configured in `override_config.ini`: the template's "any application" entry uses `override_any_app_key=R`, and a leading `!` inverts it so that holding the key launches the real game instead `[documented]` (`Atmosphere` `config_templates/override_config.ini` and `docs/features/configurations.md`).

How much memory that gives is a port report, not an Atmosphere document: NFSMW's notes say one program running this way saw about 3,189 MB of heap at start `[reported]` (`nfsmw-nx` `docs/platform-notes.md`, "In short", commit `20f8dc1`), and ports require title mode because the Album applet's budget is too small `[reported]` (`srb2-switch` `README.md`; `SwitchWakerHD` `README.md`). This wiki has no per-firmware table of the budgets.

emuMMC (emulated internal flash, backed by files or a partition on the SD card) optionally redirects all internal-flash writes to an SD image so experiments never touch the real system partition. Use it when testing installs, system mods, or anything that writes outside `/switch/`.

## Technical details

Entry paths differ by hardware. The RCM mode is a boot-ROM feature that lets a connected PC send a payload; early Erista units accept an unsigned one. AutoRCM is a setting that makes the console enter RCM on every boot. Atmosphere's changelog records a fix for "a final edge case where AutoRCM might be removed by HOS, which could cause a user to burn fuses" `[documented]` (`Atmosphere` `docs/changelog.md` line 1236), which shows that the official system software can remove AutoRCM and that doing so is tied to fuse burning. The details of the bug are in the fail0verflow write-up named in Sources, which was not saved with the other sources.

| Hardware | Entry | Persistence |
| -------- | ----- | ----------- |
| Early Erista (RCM-vulnerable) | USB RCM payload (Fusée or Hekate) from a jig and a PC or injector, no hardware modification. A cold boot returns to stock unless AutoRCM is set. | Tethered, or AutoRCM; no flash rewrite needed. |
| Later Erista, Mariko, Lite, OLED | Modchip that glitches the boot to load Hekate or Atmosphere, or another entry point from the guide. | The chip stays installed; the stock system can still be booted through the bootloader menu. |

`[reported]` (old Homebrew Guide, `gettingstarted/`, 2022, and the Atmosphere and Hekate documentation). Which units are vulnerable is a manufacturing-date question the guide answers; this wiki does not reproduce it.

What the loader patching changes for developers:

- Title override launches the homebrew menu in the place of an application, so the program runs with application rather than applet limits (see the memory reports above).
- The address-space width of the homebrew menu and the programs it launches is configurable per override with `override_address_space` (`39_bit`, `36_bit` or `32_bit`); when unset it defaults to 39 bits `[documented]` (`Atmosphere` `docs/changelog.md` lines 738-743: "If a configuration has not been set, hbl will now default to 39-bit address space. Previously, a legacy 36-bit address space was always used"; `config_templates/override_config.ini`). The kernel recognises 32-, 36-, 39- and 42-bit process address-space types (`libraries/libvapours/include/vapours/svc/svc_types_common.hpp`, `CreateProcessParameterFlag_AddressSpace*`). Compare the 38-bit figure SwitchBrew gives for system version 2.0.0 and later on [NX memory](../hardware/memory.md#address-space-widths).
- LayeredFS (`fs_mitm`) overlays SD-card files on a game's content for mods, without repacking (`stratosphere/ams_mitm/source/fs_mitm`).
- `dns_mitm` by default redirects resolution of Nintendo's official telemetry servers to a loopback address, and can be given a hosts file `[documented]` (`Atmosphere` `docs/features/dns_mitm.md`). Whether this reduces ban risk is not something that document claims.

Cheats and overclocking sysmodules attach to the same services games use. They are convenient for testing; community reports say they can leave traces that matter for bans, but this page has no source that measures it `[hypothesis]`.

This page keeps no local copy of bootloader wiring, fuse counts or SD file trees; those change per release and live in the guide and release notes linked below.

## Limits and risks

Be blunt: any of these can cost a console, an account or a save.

- **Bans.** Community reports say piracy, unsigned installs, downloads from Nintendo's servers while modified and online cheating lead to bans `[reported]` (community ban reports; consoles, firmware versions and dates vary and were not reproduced by this wiki, and no report was saved with the sources). Offline `.nro` homebrew is the lowest-risk shape, not zero-risk.
- **Fuses and downgrades.** Nintendo's system updates burn anti-downgrade fuses and the bootloader compares them against the installed system; a system older than the fuse count will not boot. The secure monitor reads a fuse version from `FUSE_RESERVED_ODM4` `[reverse-engineered]` (SwitchBrew "Secure Monitor", `FuseVersion`, edited 2026-11-06). Atmosphere ships a replacement warm-boot firmware that skips the anti-downgrade check, which fixes sleep mode on a downgraded NAND `[documented]` (`Atmosphere` `docs/changelog.md` lines 1366-1367). Do not downgrade as a way to avoid a ban.
- **Data loss.** Misconfiguration, forced shutdown during internal-flash writes and unreliable SD cards can corrupt the NAND or the SD filesystem. Keep a NAND backup and keep emuMMC on a reliable card.
- **Overclocking.** `[reported]` that sustained overclocking and overvolting shortens chip life and can corrupt storage on unstable power (sys-clk documentation and user reports; not reproduced here; no source was saved). Profile at stock clocks first (see [performance and porting](../engineering/performance-and-porting.md)).
- **System updates break custom firmware.** A system update can change services faster than Atmosphere is patched. After updating, wait for an Atmosphere release that supports that version. The current Atmosphere release in the pinned tree is 1.12.2 `[documented]` (`docs/changelog.md`).

If a console must stay clean for online play, keep a stock system partition separate from the emuMMC used for homebrew `[reported]` (guide convention).

## Sources

- [Atmosphere](https://github.com/Atmosphere-NX/Atmosphere) at `4e3f37038d9e82dde7054897213ab32b2ecf651f` — `README.md` (components), `docs/changelog.md`, `docs/features/configurations.md`, `docs/features/dns_mitm.md`, `config_templates/override_config.ini`, `stratosphere/ams_mitm`, `libraries/libvapours/include/vapours/svc/svc_types_common.hpp`.
- [Hekate](https://github.com/CTCaer/hekate) — what bootloader menus, boot entries, and NAND/emuMMC tooling cover.
- [switch.homebrew.guide](https://switch.homebrew.guide/) — what install, SD layout, and update procedures cover (linked, not duplicated).
- [fail0verflow ShofEL2 write-up](https://fail0verflow.com/blog/2018/shofel2/) — the boot-ROM flaw behind RCM entry (named, not saved or re-read).
- [noahc3 Homebrew Guide](https://github.com/noahc3/Homebrew-Guide) at `6d7b7bc` (2022, archived) — RCM and Hekate procedures as background.
- [nfsmw-nx](https://github.com/StevensND/nfsmw-nx) at `20f8dc1`, [srb2-switch](https://github.com/P-AS/srb2-switch) at `90194061a`, [SwitchWakerHD](https://github.com/centollOS/SwitchWakerHD) at `9aec4e3` — reports on title-override memory.
- SwitchBrew [Secure Monitor](https://switchbrew.org/wiki/Secure_Monitor) (edited 2026-11-06) — fuse version bits.
