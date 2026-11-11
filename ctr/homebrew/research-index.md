# Who researched what: a 3DS research index

Most of what is known about the 3DS's internals comes from a few dozen people working in the open between 2013 and today: exploit authors, emulator developers, library maintainers and reverse engineers. Their findings are scattered across talks, wiki edits, source comments and personal notes, some of which have already disappeared. This page maps people to the subjects they are credited with **on this wiki**, so a reader can find the original source and an agent can tell who to trust on what.

It is for anyone tracing a fact back to its origin. Each entry links to the wiki page where the work is described and cited; this page does not repeat the details.

## What you need

Nothing; it is a reading aid. Attributions here are only those already made, with sources, on the linked pages. If you know of a missing credit, add it to the relevant page with its source first, then here.

## How it works

Entries are grouped by area and list the person or group, what this wiki credits them with, and where to read more. Many people worked in several areas; they appear once per area.

### Libraries, toolchain and tools

| Who | Credited with | Read more |
| --- | ------------- | --------- |
| smea (Smealum) | `ctrulib` (now libctru), released in early 2014 with yellows8, ichfly, WinterMute, fincs, mtheall and plutoo; `aemstro`, the first PICA200 shader (dis)assembler | [history](history.md), [libctru](libctru.md), [development workflow](development-workflow.md#2-install-devkitpro) |
| WinterMute and devkitPro | `devkitARM`, maintenance of libctru, citro3d, citro2d and the tools | [development workflow](development-workflow.md#2-install-devkitpro) |
| fincs | `picasso`, `citro3d`, `citro2d`; co-author of libctru | [picasso and citro3d](picasso-and-citro3d.md) |
| neobrain | `nihstro` shader assembler; `braindump` and `uncart` dumping tools | [development workflow](development-workflow.md#2-install-devkitpro) |
| 3dsguy, profi200 | `makerom` (author, maintainer) | [file formats](../software/file-formats.md) |
| Steveice10 | `bannertool` | [development workflow](development-workflow.md#2-install-devkitpro) |
| Pierre Wendling | SDL's Nintendo 3DS port (SDL2 2.26+ and SDL3) | [porting](../engineering/porting.md#libraries-that-shorten-a-port) |
| Gericom | Super Mario 64 3DS port | [porting](../engineering/porting.md#what-published-ports-report) |

### Custom firmware and system internals

| Who | Credited with | Read more |
| --- | ------------- | --------- |
| Aurora Wright | Created Luma3DS (2015, from AuReiNand) | [Luma3DS](luma3ds.md#technical-details) |
| TuxSH | Lead Luma3DS maintainer since 2016: Rosalina, GDB stub, `pm`/`sm`/`pxi` reimplementations; scheduling and CPU-time notes in Luma3DS's `pm`; `universal-otherapp`; full QTM support and documentation in libctru 2.4.0 and the GSP performance-log API (libctru git history) | [Luma3DS](luma3ds.md), [threading](../engineering/threading.md), [libctru](libctru.md#version-notes) |
| PabloMK7 | Luma3DS game-plugin loader (v13.0); the Citra fork that became part of Azahar | [Luma3DS](luma3ds.md), [debugging](../engineering/debugging.md#emulators) |
| Normmatt | SD-card controller code used by Luma3DS's emuNAND support | [Luma3DS](luma3ds.md#emunand-support) |

### Exploits and security research

| Who | Credited with | Read more |
| --- | ------------- | --------- |
| smea | redNAND (2013); the original Homebrew Launcher; `arm9loaderhax`, `memchunkhax2` and related reveals at 32C3 (with derrek and plutoo) | [history](history.md), [custom firmware](custom-firmware.md), [development workflow](development-workflow.md#8-reference-links) |
| yellows8 | `memchunkhax` (first ARM11 kernel exploit); TWL key entropy and key-generator analysis (with plutoo); `smashbroshax`/`beaconhax` | [history](history.md), [exploits](exploits.md) |
| derrek | Boot ROM dumping and **sighax** (33C3, December 2016); RAM not cleared on MCU reboot; `soundhax`/`fasthax` reveal | [boot9strap](boot9strap.md), [exploits](exploits.md#hardware-and-boot-rom) |
| SciresM, Myria | Practical sighax signatures and **boot9strap**; principles of **ntrboot** (33.5C3) | [boot9strap](boot9strap.md) |
| plutoo | `freakyhax`; key-generator analysis with yellows8 | [exploits](exploits.md) |
| zoogie | `MSET9`, `bannerbomb3`, `smilehax IIe`, `new-browserhax`; author of the history the Hacks Guide timeline adapts | [exploits](exploits.md), [history](history.md) |

### GPU and emulator reverse engineering

| Who | Credited with | Read more |
| --- | ------------- | --------- |
| fincs | "Random PICA200 RE Notes": procedural textures, shadow mapping, quirks, and the list of sources used (DMP papers, CEDEC 2012, game symbols, the CTRAging factory program) | [GPU shading features](../hardware/gpu-shading-features.md) |
| MerryMage, wwylele, fincs | Procedural-texture noise algorithm | [GPU shading features](../hardware/gpu-shading-features.md) |
| wwylele | CRO dynamic-module format | [CRO modules](../software/cro-modules.md) |
| Citra project contributors | Kernel synchronisation and scheduling notes, StreetPass (CEC) mailbox documentation, `GPU_LUTINPUT_CP` | [kernel](../software/kernel.md), [StreetPass](../software/streetpass-cec.md), [GPU shading features](../hardware/gpu-shading-features.md) |
| DMP (Digital Media Professionals) | Designers of the PICA200; CEDEC 2012 talk on its design | [GPU](../hardware/gpu.md#design-notes-from-dmp) |

### Overviews

| Who | Credited with | Read more |
| --- | ------------- | --------- |
| Rodrigo Copetti | *Nintendo 3DS Architecture*, the whole-console overview (CC BY 4.0) adapted throughout this wiki | [index](../index.md) |
| 3dbrew contributors | The primary community reference for hardware, formats and the kernel | [index](../index.md) |

## Technical details

Sources that have already vanished or moved, and where this wiki takes them from:

| Material | Status | Where the wiki cites it |
| -------- | ------ | ----------------------- |
| Citra project wiki (kernel synchronisation, CRO, CEC, `GPU_LUTINPUT_CP`, PICA200 RE notes) | Removed with the `citra-emu/citra` repository (2024) | Cited by title on each page; the PICA200 notes survive as [fincs's gist](https://gist.github.com/fincs/543f7e1375815f495f10020a053f14e9) |
| Original Citra emulator | Shut down March 2024; continued in Azahar | [debugging](../engineering/debugging.md#emulators) |
| DMP's gas-rendering patent | Japanese, poorly OCR'd; not yet decoded | [GPU shading features](../hardware/gpu-shading-features.md) |

## Limits and risks

- Credit here is only as complete as the wiki's pages. Absence from this table means "not yet cited on the wiki", not "did not contribute".
- Many results were joint work or built on earlier unpublished findings; the linked pages and their sources give the fuller picture.

## Sources

- The linked wiki pages and their Sources sections, in particular [history](history.md) (from the Hacks Guide timeline), [exploits](exploits.md), [Luma3DS](luma3ds.md), [GPU shading features](../hardware/gpu-shading-features.md) and [porting](../engineering/porting.md).
- [fincs, Random PICA200 RE Notes](https://gist.github.com/fincs/543f7e1375815f495f10020a053f14e9) — surviving copy of the PICA200 research notes.
