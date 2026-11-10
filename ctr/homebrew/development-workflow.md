# CTR homebrew development workflow

Writing homebrew for the 3DS means cross-compiling C or C++ for the ARM11 on a PC, then packaging the result in one of the console's executable formats. Most hobbyists use the **devkitPro** toolchain (`devkitARM`) with the community library **libctru**, and ship `.3dsx` files for the Homebrew Launcher. Programs that need more privileges are packaged as `.cia` or `.3ds`.

This page is for developers. For how code gets *onto* a console in the first place, see the [exploit catalogue](exploits.md) and [boot9strap](boot9strap.md); for what the hardware looks like to your code, see [CPU](../hardware/cpu.md), [GPU](../hardware/gpu.md) and [memory](../hardware/memory.md).

## What you need

- A PC with the **devkitPro** installer (Windows) or package manager, with `devkitARM` and `libctru` enabled. Keep project paths free of spaces.
- Optional libraries and tools (below).
- A 3DS running a way to launch unsigned code (Homebrew Launcher, or a custom firmware), or an emulator such as Citra for testing `.elf`/`.3dsx` files.

## How it works

### Toolchain

| Component | Role | Notes |
| --------- | ---- | ----- |
| `devkitARM` | GCC-based cross-compiler for ARM consoles | by WinterMute et al.; builds ARM11 userland code |
| `libctru` (formerly `ctrulib`) | C library for ARM11 userland: system calls, services, memory, threads ([details](libctru.md)) | by smea and others, now maintained by devkitPro |
| `citro3d` | Stateful wrapper around the PICA200 GPU ([details](picasso-and-citro3d.md)) | by fincs |
| `citro2d` | 2D drawing on top of `citro3d` | by fincs |
| `picasso` | PICA200 shader assembler, produces `.shbin` ([details](picasso-and-citro3d.md)) | by fincs; alternatives `nihstro` (neobrain) and `aemstro` (smea) |
| `tex3ds` | Converts images to `.t3x` texture files for citro3d | used by the example Makefiles |
| `firmtool` | Builds the `boot.firm` image for a CFW | needed to build [Luma3DS](luma3ds.md), with `makerom` |
| `3ds-cmake` | CMake support for `devkitARM` | |
| `makerom` | Builds NCCH, CCI and CIA | 3dsguy, maintained by profi200 |
| `bannertool` | Makes SMDH icon metadata and banners | Steveice10 |
| `braindump`, `uncart` | On-console tools to dump ExeFS/RomFS and cartridges | neobrain |

Older or deprecated libraries still listed on 3dbrew: `sf2dlib`, `gl3ds`, `libkhax` (kernel memory modification), `cpp3ds`, `SpriteTools`.

### Build and run

1. Install devkitPro, set `DEVKITPRO` and `DEVKITARM`.
2. Copy an example from `devkitPro/3ds-examples` and run `make`. The default output is a `.3dsx` plus an `.elf`.
3. For a `.3dsx` app, copy `appname.3dsx` (and optionally `.smdh`, and an `.xml` listing the installed titles the app may replace at runtime) to `/3ds/` on the SD card. The Homebrew Launcher scans for `.3dsx` files and shows icons for those with metadata.
4. Metadata (an **SMDH**): icon (48×48 PNG named `icon.png`), short name, description and author, set in the `Makefile` (`APP_TITLE`, `APP_DESCRIPTION`, `APP_AUTHOR`).
5. For `.cia`/`.3ds`: strip the ELF (`arm-none-eabi-strip`), then use `makerom` with an RSF file, an icon (the SMDH), and a banner (256×128 PNG + 16-bit WAV compiled with `bannertool`). If you do not need kernel-level access, 3DSX is preferred.

Always build with the hard-float ABI: use `-mfloat-abi=hard` everywhere. Mixing `softfp` and `hard` fails at link time (`uses VFP register arguments but … does not`).

### Example projects (`3ds-examples`)

The devkitPro `3ds-examples` repository (public domain) is the best starting point; each folder is a ready-to-build project. As of the checked copy it contains:

