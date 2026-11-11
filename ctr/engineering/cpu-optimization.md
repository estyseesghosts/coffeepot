# CPU optimisation on the ARM11

The 3DS runs games on an **[ARM11](../glossary.md#arm11) [MPCore](../glossary.md#mpcore)** at 268 MHz on an Old 3DS — a processor design from 2005 with small caches, no vector unit and no second-level cache. **New 3DS:** up to 804 MHz and an optional 2 MB L2 cache. Code written for a desktop or phone CPU usually runs several times slower than its authors expect. This page explains how the core executes code, which constructs are expensive, how to see what the compiler produced, and how to read the processor's own **[hardware performance counters](../../glossary.md#performance-counters)** to find cache misses and branch mispredictions instead of guessing.

Hardware reference: [CPU](../hardware/cpu.md). Measuring whole frames: [performance](performance.md). Threads and the second core: [threading](threading.md).

## Before you start

- Checked against ARM's *ARM11 MPCore Processor Technical Reference Manual* (DDI 0360E), libctru 2.7.1 and devkitARM r68 (GCC 16.1). The performance-counter helper below compiles without warnings with those versions; it has **not** been run on hardware for this page (Old 3DS: not run; New 3DS: not run).
- Measure before and after every change ([testing on hardware](testing-on-hardware.md#running-a-performance-experiment)). The rules of thumb here say where to look, not what you will find.

## How the core executes code

[documented: DDI 0360E §1.7, §16.4]

- **Eight-stage integer pipeline** (two fetch stages, decode, issue, then shift/ALU/saturate, a three-stage multiply-accumulate pipe, or address/two data-cache stages), with throughput approaching one instruction per cycle. Results that feed the next instruction directly, especially loads, cause stalls; the compiler schedules around this at `-O2`.
- **Branch prediction**: a 128-entry branch target address cache with static prediction for branches it misses (forward conditional branches predicted not taken, backward taken), and a 3-entry return stack that predicts `BX lr`, `LDM sp!,{..,pc}` and `LDR pc,[sp..]` returns but not `BLX reg` calls ([CPU](../hardware/cpu.md#the-mp11-core-from-arms-manual); details in [ARM11 MPCore internals](../hardware/arm11-mpcore.md#branch-prediction)). Data-dependent branches that go either way at random mispredict often; a mispredicted branch throws away the instructions already fetched.
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

Unaligned access: the MPCore supports unaligned word and halfword loads and stores in hardware when the system enables it (control register `U` bit; DDI 0360E chapter 4), but they cost extra cycles, and `LDRD`/`STRD`/`LDM`/`STM`, exclusive and coprocessor (VFP) accesses that are not aligned raise an alignment Data Abort once `U` is set ([ARM11 MPCore internals](../hardware/arm11-mpcore.md#unaligned-access)). Keep data naturally aligned; use `memcpy` into a local to read misaligned packed data portably [documented behaviour of the core; whether the 3DS kernel enables unaligned support is not confirmed here — do not depend on it].

## Floating point and fixed point

- libctru puts every thread's VFP into **default-NaN and flush-to-zero** mode (round to nearest): it writes `FPSCR = BIT(25) | BIT(24)` with all exception-enable bits clear, because the kernel does not initialise `fpscr` [documented: libctru `source/system/syscalls.c`, line 249-254]. That combination is exactly the manual's **RunFast** mode: every operation finishes in hardware in fixed time, with no support code and no user traps [documented: DDI0360E §16.5.4]. Denormals become zero: fast, but very small values behave differently from desktop builds. Code that turns on floating-point exception enables or leaves flush-to-zero off makes some operations bounce to software support code instead (see [ARM11 MPCore internals](../hardware/arm11-mpcore.md#vfp11-floating-point-unit)).
- The GPU's vertex shaders compute in 24-bit floats. Doing transforms on the GPU ([GPU rendering](gpu-rendering.md#do-work-in-the-vertex-shader)) is usually cheaper than on the CPU, even though the result is less precise.
- Fixed-point integer arithmetic (e.g. 16.16) is worth it mainly when you need bit-exact results (deterministic replays, ports of DS or GBA games whose logic is integer); single-precision VFP is not slow on this core. Measure before converting float code to fixed point.

## Memory access patterns

- **Sequential beats random.** A cache line is 32 bytes; touching one byte loads all 32. Arrays of small, densely packed elements make every loaded byte useful.
- **Split hot and cold fields.** A particle update that reads position and velocity should not drag the particle's name and colour through the cache.
- **Write streams once.** Building vertex data or pixels in linear memory that the GPU will read: write each byte once, in order, and do not read it back ([memory and cache](memory-and-cache.md)).
- **Preload** with `__builtin_prefetch(addr)` (emits `PLD`) a few iterations ahead in loops over large arrays whose addresses are predictable [documented instruction; benefit unmeasured here].
- **Avoid false sharing** between a core-0 and a core-1 thread: put data each thread writes on its own 32-byte line, or the coherency unit moves the line back and forth between the cores' caches.

## Instruction-cache pressure

Data caches get most of the attention, but the core also has to *fetch* every instruction it runs. Each Old 3DS core has a 16 KB instruction **[cache](../../glossary.md#cache)** (**I-cache**) and nothing behind it except FCRAM, because the Old 3DS has no L2 cache. Code that executes more than 16 KB of distinct instructions per frame keeps evicting itself, and the core waits for memory instead of running.

**What is established.** DDI 0360E describes the I-cache as 4-way set-associative with 32-byte lines, virtually indexed and physically tagged, with a size of 16, 32 or 64 KB chosen when the chip is built [documented: DDI 0360E; [ARM11 MPCore internals](../hardware/arm11-mpcore.md#caches)]. That the 3DS uses 16 KB comes from Copetti's analysis, not from a register read-out on a console [reverse-engineered: Copetti, via [CPU](../hardware/cpu.md)]; the G-Diffuser probe below states 16 KB for the Old 3DS as well [reported]. With no L2 on the Old 3DS, every I-cache miss goes to FCRAM.

> Unknown: this wiki has not read the cache-type register (`MRC p15,0,Rd,c0,c0,1`) on a console, and [`icachebench`](../benchmarks/README.md#icachebench) does not either; it shows the *effect* of footprint, not the configuration. **New 3DS:** the 2 MB L2 (when enabled) may soften an I-cache miss, but that has not been measured here.

### The G-Diffuser I-cache probe

`[reported]` Source: the G-Diffuser F-Zero X port's postmortem, [Road to 60, All Over Again](https://hypnotize.works/gdx-3ds/postmortem/native.html), entry dated 09-30 ("HWPROF B"). The page carries no publication date; its newest entry is 10-26 (v1.1.0, 2026). I read the page directly. Console: **Old 3DS hardware** (stated; the exact model is not). Method as stated: ARM11 performance counters plus a synthetic probe loop run at three hot-code sizes.

| Hot-code footprint | Cycles per instruction (CPI) |
| ------------------ | ---------------------------- |
| 16 KB | 1.06 |
| 24 KB | 4.10 |
| 32 KB | 5.62 |

Also stated on that page: the real render path measured CPI 4.34 with 43.6% of cycles stalled on instruction fetch, against about 219 KB of hot code (release v0.9.0); an I-cache miss costs about 37 cycles; and the emulator does not model the I-cache, so emulator results did not predict hardware. The page's conclusion is a design rule for its own renderer. The probe is synthetic; the page does not claim the numbers apply to other programs, and this wiki has not reproduced them.

> Conflict: the review that prompted this section describes the result as CPI rising "as the active code footprint increased" but leaves out that the 16 KB point is the good case (1.06) and the jump comes between 16 and 24 KB. The source's prose around the 24 KB figure is garbled; the table value 4.10 is used here.

Do not extend the numbers: one program's probe, one unit, an unstated instruction mix. [`icachebench`](../benchmarks/README.md#icachebench) is this wiki's version of the experiment; it has not been run, so nothing here is `[measured]`.

### Keeping the hot path small

`[hypothesis]` guidance derived from the facts above; each item can be tested with the I-cache-miss and fetch-stall counters (`PERFCOUNTEREVT_CORE_INST_CACHE_MISS`, `..._STALL_BY_LACK_OF_INST`; see [counters](#hardware-performance-counters)):

- **Count executed bytes, not source lines.** The question is how many distinct bytes of code run between two uses of the same code. Estimate with `arm-none-eabi-nm -S --size-sort`, confirm with the counter.
- **Lay hot code out together.** Keep the functions of one frame stage in one translation unit built with `-ffunction-sections`; move cold paths (error handling, rare formats) out of hot functions, with `__attribute__((cold))` or into separate functions.
- **Group by operation type instead of dispatching per element.** Looping over 1000 commands and calling a different handler for each pulls every handler through the I-cache each iteration, and a call through a pointer is a `BLX reg`, which the return stack does not predict. Bucketing commands by type and running one tight loop per type executes each handler's code once per batch. The price is a bucket pass and possibly lost ordering (draw order matters for alpha blending).
- **Specialise the common case.** One compact path for the common case executes far fewer distinct instructions than a generic interpreter; keep the generic path as a fallback.
- **Do not unroll or inline blindly.** `-O3`, `always_inline` and manual unrolling grow code, and past 16 KB of hot code that can be slower than the loop it replaced.

## Techniques and how to check them

Each row is a candidate to *test*, not a promised speed-up. "Label" is the strongest evidence this wiki has; nothing has been run on hardware.

| Technique | Detail | Label | How to verify |
| --------- | ------ | ----- | ------------- |
| Compact instruction paths | Specialised loops instead of generic dispatch ([above](#keeping-the-hot-path-small)) | `[reported]` for the principle (G-Diffuser, its own workload); `[hypothesis]` elsewhere | I-cache miss and fetch-stall counters, cycles per element, before and after |
| Data locality, hot/cold split | Dense arrays of the fields a loop touches; rare fields in a parallel array ([below](#hotcold-field-splitting)) | `[documented]` mechanism (32-byte lines, 16 KB D-cache); gain unmeasured | D-cache read-miss counter per element; identical output |
| Avoid variable integer division | `/` and `%` call `__aeabi_idiv` and friends (no hardware divide). Use shifts, a hoisted reciprocal, or an incremental counter | `[documented]` (DDI 0360E lists no divide; the call shows in compiled output) | Disassemble: no `__aeabi_*div*` in the loop; cycles per element |
| `float` instead of `double` | `x * 0.5` is a `double` multiply (`vmul.f64`); `x * 0.5f` is `vmul.f32`. VFP11 does double multiplies at half rate | `[documented]` (DDI 0360E throughput); gain unmeasured | Disassemble for `.f64` and `__aeabi_d*`; build with `-Wdouble-promotion` |
| Fixed point only when exactness matters | Not a speed trick; single-precision VFP is not slow | `[hypothesis]` | Compare cycles per element and results |
| ARMv6 packed SIMD | `UADD8`, `QADD16`, `USAD8`: 4 x 8-bit or 2 x 16-bit lanes in ordinary registers ([example](#armv6-packed-simd-example)) | `[documented]` (instruction set); `[reported]` once: G-Diffuser says ARMv6 SIMD made its audio synthesis 2.2x cheaper (09-29 entry, Old 3DS, its workload) | Cycles per sample against a scalar version; identical output |
| Limit allocations | Pre-allocate scene storage, audio buffers, command lists, dynamic vertex rings; no `malloc` in the frame | `[hypothesis]` for the gain | Log the worst frame with and without |
| Avoid repeated transforms | Cache static transforms; send object-space data to the vertex shader ([GPU rendering](gpu-rendering.md#do-work-in-the-vertex-shader)) | `[hypothesis]` | CPU record time per frame, same scene |
| Inspect generated code | [Checklist](#assembly-inspection-checklist) | `[documented]` method | The checklist |

### Hot/cold field splitting

```c
/* before: 48-byte struct; the update loop needs only x, y, vx, vy but loads whole lines */
typedef struct { float x, y, vx, vy; char name[24]; u32 color, flags; } Particle;

/* after: hot fields contiguous, cold data in a parallel array with the same index */
typedef struct { float x, y, vx, vy; } ParticleHot;       /* 16 bytes: two per 32-byte line */
typedef struct { char name[24]; u32 color, flags; } ParticleCold;
```

A 32-byte line holds two `ParticleHot` but less than one `Particle`, so the update loop streams roughly a third of the lines [documented line size; the gain is unmeasured]. Verify by counting D-cache read misses per particle before and after.

### ARMv6 packed SIMD example

`UADD8` adds four byte lanes modulo 256 in one instruction; `QADD16` adds two signed halfword lanes with saturation (useful for mixing 16-bit audio). Operands are ordinary 32-bit registers, so buffers must be 4-byte aligned. Whether GCC emits these from plain C loops on this toolchain was not checked `[hypothesis: it does not]`; use inline assembly.

```c
#include <stdint.h>
#include <stddef.h>

static inline uint32_t add_u8x4(uint32_t a, uint32_t b)
{
    uint32_t r;
    __asm__("uadd8 %0, %1, %2" : "=r"(r) : "r"(a), "r"(b));
    return r;
}

static inline uint32_t qadd_s16x2(uint32_t a, uint32_t b)
{
    uint32_t r;
    __asm__("qadd16 %0, %1, %2" : "=r"(r) : "r"(a), "r"(b));
    return r;
}

/* dst[i] = src[i] + k per byte lane; k4 is the byte repeated, e.g. 0x01010101 */
void add_bytes(uint32_t *dst, const uint32_t *src, uint32_t k4, size_t words)
{
    for (size_t i = 0; i < words; i++) dst[i] = add_u8x4(src[i], k4);
}
```

Verification: builds with `arm-none-eabi-gcc` (devkitARM r68) `-Wall -Wextra -O2 -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft` inside a stub `main`, together with a `QADD16` mixing loop; the object disassembles to `uadd8` and `qadd16`. Old 3DS: not run. New 3DS: not run (ARMv6 instructions, present on both). Compiling shows the instructions assemble, not that they are faster.

### Assembly inspection checklist

1. Compile the hot file alone with the project flags and disassemble: `arm-none-eabi-objdump -drwC build/hot.o`.
2. Search for `__aeabi_idiv`, `__aeabi_uidivmod`, `__aeabi_ldivmod`: a variable divide survived.
3. Search for `__aeabi_d`, `vmul.f64`, `vcvt.f64`: a `double` leaked in (a literal without `f`, a promoted `printf` argument, a libm call).
4. Look for `vcvt` between integer and float inside the loop.
5. Look for `str`/`ldr` with `sp` inside the loop: register spills.
6. Check function sizes (`nm -S --size-sort`): a hot function of several kilobytes is an I-cache risk.
7. Look for `blx` (indirect calls) inside loops.

Compile-checked observation (devkitARM r68 GCC 16.1, `-O2`, compile only): `x / w` on `int` compiled to `bl __aeabi_idiv`; `x * 0.5` on a `double` compiled to `vmul.f64` and `x * 0.5f` to `vmul.f32`. This shows what is emitted, not what it costs on hardware.

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

The two clocks are different things. `svcGetSystemTick()` counts at the fixed base rate of 268,111,856 Hz on all models and clock modes ([performance](performance.md#cpu-time-the-system-tick)); 3dbrew describes it as the CPU ticks elapsed since power-on [documented: 3dbrew SVC page]. The CP15 cycle counter (`CCNT`) counts core cycles, or every 64th cycle with `PERFCOUNTEREVT_CORE_CYCLE_COUNT_64` [documented: libctru `svc.h`; DDI 0360E §3.4.27]. So on an Old 3DS you would expect `CCNT` to advance about 1:1 with the tick. **New 3DS:** in 804 MHz mode, about 3 times faster than the tick. That ratio is `[hypothesis]` until a run records both over the same interval; never mix the two units in one table, and convert with the measured ratio, not an assumed one.

### Availability under Luma3DS (not yet verified)

What the sources show: the Luma3DS loader grants `.3dsx` programs an SVC access list covering numbers `0x00`–`0x7F` (`sysmodules/loader/source/hbldr.c`, "SVC ACL 0 to 0x7F"), and the Luma3DS source tree contains no code that uses or patches `svcControlPerformanceCounter` (searched at the pinned commit) [documented: absence in source, not proof of behaviour]. 3dbrew lists SVC `0x3E` ([3dbrew: SVC](https://www.3dbrew.org/wiki/SVC)) but this wiki has not seen a published run of it from a `.3dsx`. Whether the kernel accepts `PERFCOUNTEROP_ENABLE` for a normal process, whether it is already locked by something else, and whether the counters are saved and restored across context switches are all unknown. Treat the helper above as untested code and first run the open experiment below.

Caveats [hypothesis — to be confirmed on hardware]: the counters belong to a core, not to your thread, so they also count the kernel, interrupts and any other thread that runs on that core during the measurement; keep measured regions short and repeat them. Which core's counters `GET_VALUE` reads when called from another core is not documented in the sources used — read them from the thread you measure.

## Technical details

- `svcControlPerformanceCounter(u64 *out, PerfCounterOperation op, u32 param1, u64 param2)`; operations `ENABLE` (claim and lock), `DISABLE` (release, forcibly), `GET_VALUE`/`SET_VALUE` (per `PerfCounterRegister`), `GET_OVERFLOW_FLAGS`, `RESET` (two bitmasks of `BIT(register)`: values, overflow flags), `GET_EVENT`/`SET_EVENT`, `SET_VIRTUAL_COUNTER_ENABLED` (kernel tracks overflows to provide 64-bit values).
- Registers: core `PMN0`, `PMN1`, `CCNT` (`PERFCOUNTERREG_CORE_*` 0–2); SCU `MN0`–`MN3` (`0x10`–`0x13`), plus, **New 3DS:** `MN4`–`MN7` on production units only (without an interrupt line).
- Event IDs: core events `0x00`–`0x14` follow the CP15 PMNC event numbering in DDI 0360E; `0xFF` counts cycles and `0xFFF` counts every 64 cycles; SCU events start at `0x1000`.
- The CP15 cycle counter can count every cycle or every 64th (PMNC bit 3); with the kernel's virtual counters enabled you get 64-bit values instead of wrapping 32-bit ones.
- Memory barriers on ARMv6K are CP15 operations, not instructions: libctru's `__dmb()`, `__dsb()` and `__isb()` emit `mcr p15, 0, r, c7, c10, 5`, `c7, c10, 4` and `c7, c5, 4` [documented: `synchronization.h`].

## Pitfalls

- **Optimising the wrong thing.** Profile first ([performance](performance.md#finding-the-bottleneck)); a CPU micro-optimisation does nothing for a GPU-bound frame.
- **Benchmarking a loop the compiler deleted.** Consume results through a `volatile` or a checksum.
- **Desktop SIMD habits.** No NEON; auto-vectorisation flags for other targets do nothing useful here. The VFP11 does have VFPv2 *short-vector* instructions (up to 8 single or 4 double operations per instruction via `FPSCR.LEN`/`STRIDE`), and the integer SIMD instructions work on packed bytes and halfwords; whether compilers or libraries on this toolchain use either has not been checked here [hypothesis: devkitARM does not auto-vectorise for short vectors; check the generated code].
- **Leaving the counters enabled.** Call `PERFCOUNTEROP_DISABLE` when done so other tools can use them.

## Open questions

- Real per-event numbers for common workloads on Old 3DS first, then New 3DS (sprite update, vertex generation, decompression).
- Whether the G-Diffuser I-cache cliff (CPI 1.06 / 4.10 / 5.62 at 16 / 24 / 32 KB) reproduces with a different probe. Experiment: run [`icachebench`](../benchmarks/README.md#icachebench) on an Old 3DS, compare cycles per instruction and the I-cache-miss counter across footprints, and read the cache-type register to confirm 16 KB. **New 3DS:** repeat with and without `osSetSpeedupEnable(true)`.
- Whether grouping by operation type beats per-element indirect dispatch on a real workload (needs a dispatch-heavy loop in both forms, identical output, counters on).
- Whether the 3DS kernel enables the MPCore's unaligned-access support for user processes.
- Whether counter values read from one core can be affected by threads on another.
- Whether `svcControlPerformanceCounter` works from a `.3dsx` under Luma3DS v13.5 on Old 3DS and New 3DS. Experiment: call `PERFCOUNTEROP_ENABLE`, print the result code, then run a loop of known size (for example 1,000,000 `nop`-like register adds, and a sequential versus a random walk over 256 KiB) and compare `CCNT`, `svcGetSystemTick` and the data-cache-miss event over the same interval. Expected if it works: cycles per add near 1, miss counts near zero for the sequential walk. Record the result code and the `CCNT`/tick ratio on an Old 3DS, and on a New 3DS with and without `osSetSpeedupEnable(true)`.

## Sources

- [ARM11 MPCore Processor Technical Reference Manual, DDI 0360E](https://developer.arm.com/documentation/ddi0360/e) — pipeline (§1.7), features (§1.1), VFP throughput (§16.4, table 19-16), performance monitor registers (§3.4.27–3.4.29), SCU counters (chapter 9), unaligned access (chapter 4).
- [libctru](https://github.com/devkitPro/libctru) — `svc.h` (`svcControlPerformanceCounter`, `PerfCounter*` enums), `source/svc.s` (SVC `0x3E`), `synchronization.h` (barriers), `source/system/syscalls.c` (FPSCR).
- [Luma3DS](https://github.com/LumaTeam/Luma3DS) — `sysmodules/loader/source/hbldr.c` (3DSX SVC access list).
- [G-Diffuser: Road to 60, All Over Again](https://hypnotize.works/gdx-3ds/postmortem/native.html) — `[reported]` I-cache probe, render-path CPI, ARMv6 SIMD audio figure (read directly; third-party, not reproduced).
- [3dbrew: SVC](https://www.3dbrew.org/wiki/SVC) — system call numbers and access restrictions.
