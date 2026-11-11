# CTR memory

The 3DS has several separate pools of memory, not one big one. The main pool is 128 MB of slow-but-large RAM (**New 3DS:** 256 MB); next to it sit a few small, fast on-chip memories that hold the kernels and video data. About half of the main RAM is reserved for the operating system, so games get roughly 64 MB on an Old 3DS (**New 3DS:** up to 124 MB by default).

This page explains the pools and the address maps of the two big processors. How the OS hands out memory to programs is covered on the [kernel page](../software/kernel.md).

## Where it fits

Both the [ARM11 and ARM9](cpu.md) see the main RAM, but each has its own private on-chip memory (the ARM11 kernel's home is [AXI WRAM](../glossary.md#axi-wram); the ARM9's is its own internal RAM). The [GPU](gpu.md) renders into [VRAM](../glossary.md#vram) or main RAM and the [DSP](audio.md) has its own 512 KB window. Each processor gets its own view of addresses — the same real memory has a different number depending on who is looking.

## How it works

| Pool | Size (Old 3DS) | Who uses it | Notes |
| ---- | ---- | ----------- | ----- |
| **[FCRAM](../glossary.md#fcram)** (main memory) | 128 MB | Everything | A Fujitsu "Fast Cycle" DRAM variant with low random-access latency, wired to behave like DDR memory. **New 3DS:** 256 MB |
| **[VRAM](../glossary.md#vram)** | 6 MB, inside the SoC | GPU first; framebuffers and fast textures | Split into two areas (VRAM A and B), four banks total |
| **[AXI WRAM](../glossary.md#axi-wram)** | 512 KB | ARM11 kernel | Fast on-chip RAM reached over the AXI bus |
| **ARM9 internal RAM** | 1 MB | ARM9 kernel and Process9 | Visible only to the ARM9. **New 3DS:** 1.5 MB |
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
| no | `0x1F000000` | `0x400000` | New 3DS additional memory (used by head tracking and the New 3DS kernel; see below) |
| yes | `0x1FF00000` | `0x80000` | DSP memory |
| yes | `0x1FF80000` | `0x80000` | AXI WRAM |
| yes | `0x20000000` | `0x8000000` | FCRAM |
| no | `0x28000000` | `0x8000000` | New 3DS FCRAM extension |
| yes | `0xFFFF0000` | `0x10000` | Boot ROM mirror |

> **New 3DS:** L2 cache control: the 32-bit register at `0x17E10000+0x100` has bit 0 set only when `PTMSYSM:ConfigureNew3DSCPU` was called with bit 1 (the L2 flag) set in its input; all other bits are normally zero, so bit 0 is how the ARM11 kernel checks whether the additional cache hardware is on. The kernel enables it by setting that bit; it disables it by writing `0xFFFF` to `+0x77C`, waiting for bit 0 of `+0x730` to clear, writing 0 to `+0x0` and clearing bit 0 of `+0x100`. The same cache hardware is switched off by `KernelSetState` types 0, 2 and 7 (see the [SVC reference](../software/svc-reference.md#kernelsetstate-types)).

> **New 3DS:** the additional memory at `0x1F000000` is used by the QTM services and Kernel11, starting at offset `0x200000` with size `0x180000`. It is not accessible to the GPU on the Old 3DS (which has no such memory), and on the New 3DS only its first `0x200000` bytes (half) are GPU-accessible. The GSP module on both models contains virtual-to-physical conversion code for the whole region. Userland sees it at `0x1E800000`; the last 128 KiB are reserved by the kernel (see the libctru constants below).

#### ARM11 kernel-side physical layout (first sampled firmware)

3dbrew's detailed map of the AXI WRAM kernel area describes an early firmware; later versions differ (3dbrew keeps per-firmware virtual-map images that are not reproduced here).

| Physical range | Contents |
| -------------- | -------- |
| `0x18000000`-`0x18600000` | VRAM |
| `0x1FF80000`-`0x1FFAB000` | Kernel code |
| `0x1FFAB000`-`0x1FFF0000` | Slab heap (temporarily holds the boot processes) |
| `0x1FFF0000`-`0x1FFF4000` | Four unknown 4 KiB pages |
| `0x1FFF4000`-`0x1FFF5000` | Exception vectors |
| `0x1FFF5800`-`0x1FFF7C00` | L2 MMU tables (256 entries, `0x400` bytes each) for the thread-context region `0xFF4xx000` up to `0xFFBxx000`, one per `0x100000` of virtual space (`0x1FFF7800`-`0x1FFF7C00` is an unused table) |
| `0x1FFF7C00`-`0x1FFF8000` | L2 MMU table for `0xFFFxx000` |
| `0x1FFF8000`-`0x1FFFC000` | L1 MMU table (4096 entries) for core 0 |
| `0x1FFFC000`-`0x20000000` | L1 MMU table for core 1 |
| `0x20000000`-`0x28000000` | Main memory (FCRAM) |

The whole of FCRAM is cleared during `NATIVE_FIRM` boot, in region order, by the ARM11 kernel after it has read the FIRM launch parameters from FCRAM.

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

#### ARM9 MPU regions

The ARM9 [MPU](../../glossary.md#mpu) regions below are what Kernel9 programs in its start-up code. "Data" is read/write access and "Instr." is execute access (RO in an instruction column means executable); P = privileged mode, U = user mode.

`NATIVE_FIRM`/`SAFE_MODE_FIRM` Kernel9:

| Region | Address | Size | Data P / U | Instr. P / U |
| ------ | ------- | ---- | ---------- | ------------ |
| 0 | `0xFFFF0000` | 32 KB | RO / none | RO / none |
| 1 | `0x01FF8000` | 32 KB | RW / RW | RO / RO |
| 2 | `0x08000000` | 1 MB (from 8.0.0: 2 MB) | RW / RW | RO / RO |
| 3 | `0x10000000` | 128 KB | RW / RW | none / none |
| 4 | `0x10100000` | 512 KB | RW / RW | none / none |
| 5 | `0x20000000` | 128 MB (from 8.0.0: 256 MB) | RW / RW | none / none |
| 6 | `0x08000000` | 128 KB | RW / none | RO / none |
| 7 | `0x08020000` | 64 KB before 3.0.0-5; 32 KB from 3.0.0-5 | RW / none | RO / none |

3dbrew states that the New 3DS Kernel9 MPU settings equal the Old 3DS settings from 8.0.0 on. At the start of the Process9 function run in kernel mode via `svc 0x7B` during a firm launch some regions are changed, and the MPU is disabled at its end, just before the ARM9/ARM11 entry-point fields are used.

> **New 3DS:** the New 3DS `arm9loader` (which runs entirely in supervisor mode) uses only regions 0-3: `0xFFFF0000` 32 KB RO; `0x01FF8000` 32 KB RW (P only), executable; `0x08000000` 2 MB RW (P only), executable; `0x10000000` 128 KB RW (P only), not executable. Regions 4-7 are disabled.

`TWL_FIRM`/`AGB_FIRM` Kernel9 uses a different set: region 0 `0xFFFF0000` 32 KB; 1 `0x01FF8000` 32 KB; 2 `0x08000000` 1 MB (**New 3DS:** 2 MB); 3 `0x10000000` 2 MB; 4 `0x1FF00000` 512 KB; 5 `0x20000000` 128 MB (**New 3DS:** 256 MB); 6 `0x08000000` 256 KB before 3.0.0 and 128 KB from 3.0.0; 7 `0x08080000` 128 KB.

Boot9 (the boot ROM) uses: 0 `0x20000000` (`0x8000000`, no access); 1 `0x10000000` (`0x10000000`, RW data only); 2 `0x08000000` (`0x100000`, RW data only); 3 `0x08000000` (`0x400`, RW, executable); 4 `0xFFF00000` (`0x4000`, RW data); 5 `0x07FF8000` (`0x8000`, RW, executable); 6 `0xFFFF0000` (`0x10000`, all read-only, executable); 7 `0x1FFFE000` (`0x800`, RW data). Instruction-cacheable bits are `0x40` (region 6 only); data-cacheable and data-bufferable bits are `0x44` (regions 2 and 6 only), identical on both models [reverse-engineered: 3dbrew Memory layout].

> **New 3DS:** the "from 8.0.0" sizes above (2 MB internal memory, 256 MB FCRAM window) are what 3dbrew's Kernel9 table gives for system versions 8.0.0 and later, and it says the New 3DS settings match them. The 256 MB covers the New 3DS FCRAM size; whether an Old 3DS on 8.0.0 or later programs the larger windows too is not stated.

#### ARM9 instruction TCM at boot

The first `0x8000` bytes of ITCM (physical `0x01FF8000`, bootrom alias `0x07FF8000`) hold data left by the bootrom. Offsets are from `0x01FF8000` [reverse-engineered: 3dbrew Memory layout]:

| Offset | Size | Contents |
| ------ | ---- | -------- |
| `0x0` | `0x3700` | Uninitialised |
| `0x3700` | `0x100` | Code copied from the unprotected bootrom (via alias `0x07FFB700`) and run to disable access to the bootroms |
| `0x3800` | `0x100` | First `0x90` bytes of the plaintext [OTP](../../glossary.md#otp) when OTP hash verification succeeded, the rest cleared (the bootrom leaves neither console-specific keys nor the OTP hash in ITCM) |
| `0x3900` | `0x200` | NAND sector 0 |
| `0x3B00` | `0x200` | Plaintext header of the FIRM that Boot9 loaded (the only place Boot9 keeps loaded FIRM headers) |
| `0x3D00`-`0x4000` | `0x100` each | RSA-2048 moduli for RSA-engine slots 0-3 (slots 0 and 1 set by the bootrom) |
| `0x4100` | `0x800` | RSA-2048 keys: four `0x200`-byte slots, modulus at +0 and private exponent at +`0x100`. FIRM never seems to use them and they are unrelated to the RSA keyslot 0 used for 6.0/7.0 key generation |
| `0x4900` | `0x400` | RSA moduli copied by the bootrom from AXI WRAM (retail source `0x1FFFD000`, dev source `0x1FFFD400`): the first `0x100` bytes are the modulus for CFA NCCH and gamecard NCSD headers, the next `0x100` the CXI accessdesc signature modulus (written to RSA slot 1 by `NATIVE_FIRM`); `0x200` bytes at `+0x200` are unknown |
| `0x4D00` | `0x80` | Unknown, not console-unique. The 6.0/7.0 `NATIVE_FIRM` key-init function clears it if its first `0x10` bytes are non-zero |
| `0x4D80` | `0x64` | Initialised by Process9 and the bootrom: NAND CID at `0x4D84` (`0x10` bytes), total NAND sector count (from an MMC command) at `0x4DC4` |
| `0x5000` | `0x2470` | DSi-related data copied from AXI WRAM `0x1FFFA000`: RSA-1024 moduli (System Menu, DSi Wi-Fi/sound firmware, base DSi apps, DSiWare and cartridge headers), the DSi common key, key X/Y values for modcrypt, ES blocks and eMMC partition crypto, and the `0x1048`-byte Blowfish tables for DSi and DS cartridge crypto. On the 3DS keyslots 0x01 and 0x03 have `0xE1A00005` as their last word, as on retail DSi |
| `0x7C00` | `0x100` | From 9.5.0, the FIRM header used during firm launch |

### FCRAM regions (`APPMEMTYPE`)

APPLICATION always starts at FCRAM offset 0. SYSTEM follows it; BASE is at the end. SYSTEM size = total FCRAM size − (APPLICATION + BASE). The set of possible sizes is fixed; `APPMEMTYPE` is read from configuration memory, which is filled from the firmware launch parameters. Most processes can only allocate from the one region named in their kernel flags, and applications only get the larger New 3DS regions when their extended header asks for it.

| APPMEMTYPE | APPLICATION | SYSTEM | BASE | Notes |
| ---------- | ----------- | ------ | ---- | ----- |
| 0 (Old 3DS default) | 64 MB | `0x2C00000` (44 MB) | `0x1400000` (20 MB) | Also used by the Old 3DS kernel when the type is not 2-5 |
| 2 | 96 MB | `0xC00000` (12 MB) | 20 MB | |
| 3 | 80 MB | `0x1C00000` (28 MB) | 20 MB | |
| 4 | 72 MB | `0x2400000` (36 MB) | 20 MB | |
| 5 | 32 MB | `0x4C00000` (76 MB) | 20 MB | |
| 6 and 8 (**New 3DS** default) | 124 MB | `0x6400000` (100 MB) | `0x2000000` (32 MB) | New 3DS kernel only |
| 7 | 178 MB | `0x2E00000` (46 MB) | 32 MB | New 3DS kernel only; per the 3dbrew table (Copetti quotes "up to 176 MB") |

Region start offsets within FCRAM (3dbrew): APPLICATION `0x0`; SYSTEM at `0x4000000` (type 0), `0x6000000` (2), `0x5000000` (3), `0x4800000` (4), `0x2000000` (5), `0x7C00000` (6/8) or `0xB200000` (7); BASE at `0x6C00000` for types 0-5 and `0xE000000` for types 6-8. Types 6–8 were added in system version 8.0.0-18 (support in `NS`) and exist only on the New 3DS ARM11 kernel, where they are the only types supported; type 6 is the default there, and the New 3DS kernel treats any value other than 7 or 8 as 6 (type 8 is marked with a question mark in 3dbrew). All kernel-allocated memory lives in BASE, and most system modules run in BASE too. 

Memory use sampled on system version 4.5.0-10 with the default `APPMEMTYPE` on retail (Old 3DS) [reverse-engineered: 3dbrew Memory layout]:

| Region | Size | Used with Home Menu loaded | Used with Internet Browser instead | Free with Home Menu | Free with Browser |
| ------ | ---- | -------------------------- | ---------------------------------- | ------------------- | ----------------- |
| APPLICATION | `0x4000000` | `0x0` | n/a | `0x4000000` | n/a |
| SYSTEM | `0x2C00000` | `0x1488000` | `0x2A50000` | `0x1778000` | `0x1B0000` |
| BASE | `0x1400000` | `0x1202000` | `0x1236000` | `0x1FE000` | `0x1CA000` |

An application's `.text` + `.rodata` + `.data` in the application region are mapped at `FCRAM + APPMEMALLOC − (page-aligned size of those segments)`, where the total for the memory type is stored in configuration memory; the application's `.bss` is mapped at `CODEADDR − (.bss size aligned down to a page)`.

### ARM11 userland virtual map (NATIVE_FIRM)

Each process sees its own address space up to `0x40000000` (`0x20000000` before 8.0.0-18); above that is privileged-only.

| Virtual base | Maps to | Max size | `MapMemoryBlock` window? | Purpose |
| ------------ | ------- | -------- | ------------------------ | ------- |
| `0x00100000` / `0x14000000` | — | `0x3F00000` | No | Executable (`.code`); the `0x00100000` base is used unless the "special memory" flag is set |
| `0x04000000` | — | — | No | Buffers mapped during IPC |
| `0x08000000` | process heap | up to `0x08000000` | Yes | Heap from `ControlMemory`; the `svcMapMemoryBlock` window |
| `0x10000000 − StackSize` | — | typically `0x4000` | n/a | Main thread stack |
| `0x10000000` | — | `0x04000000` | Yes | Shared memory |
| `0x14000000` | FCRAM+0 | `0x08000000` | No | "LINEAR"/GSP heap (physically contiguous) |
| `0x1E800000` | `0x1F000000` | `0x400000` | No | New 3DS extra memory (per exheader) |
| `0x1EC00000` | `0x10100000` | `0x400000` | No | I/O registers permitted by exheader |
| `0x1F000000` | `0x18000000` | `0x600000` | No | VRAM (per exheader) |
| `0x1FF00000` | `0x1FF00000` | `0x80000` | No | DSP memory (per exheader) |
| `0x1FF80000` | kernel page | `0x1000` | No | Configuration memory, read-only for all processes |
| `0x1FF81000` | kernel page | `0x1000` | No | Shared page: readable by all processes; writable only with the exheader "shared page writing" kernel flag |
| `0x1FF82000` | BASE region | threads × `0x1000` ÷ 8 | No | Thread-local storage |
| `0x30000000` | FCRAM+0 | `0x08000000` (**New 3DS:** `0x10000000`) | No | New LINEAR mapping (from 8.0.0-18) |

A process's address space ends at `0x40000000` (`0x20000000` before 8.0.0-18); memory from there up is accessible only in privileged mode, and everything below is unique to the process. The Old 3DS kernel range-checks LINEAR addresses against a size of `0x08000000`, the New 3DS kernel against `0x10000000`, while system-module code converting virtual to physical addresses uses `0x10000000` on both [reverse-engineered: 3dbrew Memory layout]. Executable pages are read-only; data pages are execute-never. The kernel's virtual map (valid for the earliest firmware sampled) puts mapped VRAM at `0xE8000000`, internal memory at `0xEFF00000`, main memory at `0xF0000000` and the IRQ block at `0xFFFEE000`. I/O registers map into userland at `physical = virtual − 0x1EC00000 + 0x10100000` (equivalently, a process's virtual address is the physical address plus `0xEB00000`); kernel mappings of I/O have no fixed equivalence. Each process's accessible I/O pages are listed in its exheader; applications normally have none.

> Conflict: largest New 3DS application region. 3dbrew's memory-layout table gives APPMEMTYPE 7 an APPLICATION size of `0x0B200000` (178 MB); Copetti says "up to 176 MB". This page follows 3dbrew because it is a table with exact byte sizes that also sum to 256 MB with SYSTEM and BASE (178 + 46 + 32); the 2 MB difference is unexplained (rounding or a different firmware). Unresolved.

### ARM11 kernel virtual map and thread-context pages

The kernel's own virtual map (3dbrew documents it exactly only for the earliest sampled firmware, "FW0B"; later firmware images are not reproduced here) [reverse-engineered: 3dbrew Memory layout]:

| Virtual range | Maps | Notes |
| ------------- | ---- | ----- |
| `0xE8000000`-`0xE8600000` | VRAM (`0x18000000`) | |
| `0xEFF00000`-`0xF0000000` | Internal memory (`0x1FF00000`) | |
| `0xF0000000`-`0xF8000000` | Main memory | |
| `0xFF401000`, `0xFF403000`, `0xFF405000`, ... | Per-thread context pages (physical `0x27FC7000`, `0x27FC2000`, `0x27FBB000`, ... in FCRAM) | One `0x1000`-byte page per thread, every other page. See below |
| `0xFFF00000`-`0xFFF45000` | Slab heap | |
| `0xFFF60000`-`0xFFF8B000` | Kernel code | |
| `0xFFFCC000`-`0xFFFDB000` | Kernel I/O mappings, one 4 KiB page each: I2C bus 2 (`0x10144000`), PDC/LCD (`0x10400000`), PDN (`0x10141000`), PXI (`0x10163000`), PAD (`0x10146000`), LCD (`0x10202000`), DSP (`0x10140000`), XDMA (`0x10200000`) | |
| `0xFFFEE000`-`0xFFFF0000` | MPCore private memory region / IRQ block (`0x17E00000`) | |
| `0xFFFF0000`-`0xFFFF1000` | Exception vectors | |
| `0xFFFF1000` | The current core's `KCoreContext` | See [kernel objects](../software/kernel-objects.md#per-core-state) |
| `0xFFFF2000`-`0xFFFF6000`, `0xFFFE5000`-`0xFFFE9000` | L1 MMU table views | |
| `0xFFFFB000`-`0xFFFFE000` | L2 MMU tables (`0x1FFF5000`-`0x1FFF8000`) | |

The remaining kernel pages map the AXI WRAM pages `0x1FFF0000`-`0x1FFF4000` and `0x1FFF8000`-`0x1FFFC000` in several places; their purpose is not documented, which is the open question below.

**Thread-context pages (`0xFF4XX000`).** Each thread is given one `0x1000`-byte page in this region: the first page, `0xFF401000`, is for the first created thread, `0xFF403000` for the second. It holds the thread's supervisor-mode stack and the context data used for context switching (field layout on [kernel objects](../software/kernel-objects.md#thread-structures)). When the IRQ, prefetch-abort, data-abort and undefined-instruction handlers are entered from user mode they store `LR` and `SPSR` on the supervisor stack and then switch to supervisor mode. There is no dedicated block for `r0-pc`: user-mode registers are saved on the supervisor stack when an interrupt such as the context-switch timer fires. On `NATIVE_FIRM` these pages are in FCRAM; on `TWL_FIRM` they are in AXI WRAM.

### TWL_FIRM userland memory

`TWL_FIRM` (the DS/DSi compatibility mode) has its own ARM11 process layout. The kernel maps these regions; the process later calls `KernelSetState` type 4, which unmaps(?) `0x00300000..0x04300000`, `0x08000000..0x0FE00000` and `0x10000000..0xF8000000` [reverse-engineered: 3dbrew Memory layout].

| Virtual base | Physical base | Size | Description |
| ------------ | ------------- | ---- | ----------- |
| `0x00100000` | `0x1FFAB000` (`0x1FFAC000` on newer `TWL_FIRM` such as v6704) | `0x55000` | Code and read-only data copied from the process's `0x00300000` region; `.bss` is here too |
| `0x00155000` | `0x18555000` | `0xAB000` | Not described |
| `0x00200000` | `0x18500000` | `0x100000` | Not described |
| `0x00300000` | `0x24000000` | `0x4000000` | Start of the ARM11 process's `.text` |
| `0x08000000` | `0x20000000` | `0x7E00000` | Not described (see the DS RAM window below) |
| `0x1EC00000` | `0x10100000` | `0x400000` | I/O |
| `0x1F000000` | `0x18000000` | `0x600000` | VRAM |
| `0x1FF00000` | `0x1FF00000` | `0x80000` | DSP memory |

Inside the `0x08000000` window: the DS(i) `0x02000000` RAM appears at `virtual = (DSRAMOffset × 4) + 0x08000000`, where `DSRAMOffset = DSRAMAddr − 0x02000000`; the loaded SRL binary sits at physical `0x27C00000` (initially the dev DSi launcher SRL, copied there by the ARM11 process); a TWL NAND bootloader image (same format and verification as the DSi stage-2 bootloader) is copied to `0x27D00000` (when loaded from FCRAM the bootloader hashes are not checked, unlike when loading from FS); and the SRL header is at `0x27DF7000` (`0x1000` bytes).

### Constants used by libctru

Verified against libctru `os.h` `[documented]` (all constants below match the clone) and `allocateHeaps.c` (linear heap gets half of the remaining memory rounded down to a page, application heap the rest; the application heap is capped at 24 MiB, preferring to grow the linear heap, and the linear heap has its own cap). `libctru`'s `os.h` names the virtual regions programs use: heap area `0x08000000–0x0E000000` (96 MiB), mappable area `0x10000000–0x14000000`, old (pre-8.x) linear FCRAM mapping at `0x14000000` → physical `0x20000000` (128 MiB), **New 3DS:** QTM memory at `0x1E800000` → `0x1F000000` (4 MiB, the last 128 KiB reserved by the kernel), MMIO at `0x1EC00000` → `0x10100000` (4 MiB), VRAM at `0x1F000000` → `0x18000000` (6 MiB), DSP RAM `0x1FF00000` (512 KiB), kernel configuration page `0x1FF80000`, shared configuration page `0x1FF81000`, and the current linear FCRAM mapping at `0x30000000` → `0x20000000` (256 MiB on the New 3DS; the Old 3DS has 128 MiB of FCRAM, see the virtual map above). The kernel configuration page (read-only) starts with the kernel version, an update flag, the NS title ID and holds `app_memtype`, the three memory-region sizes and the FIRM version; field layouts are on the [libctru page](../homebrew/libctru.md).

On start-up `libctru` allocates an application heap and a linear heap from the process's memory allowance (default: half each, with the application heap capped at 24 MiB and the linear heap at 32 MiB unless the other side has room); VRAM allocations (`vramAlloc`) never cross the A/B bank boundary.

### Typical VRAM use by system applets

The system applets place their LCD framebuffers in VRAM (offsets from the start of VRAM). Top screen, left/right eye (240×400×3 bytes each): framebuffer 0 left `0x1E6000`-`0x22C500`, right `0x22C800`-`0x272D00`; framebuffer 1 left `0x273000`-`0x2B9500`, right `0x2B9800`-`0x2FFD00`. Bottom screen (240×320×3): `0x48F000`-`0x4C7400` and `0x4C7800`-`0x4FF800`. The "3D right first framebuffer" address stored in the LCD register is set to the left buffer when 3D is off. The system does not change these addresses when launching ordinary applications, which manage their own VRAM layout, but it clears these buffers on launch [reverse-engineered: 3dbrew Memory layout].

## Homebrew notes

Buffers shared with the GPU live in the "LINEAR"/GSP heap, which maps straight onto the start of FCRAM (see the virtual map above). Copetti notes that early [userland homebrew](../homebrew/exploits.md) could use only 64 MB of RAM, the default application region size on the Old 3DS. Today a `.3dsx` under Luma3DS inherits the current memory mode, while an installed `.cia` can request a larger one (**New 3DS:** up to the larger regions in the `APPMEMTYPE` table). Which pool to use for which buffer, how libctru splits the heaps, and when to flush the CPU cache are covered on [memory and cache](../engineering/memory-and-cache.md).

## Open questions

- Exact meaning of several small AXI WRAM kernel pages (`0x1FFF0000–0x1FFF4000`) is unknown in the source (3dbrew marks them "?").
- 3dbrew's per-firmware kernel virtual-map pages (FW0B through New 3DS 11.1) are image-only in the source material and were not transcribed; only the FW0B list is reproduced.
- Several TWL_FIRM userland regions (`0x00155000`, `0x00200000`, `0x08000000` generally) have no description in the source.

## Sources

- [3dbrew: Memory layout](https://www.3dbrew.org/wiki/Memory_layout) — physical maps, MPU regions for each ARM9 kernel and the boot ROM, ARM9 ITCM contents, FCRAM region table and sampled usage, kernel and userland virtual maps, `TWL_FIRM` userland, thread-context pages, VRAM framebuffer addresses.
- [3dbrew: SVC](https://www.3dbrew.org/wiki/SVC) — `KernelSetState` cache and firmlaunch behaviour referenced above; see the [SVC reference](../software/svc-reference.md).
- [3dbrew: Hardware](https://www.3dbrew.org/wiki/Hardware) — FCRAM part, VRAM size.
- [3dbrew: IO Registers](https://www.3dbrew.org/wiki/IO_Registers) — I/O address range and user mapping formula.
- [libctru `os.h`, allocator and `allocateHeaps.c`](https://github.com/devkitPro/libctru) — virtual-region constants, VRAM bank handling, default heap split.
- [Rodrigo Copetti, Nintendo 3DS Architecture: CPU](https://www.copetti.org/writings/consoles/nintendo-3ds/) — memory pools, FCRAM background, sizes.
- [Copetti, Nintendo 3DS Architecture: Operating System](https://www.copetti.org/writings/consoles/nintendo-3ds/) — application memory budget and APPMEMTYPE.
