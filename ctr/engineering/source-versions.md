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

citro2d was read from its installed headers (package 1.7.0) and, for batching behaviour, from the upstream `master` branch's `source/base.c`.

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
| Fixed-step main loop | [frame pacing](frame-pacing.md#separating-simulation-from-rendering) | yes | not run | not run | not run |
| ndsp streaming setup | [audio](audio.md#setting-up-a-channel) | yes | not run | not run | not run |
| citro2d sprite and text fragments | [2D rendering](rendering-2d.md) | yes (inside a minimal program) | not run | not run | not run |
| Frame and render-target fragments | [GPU rendering](gpu-rendering.md#anatomy-of-a-frame) | taken from `3ds-examples/graphics/gpu/simple_tri`, which builds | — | — | — |

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
