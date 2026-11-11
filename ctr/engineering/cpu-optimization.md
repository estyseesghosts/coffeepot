# CPU optimisation on the ARM11

The 3DS runs games on an **[ARM11](../glossary.md#arm11) [MPCore](../glossary.md#mpcore)** at 268 MHz (Old 3DS) or up to 804 MHz (New 3DS) — a processor design from 2005 with small caches, no vector unit and, on the original models, no second-level cache. Code written for a desktop or phone CPU usually runs several times slower than its authors expect. This page explains how the core executes code, which constructs are expensive, how to see what the compiler produced, and how to read the processor's own **[hardware performance counters](../../glossary.md#performance-counters)** to find cache misses and branch mispredictions instead of guessing.

Hardware reference: [CPU](../hardware/cpu.md). Measuring whole frames: [performance](performance.md). Threads and the second core: [threading](threading.md).

## Before you start

- Checked against ARM's *ARM11 MPCore Processor Technical Reference Manual* (DDI 0360E), libctru 2.7.1 and devkitARM r68 (GCC 16.1). The performance-counter helper below compiles without warnings with those versions; it has **not** been run on hardware for this page.
- Measure before and after every change ([testing on hardware](testing-on-hardware.md#running-a-performance-experiment)). The rules of thumb here say where to look, not what you will find.

## How the core executes code

[documented: DDI 0360E §1.7, §16.4]

- **Eight-stage integer pipeline** (two fetch stages, decode, issue, then shift/ALU/saturate, a three-stage multiply-accumulate pipe, or address/two data-cache stages), with throughput approaching one instruction per cycle. Results that feed the next instruction directly, especially loads, cause stalls; the compiler schedules around this at `-O2`.
- **Branch prediction**: a 128-entry branch target address cache with static prediction for branches it misses, and a 3-entry return stack ([CPU](../hardware/cpu.md#the-mp11-core-from-arms-manual)). Data-dependent branches that go either way at random mispredict often; a mispredicted branch throws away the instructions already fetched.
- **Caches** per core: 16 KB instruction + 16 KB data, 4-way set-associative, 32-byte lines; the data cache is write-back, write-allocate and non-blocking (hit-under-miss). The Old 3DS has nothing between the L1 cache and FCRAM; the New 3DS adds an optional 2 MB L2 ([CPU](../hardware/cpu.md#clocks-and-core-counts)).
- **VFP11 floating point**: single-precision add/multiply/multiply-accumulate at one per cycle (8-cycle latency); double multiply and multiply-accumulate at one per two cycles; divide and square root take about 15 (single) and 29 (double) cycles each. No NEON.
- **Instruction sets**: ARMv6K ARM and Thumb-1 (no Thumb-2), the ARMv6 SIMD instructions (packed 8-/16-bit arithmetic in normal registers, e.g. `uadd8`, `qadd16`, `usad8`), `LDRD`/`STRD`, `PLD` (preload hint), `LDREX`/`STREX` exclusives, **no hardware integer divide**.

## Expensive constructs and their replacements

| Construct | Why it is slow | Replacement |
| --------- | -------------- | ----------- |
| `/` and `%` by a variable | Library call (`__aeabi_idiv`, `__aeabi_uidivmod`), tens of cycles | Divide once outside the loop and multiply by the reciprocal; use shifts and masks for powers of two; keep running counters instead of computing `i % n` |
| `double` arithmetic, double literals (`0.5`), `sqrt`, `sin` | Half-rate multiply, slow divide/sqrt, conversions; libm `double` functions | `float`, `0.5f`, `sqrtf`, `sinf`; compile with `-Wdouble-promotion` |
| 64-bit integer division or modulo | Library call (`__aeabi_ldivmod`) | Avoid in hot code; convert ticks to time once per report |
| Random access over more than 16 KB | L1 misses go to FCRAM (no L2 on Old 3DS) | Smaller data types, structure-of-arrays for hot fields, sort or bucket work by memory locality |
| Large lookup tables | Each lookup can miss the cache | Measure against computing the value; keep tables small |
| Unpredictable branches in inner loops | Mispredictions | Branch-free arithmetic (conditional execution, `min`/`max`, masks), or sort data so branches go the same way in runs |
| Virtual calls / function pointers per element | Indirect branch, no inlining | Batch elements by type and loop per type |
| `memcpy`/`memset` of small fixed sizes through a pointer | Call overhead | Let the compiler see constant sizes; copy whole structs |
| `printf`, `malloc`, file I/O in the frame | Unbounded time | Pre-allocate, format and log once per second |

Unaligned access: the MPCore supports unaligned word and halfword loads and stores in hardware when the system enables it (DDI 0360E chapter 4), but they cost extra cycles and `LDRD`/`STRD`/`LDM`/`STM` still need alignment. Keep data naturally aligned; use `memcpy` into a local to read misaligned packed data portably [documented behaviour of the core; whether the 3DS kernel enables unaligned support is not confirmed here — do not depend on it].

## Floating point and fixed point

- libctru puts every thread's VFP into **default-NaN and flush-to-zero** mode (round to nearest) [documented: libctru `source/system/syscalls.c`]. Denormals become zero: fast, but very small values behave differently from desktop builds.
- The GPU's vertex shaders compute in 24-bit floats. Doing transforms on the GPU ([GPU rendering](gpu-rendering.md#do-work-in-the-vertex-shader)) is usually cheaper than on the CPU, even though the result is less precise.
- Fixed-point integer arithmetic (e.g. 16.16) is worth it mainly when you need bit-exact results (deterministic replays, ports of DS or GBA games whose logic is integer); single-precision VFP is not slow on this core. Measure before converting float code to fixed point.

## Memory access patterns

- **Sequential beats random.** A cache line is 32 bytes; touching one byte loads all 32. Arrays of small, densely packed elements make every loaded byte useful.
- **Split hot and cold fields.** A particle update that reads position and velocity should not drag the particle's name and colour through the cache.
- **Write streams once.** Building vertex data or pixels in linear memory that the GPU will read: write each byte once, in order, and do not read it back ([memory and cache](memory-and-cache.md)).
- **Preload** with `__builtin_prefetch(addr)` (emits `PLD`) a few iterations ahead in loops over large arrays whose addresses are predictable [documented instruction; benefit unmeasured here].
- **Avoid false sharing** between a core-0 and a core-1 thread: put data each thread writes on its own 32-byte line, or the coherency unit moves the line back and forth between the cores' caches.

## Reading the generated code

```sh
arm-none-eabi-objdump -drwC build/hot.o | less                 # disassembly with source line info
arm-none-eabi-nm -S --size-sort -C myapp.elf | tail -30        # largest functions and objects
arm-none-eabi-objdump -d myapp.elf | grep -E "__aeabi_(i|u|l|ul)div|__aeabi_d" | wc -l
```

What to look for in a hot function: calls to `__aeabi_*` helpers (division, `double` arithmetic and conversions), `vcvt` between float and integer in a loop (conversions are not free), unexpected spills to the stack (`str`/`ldr` of `sp`-relative values) and, in the opposite direction, loops the compiler has unrolled so far that they no longer fit the instruction cache.

Use `__attribute__((hot))`, `__builtin_expect` and `restrict` pointers where they are true; they let the compiler lay out code and schedule loads better. Compiler flag guidance follows.

## Compiler flags

The devkitPro templates build with `-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft -O2 -mword-relocations` ([development workflow](../homebrew/development-workflow.md)). Before changing them:

- `-O3` and aggressive inlining or unrolling increase code size, and with a 16 KB instruction cache that can make code slower [hypothesis — measure per project].
- `-Os` is a reasonable choice for cold code (menus, loaders) to save memory; keep hot files at `-O2`.
- `-ffast-math` changes results (reassociation, NaN handling); only enable it for files whose results you have tested.
- Link-time optimisation (`-flto`) is supported by the toolchain; its effect has not been measured here.

Always check the result: `arm-none-eabi-objdump -drwC file.o` for the generated code of a hot function, `arm-none-eabi-nm -S --size-sort app.elf` for what is taking space, and grep the disassembly for calls such as `__aeabi_idiv`, `__aeabi_d*` (software `double` helpers, e.g. for conversions) or `__aeabi_ldivmod` in hot paths.

## Hardware performance counters

Each ARM11 core has a cycle counter and two configurable event counters, and the Snoop Control Unit (SCU) has more counters for coherency and memory traffic [documented: DDI 0360E §3.4.27, chapter 9]. The kernel controls them through **`svcControlPerformanceCounter`** (system call `0x3E`), which libctru wraps with named operations, registers and events [documented: libctru `svc.h`]. Normal titles cannot call it, but Luma3DS's 3DSX extended header allows every system call ([development workflow](../homebrew/development-workflow.md#5-run-it)), so `.3dsx` homebrew can.

Events available per core counter include instruction-cache misses, stalls waiting for instructions, stalls on data hazards, micro-TLB and main-TLB misses, branches executed, branches mispredicted or not predicted, instructions executed, data-cache reads, read misses, writes, write misses and line evictions, store-buffer drains and stalls because the load/store unit is full. SCU events include line fills that missed or hit per core, line migrations between cores, and external reads and writes.

```c
#include <3ds.h>

#define PERF_REGS (BIT(PERFCOUNTERREG_CORE_COUNT_REG_0) | BIT(PERFCOUNTERREG_CORE_COUNT_REG_1) | \
                   BIT(PERFCOUNTERREG_CORE_CYCLE_COUNTER))

typedef struct { u64 cycles, ev0, ev1; } PerfSample;

/* Claim the counters and choose two events. Returns false if the kernel refuses
   (for example because another process holds the performance monitor). */
static bool perf_begin(PerfCounterEvent ev0, PerfCounterEvent ev1)
{
    u64 unused;
    if (R_FAILED(svcControlPerformanceCounter(&unused, PERFCOUNTEROP_ENABLE, 0, 0)))
        return false;
    svcControlPerformanceCounter(&unused, PERFCOUNTEROP_SET_VIRTUAL_COUNTER_ENABLED, true, 0); /* 64-bit values */
    svcControlPerformanceCounter(&unused, PERFCOUNTEROP_SET_EVENT, PERFCOUNTERREG_CORE_COUNT_REG_0, ev0);
    svcControlPerformanceCounter(&unused, PERFCOUNTEROP_SET_EVENT, PERFCOUNTERREG_CORE_COUNT_REG_1, ev1);
    svcControlPerformanceCounter(&unused, PERFCOUNTEROP_SET_EVENT, PERFCOUNTERREG_CORE_CYCLE_COUNTER,
                                 PERFCOUNTEREVT_CORE_CYCLE_COUNT);
    svcControlPerformanceCounter(&unused, PERFCOUNTEROP_RESET, PERF_REGS, PERF_REGS);
    return true;
}

static PerfSample perf_read(void)
{
    PerfSample s = { 0, 0, 0 };
    svcControlPerformanceCounter(&s.cycles, PERFCOUNTEROP_GET_VALUE, PERFCOUNTERREG_CORE_CYCLE_COUNTER, 0);
    svcControlPerformanceCounter(&s.ev0, PERFCOUNTEROP_GET_VALUE, PERFCOUNTERREG_CORE_COUNT_REG_0, 0);
    svcControlPerformanceCounter(&s.ev1, PERFCOUNTEROP_GET_VALUE, PERFCOUNTERREG_CORE_COUNT_REG_1, 0);
    return s;
}

static void perf_end(void)
{
    u64 unused;
    svcControlPerformanceCounter(&unused, PERFCOUNTEROP_DISABLE, 0, 0);
}

/* Usage:
   if (perf_begin(PERFCOUNTEREVT_CORE_DATA_CACHE_READ_MISS, PERFCOUNTEREVT_CORE_BRANCH_MISS_PREDICTED)) {
       PerfSample a = perf_read();
       hot_function();
       PerfSample b = perf_read();
       printf("%llu cycles, %llu D-cache read misses, %llu mispredicted branches\n",
              b.cycles - a.cycles, b.ev0 - a.ev0, b.ev1 - a.ev1);
       perf_end();
   } */
```

Interpreting the numbers:

- Cycles per element of work tell you whether an optimisation helped; compare with the 4.47 million cycles of an Old 3DS frame ([performance](performance.md#the-budget)).
- Many data-cache read misses per element: a memory-layout problem, not an arithmetic one.
- Many instruction-cache misses: code too large or too spread out (aggressive inlining or unrolling, hot code scattered across files).
- Many mispredicted branches: data-dependent branching in an inner loop.
- High "stall by data hazard": long dependency chains (each step waits for the previous result).

### Counters versus the system tick

The two clocks are different things. `svcGetSystemTick()` counts at the fixed base rate of 268,111,856 Hz on every model and clock mode ([performance](performance.md#cpu-time-the-system-tick)); 3dbrew describes it as the CPU ticks elapsed since power-on [documented: 3dbrew SVC page]. The CP15 cycle counter (`CCNT`) counts core cycles, or every 64th cycle with `PERFCOUNTEREVT_CORE_CYCLE_COUNT_64` [documented: libctru `svc.h`; DDI 0360E §3.4.27]. So on a New 3DS in 804 MHz mode you would expect `CCNT` to advance about 3 times faster than the tick, and on an Old 3DS about 1:1. That ratio is `[hypothesis]` until a run records both over the same interval; never mix the two units in one table, and convert with the measured ratio, not an assumed one.

### Availability under Luma3DS (not yet verified)

What the sources show: the Luma3DS loader grants `.3dsx` programs an SVC access list covering numbers `0x00`–`0x7F` (`sysmodules/loader/source/hbldr.c`, "SVC ACL 0 to 0x7F"), and the Luma3DS source tree contains no code that uses or patches `svcControlPerformanceCounter` (searched at the pinned commit) [documented: absence in source, not proof of behaviour]. 3dbrew lists SVC `0x3E` ([3dbrew: SVC](https://www.3dbrew.org/wiki/SVC)) but this wiki has not seen a published run of it from a `.3dsx`. Whether the kernel accepts `PERFCOUNTEROP_ENABLE` for a normal process, whether it is already locked by something else, and whether the counters are saved and restored across context switches are all unknown. Treat the helper above as untested code and first run the open experiment below.

Caveats [hypothesis — to be confirmed on hardware]: the counters belong to a core, not to your thread, so they also count the kernel, interrupts and any other thread that runs on that core during the measurement; keep measured regions short and repeat them. Which core's counters `GET_VALUE` reads when called from another core is not documented in the sources used — read them from the thread you measure.

## Technical details

- `svcControlPerformanceCounter(u64 *out, PerfCounterOperation op, u32 param1, u64 param2)`; operations `ENABLE` (claim and lock), `DISABLE` (release, forcibly), `GET_VALUE`/`SET_VALUE` (per `PerfCounterRegister`), `GET_OVERFLOW_FLAGS`, `RESET` (two bitmasks of `BIT(register)`: values, overflow flags), `GET_EVENT`/`SET_EVENT`, `SET_VIRTUAL_COUNTER_ENABLED` (kernel tracks overflows to provide 64-bit values).
- Registers: core `PMN0`, `PMN1`, `CCNT` (`PERFCOUNTERREG_CORE_*` 0–2); SCU `MN0`–`MN3` (`0x10`–`0x13`), plus `MN4`–`MN7` on production New 3DS only (without an interrupt line).
- Event IDs: core events `0x00`–`0x14` follow the CP15 PMNC event numbering in DDI 0360E; `0xFF` counts cycles and `0xFFF` counts every 64 cycles; SCU events start at `0x1000`.
- The CP15 cycle counter can count every cycle or every 64th (PMNC bit 3); with the kernel's virtual counters enabled you get 64-bit values instead of wrapping 32-bit ones.
- Memory barriers on ARMv6K are CP15 operations, not instructions: libctru's `__dmb()`, `__dsb()` and `__isb()` emit `mcr p15, 0, r, c7, c10, 5`, `c7, c10, 4` and `c7, c5, 4` [documented: `synchronization.h`].

## Pitfalls

- **Optimising the wrong thing.** Profile first ([performance](performance.md#finding-the-bottleneck)); a CPU micro-optimisation does nothing for a GPU-bound frame.
- **Benchmarking a loop the compiler deleted.** Consume results through a `volatile` or a checksum.
- **Desktop SIMD habits.** No NEON; auto-vectorisation flags for other targets do nothing useful here.
- **Leaving the counters enabled.** Call `PERFCOUNTEROP_DISABLE` when done so other tools can use them.

## Open questions

- Real per-event numbers for common workloads on Old and New 3DS (sprite update, vertex generation, decompression).
- Whether the 3DS kernel enables the MPCore's unaligned-access support for user processes.
- Whether counter values read from one core can be affected by threads on another.
- Whether `svcControlPerformanceCounter` works from a `.3dsx` under Luma3DS v13.5 on Old and New 3DS. Experiment: call `PERFCOUNTEROP_ENABLE`, print the result code, then run a loop of known size (for example 1,000,000 `nop`-like register adds, and a sequential versus a random walk over 256 KiB) and compare `CCNT`, `svcGetSystemTick` and the data-cache-miss event over the same interval. Expected if it works: cycles per add near 1, miss counts near zero for the sequential walk. Record the result code and the `CCNT`/tick ratio on both models, with and without `osSetSpeedupEnable(true)`.

## Sources

- [ARM11 MPCore Processor Technical Reference Manual, DDI 0360E](https://developer.arm.com/documentation/ddi0360/e) — pipeline (§1.7), features (§1.1), VFP throughput (§16.4, table 19-16), performance monitor registers (§3.4.27–3.4.29), SCU counters (chapter 9), unaligned access (chapter 4).
- [libctru](https://github.com/devkitPro/libctru) — `svc.h` (`svcControlPerformanceCounter`, `PerfCounter*` enums), `source/svc.s` (SVC `0x3E`), `synchronization.h` (barriers), `source/system/syscalls.c` (FPSCR).
- [Luma3DS](https://github.com/LumaTeam/Luma3DS) — `sysmodules/loader/source/hbldr.c` (3DSX SVC access list).
- [3dbrew: SVC](https://www.3dbrew.org/wiki/SVC) — system call numbers and access restrictions.
