# CTR memory

The 3DS has several separate pools of memory, not one big one. The main pool is 128 MB of slow-but-large RAM (256 MB on the New 3DS); next to it sit a few small, fast on-chip memories that hold the kernels and video data. About half of the main RAM is reserved for the operating system, so games get roughly 64 MB on an Old 3DS.

This page explains the pools and the address maps of the two big processors. How the OS hands out memory to programs is covered on the [kernel page](../software/kernel.md).

## Where it fits

Both the [ARM11 and ARM9](cpu.md) see the main RAM, but each has its own private on-chip memory (the ARM11 kernel's home is [AXI WRAM](../glossary.md#axi-wram); the ARM9's is its own internal RAM). The [GPU](gpu.md) renders into [VRAM](../glossary.md#vram) or main RAM and the [DSP](audio.md) has its own 512 KB window. Each processor gets its own view of addresses — the same real memory has a different number depending on who is looking.

## How it works

| Pool | Size | Who uses it | Notes |
| ---- | ---- | ----------- | ----- |
| **[FCRAM](../glossary.md#fcram)** (main memory) | 128 MB Old 3DS; 256 MB New 3DS | Everything | A Fujitsu "Fast Cycle" DRAM variant with low random-access latency, wired to behave like DDR memory |
| **[VRAM](../glossary.md#vram)** | 6 MB, inside the SoC | GPU first; framebuffers and fast textures | Split into two areas (VRAM A and B), four banks total |
| **[AXI WRAM](../glossary.md#axi-wram)** | 512 KB | ARM11 kernel | Fast on-chip RAM reached over the AXI bus |
| **ARM9 internal RAM** | 1 MB Old 3DS; 1.5 MB New 3DS | ARM9 kernel and Process9 | Visible only to the ARM9 |
| **ARM9 TCM** | 32 KB code + 16 KB data | ARM9 | Tightly-coupled memory, a DS-era feature |
| **DSP memory** | 512 KB | DSP and ARM11 | Double-buffered 256 KB halves so both sides can work at once |

The OS splits FCRAM into three **memory regions**: APPLICATION (the running game), SYSTEM (system services and applets) and BASE (kernel allocations and core modules). A game can ask for more by setting a metadata field called **APPMEMTYPE**, which makes the system reboot into a layout that skips loading the Home Menu first.

The GPU also does not display from its render target directly: games reserve separate [LCD framebuffers](display.md) in the format the screens expect.

## Technical details

### ARM11 physical memory map

| Old 3DS? | Address | Size | Description |
| -------- | ------- | ---- | ----------- |
| yes | `0x00000000` | `0x10000` | Boot ROM (secret code from `0x8000`) |
| yes | `0x00010000` | `0x10000` | Boot ROM mirror |
| yes | `0x10000000` | — | I/O registers (see [I/O](io-and-peripherals.md)) |
| yes | `0x17E00000` | `0x2000` | [MPCore private memory region](cpu.md#mpcore-private-memory-region) |
| no | `0x17E10000` | `0x1000` | L2C-310 level-2 cache controller (2 MB L2) |
| yes | `0x18000000` | `0x600000` | VRAM (two banks, A and B, of 3 MiB each) |
| no | `0x1F000000` | `0x400000` | New 3DS additional memory (used by head tracking and the New 3DS kernel; not GPU-accessible on Old 3DS) |
| yes | `0x1FF00000` | `0x80000` | DSP memory |
| yes | `0x1FF80000` | `0x80000` | AXI WRAM |
| yes | `0x20000000` | `0x8000000` | FCRAM |
| no | `0x28000000` | `0x8000000` | New 3DS FCRAM extension |
| yes | `0xFFFF0000` | `0x10000` | Boot ROM mirror |

L2 cache control: on a New 3DS, bit 0 of the 32-bit register at `0x17E10000+0x100` is set when the L2 cache is enabled via the `PTMSYSM:ConfigureNew3DSCPU` service call. The kernel enables it by setting that bit; it disables it by writing `0xFFFF` to `+0x77C`, waiting for bit 0 of `+0x730` to clear, writing 0 to `+0x0` and clearing bit 0 of `+0x100`.

### ARM9 physical memory map

| Old 3DS? | Address | Size | Description |
| -------- | ------- | ---- | ----------- |
| yes | `0x00000000` | `0x08000000` | Instruction TCM (repeats every `0x8000`) |
| yes | `0x01FF8000` | `0x8000` | Instruction TCM (kernel/process view) |
| yes | `0x07FF8000` | `0x8000` | Instruction TCM (boot ROM view) |
| yes | `0x08000000` | `0x100000` | ARM9-only internal memory (ARM7 internal regions mapped here too) |
| no | `0x08100000` | `0x80000` | New 3DS ARM9 extension (enabled by a CONFIG register) |
| yes | `0x10000000` | `0x08000000` | I/O |
| yes | `0x18000000` | `0x600000` | VRAM |
| yes | `0x1FF00000` | `0x80000` | DSP memory |
| yes | `0x1FF80000` | `0x80000` | AXI WRAM |
| yes | `0x20000000` | `0x08000000` | FCRAM |
| no | `0x28000000` | `0x08000000` | New 3DS FCRAM extension |
| yes | `0xFFF00000` | `0x4000` | Data TCM (enabled by boot ROM; disabled by Kernel9/arm9loader) |
| yes | `0xFFFF0000` | `0x10000` | Boot ROM (main region at `+0x8000`, disabled after boot) |

The ARM9 [MPU](../../glossary.md#mpu) regions programmed by `NATIVE_FIRM`/`SAFE_MODE_FIRM`'s Kernel9 include: `0xFFFF0000` (32 KB, read-only code), `0x01FF8000` (32 KB, RW), `0x08000000` (1 MB, 2 MB from firmware 8.0.0), `0x10000000` (128 KB, no execute), `0x10100000` (512 KB, no execute) and `0x20000000` (128 MB, 256 MB from 8.0.0, covering FCRAM). Different regions are used for `TWL_FIRM`/`AGB_FIRM`.

### FCRAM regions (`APPMEMTYPE`)

APPLICATION always starts at FCRAM offset 0. SYSTEM follows it; BASE is at the end. SYSTEM size = total − (APPLICATION + BASE).

| APPMEMTYPE | APPLICATION | SYSTEM | BASE | Notes |
| ---------- | ----------- | ------ | ---- | ----- |
| 0 (Old 3DS default) | 64 MB | `0x2C00000` (44 MB) | `0x1400000` (20 MB) | |
| 2 | 96 MB | `0xC00000` (12 MB) | 20 MB | |
| 3 | 80 MB | `0x1C00000` (28 MB) | 20 MB | |
| 4 | 72 MB | `0x2400000` (36 MB) | 20 MB | |
| 5 | 32 MB | `0x4C00000` (76 MB) | 20 MB | |
| 6 and 8 (New 3DS default) | 124 MB | `0x6400000` (100 MB) | `0x2000000` (32 MB) | New 3DS kernel only |
| 7 | 178 MB | `0x2E00000` (46 MB) | 32 MB | New 3DS kernel only; per the 3dbrew table (Copetti quotes "up to 176 MB") |

Types 6–8 were added in system version 8.0.0-18 and exist only on the New 3DS ARM11 kernel. All kernel-allocated memory lives in BASE. On firmware 4.5.0-10 with the Home Menu running, the SYSTEM region was about 0x1488000 bytes used and BASE about 0x1202000 used.

### ARM11 userland virtual map (NATIVE_FIRM)

Each process sees its own address space up to `0x40000000` (`0x20000000` before 8.0.0-18); above that is privileged-only.

| Virtual base | Maps to | Max size | Purpose |
| ------------ | ------- | -------- | ------- |
| `0x00100000` / `0x14000000` | — | `0x3F00000` | Executable (`.code`); the `0x00100000` base is used unless the "special memory" flag is set |
| `0x04000000` | — | — | Buffers mapped during IPC |
| `0x08000000` | process heap | up to `0x08000000` | Heap from `ControlMemory`; the `svcMapMemoryBlock` window |
| `0x10000000 − StackSize` | — | typically `0x4000` | Main thread stack |
| `0x10000000` | — | `0x04000000` | Shared memory |
| `0x14000000` | FCRAM+0 | `0x08000000` | "LINEAR"/GSP heap (physically contiguous) |
| `0x1E800000` | `0x1F000000` | `0x400000` | New 3DS extra memory (per exheader) |
| `0x1EC00000` | `0x10100000` | `0x400000` | I/O registers permitted by exheader |
| `0x1F000000` | `0x18000000` | `0x600000` | VRAM (per exheader) |
| `0x1FF00000` | `0x1FF00000` | `0x80000` | DSP memory (per exheader) |
| `0x1FF80000` | kernel page | `0x1000` | Configuration memory, read-only for all processes |
| `0x1FF81000` | kernel page | `0x1000` | Shared page |
| `0x1FF82000` | BASE region | threads × `0x1000` ÷ 8 | Thread-local storage |
| `0x30000000` | FCRAM+0 | `0x08000000` Old / `0x10000000` New | New LINEAR mapping (from 8.0.0-18) |

Executable pages are read-only; data pages are execute-never. The kernel's virtual map (valid for the earliest firmware sampled) puts mapped VRAM at `0xE8000000`, internal memory at `0xEFF00000`, main memory at `0xF0000000` and the IRQ block at `0xFFFEE000`. I/O registers map into userland at `physical = virtual − 0x1EC00000 + 0x10100000`.

### Constants used by libctru

`libctru`'s `os.h` names the virtual regions programs use: heap area `0x08000000–0x0E000000` (96 MiB), mappable area `0x10000000–0x14000000`, old (pre-8.x) linear FCRAM mapping at `0x14000000` → physical `0x20000000` (128 MiB), New 3DS QTM memory at `0x1E800000` → `0x1F000000` (4 MiB, the last 128 KiB reserved by the kernel), MMIO at `0x1EC00000` → `0x10100000` (4 MiB), VRAM at `0x1F000000` → `0x18000000` (6 MiB), DSP RAM `0x1FF00000` (512 KiB), kernel configuration page `0x1FF80000`, shared configuration page `0x1FF81000`, and the current linear FCRAM mapping at `0x30000000` → `0x20000000` (256 MiB). The kernel configuration page (read-only) starts with the kernel version, an update flag, the NS title ID and holds `app_memtype`, the three memory-region sizes and the FIRM version; field layouts are on the [libctru page](../homebrew/libctru.md).

On start-up `libctru` allocates an application heap and a linear heap from the process's memory allowance (default: half each, with the application heap capped at 24 MiB and the linear heap at 32 MiB unless the other side has room); VRAM allocations (`vramAlloc`) never cross the A/B bank boundary.

### Typical VRAM use by system applets

The system applets place their LCD framebuffers in VRAM: top-screen left/right 3D buffers (240×400×3 bytes each) at `0x1E6000` and `0x22C800` (framebuffer 0) and `0x273000`/`0x2B9800` (framebuffer 1), and the bottom screen (240×320×3) at `0x48F000` and `0x4C7800`. Ordinary games manage their own VRAM layout; the system clears these buffers when launching a game.

## Homebrew notes

Buffers shared with the GPU live in the "LINEAR"/GSP heap, which maps straight onto the start of FCRAM (see the virtual map above). Copetti notes that early [userland homebrew](../homebrew/exploits.md) could use only 64 MB of RAM, the default application region size. Today a `.3dsx` under Luma3DS inherits the current memory mode, while an installed `.cia` can request a larger one. Which pool to use for which buffer, how libctru splits the heaps, and when to flush the CPU cache are covered on [memory and cache](../engineering/memory-and-cache.md).

## Open questions

- Exact meaning of several small AXI WRAM kernel pages (`0x1FFF0000–0x1FFF4000`) is unknown in the source.
- 176 MB vs 178 MB for the largest New 3DS application memory type: sources disagree by 2 MB in rounding; the 3dbrew table (`0x0B200000`) is used here.

## Sources

- [3dbrew: Memory layout](https://www.3dbrew.org/wiki/Memory_layout) — physical maps, MPU regions, FCRAM region table, userland virtual map, VRAM framebuffer addresses.
- [3dbrew: Hardware](https://www.3dbrew.org/wiki/Hardware) — FCRAM part, VRAM size.
- [3dbrew: IO Registers](https://www.3dbrew.org/wiki/IO_Registers) — I/O address range and user mapping formula.
- [libctru `os.h`, allocator and `allocateHeaps.c`](https://github.com/devkitPro/libctru) — virtual-region constants, VRAM bank handling, default heap split.
- [Rodrigo Copetti, Nintendo 3DS Architecture: CPU](https://www.copetti.org/writings/consoles/nintendo-3ds/) — memory pools, FCRAM background, sizes.
- [Copetti, Nintendo 3DS Architecture: Operating System](https://www.copetti.org/writings/consoles/nintendo-3ds/) — application memory budget and APPMEMTYPE.
