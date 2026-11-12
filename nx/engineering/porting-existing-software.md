# Porting existing software to Switch homebrew

This page is for a developer bringing an existing program (an SDL game, a desktop engine, an emulator, a statically recompiled game) to the Switch as a native homebrew program. The short answer: decide what kind of port it is before choosing an architecture, audit the original's assumptions in a written table, keep the existing renderer until a measurement says it is the limit, and treat any speed technique from another port as a hypothesis for yours. How the existing ports did it is in the [case studies](case-studies/techniques.md); measurement is on [profiling](profiling-and-benchmarks.md).

## Before you start

- Toolchain: devkitA64 r30-1, `libnx` 4.12.0-1 ([source versions](source-versions.md)); install and run steps are on [toolchain and workflow](../homebrew/toolchain-and-workflow.md).
- Verification: no program was ported for this page. It is organised from the case studies (all `[reported]` or read from source) and the toolchain files; nothing was built or run for it.

## What kind of port is it?

| Project type | Main problem | Reasonable first approach |
| ------------ | ------------ | ------------------------- |
| Simple 2D game | Input, audio, presentation, assets | SDL2, or a small native renderer |
| Software-rendered game | CPU rasterization cost, presentation | Keep the software renderer; measure; scale up from a small framebuffer |
| Existing OpenGL engine | Graphics compatibility, driver overhead | Use the existing GL path first |
| New native 3D game | Rendering, resources, synchronization | `libnx` + `deko3d` ([renderer](deko3d-renderer.md)) |
| Emulator | Guest timing, CPU execution, video and audio accuracy | Deterministic guest execution, then native backends |
| Statically recompiled game | Guest ABI, memory semantics, graphics translation | Recompiled CPU code plus compatibility layers |

There is no rule that 3D needs `deko3d` for 60 fps: the open-engine ports record no frame-rate or draw-count threshold, and none shows that OpenGL cannot reach 60 fps on this hardware `[hypothesis]`. Find the smallest change that meets your correctness and performance goals, then measure it.

## Audit first: a feasibility table

Write one row per assumption before any Switch-specific code.

| Area | Questions |
| ---- | --------- |
| CPU and language | Target architecture? x86 assembly or SSE intrinsics? Assumed endianness, pointer width, alignment, calling convention? |
| Operating system | POSIX facilities, signals, `fork`, dynamic libraries, memory mapping, filesystem behaviour not available the same way under Horizon and newlib? |
| Graphics | OpenGL, GLES, Vulkan, Direct3D, a software rasterizer? A shader compiler at run time? Particular texture formats? |
| Audio | Output format and rate? Callbacks, blocking writes, a mixer? ([audio](audio.md)) |
| Dependencies | Which libraries already have maintained devkitPro ports? Which need cross-compiling or replacing? |
| Data | What must be extracted, converted or packaged, and under what licence? |
| Runtime | Peak memory, thread count, storage bandwidth, target frame rate ([memory](memory-management.md)) |

The result is a matrix, not a statement that the project is "portable".

## The build

Turn a host application into a devkitA64 project by starting from `switch-examples` `templates/application`: the compiler and linker for AArch64, target flags (`-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE` in the template), include paths and portlibs, a linker script, output format and packaging. A normal homebrew **NRO** is produced from the ELF by `elf2nro`; an NSO, an NSP or a sysmodule is a different build with different tools and constraints, so do not treat them as interchangeable executables. Link `-lnx`, and `-ldeko3d` or `-ldeko3dd` for GPU code. The template Makefile compiles `source/*.glsl` to DKSH with `uam` ([renderer](deko3d-renderer.md)).

## Which desktop facilities carry over

