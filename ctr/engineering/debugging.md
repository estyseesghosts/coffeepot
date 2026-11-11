# Debugging 3DS homebrew

When a 3DS program misbehaves you have four tools, from cheapest to most powerful: text logs sent to your PC over Wi-Fi, the console's own error screens and crash dumps, a full debugger (GDB) connected to the running console, and an emulator. All four work on a console running [Luma3DS](../homebrew/luma3ds.md), which today is every hacked 3DS. This page shows how to set each one up, what it tells you, and which to reach for first.

## Before you start

- Checked against: devkitARM r68 (GCC 16.1), devkitARM-gdb 14.1, libctru 2.7.1, 3dslink 0.6.3 (the `3ds-dev` packages as installed by `dkp-pacman` in November 2026), and Luma3DS v13.5 source.
- Build with debug information. The example Makefiles already pass `-g`; keep the `.elf` file that `make` produces next to the `.3dsx` — every tool below needs it to turn addresses into function names and line numbers.
- The console and PC must be on the same network for 3dslink and GDB.

## Choosing a tool

| Symptom | Start with |
| ------- | ---------- |
| Wrong values, logic bugs, "what is the program doing?" | [Logging over 3dslink](#logging-over-the-network-3dslink) |
| Program crashes or freezes the console | [Error screens and crash dumps](#error-screens-and-crash-dumps), then [GDB](#gdb-with-rosalina) |
| Crash only after minutes of play, hard to reproduce | An [exception handler](#catching-exceptions-in-your-own-program) that logs registers, plus logs |
| GPU hangs (screen stops updating, rest of program alive) | [GPU-specific checks](#gpu-hangs) |
| Need to inspect memory, step code, or break on start-up | [GDB](#gdb-with-rosalina) |
| Fast edit–run cycle without touching the console | [Emulator](#emulators) — then confirm on hardware |

## Logging over the network (3dslink)

**3dslink** is a small protocol and PC program: it sends a `.3dsx` to the console over Wi-Fi and can keep the connection open so the program's text output appears in your PC's terminal ([glossary](../glossary.md#3dslink)). It avoids copying files to the SD card for every build and gives you a log that survives a crash.

1. On the console, open the **[Homebrew Menu](../glossary.md#hbmenu)** and press **Y** to start its netloader.
2. On the PC: `3dslink -s myapp.3dsx` (add `-a <console IP>` if UDP broadcast does not reach the console). `-s` keeps a server running after the upload so the program can send output back.
3. In the program, after initialising sockets, call `link3dsStdio()` (both `stdout` and `stderr`) or `link3dsStdioForDebug()` (only `stderr`). Both are in `<3ds/3dslink.h>` and return a socket descriptor (negative on failure); close it on exit. The PC address the program was sent from is available as `__3dslink_host`.

```c
#include <3ds.h>
#include <malloc.h>
#include <stdio.h>
#include <unistd.h>

#define SOC_BUFFERSIZE 0x100000
static u32 *soc_buffer;

int main(void)
{
    gfxInitDefault();
    soc_buffer = memalign(0x1000, SOC_BUFFERSIZE);       // soc: needs a page-aligned work buffer
    int link = -1;
    if (soc_buffer && R_SUCCEEDED(socInit(soc_buffer, SOC_BUFFERSIZE)))
        link = link3dsStdioForDebug();                    // stderr -> PC terminal
    fprintf(stderr, "hello from the 3DS\n");
    /* ... main loop ... */
    if (link >= 0) close(link);
    socExit();
    gfxExit();
    return 0;
}
```

The `soc` service (sockets) takes its work buffer from the application heap; 1 MiB (`0x100000`) is the size the `3ds-examples` network samples use [documented: `3ds-examples/network/sockets`]. The connection port is `LINK3DS_COMM_PORT` (17491).

Keep logging out of hot loops: formatting text and sending it over Wi-Fi costs far more than the code you are measuring. Aggregate numbers in memory and print once a second (see [performance](performance.md#measuring-time)).

### GDB file I/O as a second channel

When a GDB session is attached (below), libctru can route the C standard streams through the debugger instead of the network: `gdbHioDevInit()` then `gdbHioDevRedirectStdStreams(in, out, err)` (`<3ds/gdbhio_dev.h>`). Luma3DS's GDB stub implements the GDB "File I/O" extension for this (`sysmodules/rosalina/source/gdb/hio.c`) [documented]. Output then appears in your GDB console, and `gdbHioDevSystem()` can even run a command on the PC. Use it when Wi-Fi sockets are unavailable or are what you are debugging.

## Error screens and crash dumps

What you see after a crash depends on what crashed:

| What failed | What appears | What to do |
| ----------- | ------------ | ---------- |
| A user-mode CPU exception (bad pointer, undefined instruction) in your program, no handler installed | Luma3DS's ARM11 exception screen with registers; press A to save a dump to `/luma/dumps/arm11/crash_dump_NNNNNNNN.dmp` | Parse the dump (below) |
| Your program calls a fatal-error function (`ERRF_Throw*`, or libctru calls `svcBreak` on an unrecoverable start-up failure) | Rosalina's reimplementation of the system fatal-error screen ("An error occurred (ErrDisp)") showing process name, title ID, address, error code and, if given, a message | Read the error code (`osStrError`), the address and the message |
| The ARM9 side (`Process9`) or a kernel panic | The same Luma3DS exception screen labelled ARM9, dump in `/luma/dumps/arm9/` | Usually a bug in a patch or a hardware-level request, not in ordinary homebrew |

Sources: dump paths from `arm9/source/exceptions.c` and `arm9/source/fs.c`; ErrDisp text from `sysmodules/rosalina/source/errdisp.c` [documented]. The Luma3DS option `disable_arm11_exception_handlers` turns the ARM11 screen off — leave it off while developing.

**Report your own fatal errors with a message.** `ERRF_ThrowResultWithMessage(result, "texture load failed: font.t3x")` (`<3ds/errf.h>`) shows your result code and text on the ErrDisp screen (Rosalina prints the first 96 characters as "Reason"); the console then has to reboot, so use it only for unrecoverable errors. `ERRF_LogResult` records an error without showing anything or rebooting, and `ERRF_SetUserString` supplies an extra string (up to 256 bytes) for error reports. A clear fatal message is far more useful than a silent hang.

**Parsing a dump.** LumaTeam publishes a parser: `pip install -U git+https://github.com/LumaTeam/luma3ds_exception_dump_parser.git`, then run it on the `.dmp`. It prints the exception type, registers, the code around the faulting address and a stack dump. Feed the program counter (`pc`) and the link register (`lr`) to `arm-none-eabi-addr2line -f -C -e myapp.elf 0x00123456` to get function and line. devkitARM's `3dsx.ld` links programs at virtual address `0x00100000` (`__start__`), and the 3DSX loader places the code there, so crash addresses can be looked up in the `.elf` without adjustment [documented: `3dsx.ld`].

## Catching exceptions in your own program

libctru lets each thread install its own exception handler with `threadOnException(handler, stack_top, exception_data)` (`<3ds/thread.h>`): when that thread faults, the kernel jumps to `handler` with the exception information and all CPU registers. Use it to log the registers through your existing channel (3dslink, a file) before calling `ERRF_ExceptionHandler` or exiting. Constants choose where the handler runs: `RUN_HANDLER_ON_FAULTING_STACK` reuses the faulting thread's stack; `WRITE_DATA_TO_HANDLER_STACK`/`WRITE_DATA_TO_FAULTING_STACK` choose where the exception data is pushed [documented]. Give the handler its own small static stack if the bug might be a stack overflow. The handler must not return.

Do not take locks or allocate memory in the handler: the crashing thread may hold them.

## GDB with Rosalina

[Rosalina](../glossary.md#rosalina), the Luma3DS overlay menu, contains a **GDB server** — a "stub" that lets the standard GNU debugger on your PC stop, inspect and step any process on the console over Wi-Fi ([glossary](../../glossary.md#gdb)). devkitPro ships a matching debugger, `arm-none-eabi-gdb` (package `devkitARM-gdb`).

### Enabling it

1. Open Rosalina (default **L + Down + Select**) → *Debugger options…* → *Enable debugger*. Rosalina refuses to start the debugger until the system's socket service is up ("before the system has finished loading"); the console must be on Wi-Fi to accept a connection.
2. Ports: **4000–4002** are general-purpose slots (one process each). **4003** is the "next application" port: choose *Force-debug next application at launch*, then start the program, and it will be stopped at its first instruction until GDB connects [documented: `sysmodules/rosalina/source/gdb/server.c`, `menus/debugger.c`, Luma3DS README].

### Attaching to a running program

```text
$ arm-none-eabi-gdb myapp.elf
(gdb) target extended-remote 192.168.1.50:4000
(gdb) info os processes          # list processes on the console
(gdb) attach <pid>               # pick your program's PID
(gdb) continue
```

The README recommends `extended-remote` together with `attach` and `info os processes` for ports 4000–4002. For port 4003 (break-on-start) use plain `target remote 192.168.1.50:4003`, without `extended-remote` [documented: Luma3DS README].

Once attached, the usual GDB workflow applies: `break`, `watch` (hardware watchpoints), `bt`, `info registers`, `x/16wx 0x08000000`, `info threads`, `thread N`.

### Rosalina-specific monitor commands

Send with `monitor <command>` [documented: `sysmodules/rosalina/source/gdb/remote_command.c`]:

| Command | Use |
| ------- | --- |
| `getmemregions` | List the process's mapped memory regions and permissions — find the heap, linear heap and stack |
| `getmmuconfig` | MMU configuration of the process |
| `convertvatopa <va>` | Physical address of a virtual address (useful for checking what the GPU sees) |
| `flushcaches` | Flush CPU caches |
| `toggleextmemaccess` | Allow reads/writes outside the process's own mappings |
| `listallhandles`, `translatehandle <h>` | Inspect kernel handles the process holds |
| `syncrequestinfo` | Details of the IPC request the stopped thread is making — shows which service call is blocking |
| `catchsvc <ids>` | Stop when the process issues the given [system calls](../software/kernel.md) |
| `getthreadpriority <tid>` | Thread priority |

`syncrequestinfo` and `catchsvc` are especially useful for "my program hangs inside a service call".

### Debugging release-style builds

`-O2` builds can be debugged; values may show as `<optimized out>` and lines may jump. Build a `-O0 -g` variant only to localise a bug, then confirm the fix in the optimised build: optimisation changes timing and can hide or reveal races.

## Bounded renderer diagnostics

A useful capture answers which work was attempted, admitted and actually presented. Record a bounded in-memory ring with frame and pass IDs; command order, material/texture identifiers and vertex ranges; batch-end reasons; target changes; queue high-water marks and dropped submissions; texture-cache hit/miss/invalidation and uploaded bytes; and resource ownership, current/peak usage and allocation failures. Capture CPU phases, GPU timing, waits and frame age as distinct values. Flush the log outside deadline-critical work so synchronous output does not become the hitch being investigated.

For a reproducible visual defect, freeze the scene state, simulation frame, input, camera, random seed, assets, model, build and rendering configuration. Capture the same phase from reference and candidate, including relevant prior buffers and state. Compare dimensions and colour channels, count pixels exceeding an explicitly stated per-channel tolerance, report maximum error and retain a difference image. Exact comparison is appropriate only when capture format and rendering semantics match; screen photographs include capture and display differences. A trace or a green image comparison does not prove an unexecuted branch correct. The wiki does not yet provide a general frame-capture or image-diff tool; use an external one and preserve its version and settings. See [testing on hardware](testing-on-hardware.md#compare-correctness-as-well-as-speed).

## GPU hangs

A frozen picture with a still-running CPU (logs keep printing, HOME still works) usually means the GPU stopped processing commands. `C3D_FrameBegin` waits for the previous GPU work forever by default, so the program then blocks there. Check, in order:

1. **Vertex layout.** Attribute buffer stride must exactly equal the real vertex size; attribute counts and types must match the shader's inputs ([GPU pitfalls](../hardware/gpu.md#known-pitfalls)).
2. **Shader outputs.** Every configured output written exactly once; no two consecutive `mova` instructions ([picasso caveats](../homebrew/picasso-and-citro3d.md)).
3. **Transfer parameters.** `GX_DisplayTransfer` and `GX_TextureCopy` have alignment and minimum-size rules; malformed ones can hang the transfer engine ([display](../hardware/display.md)).
4. **Command buffer overflow.** Very large frames can exceed the citro3d command buffer (default `C3D_DEFAULT_CMDBUF_SIZE` = `0x40000` bytes); pass a larger size to `C3D_Init`.
5. **Memory the GPU reads.** Buffers must be in linear memory or VRAM, and CPU writes flushed (see [memory and cache](memory-and-cache.md)). Stale data gives corruption rather than hangs, but pointers into the normal heap can do either.

To find the guilty draw, bisect: skip half the draws in a frame and see whether the hang persists. `C3D_FrameBegin(C3D_FRAME_NONBLOCK)` returns `false` instead of waiting, which lets the program log "GPU still busy after N frames" rather than freezing.

## Emulators

**Azahar** is the actively maintained 3DS emulator; it was formed in 2025 by merging Lime3DS and PabloMK7's Citra fork, after the original Citra project shut down in March 2024. It runs `.3dsx` and `.elf` files directly, which makes it convenient for quick iteration and for debugging logic.

Treat it as a correctness and debugging tool, not as evidence of speed: its CPU, memory, cache and GPU costs do not model the hardware, and it is more forgiving of some GPU misconfigurations (a program that runs in an emulator can still hang a real console) [hypothesis based on how high-level GPU emulation works; record concrete cases in Open questions]. Always confirm on a console, and on an [Old 3DS](../glossary.md#old-new-3ds) if you target one ([testing on hardware](testing-on-hardware.md)).

## Lessons from ported games

- **Watch for a hang from a thread that outranks the game.** sm64ds-enhanced found that a hang froze the picture while music kept playing and the log stopped, because the log writer ran below the spinning game thread. Its watchdog runs one priority above the game thread, wakes each second, and if the frame counter has not moved for 4 seconds (and the game is not in HOME, sleep or its menu) prints a `[hang]` report with the current phase, scene and stack words and flushes the SD log itself; it repeats every 10 seconds and prints "went on" if the game recovers, and can be turned off with `GAME3DS_WATCHDOG=0` `[documented]` (`port3ds/game/watchdog3ds.cpp`, [case study](case-studies/case-study-sm64ds-enhanced.md)). Look the stack words up with `addr2line` against the ELF.
- **Write an early boot trace to the SD card.** The same port writes `sdmc:/sm64ds3ds/game3ds_early.log` before most of the game starts, so a crash before the first frame still leaves evidence `[documented]`.
- **Name the missing thing at startup.** DXX's and Xash3DS's README entries say only that something crashes. Check data files and services at boot and print what is missing (`stat` the file; print the service result); the Switch port max_nx shows an on-screen error naming each missing data file, and so does Vril for fatal errors.
- **Quake2CTR's `Sys_Error` requires the Start button and writes no log file,** so an error on a console with no attached terminal leaves only a screen ([Quake2CTR](case-studies/case-study-quake2ctr.md)). Combine it with [3dslink logging](#logging-over-the-network-3dslink) or a file.
- **Screenshots from a GL wrapper.** picaGL has no `glReadPixels`; Vril's screenshot path calls `glFinish`, transfers the top screen to a readable framebuffer, invalidates the cache and de-rotates the columns in software `[documented]` ([Vril](case-studies/case-study-vril-engine.md)).

## Technical details

- 3dslink: TCP port 17491 (`LINK3DS_COMM_PORT`), `link3dsConnectToHost(redirStdout, redirStderr)`; the receiving netloader is part of the Homebrew Menu (`3ds-hbmenu`). The PC tool's options are `-a` (address), `-r` (retries), `-0` (set `argv[0]`) and `-s` (stay as a server after upload).
- GDB stub: `GDB_PORT_BASE` 4000; `MAX_DEBUG` is 3, so at most three processes can be debugged at once. Port 4003 (base + `MAX_DEBUG`) serves the "next application" request, which takes one of those three slots — Rosalina reports "Failed to allocate a slot" when all are in use. It is triple-licensed MIT / GPLv2+ inside the GPLv3 Luma3DS project.
- `osTickCounterRead`, `svcGetSystemTick`: see [performance](performance.md#measuring-time) for timing your way to a bug.
- Exception dumps are written by the ARM9 part of Luma3DS, so a dump is saved even if the ARM11 side is unusable; the file name counter is chosen by scanning existing `crash_dump_*.dmp` files.

## Pitfalls

- **Stripped or mismatched ELF.** addr2line and GDB give nonsense when the `.elf` doesn't match the `.3dsx` that crashed. Keep them together, e.g. named by commit hash.
- **Logging changes timing.** A race that disappears when logging is on is still a race. Log into a memory ring buffer and print later.
- **Printing to the on-screen console every frame** (`consoleInit`) is slow and fights your renderer for the screen; use 3dslink for anything high-volume.
- **Cheats and Rosalina debugging on thread-heavy programs.** Luma3DS documents a kernel race when a debugger attaches to processes that create and destroy many threads; programs with thread pools may crash only while debugged ([Luma3DS](../homebrew/luma3ds.md#limits-and-risks)).

## Open questions

- Azahar's GDB support for homebrew (inherited from Citra's GDB stub) has not been checked for this page.
- No catalogue yet of GPU configurations that work in Azahar but hang hardware; contributions should include a minimal reproducer.

## Sources

- [Luma3DS (LumaTeam)](https://github.com/LumaTeam/Luma3DS) — README (GDB ports, `extended-remote`, `info os processes`, break-on-start), `sysmodules/rosalina/source/gdb/` (server, monitor commands, HIO), `sysmodules/rosalina/source/menus/debugger.c`, `sysmodules/rosalina/source/errdisp.c`, `arm9/source/exceptions.c` (dump paths).
- [luma3ds_exception_dump_parser](https://github.com/LumaTeam/luma3ds_exception_dump_parser) — crash-dump parser and its install command.
- [libctru](https://github.com/devkitPro/libctru) — `3dslink.h`, `gdbhio_dev.h`, `errf.h`, `thread.h` (`threadOnException`).
- [3ds-hbmenu](https://github.com/devkitPro/3ds-hbmenu) — netloader (Y button), `-a` fallback.
- [citro3d](https://github.com/devkitPro/citro3d) — `renderqueue.c` (`C3D_FRAME_NONBLOCK`), `base.h` (default command buffer size).
- [3ds-examples](https://github.com/devkitPro/3ds-examples) — `network/sockets` (socket buffer setup), `network/3dslink-demo`.
- [Azahar](https://github.com/azahar-emu/azahar) — emulator; history from [Recalbox: Azahar, the new 3DS emulator to replace Citra](https://recalbox.com/blog/2025-03-18-azahar-citra-emulateur-3ds-retrogaming-actualite).
