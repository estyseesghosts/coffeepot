# CTR CPU

The 3DS has more processors than most people expect, but games only ever use one group of them. Games run on a dual-core processor. **New 3DS:** four cores, two of them mostly off-limits to games. Two older processors, inherited from the Nintendo DS and Game Boy Advance, work behind the scenes on security, input/output and backwards compatibility.

All of them sit inside one chip, a **system-on-a-chip** (**[SoC](../../glossary.md#soc)**) that Nintendo's boards label "CPU CTR" (Old 3DS); **New 3DS:** "CPU LGR". Which processor is "in charge" depends on the mode the console is running in: normal 3DS, DS/DSi, or Game Boy Advance.

## Where it fits

The [ARM11](../glossary.md#arm11) cores run games and system services under the [Horizon OS](../software/os.md). The [ARM9](../glossary.md#arm9) runs the security and storage half of that OS and is the first thing to run at power-on (see [boot process](../software/boot-process.md)). The [ARM7](../glossary.md#arm7) is only used for DS/DSi/GBA software. The processors share [memory](memory.md), talk through a message channel called [PXI](../glossary.md#pxi), and the ARM11 feeds the [GPU](gpu.md) with command lists.

## How it works

### The three processors and three modes

| Processor | Role in native 3DS mode | Role in DS/DSi mode | Role in GBA mode |
| --------- | ----------------------- | ------------------- | ---------------- |
| ARM11 MPCore | Runs games, apps and system services | Background: emulates relocated DS hardware (clock, power, keys, screen pipeline) | Background, as in native mode |
| ARM9 | Security, crypto, storage, I/O ("security processor") | Main processor (runs the DS/DSi game) | Background, as in native mode |
| ARM7 | Powered off | Co-processor with I/O access, as on the DS | Main processor (runs the GBA game) |

The three modes correspond to the [firmware images](../software/firmware.md) `NATIVE_FIRM`, `TWL_FIRM` and `AGB_FIRM`. Only the native firmware can be started from a cold power-on; it then reboots into the others on demand.

### What a game developer sees

Developers only target the ARM11. They write multi-threaded code and let the OS place [threads](../../glossary.md#thread) on cores. Policies are strict, though: the [kernel](../software/kernel.md) schedules the first core ("appcore") for the game and the second ("syscore") mostly for system work. **New 3DS:** two extra cores are mostly off-limits (see [Technical details](#technical-details)).

### New 3DS compatibility layer

> **New 3DS:** this section applies only to the New 3DS; the Old 3DS has no such features to switch.

A game's metadata says whether it targets the new hardware. If it does, the OS enables the faster clock, extra memory and level-2 [cache](../../glossary.md#cache) for that game. If not, those features stay off so old games see the hardware they expect. Luma3DS lets users override this per game with its `app_launch_new_3ds_cpu` setting (`off`, `clock`, `l2`, `clock+l2`; see [Luma3DS](../homebrew/luma3ds.md)).

## Technical details

### Clocks and core counts

| Item | Old 3DS | New 3DS |
| ---- | ------------- | --------------------------------- |
| ARM11 cores | 2 × MP11 | 4 × MP11 |
| ARM11 clock | 268 MHz (268,111,856 Hz ± small drift; exactly twice the ARM9 clock) | 268 MHz normally; up to 804 MHz (3×) for software that asks for it. The Home Menu runs at 804 MHz. |
| ARM11 L1 cache (per core) | 16 KB instruction + 16 KB data, 4-way set-associative | same |
| ARM11 L2 cache | none | optional 2 MB, shared, 16-way (L2C-310 controller at physical `0x17E10000`) |
| FPU | VFPv2 (VFP11) per core | same |
| ARM9 | ARM946E-S, ~134 MHz (134,055,927.9 Hz) | same, with extra internal memory |
| ARM7 | ARM7TDMI, DS/GBA mode only | same |

The ARM9 has a [memory protection unit](../../glossary.md#mpu), 32 KB instruction and 16 KB data tightly-coupled memory (TCM). The ARM11 has an [MMU](../../glossary.md#mmu) instead. Memory sizes and maps are on the [memory page](memory.md).

**New 3DS:** only one multiplier other than 1× exists in the official kernel: 3×. Other multipliers exist in hardware but no official code uses them.

**Clock tree.** `libctru` defines every clock from one base value, which pins down the exact figures: `SYSCLOCK_SOC` = 16,756,991 Hz; the system clock is 2× that (33,513,982 Hz); the SD/MMC clock is 2× the system clock; the ARM9 clock is 4× the system clock (134,055,928 Hz); the ARM11 clock is 2× the ARM9 clock (268,111,856 Hz); **New 3DS:** the ARM11 can also run at 2× (536,223,712 Hz) or 3× (804,335,568 Hz) its base clock. The same constants set the CPU tick rate (`CPU_TICKS_PER_MSEC`). A program asks for the New 3DS fast mode with `osSetSpeedupEnable`, which calls `PTMSYSM_ConfigureNew3DSCPU(3)` (clock and L2-cache bits both set; see [libctru](../homebrew/libctru.md)); guard it with a model check so the code still works on an Old 3DS.

> **New 3DS:** timing note: the faster clock was measured by reading `svcGetSystemTick` around an idle loop; that counter keeps ticking at 268 MHz in the fast mode, so elapsed time can be derived. It appears to apply only to the running application's cores.

### The MP11 core (from ARM's manual)

The ARM11 MPCore is documented in ARM's technical reference manual DDI 0360E (r1p0). The Old 3DS uses a two-core instance of it. **New 3DS:** four cores. Checked against DDI0360E `[documented]`: eight-stage pipeline (section 1.7), 128-entry BTAC and static prediction (6.2), three-entry return stack (6.3), 64-entry two-way unified main TLB with eight lockable entries (7.4 and CP15 TLB Type Register), 8-word cache lines (CP15 Cache Type Register), duplicated tag RAMs and migratory lines in the SCU. The manual describes a configurable design (1–4 cores, 16/32/64 KB caches); the sizes above are what the 3DS actually uses. Cache and TLB operations, page-table formats and memory types, the interrupt distributor and timer registers, the SCU and performance monitor registers, and the VFP11 modes and timings are all on [ARM11 MPCore internals](arm11-mpcore.md).

| Feature | Detail (ARM11 MPCore r1p0) |
| ------- | -------------------------- |
| Architecture | ARMv6K: ARM, Thumb and Jazelle (Java bytecode) instruction sets, ARMv6 SIMD (4×8-bit / 2×16-bit integer operations in 32-bit registers) and multiprocessing instructions (`LDREX`/`STREX` for bytes, halfwords, doublewords; `CLREX`; `WFI`; `WFE`/`SEV`; true NOP and `YIELD`) |
| Pipeline | 8 stages: Fe1, Fe2, De, Iss, then Sh/ALU/Sat/WBex (integer), MAC1–MAC3 (multiply-accumulate) or ADD/DC1/DC2/WBls (load/store) |
| Branch prediction | Dynamic via a 128-entry branch target address cache (BTAC); static prediction for branches that miss it; 3-entry return stack |
| Registers | 31 general-purpose 32-bit registers plus 6 dedicated; 16 visible at a time, the rest banked per exception mode |
| Caches | 4-way set-associative; line length 8 words (32 bytes). Instruction cache is virtually indexed, physically tagged; data cache is physically indexed and tagged, write-back, write-allocate, non-blocking with hit-under-miss |
| TLB | Two-level: micro-TLBs backed by a unified 64-entry, 2-way main TLB with 8 lockable entries |
| Coherency | Snoop Control Unit (SCU) keeps L1 data caches coherent using a MESI-type write-invalidate protocol with direct data intervention, duplicated tag RAMs and migratory lines |
| Bus | 64-bit AXI interfaces; the SCU houses the AXI interconnect |
| Coprocessors | CP15 (system control: MMU, cache, CPU ID), CP14 (debug), VFP11 floating point |
| FPU | VFP11 implementing VFPv2; 32 single-precision registers (pairs usable as doubles); IEEE 754 compliant |

### MPCore private memory region

Everything shared across cores sits in two 4 KB pages reached through a dedicated bus. On the 3DS these are at physical `0x17E00000` (size `0x2000`; see [memory](memory.md)). Offsets from the manual:

| Offset | Block |
| ------ | ----- |
| `0x0000–0x00FF` | SCU registers (control, configuration, CPU status, invalidate-all, performance monitor) |
| `0x0100–0x01FF` | Per-core interrupt interface (each core sees its own at the same address) |
| `0x0200–0x05FF` | Aliased interrupt interfaces for cores 0–3 (debug aliasing) |
| `0x0600–0x06FF` | Per-core timer and watchdog (each core sees its own) |
| `0x0700–0x0AFF` | Aliased timer/watchdog for cores 0–3 (`0x0700`, `0x0800`, `0x0900`, `0x0A00`; the manual's table is badly laid out in the PDF text, so this reading is inferred) |
| `0x0B00–0x0FFF` | Reserved |
| `0x1000–0x1FFF` | Global interrupt distributor |

Verified against DDI0360E Table 9-1 (section 9.1), Software Interrupt Register (section 10.5.10, offset `0xF00`) and timer interval equation (section 10.7.1).

The **distributed interrupt controller** routes hardware interrupts to cores and lets one core interrupt another by writing the Software Interrupt Register at distributor offset `0xF00` (bits 9:0 interrupt ID, 23:16 target-core list, 25:24 target filter: `00` list, `01` all but requester, `10` requester only). This is the "inter-core interrupt" mechanism used by the 3DS kernel.

Each core's **timer** and **watchdog** (interrupt IDs 29 and 30) have a 32-bit down-counter, an 8-bit prescaler and single-shot or auto-reload modes. Period = (prescaler + 1) × (load + 1) × 2 ÷ CPU clock. Timer registers: load `0x00`, counter `0x04`, control `0x08`, interrupt status `0x0C`; watchdog registers start at `0x20`.

### Inter-processor communication

- ARM11 ↔ ARM11: software-generated interrupts through the distributor.
- ARM11 ↔ ARM9 and ARM9 ↔ ARM7: FIFO channels inherited from the DS; the ARM11↔ARM9 one is called **PXI**.
- All processors can also exchange data through shared memory.

### DMA engines

ARM CoreLink DMA-330 controllers act as bus masters: **XDMA** next to the ARM9 (two channels at `0x1000C000`, 32-bit bus width, DMA-330 r0p0) and **CDMA** next to the ARM11 (eight channels, `0x10200000`). **New 3DS:** another DMA-330 revision (`0x10206000`) is used by the ARM11 kernel; the original CDMA is used only by the boot ROM there. There is also a simple AHB "NDMA" on the ARM9 side (`0x10002000`).

> Conflict: XDMA channel count. 3dbrew's IO Registers page says XDMA has two channels and a 32-bit bus width; Copetti's article lists four. This page follows 3dbrew because it gives a register block, revision (r0p0) and bus width that match the hardware description, whereas the Copetti figure is unreferenced. Unresolved: no hardware read-out of the channel count was found in the sources.

### New 3DS extra cores

> **New 3DS:** the Old 3DS has only two cores, so this section does not apply to it.

Core 3 is idle unless a title's metadata sets a flag that enables scheduling on it (confirmed in use by *Xenoblade Chronicles 3D* only, per Copetti). Core 4 is reserved for the software head-tracking that stabilises the 3D screen. Details of the scheduler are on the [kernel page](../software/kernel.md).

## Homebrew notes

Homebrew code is ordinary ARMv6K code built with `devkitARM` (see [development workflow](../homebrew/development-workflow.md)): use hard-float ABI (`-mfloat-abi=hard`) to match the VFP. Privilege levels matter: [userland](../../glossary.md#userland) code reaches only the ARM11; ARM9 access needs a [kernel/ARM9 exploit or custom firmware](../homebrew/exploits.md).

## Open questions

- The ARM11 distributor/timer behaviour above is from ARM's generic manual; which interrupt IDs the 3DS wires to which devices is not covered by the sources used here (see also [ARM11 MPCore internals](arm11-mpcore.md#open-questions)).
- Exact clock-multiplier registers (**New 3DS:** only) are not documented in the sources used.

## Sources

- [ARM11 MPCore Processor Technical Reference Manual (ARM DDI 0360E, r1p0)](https://developer.arm.com/documentation/ddi0360/e) — pipeline, caches, TLB, SCU/MESI coherency, private memory region, interrupt distributor, timers, VFP (register-level detail on [ARM11 MPCore internals](arm11-mpcore.md)).
- [3dbrew: Hardware](https://www.3dbrew.org/wiki/Hardware) — clock speeds, core counts, New 3DS 804 MHz behavior.
- [3dbrew: Memory layout](https://www.3dbrew.org/wiki/Memory_layout) — L2 cache controller address, MPCore private region location.
- [libctru `os.h` and `os.c`](https://github.com/devkitPro/libctru) — the clock tree constants and the New 3DS speed-up call.
- [Luma3DS configuration code](https://github.com/LumaTeam/Luma3DS) — `app_launch_new_3ds_cpu` options.
- [3dbrew: IO Registers](https://www.3dbrew.org/wiki/IO_Registers) — DMA controller addresses and revisions.
- [Rodrigo Copetti, Nintendo 3DS Architecture: CPU](https://www.copetti.org/writings/consoles/nintendo-3ds/) — the three-CPU design, operating modes, L1 cache sizes, New 3DS compatibility layer, DMA overview.
- [Copetti, Nintendo 3DS Architecture: Operating System](https://www.copetti.org/writings/consoles/nintendo-3ds/) — appcore/syscore and New 3DS core usage.
