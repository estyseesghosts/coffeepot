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
| smea (Smealum) | `ctrulib` (now libctru), released in early 2014 with yellows8, ichfly, WinterMute, fincs, mtheall and plutoo; `aemstro`, the first PICA200 shader (dis)assembler | [history](history.md), [libctru](libctru.md), [picasso and citro3d](picasso-and-citro3d.md#related-shader-tools) |
| WinterMute and devkitPro | `devkitARM` (3dbrew: "WinterMute et al."), maintenance of libctru, citro3d, citro2d and the tools | [development workflow](development-workflow.md#2-install-devkitpro), [libctru](libctru.md) |
| fincs | `picasso`, `citro3d`, `citro2d`; co-author of libctru | [picasso and citro3d](picasso-and-citro3d.md) |
| neobrain | `nihstro` shader assembler; `braindump` and `uncart` dumping tools (3dbrew lists all three as unmaintained) | [development workflow](development-workflow.md#other-libraries-and-tools) |
| 3dsguy, profi200 | `makerom` (author, maintainer) | [file formats](../software/file-formats.md), [development workflow](development-workflow.md#other-libraries-and-tools) |
| Steveice10 | `bannertool` | [development workflow](development-workflow.md#icon-name-and-banner-smdh) |
| Pierre Wendling | SDL's Nintendo 3DS port (SDL2 2.26+ and SDL3) | [porting](../engineering/porting.md#example-c-sdl-software-rendered-2d-game) |
| Gericom | Super Mario 64 3DS port | [porting](../engineering/case-studies/case-studies-ports.md) |

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
| smea | redNAND (2013; the Hacks Guide names smealum and Yellows8 for the private payload, see [history](history.md)); the original Homebrew Launcher; `arm9loaderhax`, `memchunkhax2` and related reveals at 32C3 (with derrek and plutoo) | [history](history.md), [custom firmware](custom-firmware.md), [development workflow](development-workflow.md#8-reference-links) |
| yellows8 | `memchunkhax` (first ARM11 kernel exploit, February 2014 per 3dbrew); TWL key entropy and key-generator analysis (with plutoo); `smashbroshax`/`beaconhax` | [history](history.md), [exploits](exploits.md) |
| derrek | Boot ROM dumping and **sighax** (33C3, December 2016); RAM not cleared on MCU reboot; presented the 33C3 talk where `soundhax` and `fasthax` were revealed (the Hacks Guide credits their authorship to nedwill) | [boot9strap](boot9strap.md), [exploits](exploits.md#hardware-and-boot-rom) |
| SciresM, Myria | Practical sighax signatures and **boot9strap**; principles of **ntrboot** (33.5C3) | [boot9strap](boot9strap.md) |
| nedwill | `soundhax` (free primary exploit) and `fasthax` (kernel), per the Hacks Guide; revealed at 33C3 | [history](history.md) |
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
| Citra project wiki, "GPU_LUTINPUT_CP" (author wwylele) | Wiki gone; text survives as the author's [gist](https://gist.github.com/wwylele/f61d6653620bb763ab9dc7a60a9831ce) (last active 2022-09-19) | [GPU shading features](../hardware/gpu-shading-features.md) |
| Citra project wiki, "Random PICA200 RE Notes" (author fincs) | Wiki gone; survives as [fincs's gist](https://gist.github.com/fincs/543f7e1375815f495f10020a053f14e9) | [GPU shading features](../hardware/gpu-shading-features.md) |
| Citra project wiki, "3DS CRO Information" (author wwylele; the text itself notes it is incomplete) | Original-author [CRO notes](https://gist.github.com/wwylele/325d53ee6a0f1dff6aa3473377335d93/raw/cab6bdf3f3d65ecd17b86bf969f636c21df994fe/CRO_doc.md) survive; the author marks the format incomplete | [CRO modules](../software/cro-modules.md) |
| Citra project wiki, "Kernel synchronization" | Subv’s [scheduler notes](https://gist.github.com/Subv/02f29bd9f1e5deb7aceea1e8f019c8f4/raw/d974d3d1b540936355c054ab7dc45658e16a8a5f/kernel_scheduling.md) survive; several cases remain unverified | [kernel](../software/kernel.md) |
| Citra project wiki, "3DS CEC (StreetPass) Documentation" | wwylele’s [CEC notes](https://gist.github.com/wwylele/29a8caa6f5e5a7d88a00bedae90472ed/raw/80d12c49fd03555c0612919c69df63e37f8dc7d7/doc.md) survive as a work in progress | [StreetPass](../software/streetpass-cec.md) |
| Original Citra emulator | Shut down March 2024; continued in Azahar | [debugging](../engineering/debugging.md#emulators) |
| DMP's gas-rendering patent | Japanese, poorly OCR'd; not yet decoded | [GPU shading features](../hardware/gpu-shading-features.md) |

### Looking for the vanished Citra wiki pages

The original Citra wiki repository was removed. Its [archived documentation index](https://citra.azahar-emu.org/wiki/archive-of-3ds-documentation/) links original-author notes for CRO, CEC and the kernel scheduler as well as the two GPU gists above. The three raw gist URLs in the table pin the file revisions retrieved on 2026-11-11; they are surviving author documents, not proof of an identical snapshot of the removed wiki. The CRO author explicitly warns that the format is incompletely understood; the CEC note is work in progress, and the scheduler note marks some behavior unverified.

## Limits and risks

- Credit here is only as complete as the wiki's pages. Absence from this table means "not yet cited on the wiki", not "did not contribute".
- Many results were joint work or built on earlier unpublished findings; the linked pages and their sources give the fuller picture.

## Sources

- The linked wiki pages and their Sources sections, in particular [history](history.md) (from the Hacks Guide timeline), [exploits](exploits.md), [Luma3DS](luma3ds.md), [GPU shading features](../hardware/gpu-shading-features.md) and [porting](../engineering/porting.md).
- [fincs, Random PICA200 RE Notes](https://gist.github.com/fincs/543f7e1375815f495f10020a053f14e9) — surviving copy of the PICA200 research notes.