| Folder | Shows |
| ------ | ----- |
| `templates/application`, `templates/library` | Skeleton projects |
| `graphics/gpu/*` | `simple_tri`, `textured_cube`, `lenny`, `fragment_light`, `normal_mapping`, `toon_shading`, `proctex`, `geoshader`, `loop_subdivision`, `particles`, `cubemap`, `mipmap_fog`, `immediate`, `gpusprites`, `2d_shapes`, `both_screens`, `stereoscopic_2d`, `wide_mode_3d`, `composite_scene`, `multiple_buf`; plus `graphics/bitmap` and `graphics/printing` |
| `audio/*` | Streaming, microphone, filters, and decoding of Ogg Vorbis, Opus and module music |
| `input/*` | Controls, touch screen, software keyboard |
| `camera/*` | Still image and video capture |
| `network/*` | `3dslink` demo, HTTP and POST, sockets, SSL, local wireless (`uds`), SpotPass (`boss`) |
| `nfc`, `mvd`, `qtm/*` | amiibo access (New 3DS or a host process with `nfc:u`), the New 3DS video-decoder block (needs access to `MVDSTD` from a modified firmware or an old userland exploit), head-tracking data |
| `app_launch`, `libapplet_launch`, `mii_selector`, `get_system_language`, `romfs`, `sdmc`, `threads/*`, `time/rtc`, `physics/box2d` | Launching other applications and applets, Mii picker, system language, packaged resources, SD card access, threads and events, the real-time clock, a physics demo |

The GPU examples build their shaders from `*.v.pica` (vertex) and `*.g.pica` (geometry) files with `picasso` into `.shbin`, and their textures from images with `tex3ds`.

### Language options

C, C++ and some assembly; Lua via the Lua Player Plus interpreter; Pascal via a community project.

## Technical details

- A `.3dsx` runs inside a hijacked retail process (via the Homebrew Launcher's exploit, [see exploits](exploits.md#homebrew-launcher)); the app inherits that process's permissions. The `.xml` lists the titles the launcher may use for that purpose.
- Services are accessed through `libctru`'s wrappers (`gfx`, `hid`, `fs`, `soc`, `dsp`, …; see [services](../software/services.md)). Early homebrew had only 64 MB of RAM and no DSP access in userland.
- GPU work: write shaders in `picasso` assembly, upload them, issue draw calls via `citro3d` ([GPU](../hardware/gpu.md)). Buffers shared with the GPU come from the "LINEAR"/GSP heap ([memory](../hardware/memory.md)).
- `makerom` command shapes from the WikiTemp guide (output formats `cci`/`cia`): `makerom -f cci -o x.3ds -rsf … -target d -exefslogo -elf x.elf -icon x.smdh -banner banner.bin` and `-f cia … -target t`. That guide is dated; installing CIAs on retail consoles requires a custom firmware today, and its claim about "firmware below 9.2" is historical.

## Limits and risks

- Userland homebrew can't touch NAND or most hardware; kernel/ARM9 code can ([custom firmware](custom-firmware.md)) and can brick a console.
- The sources used here are largely from 2015–2018 (the WikiTemp guide) and may describe obsolete steps. Check the project READMEs for current commands.
- Piracy tools and dumps of retail content raise legal issues; the wiki does not cover how to obtain content.

## Open questions

- Current maintenance status of several tools (e.g. `aemstro`, `nihstro`, `braindump`) is marked "no"/unknown on 3dbrew.
- How emulators run homebrew `.3dsx` (Citra) is not documented in the sources used.

## Sources

- [WikiTemp: 3DS Homebrew Development](https://wiki.gbatemp.net/w/index.php?title=3DS_Homebrew_Development) — devkitARM install, make, SMDH, 3DSX vs CIA/3DS packaging, common errors.
- [3dbrew: Homebrew Libraries and Tools](https://www.3dbrew.org/wiki/Homebrew_Libraries_and_Tools) — libraries and PC/3DS tools.
- [3dbrew: Homebrew Applications](https://www.3dbrew.org/wiki/Homebrew_Applications) — `.3dsx` / `.smdh` / `.xml` installation convention, launchers.
- [3dbrew: Makerom](https://www.3dbrew.org/wiki/Makerom) — packaging NCCH/CCI/CIA.
- [3ds-examples (devkitPro)](https://github.com/devkitPro/3ds-examples) — example projects and their Makefile rules.
- [libctru](https://github.com/devkitPro/libctru), [citro3d](https://github.com/devkitPro/citro3d) and [picasso](https://github.com/devkitPro/picasso) — library sources and manuals.
- [Luma3DS README](https://github.com/LumaTeam/Luma3DS) — build requirements (`makerom`, `firmtool`, devkitARM, libctru).
- [Rodrigo Copetti, Nintendo 3DS Architecture: Anti-Piracy and Homebrew](https://www.copetti.org/writings/consoles/nintendo-3ds/) — history of libctru, nihstro, the Homebrew Launcher and its userland limits.
