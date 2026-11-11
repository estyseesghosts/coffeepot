# libctru

**libctru** is the standard C library for writing 3DS programs that run as ordinary ARM11 user-mode code. It sits between the operating system and a program: it wraps [system calls](../software/kernel.md) and [system services](../software/services.md), sets up memory, threads and the screen, and leaves higher-level features (such as a game engine, or a port of SDL) to other libraries. It was originally Smealum's `ctrulib` and is now maintained by the devkitPro group alongside the `devkitARM` toolchain.

Who it is for: homebrew developers and anyone reading the code to learn how the console is used. Requires `devkitARM`; its documentation is built from the source headers at [devkitpro.github.io/libctru](https://devkitpro.github.io/libctru/).

## What you need

- `devkitARM` (installed with `dkp-pacman`; the `3ds-dev` metapackage installs libctru and friends).
- A way to run the result: a `.3dsx` via the Homebrew Launcher, or other packaging ([development workflow](development-workflow.md)).
- Match versions: libctru and Luma3DS releases are kept in sync, and Luma3DS needs a current libctru to build ([Luma3DS](luma3ds.md)).

## How it works

### What it initialises

At start-up (`ctru_init`, `appInit`) libctru:

1. Asks the kernel how much memory the process may commit (the `COMMIT` resource limit; for an application this equals its APPMEMALLOC).
2. Splits it between the **application heap** (allocated with `svcControlMemory` at virtual `0x08000000`, up to 96 MiB of address space) and the **[linear heap](../glossary.md#linear-heap)** (physically contiguous memory used for GPU/DSP/DMA buffers): by default half each, but the application heap is capped at 24 MiB and the linear heap at 32 MiB when the other side has room to grow. A program can override both sizes with `__ctru_heap_size` and `__ctru_linear_heap_size`.
3. Sets up the C library's heap on the application heap, thread-local state and arguments.

### Allocators

| Allocator | Provides |
| --------- | -------- |
| `malloc` heap | Normal memory (application heap) |
| `linearAlloc` | Physically contiguous memory, 0x80-byte aligned by default; required for GPU, DSP and many service buffers |
| `vramAlloc`, `vramAllocAt` | Memory in the 6 MiB of [VRAM](../hardware/memory.md), which is split into two banks (A and B, 3 MiB each); `vramAllocAt` chooses a bank and allocations never cross a bank boundary |
| `mappableAlloc` | Finds free virtual address space for mapping shared memory |

### Subsystems

| Area | Provided by |
| ---- | ----------- |
| System calls | `svc.h`: wrappers for every [SVC](../software/kernel.md) with documented parameters and the resource-limit, arbitration, process and thread enums |
| Threads | `thread.h`: `threadCreate(core_id)` where core 0 is the application core, core 1 the system core, `-1` all cores and `-2` the default from the exheader; the C library's `pthread`, C11 threads and `std::thread` are supported experimentally (2.3.0) |
| Synchronisation | Light locks, events, semaphores built on kernel objects |
| Time and information | `os.h`: system clock constants, system version, memory-region sizes, tick counters, 3D slider, headset detection, Wi-Fi strength, `osSetSpeedupEnable` |
| Graphics | `gfx.h`: a simple framebuffer API over the `gsp` service (portrait panels: the top screen is 240 wide and 400 tall); `gpu/`: GPU command and shader (SHBIN) handling used by `citro3d` ([picasso and citro3d](picasso-and-citro3d.md)) |
| Audio | `ndsp`: a userland driver for the [DSP](../hardware/audio.md); `csnd` for the legacy block |
| Services | `services/*.h`: `ac`, `act`, `am`, `apt`, `boss`, `cam`, `cdcchk`, `cfgnor`, `cfgu`, `csnd`, `dsp`, `frd`, `fs`, `fspxi`, `gspgpu`, `gsplcd`, `hid`, `httpc`, `ir`, `irrst`, `loader`, `mcu*` (battery, LEDs, RTC...), `mic`, `mvd`, `ndm`, `news`, `nfc`, `nim`, `ns`, `ptm*`, `pxi*`, `qtm`, `soc`, `sslc`, `uds`, `y2r` and more |
| Applets | `swkbd` (software keyboard), `error`, `miiselector` |
| Devices | `romfs_dev` (RomFS as a filesystem), `archive_dev`, `3dslink` (redirect `stdout`/`stderr` to the PC that sent the program), `gdbhio_dev` (standard streams and file I/O through an attached GDB) and a console for text output |

### Clocks and the New 3DS speed-up

`os.h` defines the clock tree from a base value, which resolves the clock figures listed on [CPU](../hardware/cpu.md#technical-details):

| Name | Formula | Value |
| ---- | ------- | ----- |
| `SYSCLOCK_SOC` | base | 16,756,991 Hz |
| `SYSCLOCK_SYS` | SOC × 2 | 33,513,982 Hz |
| `SYSCLOCK_SDMMC` | SYS × 2 | 67,027,964 Hz |
| `SYSCLOCK_ARM9` | SYS × 4 | 134,055,928 Hz |
| `SYSCLOCK_ARM11` | ARM9 × 2 | 268,111,856 Hz |
| `SYSCLOCK_ARM11_LGR1` | ARM11 × 2 | 536,223,712 Hz (**New 3DS:** 2×) |
| `SYSCLOCK_ARM11_LGR2` / `_NEW` | ARM11 × 3 | 804,335,568 Hz (**New 3DS:** 3×) |

**New 3DS:** `osSetSpeedupEnable(true)` calls `PTMSYSM_ConfigureNew3DSCPU(3)` — a value with the clock bit and the L2-cache bit both set; with `false` it passes 0. This is the call that corresponds to the "New 3DS faster mode" described in [CPU](../hardware/cpu.md).

### Kernel and shared pages

`os.h` also describes the read-only **kernel configuration page** (`0x1FF80000`) and the **shared system configuration page** (`0x1FF81000`) from the [memory map](../hardware/memory.md): kernel version, update flag, NS title ID, environment and unit info, boot environment, firmlaunch flags, `app_memtype`, the three memory-region sizes, FIRM version, and for the shared page the time reference (value in ms, tick, measured `sysclock_hz`, drift), Wi-Fi MAC address and strength, network state, 3D slider value, 3D and battery LED state, menu title IDs and headset status.

Virtual-address constants in `os.h`: heap area `0x08000000–0x0E000000`, mappable area `0x10000000–0x14000000`, old (pre-8.x) linear FCRAM mapping at `0x14000000` (physical `0x20000000`, 128 MiB), MMIO `0x1EC00000` (4 MiB), VRAM `0x1F000000` (6 MiB), DSP RAM `0x1FF00000` (512 KiB), and the current linear FCRAM mapping at `0x30000000` (`OS_FCRAM_SIZE` is a 256 MiB window; the Old 3DS has 128 MiB of FCRAM behind it. **New 3DS:** 256 MiB, per the [memory map](../hardware/memory.md)). **New 3DS:** QTM memory at `0x1E800000` (4 MiB, last 128 KiB reserved).

## Technical details

### Audio sample rate

`NDSP_SAMPLE_RATE` is `SYSCLOCK_SOC / 512` = **32,728.498… Hz**. This is the DSP's output sample rate, which the 3dbrew figure "32728 Hz" rounds.

### QTM (New 3DS head tracking)

libctru 2.4.0 added full documentation of the **[QTM](../glossary.md#qtm)** services (`qtm:u`, `qtm:s`, `qtm:c`). QTM tracks and predicts the user's head and eye positions using the inner camera (320×240 at 30 FPS) plus the gyroscope to predict positions between camera frames (doubling the effective rate), manages the infrared LED for low light, and positions the **parallax barrier** by driving a TI TCA6416A I²C-to-parallel expander (12 barrier positions per interocular-distance unit; barrier pattern and polarity are dithered about every 10 ms). Nintendo's "Super Stable 3D" shifts the barrier according to the measured eye midpoint relative to the calibration data; all calculations assume a 62 mm interocular distance. QTM is absent on Old 3DS and may be unavailable on the New 2DS XL. `qtm:c` allows direct control of the barrier pattern. See also [displays](../hardware/display.md).

### Version notes

Selected changelog facts: 2.1.0 introduced VRAM bank handling and changed the platform macro to `__3DS__`; 2.1.1 gave every thread a predictable initial `FPSCR` (default-NaN and flush-to-zero, round to nearest — see [performance](../engineering/cpu-optimization.md#floating-point-and-fixed-point)); 2.1.2 added `clock_gettime`; 2.2.0 added deliver-argument support to the chainloader; 2.3.0 added experimental standard threading (`pthread`, C11 threads, `std::thread`) and the `CTR_` prefix on `ALIGN`/`PACKED`/`DEPRECATED`; 2.4.0 added the full QTM service support (a breaking API change) and `MCUHWC_SetInfoLedPattern`.

Later releases (from the git history; the `Changelog.md` file stops at 2.4.0): **2.5.0** added the GSP performance-log API (`GSPGPU_SetPerfLogMode`, `GSPGPU_GetPerfLog` — per-operation GPU timings, see [performance](../engineering/performance.md#per-operation-gpu-times-the-gsp-performance-log)), `aptLaunchSystemApplet`, full ACT and FRD service implementations, and woke the DSP correctly around system applets. **2.6.x** brought ANSI console fixes (bright colours, 1-based coordinates), `getaddrinfo`/`sockaddr` length fixes and play/step-history `ptm` calls. **2.7.0** completed the AM service, added the `ptm:plays` service and an `appmemtype` parameter to `NS_RebootToTitle`. **2.7.1** completed the MCU services, clamps the application heap to the 96 MiB heap area (the excess goes to the linear heap, see [memory and cache](../engineering/memory-and-cache.md#how-libctru-splits-it)), and fixed `MCUHWC_Get3dSliderLevel`, `gdbhio` file modes and console scrolling.

## Limits and risks

- libctru exposes what the OS exposes: without kernel privileges many hardware blocks are unreachable.
- Some services are not available to every process (the exheader decides), and **New 3DS:** some services (`mvd`, `qtm`) exist only there.
- Breaking changes occur between major versions (for example `__3DS__`, QTM API changes in 2.4.0); pin the toolchain version used to build a project.
- Using libctru well — heap sizing, cache flushing, timing, threads on the second core — is covered on the engineering pages: [memory and cache](../engineering/memory-and-cache.md), [performance](../engineering/performance.md), [debugging](../engineering/debugging.md) (3dslink, GDB file I/O, exception handlers).

## Sources

- [libctru (devkitPro)](https://github.com/devkitPro/libctru) — source, headers, `Changelog.md`; [API documentation](https://devkitpro.github.io/libctru/).
- [3dbrew: Homebrew Libraries and Tools](https://www.3dbrew.org/wiki/Homebrew_Libraries_and_Tools) — library description.
- [3dbrew: Memory layout](https://www.3dbrew.org/wiki/Memory_layout) — memory areas libctru references.
- [Rodrigo Copetti, Nintendo 3DS Architecture: Anti-Piracy and Homebrew](https://www.copetti.org/writings/consoles/nintendo-3ds/) — history of ctrulib.
