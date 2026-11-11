# CTR ARM11 MPCore internals

The processor that runs 3DS games is ARM's **ARM11 MPCore**: two small ARM11 cores (called **MP11** cores) plus the shared parts that tie them together. This page is the reference for how those cores' caches, address translation, interrupts, timers, performance counters and floating-point unit actually behave. It is written from ARM's own manual, so it describes the processor design; where the 3DS sets a configuration option (cache size, core count) the page says so, and where the sources do not show what the 3DS does, it says that too. For the big picture (three processors, clocks, memory sizes) start with the [CPU page](cpu.md).

**New 3DS:** the same design with four cores and an extra level-2 cache. Anything below that depends on the core count is marked.

## Where it fits

The MP11 cores run the [Horizon](../software/os.md) user processes and the ARM11 [kernel](../software/kernel.md). Their data caches are kept in step by the **Snoop Control Unit** (SCU), which also contains the bus switch to the rest of the [SoC](../../glossary.md#soc) ([memory map](memory.md)). Interrupts from hardware blocks reach the cores through the **interrupt distributor**, and every core has a private timer and watchdog; all three live in one 8 KB register window (the *private memory region*, physical `0x17E00000` on the 3DS per [memory](memory.md)). Software controls the cores themselves through **CP15**, the system-control coprocessor, using `MRC`/`MCR` instructions. The floating-point unit (VFP11) is a separate coprocessor next to each core.

Engineering consequences (what to flush when, how to read the counters) are on [memory and cache](../engineering/memory-and-cache.md) and [CPU optimisation](../engineering/cpu-optimization.md); this page holds the facts they rest on.

## How it works

- **Caches.** Each core has its own instruction cache and data cache. Software that shares memory with the GPU, the DSP or another core must clean and invalidate caches explicitly with CP15 operations (listed below); the hardware only keeps the *cores'* data caches consistent with each other, and only for memory marked as shared.
- **Address translation.** A small translation cache (**TLB**) in front of a two-level page table decides where every address goes and whether it may be read, written or executed. On a miss the hardware walks the page table itself, reading it from memory, not from the data cache.
- **Branch prediction.** The core guesses where branches go so its eight-stage pipeline stays full.
- **Interrupts.** A distributor ranks pending interrupts by a 4-bit priority and hands the best one to each core. A core can interrupt another by writing one register.
- **Timers.** Each core has a down-counting timer and a watchdog that can run as a second timer.
- **Counters.** Each core has a cycle counter and two event counters; the SCU has more for cache-to-cache traffic.
- **Floating point.** VFP11 does IEEE 754 single and double arithmetic in hardware; in the right mode it never needs software help.

## Technical details

All items are `[documented]` in ARM DDI 0360E (ARM11 MPCore TRM, r1p0) unless stated. Section numbers are the manual's.

### Configuration the manual allows, and what the 3DS uses

| Option (manual Table 1-3) | Range | 3DS |
| ------------------------- | ----- | --- |
| MP11 cores | 1 to 4 | 2 (Old 3DS) / **New 3DS:** 4 |
| Instruction cache per core | 16, 32 or 64 KB | 16 KB per [CPU](cpu.md) (Copetti) |
| Data cache per core | 16, 32 or 64 KB | 16 KB per [CPU](cpu.md) (Copetti) |
| Interrupt input lines | 0 to 224 in steps of 32 | > Unknown: not in the sources used |
| Master AXI ports | 1 or 2 | > Unknown: not in the sources used |
| VFP per core | included or not | included (every 3DS core has VFP11) |

> Unknown: the 3DS cache sizes come from Copetti, not from a register read-out. The ID registers below would confirm them; a hardware read on an Old 3DS and a New 3DS has not been recorded here.

### CP15 identification registers (reads, privileged only)

| Register | Instruction | Value / meaning |
| -------- | ----------- | --------------- |
| Main ID | `MRC p15,0,Rd,c0,c0,0` | `0x410FB024` reset value: implementer `0x41` (ARM), variant 0, architecture `0xF`, part `0xB02`, revision 4 (§3.4.1, Table 3-2) |
| Cache Type | `MRC p15,0,Rd,c0,c0,1` | Separate I and D caches; write-back; "Format C" way-based lockdown. Size field: `101` = 16 KB, `110` = 32 KB, `111` = 64 KB; associativity field `010` = 4-way; line length field `10` = 8 words (§3.4.2) |
| TLB Type | `MRC p15,0,Rd,c0,c0,3` | Unified TLB (U = 0), 0 lockable instruction entries, 8 lockable data/unified entries (§3.4.3) |
| CPU ID | `MRC p15,0,Rd,c0,c0,5` | Bits 3:0 = this core's number (0 to 3), bits 11:8 = cluster ID (§3.4.4). Software uses it to find out which core it runs on |
| Feature registers | `c0,c1,{0-7}` | ARM, Thumb-1 and Java-interface states supported; Thumb-2 and TrustZone-style security extensions not supported; v6 debug model (CP14); VMSAv6 virtual memory; no TCM (§3.4.5, tables 3-6 to 3-17) |

### Control register (CP15 c1)

`MRC/MCR p15,0,Rd,c1,c0,0`. Always change it with read-modify-write (§3.4.7, Table 3-20).

| Bit | Name | Meaning |
| --- | ---- | ------- |
| 28 | TEX remap | `1`: TEX/C/B bits are looked up through the remap registers (c10) |
| 23 | XP | `1`: ARMv6 page-table format (no subpage permissions; enables XN, nG, S, APX) |
| 22 | U | `1`: ARMv6 unaligned word/halfword access permitted |
| 13 | V | Exception vectors at `0xFFFF0000` instead of `0x00000000` |
| 12 | I | Instruction cache enable |
| 11 | Z | Program-flow (branch) prediction enable; 0 after reset |
| 2 | C | Data cache enable |
| 1 | A | Strict alignment faults; takes priority over U |
| 0 | M | MMU enable |

The **auxiliary control register** (`c1,c0,1`, §3.4.8) holds: bit 5 SMP/nAMP (take part in cache coherency), bit 4 L1/L2 exclusive mode, bit 2 static prediction enable, bit 1 dynamic prediction enable, bit 0 return-stack enable. Switching a core between asymmetric (AMP, the reset state) and symmetric (SMP) mode needs a fixed sequence: write the SCU CPU mode, disable interrupts, clean and invalidate the whole data cache, then change bit 5, then re-enable interrupts (the reverse order for leaving SMP). In AMP mode, memory marked shared and write-back is treated as non-cacheable.

The **coprocessor access register** (`c1,c0,2`, §3.4.9) gates VFP: bit pairs for CP10 and CP11 (`00` denied, `01` privileged only, `11` full); all denied after reset, and an IMB sequence is required after changing it. The VFP also has its own enable bit, `FPEXC.EN`.

### Caches

| Property | Value | Source |
| -------- | ----- | ------ |
| Line | 8 words (32 bytes) | §3.4.2 |
| Ways | 4 | §3.4.2 |
| Data cache | physically indexed, physically tagged, write-back, write-allocate on every miss; non-blocking | §5.6, §7.2 |
| Instruction cache | virtually indexed, physically tagged; 32 KB and 64 KB variants can alias with 4 KB pages | §5.11.3 |
| Write policies | Write-through memory behaves as non-cacheable; "write-back no-allocate" behaves as write-back write-allocate | §5.6 |
| Store buffer | 4 entries of 64 bits + 32-bit address; merges writes; reads can hit in it | §7.2.6 |
| Line fill buffers | 2 (two outstanding misses); fill data is streamed to the core | §7.2.7 |
| Eviction buffer | one 256-bit line | §7.2.9 |
| Tag RAM | per way up to 512 lines x 22 bits (20 tag, MESI exclusive, valid) | §7.2.5 |
| Ordering of strongly ordered/device/non-cacheable | the core drains the store buffer first (and line-fill buffers for strongly ordered, locked and exclusive accesses) | §7.2.2 |
| Aborts | strongly ordered, locked and exclusive-store accesses give *precise* aborts; cacheable, device and normal non-cacheable accesses give *imprecise* ones | §7.2.3 |

A write to a non-cacheable-bufferable address followed by a cacheable write to the same line while the first is still in the store buffer is a programming error with unpredictable result (§7.2.6).

**Cache maintenance operations** (`MCR p15,0,Rd,c7,...`, privileged unless marked; §3.4.17, Table 3-29):

| Operation | CRm, Op2 | Rd holds |
| --------- | -------- | -------- |
| Wait for interrupt | c0, 4 | SBZ |
| Invalidate entire instruction cache (also flushes branch target cache) | c5, 0 | SBZ |
| Invalidate instruction cache line | c5, 1 (by address), c5, 2 (by set/way) | address / set-way |
| Flush prefetch buffer (user mode allowed) | c5, 4 | SBZ |
| Flush entire branch target cache / one entry | c5, 6 / c5, 7 | SBZ / address |
| Invalidate entire data cache | c6, 0 | SBZ |
| Invalidate data line | c6, 1 / c6, 2 | address / set-way |
| Invalidate both caches | c7, 0 | SBZ |
| Clean entire data cache | c10, 0 | SBZ |
| Clean data line | c10, 1 / c10, 2 | address / set-way |
| Data synchronization barrier (drain write buffer; user mode allowed) | c10, 4 | SBZ |
| Data memory barrier (user mode allowed) | c10, 5 | SBZ |
| Clean and invalidate entire data cache | c14, 0 | SBZ |
| Clean and invalidate data line | c14, 1 / c14, 2 | address / set-way |

Notes from the manual: address-form operations need no alignment (bits 4:0 ignored); invalidation applies to locked lines too; after invalidating instruction-cache *lines* the branch target cache must be flushed explicitly (not needed after an *entire* instruction-cache invalidate); whole-cache operations can be interrupted and restart; they are blocking.

Set/way operand format: bits 31:30 = way, set number starts at bit 5 (`S+2:5`) where S is 7 for 16 KB, 8 for 32 KB and 9 for 64 KB caches (Tables 3-30 and 3-31). A loop over all 4 ways and all sets cleans a whole cache (the manual's example is for 32 KB).

**Instruction memory barrier (IMB).** Self-modifying or freshly loaded code needs: clean the data cache for the modified range, invalidate the instruction cache (and branch target cache), flush the prefetch buffer. The manual recommends exposing it as `SWI 0xF00000` (everything) and `SWI 0xF00001` (range) and warns that an IMB can cost many thousands of cycles (§6.4, §6.5.1). > Unknown: those SWI numbers are the manual's recommendation, not a statement about Horizon; how the 3DS kernel exposes the same sequence to processes is on [memory and cache](../engineering/memory-and-cache.md#keeping-caches-coherent).

**Barriers.** The ARMv6K barriers are CP15 operations: data memory barrier `c7,c10,5`, data synchronization barrier `c7,c10,4`, prefetch flush `c7,c5,4` (§6.5, Table 3-29).

**Data-cache lockdown** (c9, §3.4.21). Only the way-based "Format C" scheme: bits 3:0 of `MCR p15,0,Rd,c9,c0,0` lock ways 3 to 0 (a set bit means no new lines are allocated into that way). If all four ways are locked the core treats way 0 as unlocked. The manual gives a seven-step procedure; it requires a data synchronization barrier before changing the register and that the data is cacheable and not already in the cache. > Unknown: whether Horizon ever uses lockdown.

### Address translation (MMU)

- **Micro-TLBs and main TLB.** Eight-entry fully associative micro-TLBs, one for instruction fetches and one for data. Behind them one unified **main TLB**: 8 fully associative lockable entries plus a 64-entry two-way set-associative array (§5.2, §7.4). TLB maintenance operations flush the micro-TLBs too. Main-TLB misses are resolved by a hardware page-table walk.
- **Page-table walk bypasses the data cache.** "MPCore hardware page table walks do not cause a read from the level one Unified/Data Cache" (§5.11). Page-table entries written by software must reach memory (clean the data cache line) before the walker can see them. The attributes the walker uses (outer cacheable, shared) come from bits in the translation table base registers.
- **Two translation table base registers** (c2): TTBR0 and TTBR1, and a control register whose `N` field picks how many high address bits select TTBR1 (§3.4.10 to 3.4.12). The first-level table holds 4096 4-byte entries per 4 GB (16 KB when N = 0).
- **Mappings.**

| Size | Level | Notes |
| ---- | ----- | ----- |
| 4 KB small page / extended small page | second | Extended small page carries TEX, XN |
| 64 KB large page | second | |
| 1 MB section | first | |
| 16 MB supersection | first | 16 identical first-level entries; domain 0 only |

  A coarse second-level table has 256 four-byte entries and covers 1 MB. A descriptor with bits 1:0 = `00` is unmapped (translation fault); software may use the remaining bits.
- **Page-table formats.** Control register bit 23 (XP) = 0 gives the ARMv4/v5-compatible format with sub-page permissions (all pages global, executable, non-shared). XP = 1 gives the ARMv6 format: **nG** (per-process, matched on ASID), **S** (shared), **XN** (never execute), **APX** (extra permission bit) and the full TEX field.
- **ASID.** 8 bits in the low bits of the Context ID register (c13, op2 1). Changing it requires a data synchronization barrier first and flushes the branch target cache (§3.4.25).
- **Domains.** 16 domains, each in client or manager mode in c3 (§5.5.1).
- **Access permissions (ARMv6 format, XP = 1; Tables 5-1 and 5-2).**

| APX | AP[1:0] | Privileged | User |
| --- | ------- | ---------- | ---- |
| 0 | 00 | none | none |
| 0 | 01 | read/write | none |
| 0 | 10 | read/write | read-only |
| 0 | 11 | read/write | read/write |
| 1 | 01 | read-only | none |
| 1 | 10 or 11 | read-only | read-only |

- **Memory types (TEX / C / B, Tables 5-3 and 5-4; remap off).**

| TEX | C | B | Meaning |
| --- | - | - | ------- |
| `000` | 0 | 0 | Strongly ordered (always shared) |
| `000` | 0 | 1 | Shared device |
| `000` | 1 | 0 | Normal, write-through, no write allocate (acts as non-cacheable on MP11) |
| `000` | 1 | 1 | Normal, write-back, no write allocate (acts as write-back write-allocate) |
| `001` | 0 | 0 | Normal, non-cacheable |
| `001` | 1 | 1 | Normal, write-back, write allocate |
| `010` | 0 | 0 | Non-shared device |
| `1BB` | A | A | Normal cached: BB = outer policy, AA = inner policy (`00` non-cacheable, `01` write-back write-allocate, `10` write-through, `11` write-back no-allocate) |

  Other combinations are reserved. For Normal memory the page's **S** bit selects shared/non-shared. **Coherency across cores is guaranteed only when the page is write-back write-allocate, marked shared, and the core is in SMP mode**; a shared cacheable page that does not meet all three is treated as non-cacheable (§5.2.5). Mapping the same physical memory with two different types gives unpredictable behaviour (§5.7).
- **TEX remap.** With control bit 28 set, TEX[0], C and B index the primary and normal memory region remap registers (`c10,c2,0/1`, PRRR/NMRR), freeing TEX[2:1] for software (§5.6.3).
- **Faults.** Fault status encodings (data and instruction, in c5): alignment `00001`, instruction cache maintenance fault `00100`, translation (section `00101`, page `00111`), access bit (section `00011`, page `00110`), domain (section `01001`, page `01011`), permission (section `01101`, page `01111`), precise external abort `01000`, imprecise external abort `10110`, debug event `00010`, external abort on translation first/second level `01100`/`01110`. The fault address register (c6) is set only for precise data faults; instruction faults must be located from the saved `r14` (§5.10).
- **Translation lookup helper.** Writing a virtual address to `c7,c8,{0..3}` translates it with privileged/user read/write permission checks; the physical address (or the fault status, with bit 0 set) is read back from `c7,c4,0` (§3.4.19). The result also reports the memory type and inner/outer cache attributes.
- **TLB maintenance** (c8, §3.4.20): invalidate whole TLB, by address, by ASID or address-only, for instruction, data or unified TLB. **Lockdown** (c10 `c0,0`): bit 0 P and bits 28:26 victim select which of the 8 lockable entries a table walk fills. A TLB debug control register in c15 can turn off main-TLB loading for debugging.

### Unaligned access

Control register bits U (22) and A (1) select the model (Table 4-1, Table 3-19):

| U | A | Behaviour |
| - | - | --------- |
| 0 | 0 | ARMv5-style: misaligned word loads are rotated, halfword/word address bits ignored |
| 0 | 1 | ARMv5-style with alignment faults |
| 1 | 0 | Unaligned halfword and word loads/stores work; `LDM`, `STM`, `LDRD`, `STRD`, exclusive and coprocessor accesses must be aligned or they fault |
| 1 | 1 | Strict: any unaligned halfword/word access faults |

> Unknown: which setting Horizon applies to user processes is not stated in the sources used (already tracked in [CPU optimisation](../engineering/cpu-optimization.md#open-questions)).

### Branch prediction

Three layers, enabled by control bit Z and individually by the auxiliary control register (§6.2, §6.3):

| Predictor | Detail |
| --------- | ------ |
| Dynamic: branch target address cache (BTAC) | 128 entries, direct-mapped, virtually addressed, 2-bit saturating history (strong/weak taken/not taken). Hits predict with zero-cycle delay. Both taken and not-taken branches are allocated. Flush it when mappings change |
| Static | Forward conditional branches predicted not taken, backward taken; the manual says about 65% of branches are fully covered |
| Return stack | 3 entries. Pushes on `BL`/`BLX label`/`BLX reg` (the register form is not otherwise predicted). Pops for `BX r14`, `LDM sp!,{...,pc}`, `LDR pc,[sp...]`. Only unconditional returns; an empty stack gives no prediction |

An untaken branch costs 1 cycle and a taken one three or more without prediction (§6.2). Branch folding happens before the instruction-cache lookup (§7.3). Performance event 0x09 counts folded instructions.

### Snoop Control Unit (SCU)

Registers at offset `0x0000-0x00FF` of the private memory region (byte accessible; §9.1.1 to 9.1.8):

| Offset | Register | Notes |
| ------ | -------- | ----- |
| `0x00` | Control | Bit 0 SCU enable (coherency on/off); bits 4:1 per-core write access to SCU registers (default all set); bits 8:5 interrupt-interface aliasing per core; bits 12:9 timer/watchdog aliasing per core. Reset `0x00001FFE` |
| `0x04` | Configuration (RO) | Bits 1:0 number of cores minus one; bits 7:4 which cores are in SMP mode; bits 15:8 tag RAM size per core (`00` 16 KB, `01` 32 KB, `10` 64 KB) |
| `0x08` | CPU status | 2 bits per core: `00` normal, `10` dormant, `11` powered off/absent |
| `0x0C` | Invalidate all (WO) | 4 bits per core select ways whose tag RAM entries are invalidated; atomic |
| `0x10` | Performance monitor control | Bit 0 enable, bit 1 reset, bits 15:8 interrupt enables, bits 23:16 overflow flags (write 1 to clear) |
| `0x14`, `0x18` | Monitor event select 0 and 1 | 8 bits per counter: counters MN0 to MN3 in register 0, MN4 to MN7 in register 1 |
| `0x1C-0x38` | Counters MN0 to MN7 | Number present: 2 cores = MN0 to MN3; 4 cores = MN0 to MN7 |

SCU event numbers (Table 9-9): `1`-`4` core 0 to 3 line fill missed all other cores (went to external memory); `5`-`8` line fill hit another core's cache; `9`-`12` core expected to hold a line but did not; `13` line migration (dirty line moved directly between cores); `14`/`15` master port 0/1 read busy; `16`/`17` write busy; `18` read sent to external memory; `19` write sent to external memory; `31` cycle count; `0` counter disabled.

**Coherency protocol.** MESI (modified, exclusive, shared, invalid), write-invalidate. Three optimisations (§7.1.1): *direct data intervention* (clean data copied between cores without going to memory), *duplicated tag RAMs* (the SCU asks only cores that really hold the line) and *migratory lines* (dirty data moved core to core without a memory write-back).

**L2 interface** (chapter 8): the MPCore has one or two AXI master ports for the next cache level or memory; L1/L2 can be configured inclusive (default) or exclusive (auxiliary control bit 4). **New 3DS:** the optional 2 MB L2 cache is an L2C-310 controller outside the MPCore ([CPU](cpu.md#clocks-and-core-counts)). > Unknown: how the Old 3DS wires its master ports is not in the sources used.

### Interrupt distributor (private memory region `0x1000-0x1FFF`)

Interrupt IDs (§10.3): `0-15` software-generated, private to each core; `16-28` not described in the sources used here; `29` private timer; `30` private watchdog; `31` legacy interrupt pin of the core; `32` and up: hardware interrupt lines (up to 224), each programmed as level-high or rising-edge. Priority is 4 bits, **0 is highest, 15 lowest**; equal priorities are broken by lowest ID. Spurious interrupt ID is 1023. The distributor clock is half the CPU clock, so external interrupt pulses must be at least two CPU cycles wide.

> Unknown: which 3DS hardware block is wired to which interrupt ID is not covered by the sources used here.

| Offset | Register |
| ------ | -------- |
| `0x000` | Distributor control (bit 0 = enable; if 0 no interrupts reach any core) |
| `0x004` | Controller type (bits 7:5 core count minus one, bits 4:0 interrupt-line groups of 32) |
| `0x100-0x11C` / `0x180-0x19C` | Set-enable / clear-enable (reset `0x0000FFFF` for IDs 0-31, 0 above) |
| `0x200-0x21C` / `0x280-0x29C` | Set-pending / clear-pending |
| `0x300-0x31C` | Active bits (read-only) |
| `0x400-0x4FC` | Priority (one byte per interrupt) |
| `0x800-0x8FC` | CPU targets (one byte per interrupt, bit n = core n) |
| `0xC00-0xC3C` | Configuration (2 bits per interrupt: edge/level, 1-N / N-N model) |
| `0xD00` | Interrupt line level (read) |
| `0xF00` | Software interrupt: bits 9:0 ID, 23:16 target list, 25:24 filter (`00` list, `01` all but requester, `10` requester only) |
| `0xFE0-0xFFC` | Peripheral and PrimeCell ID registers (`0x90, 0x13, 0x04, 0x00`; `0x0D, 0xF0, 0x05, 0xB1`) |

Software model per interrupt: **1-N** (one core takes it and it clears for all) or **N-N** (each core handles its own copy). Two cores acknowledging within a few cycles can still both take a 1-N interrupt.

Per-core CPU interface (offset `0x100`, 32-bit access only; §10.6): `0x00` control (bit 0 enable), `0x04` priority mask (reset `0xF0`; only interrupts with priority value below the mask are delivered, so a mask of `0xF0` lets priorities 0 to 14 through), `0x08` binary point (reset 3), `0x0C` interrupt acknowledge (read to take the highest-priority pending interrupt; yields the ID and, for software interrupts, the sending core), `0x10` end of interrupt (write the ID back), `0x14` running priority, `0x18` highest pending.

### Timers and watchdogs (offset `0x600`, per core; word access only)

| Offset | Register | Notes |
| ------ | -------- | ----- |
| `0x00` | Timer load | Writing also writes the counter |
| `0x04` | Timer counter | 32-bit down counter; stops while the core is in debug state |
| `0x08` | Timer control | Bit 0 enable, bit 1 auto-reload, bit 2 interrupt enable (ID 29), bits 15:8 prescaler |
| `0x0C` | Timer status | Bit 0 event flag, write 1 to clear |
| `0x20-0x2C` | Watchdog load, counter, control, status | Same layout; control bit 3 = watchdog mode; interrupt ID 30 in timer mode |
| `0x30` | Watchdog reset status | Sticky; survives a watchdog-caused reset so software can tell it from a normal boot |
| `0x34` | Watchdog disable | Write `0x12345678` then `0x87654321` back to back to return to timer mode |

Interval = (prescaler + 1) x (load + 1) x 2 / CPU clock frequency (§10.7.1). In watchdog mode the counter can only be refreshed by writing the load register, and reaching zero asserts the reset-request pin; what the 3DS does with that line > Unknown (not in the sources).

### Performance monitor (per core, CP15 c15)

`MRC/MCR p15,0,Rd,c15,c12,{0..3}` (§3.4.27 to 3.4.30): `0` control (PMNC), `1` cycle counter (CCNT), `2` event counter 0 (PMN0), `3` event counter 1 (PMN1).

| PMNC bit | Meaning |
| -------- | ------- |
| 0 E | enable all three counters |
| 1 P | write 1 to reset both event counters |
| 2 C | write 1 to reset the cycle counter |
| 3 D | `1`: CCNT counts every 64th cycle |
| 6:4 | interrupt enables (CCNT, PMN1, PMN0) |
| 10:8 | overflow flags (CCNT, PMN1, PMN0), write 1 to clear |
| 19:12 / 27:20 | event select fields for the two event counters |

> Conflict: DDI0360E Table 3-40 labels bits 27:20 "EvtCount1" as selecting Count Register 0 and bits 19:12 "EvtCount0" as selecting Count Register 1, which reads like a swapped label (the field names suggest 27:20 belongs to PMN1). This page records the fields as the manual labels them and does not decide; libctru's `svcControlPerformanceCounter` hides the register layout ([CPU optimisation](../engineering/cpu-optimization.md#technical-details)). Unresolved: a run that sets distinct events in both fields would settle it.

Events (Table 3-41; the Event-number column of the PDF text is separated from the definitions, so the pairing below follows the manual's order and is consistent with libctru's `PERFCOUNTEREVT_CORE_*` list):

| Event | Counts |
| ----- | ------ |
| `0x00` | Instruction cache miss that fetches from external memory |
| `0x01` | Stall: instruction buffer cannot deliver (cache or micro-TLB miss), per cycle |
| `0x02` | Stall from a data dependency, per cycle |
| `0x03` | Instruction micro-TLB miss |
| `0x04` | Data micro-TLB miss |
| `0x05` | Branch executed |
| `0x06` | Branch not predicted |
| `0x07` | Branch mispredicted |
| `0x08` | Instruction executed |
| `0x09` | Folded instruction executed |
| `0x0A` | Data cache read access (not cache operations) |
| `0x0B` | Data cache read miss |
| `0x0C` | Data cache write access |
| `0x0D` | Data cache write miss |
| `0x0E` | Data cache line eviction |
| `0x0F` | Software changed the PC without a mode change |
| `0x10` | Main TLB miss |
| `0x11` | External memory request (refill, non-cacheable, write-back) |
| `0x12` | Stall: load/store unit request queue full |
| `0x13` | Store buffer drained (ordering or CP15 operation) |
| `0x14` | Buffered write merged in a store-buffer slot |
| `0x15` | Load/store unit in safe mode |
| `0xFF` | Increments every cycle |

There is a three-cycle delay between enabling and counting, and counts are taken at different pipeline stages, so very short measurements are inexact. Overflow interrupts go out on a PMUIRQ pin, so using them depends on how the SoC routes it > Unknown for the 3DS. With CCNT's divide bit set, the 32-bit counter rolls over after about 2^38 cycles instead of 2^32 (§3.4.27).

### VFP11 floating-point unit

The VFP11 in the MPCore is the fourth version of the design (FPSID part number `0x20`, variant `0xB`, revision 4, implementer ARM, architecture VFPv2; §18.4.1). Everything here is per core.

- **Registers.** Thirty-two single-precision registers `s0-s31` (or sixteen doubles `d0-d15`), in four banks of eight singles for the short-vector scheme (§17.7).
- **Pipelines** (§16.4): three independent pipelines (multiply-accumulate/ALU, divide/square-root, load/store) that can run in parallel with ARM instructions. A short-vector operation looks like one instruction to the integer core.
- **Throughput and latency** (Table 19-16; forwarding removes one cycle from the latency of dependent floating-point operations):

| Instruction class | Single: throughput / latency (cycles) | Double |
| ----------------- | ------------------------------------- | ------ |
| `FABS`, `FNEG`, `FCPY`, `FCVT` | 1 / 4 | 1 / 4 |
| `FCMP*` | 1 / 4 | not reliably legible |
| integer conversions (`FSITO`, `FTOSI`, ...) | 1 / 8 | not reliably legible |
| `FADD`, `FSUB` | 1 / 8 | not reliably legible |
| `FMUL`, `FNMUL`, `FMAC` family | 1 / 8 | throughput 2 |
| `FDIV`, `FSQRT` | 15 / 19 | 29 / 33 |
| `FLD`, `FST` | 1 / 4 | 1 / 4 |
| `FMRS`, `FMSTAT` (to ARM) | 1 / 2 | n/a |
| `FMSR`, `FMXR` (from ARM) | 1 / 4 | n/a |

  The double-precision column of the PDF text extract is partly scrambled, so only legible values are given. Moves to and from control registers (`FMXR`, `FMRX`) are serializing.
- **FPSCR** (`FMRX/FMXR`, §18.4.2): bits 31:28 N, Z, C, V compare flags; bit 25 **DN** default-NaN mode; bit 24 **FZ** flush-to-zero; bits 23:22 rounding (`00` nearest, `01` +infinity, `10` -infinity, `11` toward zero); bits 21:20 **STRIDE**; bits 18:16 **LEN** (vector length minus one); exception enables bits 15 IDE, 12 IXE, 11 UFE, 10 OFE, 9 DZE, 8 IOE; cumulative flags bits 7 IDC, 4 IXC, 3 UFC, 2 OFC, 1 DZC, 0 IOC. Reserved bits must be preserved (read-modify-write).
- **Short vectors** (VFPv2): LEN and STRIDE make one instruction operate on up to 8 single or 4 double values, serially. Some LEN/STRIDE combinations reuse a register and are unpredictable; for doubles, only lengths up to 4 with stride 1 or 2 where no register repeats are safe (Table 18-7). The manual advises avoiding short-vector divides and square roots because they block the other pipelines.
- **Modes** (§16.5): *full-compliance* uses software support code for subnormals, NaNs, possible underflow, and any enabled exception; *flush-to-zero* (FZ) treats subnormal inputs and tiny results as zero; *default NaN* (DN); **RunFast** = FZ + DN + all exception enables cleared. In RunFast every operation completes in hardware with fixed timing, no user traps, and the cumulative flags still follow IEEE 754.
- **Exceptions.** Other than inexact (precise), exceptions are imprecise: the VFP refuses a *later* instruction, which "bounces" as an Undefined Instruction exception into support code that reads `FPEXC` (bit 31 EX, bit 30 EN enable, bit 28 FP2V, bits 10:8 VECITR) and `FPINST`/`FPINST2` (§20). Clear EX immediately or the handler loops. Context switches must save `FPEXC`, and `FPINST`/`FPINST2` when EX is set. libctru runs threads in default-NaN plus flush-to-zero mode with exceptions disabled, so bounces do not occur (see [CPU optimisation](../engineering/cpu-optimization.md#floating-point-and-fixed-point)).
- **Branching on a float compare:** `FCMP` then `FMSTAT` copies flags to the ARM condition codes. For simple tests against zero or a constant, `FMRS` into an integer register plus `CMP`/`CMN` can be faster than `FCMP`+`FMSTAT` (§16.8).

### Power and low-power states

Per core: run; **WFI/WFE** (clock stopped, woken by any interrupt even if masked, a debug request, or reset); dormant (core powered off, caches retained); powered off (§11.5). WFI is `MCR p15,0,Rd,c7,c0,4` and implies a data synchronization barrier first. Dormant and powered-off need an external power controller, signalled through the SCU CPU status register. > Unknown: which of these modes Horizon uses is not covered by the sources used here.

## Homebrew notes

User-mode homebrew can issue the barrier and prefetch-flush operations but not the cache-clean, TLB or performance-counter operations; those need kernel privilege, so homebrew uses `svcFlushProcessDataCache`/`svcInvalidateProcessDataCache` and `svcControlPerformanceCounter` ([libctru](../homebrew/libctru.md)). For the practical side of all this, see [memory and cache](../engineering/memory-and-cache.md) and [CPU optimisation](../engineering/cpu-optimization.md).

## Open questions

- The 3DS-specific settings: cache sizes (read the Cache Type register), interrupt line count and wiring, master port arrangement, power modes used, unaligned-access setting. A privileged read of CP15 c0 and the SCU configuration register on an Old 3DS and a New 3DS would answer most of them.
- Interrupt IDs 16 to 28 are not described in the manual pages used here.
- Whether the PMNC event-select field labels are swapped (see the conflict above).

## Sources

- [ARM11 MPCore Processor Technical Reference Manual (ARM DDI 0360E, r1p0)](https://developer.arm.com/documentation/ddi0360/e) — everything on this page unless noted: CP15 registers (chapter 3), unaligned access (4), MMU (5), prediction (6), L1 memory system and coherency (7), L2 interface (8), private memory region and SCU (9), interrupt controller, timers and watchdog (10), power (11), VFP11 (16 to 20).
- [Rodrigo Copetti, Nintendo 3DS Architecture: CPU](https://www.copetti.org/writings/consoles/nintendo-3ds/) — 16 KB I/D L1 caches and 4-way associativity on the 3DS; New 3DS L2.
- [libctru](https://github.com/devkitPro/libctru) — event names and the performance-counter system call, FPSCR defaults (as used on [CPU optimisation](../engineering/cpu-optimization.md)).
