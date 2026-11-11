# Source versions and verification status

Library behaviour changes between versions: a citro3d function may flush differently, a libctru default may move, a Luma3DS loader may grant different permissions. When a page on this wiki says "`C3D_FrameEnd` flushes the whole linear heap" or "a `.3dsx` gets an 89% core-1 limit", that statement is true of a specific version of the source. This page records which versions the CTR engineering and homebrew pages were checked against, how to look at exactly that code, and what each code example on the wiki has (and has not) been verified to do.

## Before you start

- To read the exact code behind a claim, open the repository at the pinned commit below, e.g. `https://github.com/devkitPro/citro3d/blob/9f21cf7b380ce6f9e01a0420f19f0763e5443ca7/source/renderqueue.c`.
- When updating a page against a newer version, update this table in the same change and re-run `tools/check_docs.py` and `tools/build_benchmarks.sh`.

## Pinned source commits

Checked in November 2026.

| Project | Commit | Commit date | Release | Used for |
| ------- | ------ | ----------- | ------- | -------- |
| [libctru](https://github.com/devkitPro/libctru) | `62b7c42c88b1ee837f513358409d2e5c8f9001a9` | 2026-10-31 | v2.7.1 | Heaps, allocators, threads, synchronisation, GSP/DSP/APT services, ndsp, 3dslink, GDB HIO, performance counters |
| [citro3d](https://github.com/devkitPro/citro3d) | `9f21cf7b380ce6f9e01a0420f19f0763e5443ca7` | 2023-10-28 | v1.7.1 | Frame loop, timers, flushing, draw-state updates, textures |
| [Luma3DS](https://github.com/LumaTeam/Luma3DS) | `1441535d222002353b79d290b6200ceab2e32cb0` | 2026-11-05 | v13.5 + 2 commits | 3DSX loader and exheader, Rosalina GDB stub and menus, crash dumps, scheduling notes |
| [picasso](https://github.com/devkitPro/picasso) | `d522455ea59cd5cb9219c699b93eb7750039233c` | 2023-10-11 | v2.7.1 + 1 commit (package 2.7.2) | Shader assembler manual and behaviour |
| [3ds-examples](https://github.com/devkitPro/3ds-examples) | `be2001fee08cf8ec7d3366e095e83f6f75da8942` | 2026-01-30 | v20240917 + 3 commits | Template Makefile, example code |
| [aemstro](https://github.com/smealum/aemstro) | `51bfeef9e1a0149726dca43b50919bd45917015a` | 2015-01-02 | — | Historical shader tools |
| [Architecture of Consoles](https://github.com/flipacholas/Architecture-of-consoles) | `9c41261f85f0a6ffdf2b1eea3f439208b11a68e8` | 2026-11-02 | — | Copetti's 3DS article source |
| [DaedalusX64-3DS](https://github.com/masterfeizz/DaedalusX64-3DS) | `31c5e56` | 2022-10-17 | — | Daedalus case study |
| [DXX-3DS](https://github.com/RossMeikleham/DXX-3DS) | `cdb4cc3` | 2018-12-07 | — | DXX case study |
| [OpenLara](https://github.com/XProger/OpenLara) | `3b268ca` | 2026-07-26 | — | OpenLara case study |
| [Quake2CTR](https://github.com/masterfeizz/Quake2CTR) | `993114e` | 2022-10-30 | — | Quake2CTR case study |
| [re3onN3ds](https://github.com/wabisuke-dev/re3onN3ds) | `3233ffe1` | 2021-09-03 | upstream re3, no 3DS code | re3 case study |
| [sm64_3ds](https://github.com/sm64-port/sm64_3ds) | `0a82ae2` | 2020-08-03 | — | older SM64 case study |
| [sm64ds-enhanced](https://github.com/ethanseagull/sm64ds-enhanced) | `3f6dbc1` | 2026-11-08 | — | SM64 DS case study |
| [Sonic-CD-11-3DS](https://github.com/SaturnSH2x2/Sonic-CD-11-3DS) | `8b30cf6` | 2025-02-24 | — | Sonic CD case study |
| [srb2_3ds](https://github.com/derrekr/srb2_3ds) | `e8726772` | 2026-05-03 | — | SRB2 case study |
| [vril-engine](https://github.com/nzp-team/vril-engine) | `9a26605` | 2026-11-08 | — | Vril case study |
| [Xash3DS](https://github.com/masterfeizz/Xash3DS) | `8590409d` | 2019-09-14 | — | Xash3DS case study |

The earlier batching review used an unpinned upstream `master` and installed citro2d 1.7.0 headers. The reproducible comparison source is [citro2d v1.7.0 `source/base.c` at `147b02aae021da61b1f620446ad2892ecc45411e`](https://github.com/devkitPro/citro2d/blob/147b02aae021da61b1f620446ad2892ecc45411e/source/base.c); this is the upstream release commit, but this wiki has not proved that its source exactly produced the installed package binary. Earlier installed-binary disassembly is not archived here; binary-only conclusions remain less reproducible.

## Installed toolchain

The devkitPro packages used to build every code example and benchmark (from `pacman -Q`):

| Package | Version |
| ------- | ------- |
| `devkitARM` | r68-1 (GCC 16.1.0, binutils 2.46.0, newlib 4.6.0.20260123) |
| `devkitARM-gdb` | 14.1-1 |
| `libctru` | 2.7.1-1 |
| `citro3d` / `citro2d` | 1.7.1-2 / 1.7.0-1 |
| `picasso` | 2.7.2-3 |
| `tex3ds` | 2.3.0-4 |
| `3dstools` | 1.3.1-3 |
| `3dslink` | 0.6.3-1 |
| `general-tools` | 1.4.4-1 |
| `3ds-examples` | 20240917-1 |

## Verification status of code on the wiki

"Builds" means compiled with the toolchain above with `-Wall` and no warnings. No code on the wiki has been run on hardware yet.

| Code | Where | Builds | Emulator | Old 3DS | New 3DS |
| ---- | ----- | ------ | -------- | ------- | ------- |
| `membench`, `flushbench`, `drawbench` | [benchmarks](../benchmarks/README.md) | yes | not run | not run | not run |
| 3dslink logging setup | [debugging](debugging.md#logging-over-the-network-3dslink) | yes | not run | not run | not run |
| Hardware performance counter helper | [CPU optimisation](cpu-optimization.md#hardware-performance-counters) | yes | not run | not run | not run |
| Job queue / worker thread (bounded shutdown rewrite) | [threading](threading.md#a-worker-thread-pattern) | yes (stub `main`, `-Wall -Wextra`) | not run | not run | not run |
| APT hook logger, worker pause, software keyboard | [application lifecycle](application-lifecycle.md#a-complete-example) | yes (direct `arm-none-eabi-gcc` link to ELF, `-Wall -Wextra`) | not run | not run | not run |
| Two-generation checksummed save manager (+ fault-injection build) | [filesystem and saves](filesystem-and-saves.md#a-two-generation-save-manager) | yes (same method) | not run | not run | not run |
| Raw `cfg:u` client, session wrapper, negative-test harness (`ipc.h` arithmetic also asserted on a PC) | [IPC and services](ipc-and-services.md) | yes (`-Wall -Wextra`, linked to ELF) | not run | not run | not run |
| `socInit` helper, non-blocking connect with `poll` | [networking](networking.md#socu-initialisation) | yes | not run | not run | not run |
| Bounded-memory httpc download | [networking](networking.md#bounded-memory-download) | yes | not run | not run | not run |
| Command-list capture hook | [pipeline](PICA200/pica200-command-pipeline.md#host-side-command-list-decoder) | yes (compile only, `-Wall -Wextra`) | not run | not run | not run |
| Shader load/draw fragment | [shader programming](PICA200/pica200-shader-programming.md#2-scale-rotate-translate-with-uniforms) | yes (compile only, `-Wall -Wextra`) | not run | not run | not run |
| Shaders s1-s5 (`.pica`) | [shader programming](PICA200/pica200-shader-programming.md) | assembles with picasso 2.7.2 (syntax only) | not run | not run | not run |
| `tools/pica_cmdlist_decode.py` | [pipeline](PICA200/pica200-command-pipeline.md#host-side-command-list-decoder) | self-test passes on synthetic lists only | n/a | n/a | n/a |
| Input, camera/Y2R, microphone, NFC/IR, system-info, QTM/MVD, UDS, `ac` and service-probe examples | [input and sensors](input-and-sensors.md), [cameras](camera-and-y2r.md), [microphone](microphone.md), [NFC and IR](nfc-and-infrared.md), [system info](system-info-and-power.md), [QTM and MVD](new-3ds-qtm-and-mvd.md), [local wireless](local-wireless.md), [service groups](service-groups.md) | yes (`-Wall -Wextra`, linked to ELF with `3dsx.specs`) | not run | not run | not run |
| citro3d lighting fragment | [citro3d fixed-function API](citro3d-fixed-function-api.md) | yes (compile only, `-Wall -Wextra`) | not run | not run | not run |
| `icachebench` (code-footprint probe) | [benchmarks](../benchmarks/README.md) | yes (`-Wall -Wextra` clean; devkitARM r68, libctru 2.7.1) | not run | not run | not run |
| ARMv6 packed SIMD (`UADD8`/`QADD16`) example | [CPU optimisation](cpu-optimization.md#techniques-and-how-to-check-them) | yes (`-Wall -Wextra`) | not run | not run | not run |
| Frame-time percentile helper | [performance](performance.md) | yes (builds with `bench.h`) | not run | not run | not run |
| Flush-range tracker fragment (`dirtyAdd`, `frameEndTracked`) | [memory and cache](memory-and-cache.md#flush-range-tracking) | yes (`-Wall -Wextra`, inside a stub `main`, linked with `3dsx.specs`, devkitARM r68) | not run | not run | not run |
| Stereo helper (`StereoScreenArg` / `StereoProjection`) | [stereo engineering](PICA200/stereo-engineering.md) | yes (`-Wall -Wextra`, linked) | not run | not run | not run |
| Fixed-step main loop | [frame pacing](PICA200/frame-pacing.md#separating-simulation-from-rendering) | yes | not run | not run | not run |
| ndsp streaming setup | [audio](audio.md#setting-up-a-channel) | yes | not run | not run | not run |
| Sprite demo (procedural sheet, animation, touch) | [2D rendering](PICA200/rendering-2d.md#5-a-complete-example-animated-sprite-with-touch) | yes (`-Wall -Wextra`, linked to ELF with `3dsx.specs`, devkitARM r68, citro2d 1.7.0-1) | not run | not run | not run |
| citro2d render-target, stereo-eye and text fragments | [2D rendering](PICA200/rendering-2d.md) | yes (inside a stub `main`, linked) | not run | not run | not run |
| 2D tile renderer (four tile strategies, texture/shape/bottom-screen/edit modes, scripted CSV mode; 30 configurations) | [2D tile renderer](PICA200/2d-tile-renderer.md#verification-status) | yes (`-Wall -Wextra`, linked to ELF; strategies 1 and 2 with picasso-assembled shader) | not run | not run | not run |
| Texture swizzle function | [2D rendering](PICA200/rendering-2d.md#generating-a-texture-in-code) | yes; algorithm matches `tex3ds -r` output for a 16x16 RGBA8 image (host check) | not run | not run | not run |
| Frame and render-target fragments | [GPU rendering](PICA200/gpu-rendering.md#anatomy-of-a-frame) | taken from `3ds-examples/graphics/gpu/simple_tri`, which builds | — | — | — |

Fragments that are not complete programs were compiled inside a minimal `main` with stub functions.

## Accuracy spot-checks

Facts re-checked against primary sources in this pass (November 2026), all consistent with the wiki:

| Claim | Page | Checked against |
| ----- | ---- | --------------- |
| 128-entry BTAC, three-entry return stack, 64-entry 2-way main TLB with 8 lockdown entries, eight-stage pipeline | [CPU](../hardware/cpu.md), [CPU optimisation](cpu-optimization.md) | ARM DDI 0360E §1.1, §1.7, §6.3, chapter 7 |
| Userland virtual map (heap, mappable, old/new linear FCRAM, QTM, MMIO, VRAM, DSP RAM, config pages) | [memory](../hardware/memory.md#constants-used-by-libctru) | libctru `os.h` `OS_*` constants |
| Clock tree and tick rate | [libctru](../homebrew/libctru.md#clocks-and-the-new-3ds-speed-up) | libctru `os.h` `SYSCLOCK_*`, `CPU_TICKS_PER_MSEC` |
| VFP11 throughput (single 1/cycle, double multiply 1/2 cycles, divide/sqrt ~15/29) | [CPU optimisation](cpu-optimization.md) | DDI 0360E §16.4 and table 19-16 |
| ndsp: 24 channels, 160-sample frames, `/3ds/dspfirm.cdc`, thread priority `0x18`, 4 KiB stack | [audio](audio.md) | libctru `source/ndsp/*` |

## Automated checks

- `tools/check_docs.py` — every relative link resolves, every `#anchor` exists (including glossary anchors), and every page in a console directory except the glossaries has a Sources section.
- `tools/build_benchmarks.sh` — builds every project under `ctr/benchmarks/` and fails on any warning.
- `.github/workflows/checks.yml` runs both on each push, the second inside the `devkitpro/devkitarm` container (whose toolchain version may be newer than the table above). The workflow has not yet been observed running.

## Technical details

- Pinning by commit, not branch: links to `master` change meaning over time. New pages should link source files at the pinned commit where a precise behaviour is claimed.
- The libctru commit is exactly the `v2.7.1` tag; libctru's `Changelog.md` file stops at 2.4.0, so later changes were read from the git history ([libctru version notes](../homebrew/libctru.md#version-notes)).

## Pitfalls

- **Updating a library and not the claims.** Re-read the cited files when the version changes; behaviour such as `C3D_FrameEnd`'s flush could change.
- **Treating "builds" as "verified".** A program that compiles can still be wrong; hardware results are what the status table is for.

## Open questions

- Upstream citro3d may have commits newer than the pinned one; none were reviewed for this page.

## Sources

- Local clones of the repositories above at the listed commits; `pacman -Q` output from the devkitPro installation used to build the examples.
- [citro2d `source/base.c`](https://github.com/devkitPro/citro2d/blob/master/source/base.c) — batching behaviour.

## Additional sources read in the November 2026 review pass

- 3dbrew mirror pages behind [PICA200 command pipeline](PICA200/pica200-command-pipeline.md) and [PICA200 shader programming](PICA200/pica200-shader-programming.md): GPU/Programming Guide (oldid 22469, 16 Dec 2023); other GPU pages as in the private mirror.
- picasso `d522455`: constants `MAX_VSH_SIZE` 512 and `MAX_OPDESC` 128 read from source.
- `tex3ds` 2.3.0 and `mkbcfnt` (general-tools 1.4.4): `--help` output re-read from the installed binaries; accepted format spellings were not exercised by running conversions.
- citro2d 1.7.0: `base.o` of the installed `libcitro2d.a` disassembled with `arm-none-eabi-objdump` (screen target creation, `C2D_Prepare` depth test, `C2D_Init` buffer sizes, `C2D_TargetClear`, `C2D_DrawRectangle` capacity check); `tex3ds` 2.3.0 test conversions (raw layout, `.t3x` subtexture values; the short options -a, -b and -c are rejected by the installed Windows binary although `--help` lists them).
- Third-party case-study sources (all `[reported]`, fetched November 2026, commits not pinned): the G-Diffuser postmortem (hypnotize.works/gdx-3ds/postmortem/native.html, entries dated 2026-08-11 to 2026-10-26) and `docs/NATIVE-RENDERER.md` (github.com/cruxxxxxx/gdx-3ds); the Super Mario 64 3DS port README (Wyatt-James/sm64-3ds-port); the Ocarina of Time port research (Andresac90/OOT-64-3DS-Port, `docs/3ds-native-speed-research.md`). Used on [case studies: ports](case-studies/case-studies-ports.md), [renderer architecture](PICA200/renderer-architecture.md) and the replay section of [PICA200 command pipeline](PICA200/pica200-command-pipeline.md). Figures were matched against raw downloads of each page; the chart-label counts noted on the case-study page were not.
- Commercial developer accounts (Nintendo Life, Digital Trends, Siliconera, Iwata Asks) used on [case studies: commercial games](case-studies/case-studies-commercial.md) and [stereo engineering](PICA200/stereo-engineering.md); prose only, no code.
