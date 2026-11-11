# NX source versions and verification status

Library behaviour changes between versions: a `deko3d` function can be renamed, a `libnx` input API can be removed, an Atmosphere default can move. When a page on this wiki says "`hidScanInput` no longer exists" or "`dkQueueWaitFence` is a GPU-side wait", that statement is true of a specific version of the source. This page records which versions the NX pages were checked against, how to read exactly that code, and what each code example on the wiki has (and has not) been verified to do.

## Before you start

- To read the exact code behind a claim, open the repository at the pinned commit below, for example `https://github.com/devkitPro/deko3d/blob/373bfd43d8b2d805fa2eb7d406dbb823032df958/include/deko3d.h`.
- When updating a page against newer sources, update this table in the same change and re-run the example compile check described under [Verification status](#verification-status).
- Wiki pages cite source files by repository-relative path. Pages that name no commit mean the commit in this table.

## Pinned source commits

Read in November 2026. Commit dates are author dates.

| Project | Commit | Commit date | Version | Used for |
| ------- | ------ | ----------- | ------- | -------- |
| [libnx](https://github.com/switchbrew/libnx) | `b156b4774fb2df7b93402169509266da7547fc4c` | 2026-10-19 | `Changelog.md` tops out at 4.10.0 | `PadState` input API, removal of the old HID API (4.1.3), `nxlinkStdio`, HID shared-memory size, services |
| [deko3d](https://github.com/devkitPro/deko3d) | `373bfd43d8b2d805fa2eb7d406dbb823032df958` | 2026-10-17 | — | Public header `include/deko3d.h`, `Primer.md`, `README.md` |
| [uam](https://github.com/devkitPro/uam) | `5a5afc2bae8b55409ab36ba45be63fcb73f68993` | 2020-12-16 | — | Offline shader compiler (GLSL to DKSH); Mesa-imported code |
| [switch-examples](https://github.com/switchbrew/switch-examples) | `669786898205b7beb25ff1731e72982e6d0397d3` | 2026-02-01 | — | `templates/application`, `graphics/deko3d/deko_basic`, `graphics/deko3d/deko_examples`, `network/nxlink_stdio`, directory map |
| [tegra_swizzle](https://github.com/ScanMountGoat/tegra_swizzle) | `d7ed318ce233a4d557d4a2f207ed5dc04ed813c0` | 2026-07-31 | — | Block-linear (GOB) layout notes in `tiling.md` |
| [Atmosphere](https://github.com/Atmosphere-NX/Atmosphere) | `4e3f37038d9e82dde7054897213ab32b2ecf651f` | 2026-11-10 | `docs/changelog.md` tops out at 1.12.2 | Component list, `override_config.ini`, `hbl_config` address-space options, DNS.mitm |
| [open-gpu-doc](https://github.com/NVIDIA/open-gpu-doc) | `9fdf5c4` | 2026-06-09 | — | Reference only; no wiki claim depends on it yet |
| [Homebrew-Guide](https://github.com/noahc3/Homebrew-Guide) (old guide) | `6d7b7bc` | 2022-08-02 | — | Printf debugging via nxlink, NetLoader mode, fatal-screen reading |

Ports and engines used by the [case studies](case-studies/case-study-sa2.md). None of their code was built or run by Coffeepot.

| Project | Commit | Commit date | Used for |
| ------- | ------ | ----------- | -------- |
| [sa2-nx](https://github.com/ChanseyIsTheBest/sa2-nx) | `baf523b8e93a1b02d8b73d13aebf86780f8cc0c6` | 2026-11-05 | SA2 case study (README, `docs/`) |
| [usf4-nx](https://github.com/Thorhax/usf4-nx) | `30ffae024fc30856bebe63a421221e35b5e2d9ea` | 2026-10-25 | USF4 case study |
| [nfsmw-nx](https://github.com/StevensND/nfsmw-nx) | `20f8dc122bf0cd95211606ebacd3fda550cb21a8` | 2026-11-03 | NFSMW case study, stock-clock table |
| [SwitchWakerHD](https://github.com/centollOS/SwitchWakerHD) | `9aec4e307a15a8f19fe15e8b19890d9403213220` | 2026-11-09 | Wind Waker HD case study |
| [srb2-switch](https://github.com/P-AS/srb2-switch) | `90194061a5da6af7df48be82a097affafbf70fb4` | 2023-09-09 | Open-engine case study |
| [taisei](https://github.com/taisei-project/taisei) | `baf029b05b625f5543543b8d96ecf38ae8e7f1b1` | 2026-10-28 | Open-engine case study |
| [stk-code](https://github.com/supertuxkart/stk-code) | `b2a9f223326422aa2dd4eea28319b0c5115fde03` | 2026-11-03 | Open-engine case study |
| [godot_switch](https://github.com/redthing1/godot_switch) | `0918643f62a6b7664a9d9bc6bcfdb280510a1b17` | 2022-08-28 | Open-engine case study |
| [dhewm3](https://github.com/fgsfdsfgs/dhewm3) | `920e759` | 2019-06-07 | dhewm3 case study |
| [Serious-Engine](https://github.com/fgsfdsfgs/Serious-Engine) | `13c4a1a` | 2020-05-12 | Serious Engine case study |
| [nxquake2](https://github.com/fgsfdsfgs/nxquake2) | `a8fdda02` | 2022-12-19 | nxquake2 case study |
| [melonDS (RSDuck fork with Switch frontend)](https://github.com/RSDuck/melonDS) | `6c6f1e8` | 2021-11-04 | melonDS case study |
| [max_nx](https://github.com/fgsfdsfgs/max_nx) | `264a0cc` | 2025-11-27 | Max Payne case study |

Wiki pages and papers read as saved copies (no commit; the page revision or edition is the version):

| Source | Revision read | Used for |
| ------ | ------------- | -------- |
| SwitchBrew [Memory layout](https://switchbrew.org/wiki/Memory_layout) | oldid 12866, edited 2024-07-31 | Userspace address-space widths |
| SwitchBrew [Flash Filesystem](https://switchbrew.org/wiki/Flash_Filesystem) | oldid 13575, edited 2025-07-13 | BIS partitions and where saves live |
| SwitchBrew [SD Filesystem](https://switchbrew.org/wiki/SD_Filesystem) | oldid 13379, edited 2025-06-07 | SD card layout |
| SwitchBrew [Hardware](https://switchbrew.org/wiki/Hardware) | oldid 15339, edited 2026-10-29 | Boards, SoC revisions, storage |
| SwitchBrew [Tegra X1](https://switchbrew.org/wiki/Tegra_X1) | oldid 14745, edited 2026-05-30 | SoC specification table |
| SwitchBrew [Title list](https://switchbrew.org/wiki/Title_list) | oldid 15462, edited 2026-11-10 | Sysmodule, applet and firmware-package title IDs |
| SwitchBrew [SVC](https://switchbrew.org/wiki/SVC) | oldid 15416, edited 2026-11-02 | System call numbers |
| SwitchBrew [HIPC](https://switchbrew.org/wiki/HIPC) | oldid 14205, edited 2026-01-09 | CMIF and TIPC |
| SwitchBrew [Joy-Con](https://switchbrew.org/wiki/Joy-Con) | oldid 13113, edited 2025-04-26 | Controller hardware |
| Chips and Cheese, [Nintendo Switch's iGPU: Maxwell Nerfed Edition](https://chipsandcheese.com/) | 2023-12-23 | Docked clock, shared memory, L2, FP16 behaviour of NVIDIA's own compiler |
| NVIDIA [Maxwell Tuning Guide](https://docs.nvidia.com/cuda/maxwell-tuning-guide/) | CUDA documentation 13.5 | Desktop-Maxwell compute guidance (see [rendering](rendering.md) for how little of it transfers) |

> Unknown: the SwitchBrew **Clocks** page, which several hardware pages used to cite for CPU, GPU and memory clocks, was not among the saved sources. Clock figures on this wiki are now cited to the ports that state them (`nfsmw-nx` `README.md` and `docs/platform-notes.md`) and are labelled `[reported]`.

## Installed toolchain

The packages the code examples on the NX pages were compiled with (from `pacman -Q` under the devkitPro MSYS2 on Windows 11):

| Package | Version |
| ------- | ------- |
| `devkitA64` | r30-1 (GCC 16.1.0, binutils 2.46.0, newlib 4.6.0.20260123) |
| `libnx` | 4.12.0-1 |
| `deko3d` | 0.5.0-1 |
| `uam` | 1.1.0-2 |
| `switch-tools` | 1.13.1-1 (`nxlink`, `nacptool`, `elf2nro`, `build_romfs`) |

The installed `deko3d` is older than the pinned clone: the clone's `deko3d.h` adds `dkFenceImport`, `dkImageFormatGetFlags`, `DkImageFormatFlags_*`, `DkDeviceFlags_YAxis*` and renames `DkImageFormat_A5BGR5_Unorm` to `A1BGR5_Unorm`. None of those appear in the wiki examples. All API names used in the examples exist in both headers. The installed `libnx` (4.12.0) is newer than the pinned clone (4.10.0 in its changelog); the examples use only APIs present in both.

## Verification status

| Item | Builds | Emulator-tested | Switch hardware |
| ---- | ------ | --------------- | --------------- |
| Every C fragment on [rendering](rendering.md), [performance and porting](performance-and-porting.md), [libnx and deko3d](../homebrew/deko3d-and-libnx.md) and [toolchain and workflow](../homebrew/toolchain-and-workflow.md) | Yes: compiled together as one program, `nx/engineering/examples/compile-check/` (`source/main.c` plus the upstream `deko_basic` Makefile), with `-Wall -O2` against the installed toolchain above; zero warnings from a clean copy | No | **No**; not run |
| The complete small game in [a first Switch game](first-game.md), `nx/engineering/examples/first-game/` (libnx only; `-lnx -lm`) | Yes: clean copy, `-Wall -O2`, zero warnings | No | **No**; not run, including audio, saving, crash dump and docked behaviour |
| Upstream `graphics/deko3d/deko_basic` from `switch-examples` | Yes: built to `.nro` with the installed toolchain, zero warnings | No | No |
| Command lines (`make`, `nxlink`, `uam`) | `nxlink --help` and `uam --help` read from the installed tools; `make` run on `deko_basic` | No | The `nxlink` transfer itself was not run (no console attached) |

To repeat the check: install `switch-dev`, copy `nx/engineering/examples/compile-check/` to a path without spaces, and run `make`. The page fragments are shortened copies of functions in that file (for example the page's `frame()` calls `recordScene`, defined there as a stub).

What "builds" means here: the fragment compiles and links. It does not mean the program draws correctly or that its synchronisation is right on hardware. Treat the synchronisation and save-file examples as `[documented]` readings of the headers, not as tested behaviour.

The check that the old wiki fragments were wrong was run the same way: `hidScanInput`, `hidKeysHeld`, `CONTROLLER_P1_AUTO`, `dkMemBlockUpload`, `dkCmdBufFinish`, `dkQueueSubmit` and a one-argument `dkCmdBufBindUniformBuffer` fail to compile against both the installed and the pinned headers.

## Open questions

- Run the examples on an Erista and a Mariko console in handheld and docked mode, and record the result here per model.
- Add a CI job that installs `switch-dev` and compiles the examples, so this page cannot drift from the code.
- Re-pin when `libnx` publishes a release newer than 4.10.0 in `Changelog.md`; the installed package already reports 4.12.0.

## Sources

- [libnx](https://github.com/switchbrew/libnx) at the pinned commit — `Changelog.md`, `nx/include/switch/runtime/pad.h`, `nx/include/switch/runtime/nxlink.h`.
- [deko3d](https://github.com/devkitPro/deko3d) at the pinned commit — `include/deko3d.h`.
- [switch-examples](https://github.com/switchbrew/switch-examples) at the pinned commit — `graphics/deko3d/deko_basic/source/main.c`, `templates/application/source/main.c`.
- Local `pacman -Q` output and `nxlink --help` / `uam --help` from the installed devkitPro tools — toolchain versions and flags.