| Facility | Ask |
| -------- | --- |
| Threads and mutexes | Does the threading model map to `libnx` and newlib? ([lifecycle](lifecycle-and-threading.md)) |
| Files and directories | Are path, permission, flush and rename assumptions valid on the SD card? ([storage](storage-and-saves.md)) |
| Sockets | Is `socketInitialize` called? Can a blocking call stall the game thread? |
| Timers | Monotonic and suitable for durations? (`armGetSystemTick`) |
| Dynamic loading | Does it rely on desktop `dlopen`? nxquake2 packages separately built renderer and game modules and resolves them through libsolder instead of real `dlopen` `[documented]` (`nxquake2` `doc/990_switch.md`); that proves a homebrew solution exists, not that desktop shared-library loading works unchanged |
| Virtual memory | Page protection, executable mappings, large reservations? Several ports need privileged calls stock firmware refuses |
| GPU API | Which features does the chosen driver or backend support? |
| SIMD | Are x86 intrinsics isolated behind replaceable implementations? |
| Build tools | Do asset compilers have to run on the host rather than the console? |

For each unsupported assumption write a minimal replacement, or say why replacing it changes the architecture.

## ABI and architecture for recompilation

For source ports, ARM64 calling conventions, type sizes, alignment, generated code and atomics decide compatibility. A static recompilation adds: guest addresses versus host pointers, guest object layouts and offsets, guest code versus native functions, guest threads and synchronization, emulated OS services, graphics commands and resource descriptions, translated memory operations, and guest-to-host callbacks and reentrancy. A recompiled game can run native CPU instructions and still be slow because of its graphics translation layer: SA2's work went into the per-draw cost of the translated stream, not the CPU code `[reported]` ([SA2](case-studies/case-study-sa2.md)). Those techniques solve that problem; a native game with a simple draw path may not need them.

## Speed: SIMD, LTO, PGO

- **SIMD.** The Cortex-A57 has NEON, but loops are not vectorized automatically just because flags are set. Show a scalar routine, inspect the AArch64 the compiler generated, and benchmark it against an explicit NEON version. Candidates: pixel conversion, matrix and vertex transforms, audio mixing, texture swizzling, bulk comparison. Prove a loop is hot first; do not rewrite an engine in intrinsics. dhewm3 has no NEON path and nxquake2 no NEON intrinsics `[reported]`.
- **LTO and PGO are separate experiments.** For PGO: build an instrumented target, run representative gameplay on the console, recover the profile data (where is it written, does it survive a normal exit, do paths match the build), rebuild with it, check coverage warnings, compare against the same uninstrumented reference across several scenes, and test correctness. The percentages other ports report (SA2's PGO, NFSMW's combined change) are for their games `[reported]`; do not expect them.

## Replace OpenGL with deko3d in stages

Keep assets, camera, visible geometry and resolution identical, then replace: render-target creation and presentation; shader loading and binding; textures and samplers; vertex and index buffers; render-state representation; command recording and submission; explicit synchronization. After each stage compare the picture and measure CPU and GPU time separately. That shows which part of a renderer must change and what it costs to maintain, which a general claim that `deko3d` has lower overhead cannot.

## Pitfalls

- **Assuming "builds" means "ports".** Check each facility in the table.
- **Dropping the working renderer too early.**
- **Copying a technique with its number.** Keep the label and conditions ([performance and porting](performance-and-porting.md)).
- **Testing only in the Album applet.** Some ports need title override ([memory](memory-management.md)).

## Open questions

- At what draw count, if any, does a GL engine on Mesa become CPU-bound on retail clocks? Needs the draw-scaling experiment.
- Does PGO help a small homebrew game at stock clocks? Needs a same-scene A/B.
- Which GL features do the switch-mesa drivers lack for your engine? Needs the audit.

## Sources

- [switch-examples](https://github.com/switchbrew/switch-examples) at `669786898205b7beb25ff1731e72982e6d0397d3` — `templates/application`.
- [libnx](https://github.com/switchbrew/libnx) at `b156b4774fb2df7b93402169509266da7547fc4c` — `nx/source/runtime/`.
- [nxquake2](https://github.com/fgsfdsfgs/nxquake2) at `a8fdda02` — `doc/990_switch.md`, `src/backends/nx/`; [libsolder](https://github.com/fgsfdsfgs/libsolder).
- The [case studies](case-studies/techniques.md) and the pages linked above.
- Arm [Cortex-A57 Software Optimization Guide](https://developer.arm.com/documentation/uan0015) — architecture reference for SIMD work.
