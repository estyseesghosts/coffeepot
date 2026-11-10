# CTR homebrew development workflow

Writing homebrew for the 3DS means cross-compiling C or C++ for the ARM11 on a PC with the **devkitPro** toolchain (`devkitARM`) and the community library **libctru**, then running the result on a console with [Luma3DS](luma3ds.md) or in an emulator. Most programs ship as a `.3dsx` file that the Homebrew Menu starts; programs that should appear on the HOME Menu with their own icon are packaged as an installable `.cia`.

This page is for developers: tools, project layout, how a program gets onto the console today, and packaging. For how code gets onto a stock console in the first place, see the [exploit catalogue](exploits.md) and [boot9strap](boot9strap.md). For making the program correct and fast, see the engineering pages: [debugging](../engineering/debugging.md), [performance](../engineering/performance.md), [memory and cache](../engineering/memory-and-cache.md), [GPU rendering](../engineering/gpu-rendering.md), [asset pipeline](../engineering/asset-pipeline.md) and [testing on hardware](../engineering/testing-on-hardware.md).

## What you need

- A PC with devkitPro installed: the graphical installer on Windows, or `dkp-pacman` on Linux/macOS. Install the **`3ds-dev`** group, which brings `devkitARM`, `libctru`, `citro3d`, `citro2d`, the tools below and the example projects. Keep project paths free of spaces.
- A console with [boot9strap](boot9strap.md) and [Luma3DS](luma3ds.md) (the Luma3DS release archive includes the **[Homebrew Menu](../glossary.md#hbmenu)**, `boot.3dsx`), on the same network as the PC for wireless loading and debugging.
- Optionally the **Azahar** emulator for quick iteration ([debugging: emulators](../engineering/debugging.md#emulators)).

Versions this page was checked against (installed packages, November 2026):

| Package | Version |
| ------- | ------- |
| `devkitARM` | r68 (GCC 16.1.0, binutils 2.46, newlib 4.6) |
| `devkitARM-gdb` | 14.1 |
| `libctru` | 2.7.1 |
| `citro3d` / `citro2d` | 1.7.1 / 1.7.0 |
| `picasso` | 2.7.2 |
| `tex3ds` | 2.3.0 |
| `3dstools` (`3dsxtool`, `smdhtool`, `3dsxdump`, `mkromfs3ds`) | 1.3.1 |
| `3dslink` | 0.6.3 |
| `3ds-cmake` / `catnip` | 1.5.2 / 0.2.0 |
| `3ds-examples` | 20240917 (the upstream repository has later commits) |
| Luma3DS | v13.5 (source checked at a November 2026 commit) |

Pin these in your project's README: libctru has made breaking changes between major versions ([libctru: version notes](libctru.md#version-notes)).

## How it works

### Toolchain

| Component | Role | Notes |
| --------- | ---- | ----- |
| `devkitARM` | GCC-based cross-compiler for ARM consoles | by WinterMute et al.; for the 3DS it targets `-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft` |
| `libctru` (formerly `ctrulib`) | C library for ARM11 userland: system calls, services, memory, threads ([details](libctru.md)) | by smea and others, maintained by devkitPro |
| `citro3d` | Stateful wrapper around the PICA200 GPU ([details](picasso-and-citro3d.md), [guide](../engineering/gpu-rendering.md)) | by fincs |
| `citro2d` | 2D sprites, text and shapes on top of `citro3d` | by fincs |
| `picasso` | PICA200 shader assembler, produces `.shbin` ([details](picasso-and-citro3d.md)) | by fincs; historical alternatives `nihstro` (neobrain) and `aemstro` (smea — "the original" PICA200 shader disassembler/assembler written while reverse-engineering the instruction set; its README itself recommends picasso or nihstro for real projects) |
| `tex3ds` | Converts images to `.t3x` textures and atlases ([asset pipeline](../engineering/asset-pipeline.md)) | |
| `mkbcfnt` | Converts TrueType fonts to the console's BCFNT format | used with citro2d |
| `3dsxtool`, `smdhtool` | Build the `.3dsx` and its `.smdh` icon/title metadata | run by the Makefile |
| `3dslink` | Sends a `.3dsx` to the console over Wi-Fi and receives its text output | [debugging](../engineering/debugging.md#logging-over-the-network-3dslink) |
| `arm-none-eabi-gdb` | Debugger for Luma3DS's GDB server | [debugging](../engineering/debugging.md#gdb-with-rosalina) |
| `3ds-cmake`, `catnip` | CMake support (`$DEVKITPRO/cmake/3DS.cmake`, with `ctr_generate_smdh`, `ctr_create_3dsx`, `ctr_add_shader_library`, `ctr_add_graphics_target`) and a CMake build helper | alternative to the Makefiles |
| `makerom` | Builds NCCH, CCI and CIA | not part of devkitPro; 3DSGuy's Project_CTR |
| `bannertool` | Makes the CIA banner (image + sound) and SMDH | not part of devkitPro; originally Steveice10's |
| `firmtool` | Builds `.firm` images | needed only to build CFW such as [Luma3DS](luma3ds.md) |
| `braindump`, `uncart` | On-console tools that dumped ExeFS/RomFS and game cards | neobrain; historical, listed as unmaintained on 3dbrew |

Libraries for common needs (zlib, libpng, Ogg Vorbis, Opus, libmodplug, SDL ports and more) are installed as `3ds-*` packages with `dkp-pacman` and found with `$(PORTLIBS)` / `pkg-config` in the Makefiles. Older libraries still listed on 3dbrew (`sf2dlib`, `gl3ds`, `libkhax`, `cpp3ds`, `SpriteTools`) are unmaintained; prefer citro2d/citro3d.

### Build

1. Copy `3ds-examples/templates/application` (or any example) and run `make`. Outputs: `<name>.3dsx`, `<name>.elf` (keep it — it holds the debug symbols), `<name>.smdh`.
2. Project folders are fixed by Makefile variables: `source/` (C/C++/assembly and `*.v.pica` shaders), `include/`, `data/` (raw files linked in), `gfx/` (`.t3s` texture descriptions), optionally `romfs/` (files packed into the program's RomFS) ([asset pipeline](../engineering/asset-pipeline.md)).
3. Metadata: `APP_TITLE`, `APP_DESCRIPTION`, `APP_AUTHOR` and a 48×48 `icon.png` become the SMDH shown by the Homebrew Menu.
4. Always use the hard-float ABI: mixing `-mfloat-abi=softfp` and `hard` fails at link time ("uses VFP register arguments but … does not").

### Running on a console

On a console with Luma3DS:

- **Wirelessly (recommended while developing):** open the Homebrew Menu, press **Y**, then `3dslink -s myapp.3dsx` on the PC (add `-a <console IP>` if the console is not found). `-s` keeps the connection open to show the program's output.
- **From the SD card:** copy `myapp.3dsx` to `/3ds/` (or `/3ds/myapp/myapp.3dsx`) and pick it in the Homebrew Menu. The Homebrew Menu also supports XML shortcut files with arguments, and starred favourites.

**How a `.3dsx` is started today.** Luma3DS's own loader (the `loader` system module, `hb:ldr` service) loads 3DSX files. When the title `000400000D921E00` (`hblauncher_loader`) — or another title chosen in Rosalina with *Switch the hb. title to the current app* — is launched, the loader runs a 3DSX in its place: `sdmc:/boot.3dsx` (the Homebrew Menu) by default, or whatever path was last set through the `hb:ldr` service's `SetTarget` command (with arguments from `SetArgv`). That is how the Homebrew Menu starts your program: it sets the target and has the title relaunched [documented: Luma3DS `sysmodules/loader/source/hbldr.c`, `Makefile`]. The loader gives every 3DSX a rewritten extended header: access to all services and the whole filesystem, all ARM11 I/O registers mapped, DSP memory read/write and VRAM read-only, a core-1 CPU-time limit of up to 89% (activated with `APT_SetAppCpuTimeLimit`), access to the New 3DS's third core, and the current memory mode. The 804 MHz and L2-cache flags are off, so call `osSetSpeedupEnable(true)` yourself ([performance](../engineering/performance.md#turn-on-the-new-3ds-fast-mode-yourself)).

### Packaging: `.3dsx` or `.cia`

The file format does not decide what a program may do; the **[extended header](../glossary.md#exheader)** (exheader) does (the services, memory mappings, CPU cores and kernel calls it lists), and on a Luma3DS console signature checks are patched out so any exheader is accepted ([Luma3DS](luma3ds.md)). Choose by distribution needs:

| | `.3dsx` | `.cia` (installed title) |
| --- | --- | --- |
| Started from | Homebrew Menu | HOME Menu icon |
| Privileges | Luma3DS's generous 3DSX exheader (above) | Whatever its RSF/exheader requests |
| Memory mode | Inherits the current one (64 MB on Old 3DS normally) | Can request a larger mode (e.g. 96 MB on Old 3DS), at the cost of a reboot into that layout on launch ([memory and cache](../engineering/memory-and-cache.md#how-much-memory-you-get)) |
| New 3DS clock/L2 | Call `osSetSpeedupEnable(true)` | Can request in the exheader |
| Own save data, title ID | No (files on SD) | Yes |
| Build | `make` | strip the ELF, then `makerom -f cia -rsf app.rsf -target t -elf app.elf -icon app.smdh -banner banner.bin` (banner: 256×128 PNG + WAV via `bannertool`) |

`.3ds`/`.cci` cartridge images (`makerom -f cci … -target d`) are rarely useful for homebrew. Prefer `.3dsx` unless you need one of the `.cia` columns.

### Example projects (`3ds-examples`)

The devkitPro `3ds-examples` repository (public domain) is the best starting point; each folder is a ready-to-build project. As of the checked copy (January 2026) it contains:

| Folder | Shows |
| ------ | ----- |
| `templates/application`, `templates/library` | Skeleton projects |
| `graphics/gpu/*` | `simple_tri`, `textured_cube`, `lenny`, `fragment_light`, `normal_mapping`, `toon_shading`, `proctex`, `geoshader`, `loop_subdivision`, `particles`, `cubemap`, `mipmap_fog`, `immediate`, `gpusprites`, `2d_shapes`, `both_screens`, `stereoscopic_2d`, `wide_mode_3d`, `composite_scene`, `multiple_buf`; plus `graphics/bitmap` and `graphics/printing` (system and custom fonts) |
| `audio/*` | Streaming, microphone, filters, and decoding of Ogg Vorbis, Opus and module music |
| `input/*` | Controls, touch screen, software keyboard |
| `camera/*` | Still image and video capture |
| `network/*` | `3dslink-demo`, HTTP and POST, sockets, SSL, local wireless (`uds`), SpotPass (`boss`) |
| `nfc`, `mvd`, `qtm/*` | amiibo access (New 3DS or a host process with `nfc:u`), the New 3DS video-decoder block, head-tracking data (rewritten for libctru 2.4's QTM API, plus a `lenny` + QTM demo) |
| `app_launch`, `libapplet_launch`, `mii_selector`, `get_system_language`, `romfs`, `sdmc`, `threads/*`, `time/rtc`, `physics/box2d` | Launching other applications and applets, Mii picker, system language, packaged resources, SD card access, threads and events, the real-time clock, a physics demo |

### Language options

C and C++ (`-std=gnu++11` in the template; newer standards work with the current GCC), plus assembly. Community projects provide Lua (Lua Player Plus), and SDL2 ports exist for porting existing games.

## Technical details

- 3DSX programs are linked at virtual address `0x00100000` (`3dsx.ld`); the loader sets their main-thread priority to `0x30`, affinity to core 0, and stack size field to 4 KiB (libctru sets up its own) [documented: `hbldr.c`].
- The 3DSX exheader's kernel capabilities include RW I/O `0x1EC00000–0x1F000000`, RW DSP RAM `0x1FF00000–0x1FF80000`, RO VRAM `0x1F000000–0x1F600000`, handle table size `0x200`, and the APPLICATION memory type with "shared page writing", "allow debug" and "access core 2" flags.
- Services: libctru wrappers (`gfx`, `hid`, `fs`, `soc`, `ndsp`, …; see [services](../software/services.md)).
- GPU work: shaders in `picasso` assembly, uploaded and drawn via `citro3d`; GPU buffers in linear memory ([memory and cache](../engineering/memory-and-cache.md)).

### Legacy: the original Homebrew Launcher

Before custom firmware was universal (2014–2017), `.3dsx` files were run by smea's Homebrew Launcher, started through a userland exploit; it took over an installed title's process to run each program, so the program inherited that title's limited permissions (an `.xml` file next to the `.3dsx` listed suitable titles), and early homebrew had 64 MB at most and no DSP access ([exploits](exploits.md#homebrew-launcher), [history](history.md)). The WikiTemp guide this page originally drew on describes that era, including installing CIAs on firmware below 9.2. None of this is needed on a Luma3DS console; it matters only when reading old code or forum posts.

## Limits and risks

- `.3dsx` homebrew runs with broad privileges under Luma3DS: it can read and write the SD card and NAND through filesystem services. A bug that writes to system save data or NAND can break the console; keep NAND backups ([boot9strap](boot9strap.md)).
- Code running with kernel or ARM9 privileges (custom firmware, payloads) can brick a console.
- Piracy tools and dumps of retail content raise legal issues; the wiki does not cover how to obtain content.

## Open questions

- Maintenance status of standalone `makerom` and `bannertool` forks changes over time; check before relying on a particular release.
- How Azahar handles the 3DSX exheader (permissions, memory mode) compared with Luma3DS has not been checked here.

## Sources

- [devkitPro: Getting started](https://devkitpro.org/wiki/Getting_Started) and the installed packages (`dkp-pacman -Q`) — package names and versions.
- [3ds-examples (devkitPro)](https://github.com/devkitPro/3ds-examples) — example projects, template Makefile.
- [libctru](https://github.com/devkitPro/libctru), [citro3d](https://github.com/devkitPro/citro3d), [citro2d](https://github.com/devkitPro/citro2d) and [picasso](https://github.com/devkitPro/picasso) — library sources and manuals.
- [3ds-hbmenu](https://github.com/devkitPro/3ds-hbmenu) — Homebrew Menu, netloader, shortcuts.
- [Luma3DS](https://github.com/LumaTeam/Luma3DS) — `sysmodules/loader/source/hbldr.c` (3DSX loading and exheader), `Makefile` (`HBLDR_DEFAULT_3DSX_TID`), README (build requirements).
- [aemstro](https://github.com/smealum/aemstro) — README (status and recommendation).
- [3dbrew: Homebrew Libraries and Tools](https://www.3dbrew.org/wiki/Homebrew_Libraries_and_Tools) and [Makerom](https://www.3dbrew.org/wiki/Makerom) — tool list, packaging.
- [WikiTemp: 3DS Homebrew Development](https://wiki.gbatemp.net/w/index.php?title=3DS_Homebrew_Development) — the original Makefile/SMDH/CIA workflow (dated).
- [Rodrigo Copetti, Nintendo 3DS Architecture: Anti-Piracy and Homebrew](https://www.copetti.org/writings/consoles/nintendo-3ds/) — history of libctru, nihstro and the Homebrew Launcher.
