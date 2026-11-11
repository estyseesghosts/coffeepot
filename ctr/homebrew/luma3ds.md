# Luma3DS

**[Luma3DS](../glossary.md#luma3ds)** is the standard 3DS custom firmware. It "patches and reimplements significant parts of the system software" on every 3DS-family model and aims to keep the console useful past the end of Nintendo's support. [boot9strap](boot9strap.md) is its officially supported loader: boot9strap gives it control at power-on, and Luma3DS then loads, patches and starts the real operating system. Other FIRM bootloaders exist, but LumaTeam does not guarantee their compatibility.

Who it is for: players who want homebrew, game modding and debugging features, and developers who want a GDB debugger and a system they can extend. This page describes how it is built and what it changes, from its public source repository (GPLv3).

## Console setup and development

For console modification, updating an existing setup and recovery, follow the [3DS Hacks Guide](https://3ds.hacks.guide/). Once your console is ready, the [development workflow](development-workflow.md) covers the toolchain and running your own programs. Luma3DS's [upstream repository](https://github.com/LumaTeam/Luma3DS) documents its source and builds.

## How it works

### Components

| Component | Runs on | Role |
| --------- | ------- | ---- |
| `arm9`, `arm11` | Bare metal at boot | The settings menu, the chainloader and the firmware loader. They **patch the official firmware** (the loaded `NATIVE_FIRM` image, in memory) to modify `Process9` and to inject everything else. This was the first part written (2015) |
| `k11_extension` | Kernel11 | Code injected into the ARM11 kernel by hooking its start-up. It hooks system calls, adds new ones, and hooks inter-process communication to bypass limits in Nintendo's design |
| `sysmodules/loader` | Process | Reimplementation of the `loader` module that loads non-KIP processes from storage. It is the place where all process patches happen, enabling game modding, and it loads 3DSX homebrew (introduced 2016) |
| `sysmodules/rosalina` | Process | Custom module: overlay menu, GDB server, reimplemented `err:f` (fatal error screen) and more (mid-2017) |
| `sysmodules/pxi` | Process | ARM11↔ARM9 communication module, reimplemented (late 2017) |
| `sysmodules/sm` | Process | Service manager, reimplemented to remove service-access restrictions (late 2017) |
| `sysmodules/pm` | Process | Process manager, reimplemented to start/terminate processes and tell `loader` to load them; enables break-on-start debugging and lifts filesystem access restrictions "the proper way" (2019) |

KIP means "kernel initial process": processes loaded directly into memory by the kernel (see [OS](../software/os.md), [boot process](../software/boot-process.md)).

### What the firmware patcher changes

Names are the patch functions in `arm9/source/patches.c`. Each works on the loaded firmware image in memory. The one-line descriptions here are inferred from the function names and surrounding code, not from documentation; the README only states that the loader patches `Process9` and injects the other components.

| Area | Patches |
| ---- | ------- |
| Signatures and installs | `patchSignatureChecks`, `patchOldSignatureChecks` (title/CIA signature checks), `patchTitleInstallMinVersionChecks` (allow installing older versions), `patchZeroKeyNcchEncryptionCheck`, `patchNandNcchEncryptionCheck`, `patchCheckForDevCommonKey`, `patchP9AMTicketWrapperZeroKeyIV` |
| Firmware safety | `patchFirmlaunches` (adjusting the "firmlaunch" reboot calls), `patchFirmWrites`/`patchOldFirmWrites` (protect `firm0`/`firm1` from being overwritten) |
| Kernel | `installK11Extension`, `patchKernel11`, `patchK11ModuleLoading`, `patchK11ModuleLoadingLgy` (allow extra initial processes), `patchKernel9Panic`, `patchSvcBreak9`, `patchArm9ExceptionHandlersInstall`, `patchP9AccessChecks` |
| Hardware identity | `patchUnitInfoValueSet` (pretend to be a development unit when configured), prototype-hardware helpers |
| DS/GBA compatibility | `patchLgySignatureChecks`, `patchTwlInvalidSignatureChecks`, `patchTwlNintendoLogoChecks`, `patchTwlWhitelistChecks` (cartridge whitelist), `patchTwlFlashcartChecks`, `patchTwlShaHashChecks`, `patchAgbBootSplash`, `patchTwlBg` (screen filter for DS mode) |

### Features and files

- **Boot menus:** press Select at boot for the configuration menu (`/luma/config.ini` on the SD card, or `/rw/luma/config.ini` on CTRNAND when Luma runs from NAND, which happens when the SD card is missing); press Start for the **chainloader** menu for `.firm` payloads in `/luma/payloads` (payload filenames like `x_test.firm` give hotkeys).
- **[Rosalina](../glossary.md#rosalina)** (default combination L+Down+Select): screenshots in game, blue-light and colour screen filters, input redirection for external controllers, cheat codes, network time, process list with memory viewer, miscellaneous and system settings, New 3DS CPU options.
- **Game modding:** game plugins in `3GX` format (plugin loader added in v13.0 by PabloMK7), per-game language override ("locale emulation"), **[LayeredFS](../glossary.md#layeredfs-3gx)** (replacing game assets by path redirection), and user-provided patches or full system-module replacements — important for Nintendo Network replacement projects.
- **GDB stub:** ports 4000–4002 (normal), 4003 for break-on-start; `attach` in extended-remote mode with `info os processes`; `monitor getmemregions` helps reverse engineering. Works with devkitARM's GDB and IDA Pro. Walk-through: [debugging](../engineering/debugging.md#gdb-with-rosalina).
- **Chainloading:** other `.firm` payloads, including other versions of Luma3DS.
- **Config options (INI):** boot — `autoboot_emunand`, `enable_external_firm_and_modules`, `enable_game_patching`, `app_syscore_threads_on_core_2`, `show_system_settings_string`, `show_gba_boot_screen`; misc — `use_dev_unitinfo`, `enable_dsi_external_filter`, `disable_arm11_exception_handlers`, `enable_safe_firm_rosalina`; others include `default_emunand_number`, `brightness_level`, `splash_position` and `splash_duration_ms`, a PIN lock, `app_launch_new_3ds_cpu` (`off`, `clock`, `l2`, `clock+l2` — the New 3DS options from [CPU](../hardware/cpu.md#new-3ds-compatibility-layer)) and `autoboot_mode` (including a DSi option). Some settings are stored in a free area of the MCU's registers.

### emuNAND support

Luma3DS locates an **[emuNAND](../glossary.md#emunand)** on the SD card by looking for the NCSD header at the expected position and checks three sector layouts: *legacy* (sector offset `0x400000`, or `0x200000` when the card's NAND-size value is small), *default* (offset rounded up to a 4 MB multiple past the real NAND size) and *minsize* (`0x1D8000` on Old 3DS, `0x26E000` on New 3DS sectors); multiple emuNANDs are selected by index. It also recognises the original *RedNAND* layout (an NCSD at `+1` sector). Locating the SD-card controller structure uses code by Normmatt.

### Homebrew loading (`hb:ldr`)

The reimplemented `loader` also loads `.3dsx` homebrew. It exposes an `hb:ldr` service (commands `SetTarget` and `SetArgv`; the older `LoadProcess` and `PatchExHeaderInfo` commands were removed). When the designated title is launched — `000400000D921E00`, named `hblauncher_loader`, unless the user picked another with Rosalina's *Switch the hb. title to the current app* — the loader loads the target 3DSX (default `sdmc:/boot.3dsx`, the Homebrew Menu) into that process instead. The 3DSX gets a generated extended header: process name `3dsx_app`, core 0 at priority `0x30`, the current memory mode, a core-1 CPU-time limit of up to 89% (the resource-limit field is set to 89 with the preemption bit, but per the source comment the effective default is 0 until the program calls `APT_SetAppCpuTimeLimit`), New 3DS core-2 access, all filesystem and service access, and mappings for all ARM11 I/O, DSP RAM (read/write) and VRAM (read-only); the **New 3DS:** 804 MHz and L2-cache flags are left off [documented: `sysmodules/loader/source/hbldr.c`]. Developer consequences are on [development workflow](development-workflow.md#5-run-it) and [performance](../engineering/performance.md).

### Debugging features

Rosalina's GDB server (ports 4000–4003, up to three debugged processes at once, monitor commands such as `getmemregions`, `syncrequestinfo` and `catchsvc`), its fatal-error screen (ErrDisp), and the ARM11/ARM9 exception screens that save crash dumps to `/luma/dumps/arm11` and `/luma/dumps/arm9` are described from a developer's point of view on [debugging](../engineering/debugging.md).

### Kernel extensions

`k11_extension` adds or replaces supervisor calls so that userland code with the right permissions can do things the stock kernel forbids. Files in its `svc` folder include `ControlMemory`/`ControlMemoryUnsafe`, `ControlProcess`, `ControlService`, `CopyHandle`, `CreateThread`, `GetHandleInfo`/`GetProcessInfo`/`GetSystemInfo`/`GetThreadInfo`, `KernelSetState`, `MapProcessMemoryEx`/`UnmapProcessMemoryEx`, `SendSyncRequest`, `SetGpuProt`, `SetWifiEnabled`, `TerminateProcess`, `TranslateHandle`, `Backdoor`/`CustomBackdoor` ([kernel](../software/kernel.md)).

### Rosalina menu map

Top-level entries of the Rosalina overlay menu, from `sysmodules/rosalina/source/menus.c` and `menus/*.c` [documented: Luma3DS source]. Entries marked "if..." are shown only conditionally.

| Entry | What it contains |
| ----- | ---------------- |
| Take screenshot | Saves the screens; the result screen reports conversion and file-write time in ms |
| Screen filters... | Colour-temperature presets from 6500K (default) down to 1200K ("Ember") and up to 10000K ("Aquarium"), two "enhance colours" entries recommended for IPS panels, and advanced configuration |
| Cheats... | Cheat-code engine |
| New 3DS menu... (only on a **New 3DS**) | "Enable/Disable L2 cache", "Set clock rate to 268MHz / the higher rate", and, unless the QTM service is unavailable or the model is a New 2DS XL: "Temporarily disable Super-Stable 3D", "Test parallax barrier positions", "Super-Stable 3D calibration" |
| Process list | Processes with a memory viewer |
| Debugger options... | Enable or disable the GDB debugger; force-debug the next application at launch |
| System configuration... | Volume-slider override, Wi-Fi control and toggle, LED master-brightness override, audio routing (auto, force headphones, force speakers), power-button toggle, card-slot power toggle, screen brightness |
| Miscellaneous options... | Switch the Homebrew title to the current app, change the menu combo, start InputRedirection, update time and date over NTP, nullify the user time offset, **Dump DSP firmware**, show temperature in Fahrenheit |
| Save settings | Writes the settings (most Rosalina settings are not saved automatically) |
| Return to HOME Menu; Power off / reboot; System info; Credits; Debug info | System info shows screen types, kernel version, MCU firmware version, PMIC and battery vendor; Debug info (shown only when a visibility check passes; the condition was not traced) shows the kernel-extension physical address range, SD and NAND bus speeds and the application memory type (`APPMEMTYPE`) |

Facts worth knowing as a developer:

- **DSP firmware.** *Dump DSP firmware* writes `/3ds/dspfirm.cdc` on the SD card, the file that libctru's `ndsp` needs for any audio ([audio](../engineering/audio.md)). The `3ds-examples` audio README tells homebrew users to obtain it this way; an emulator that emulates the DSP at a high level accepts an empty file, per that README ([3ds-examples catalogue](3ds-examples.md#audio)).
- **InputRedirection** streams a computer or controller's input to the console; its socket binds UDP port **4950** (`input_redirection.c`) and runs in a Rosalina thread on the system core. The protocol is not documented in the sources read here.
- **NTP** sync uses a hard-coded server address (`51.137.137.111`, commented `time.windows.com`), so it can break if that address changes [documented: `ntp.c`].
- **Game plugins (3GX).** The plugin loader looks for `/luma/plugins/<16-hex-digit title ID>` (a folder of `.3gx` files) and falls back to `/luma/plugins/default.3gx`; the file magic is the ASCII text `3GX$0002`, and a mismatched version is reported as outdated plugin or outdated loader. The loader reads `/luma/plugins/user_param.bin` for parameters and enables with the `plugin_loader_enabled` config flag [documented: `plugin/file_loader.c`, `plugin/3gx.c`, `plgloader.c`, `arm9/source/config.c`]. A cheat engine crash on thread-heavy games is why plugins exist ([limits](#limits-and-risks)).
- **Old 2DS note.** The README's roadmap says the planned `TwlBg`/`AgbBg` reimplementation would give better upscaling for DS and GBA games "except on Old 2DS"; this is the only Old-versus-New statement in the README apart from the New 3DS menu above.

## Technical details

- Licence: GPLv3 (the GDB stub is triple-licensed MIT/GPLv2+; the `pm`, `sm`, `pxi` reimplementations are MIT).
- Maintainers: TuxSH (lead, joined 2016), Aurora Wright (author, created the project in 2015, now inactive), PabloMK7 (plugin loader, joined 2023).
- Credits (README and Rosalina credits screen): devkitPro (fincs, WinterMute, mtheall) for the toolchain; Nanquitas for the first game plugin loader and GDB stub contributions; piepie62 for the current Rosalina cheat engine (Duckbill for the original); panicbit for the original screen filters; jasondellaluce for LayeredFS; LiquidFenrir for the process-list memory viewer; ChaN for FatFs; Stary for the networking code, basic GDB functionality and InputRedirection (proof of concept by ShinyQuagsire).
- GDB clients: devkitARM-patched GDB and IDA Pro (without "stepping support" enabled) are the actively supported ones.
- Roadmap (from the README): reimplement `TwlBg`/`AgbBg` (better, configurable upscaling for DS and GBA games, "except on Old 2DS" per the README; developed privately, no ETA), reimplement `Process9` for `TWL_FIRM`/`AGB_FIRM`, and eventually all of `Kernel11`.

## Limits and risks

- **Known issues:** the cheat engine crashes some applications, particularly Pokémon games, because of a race condition in Nintendo's Kernel11 (attaching a `KDebugThread` to a `KThread` on creation while another thread dereferences `thread->debugThread`); such games need a dedicated game plugin instead. The Rosalina combination is polled at intervals, so a game can see the same button press; change the combination in "Miscellaneous options" and save settings.
- It is a patched system: bugs in a patch can crash the console. For backups and recovery, follow the [3DS Hacks Guide](https://3ds.hacks.guide/).
- The project wiki is described as "very outdated" by its authors.

## Sources

- [Luma3DS (LumaTeam)](https://github.com/LumaTeam/Luma3DS) — README, component layout, patch functions (`arm9/source/patches.c`), emuNAND code (`arm9/source/emunand.c`), config options (`arm9/source/config.c`), `k11_extension`, `sysmodules` (notably `loader/source/hbldr.c`, `rosalina/source/gdb/`, `pm/source/reslimit.c`), `Makefile`. Checked at v13.5 / a November 2026 commit.
- [Rodrigo Copetti, Nintendo 3DS Architecture: Anti-Piracy and Homebrew](https://www.copetti.org/writings/consoles/nintendo-3ds/) — Luma3DS's origins and bundled tools.
- [Hacks Guide Wiki: History of 3DS Hacking](https://wiki.hacks.guide/wiki/3DS:History_of_3DS_Hacking) — AuReiNand → Luma3DS.
