# Debugging Switch homebrew

This page is for a developer whose Switch program prints nothing, aborts with an error screen, freezes, or hangs the GPU, and who needs to find out where. The short answer: log over `nxlink` from the first line of `main`, keep a crash-dump handler in the program, turn an error screen's addresses into source lines with `aarch64-none-elf-addr2line` against the `.elf`, and treat a GPU error as fatal to the process.

## Before you start

- Toolchain: devkitA64 r30-1, `libnx` 4.12.0-1, `deko3d` 0.5.0-1 ([source versions](source-versions.md)). The small program that exercises these steps is [a first Switch game](first-game.md).
- The `nxlink` and crash-dump fragments compile with `-Wall` and zero warnings, and the first game's self-test ran in Eden; none has been run on a console, and no fault has been triggered on one. For symptom-first triage see [failure modes](errors-and-failure-modes.md). The fatal-screen workflow is from an archived 2022 guide and the Atmosphere source; it has not been re-run here.
- Applies to homebrew run under Atmosphere. Error-screen behaviour belongs to the system software, so it can change with firmware.

## Choose a tool

| Symptom | Start with |
| ------- | ---------- |
| Want to see what the program is doing | `printf` over [`nxlink`](../homebrew/toolchain-and-workflow.md#print-debug-text-to-the-pc-with-nxlink) |
| Error screen with a code | [Fatal error screens](#read-a-fatal-error-screen) |
| Program disappears or returns to the menu | [Atmosphere's crash reports](#atmospheres-crash-reports) first, then a [best-effort dump](#write-a-best-effort-crash-dump) |
| Frame stops, music or system continues | [A hang](#diagnose-a-hang) |
| Black screen or the GPU stops answering | [GPU errors](#gpu-errors) |
| Wrong output, no crash | [Compare with a known-good build](performance-and-porting.md#measure-the-way-the-ports-learned-to) |

## Log first

Initialise sockets and call `nxlinkStdio()` before anything else that can fail, then print results of every fallible call (`R_SUCCEEDED`, the return code in hex). Pass the `.nro` file to `nxlink`, not the `.elf`. The old guide reports a fatal error (2347-0005, 0xb5b) when something else is sent `[reported]` (`Homebrew-Guide` `homebrew_dev/app_dev.rst`). Full steps are on [toolchain and workflow](../homebrew/toolchain-and-workflow.md#print-debug-text-to-the-pc-with-nxlink).

## Read a fatal error screen

When a homebrew program faults, the system can show a fatal error screen. The archived guide says to read three things `[reported]` (`Homebrew-Guide` `homebrew_dev/app_dev.rst`, 2022):

- **Error code.** Commonly `2168-0002` for a segmentation fault (a bad read or write) and `2168-0001` for a prefetch abort (the program counter points at non-executable memory). For other codes, look up SwitchBrew's [Error codes](https://switchbrew.org/wiki/Error_codes) page.
- **Title.** Under the Album applet, a homebrew program shows the Album's title ID `010000000000100D`. A different ID suggests that your program crashed a system module, so check your IPC and system-call arguments.
- **PC and backtrace.** Subtract the "Backtrace - Start Address" from the PC (or a backtrace entry) to get an offset into your program, then:

```sh
aarch64-none-elf-addr2line -e path/to/program.elf -f -p -C -a 0xOFFSET
```

Use the `.elf`, not the `.nro`. Build with `-g` so the lines exist; the template Makefile does.

> Unknown: whether the title ID and screen layout are unchanged on current firmware. The guide is from 2022.

## Atmosphere's crash reports

Atmosphere replaces the system's crash-report module and writes a human-readable report for each crashing process to `/atmosphere/crash_reports/<timestamp>_<title ID>.log` on the SD card, with thread-info dumps under `dumps/`, and does not upload them `[documented]` (`Atmosphere` `docs/components/modules/creport.md`; `stratosphere/creport/source/creport_crash_report.cpp` lines 56, 310, 327; `docs/changelog.md` line 1558). These logs are the first place to look after a crash that left no error screen.

## Write a best-effort crash dump

libnx calls a user-defined handler on a CPU exception if you provide an exception stack. The `exception-handler` example does this, and so does [the first-game program](first-game.md). This is **best effort, not dependable crash capture**: the handler runs on a small dedicated stack after an arbitrary fault, and opening a file and calling `fprintf` need the filesystem service, the C runtime state and its locks to be healthy, which a fault can leave broken. It may work for many crashes and silently do nothing for others. Make Atmosphere's report (above) the primary record, keep the handler small, and do not promise more than it can deliver:

```c
alignas(16) u8 __nx_exception_stack[0x1000];
u64 __nx_exception_stack_size = sizeof(__nx_exception_stack);

void __libnx_exception_handler(ThreadExceptionDump* ctx)
{
    FILE* f = fopen("sdmc:/switch/mygame/crash.txt", "w");
    if (!f) return;
    fprintf(f, "error_desc: 0x%x\npc: 0x%lx\nlr: 0x%lx\nsp: 0x%lx\nfar: 0x%lx\n",
            ctx->error_desc, ctx->pc.x, ctx->lr.x, ctx->sp.x, ctx->far.x);
    fclose(f);
}
```

Verification: compiles inside `nx/engineering/examples/first-game`, `-Wall`, no warnings; the example it comes from deliberately writes to address 8 to trigger the handler `[documented]` (`switch-examples` `exception-handler`). Not run. Avoid allocating or locking in the handler `[hypothesis]`. Resolve the saved `pc` against the `.elf` as above, using the load base your program printed at start; keep the `.elf` that matches each released `.nro` or the address will resolve to a plausible but wrong function.

## Diagnose a hang

A hung game thread can freeze the picture while music continues, and a lower-priority log writer is starved by a spinning higher-priority thread. sm64ds-enhanced's 3DS port solved the equivalent problem with a watchdog thread at a higher priority than the game thread (see the [3DS debugging page](../../ctr/engineering/debugging.md#lessons-from-ported-games)); the same idea works on Horizon, but no Switch port reviewed here does it, so it is `[hypothesis]`. What to log instead, cheaply: a frame counter and the name of the phase the game thread is in, printed over `nxlink` once a second from a thread whose priority outranks the game thread's.

Useful facts for hangs: `appletMainLoop()` returns false when the system requests exit, so a loop that ignores it cannot be asked to quit; and by default the system suspends the process when Home opens or the console sleeps, which stops frames without a hang (see [performance and porting](performance-and-porting.md#handheld-docked-and-suspend)).

## GPU errors

In `deko3d`, if the GPU hits an error the queue enters an error state, `dkQueueIsInErrorState` reports it, and the only legal call on that queue is `dkQueueDestroy`. The Primer warns that the system appears to kill processes that crash the GPU a few seconds later and that it is not known whether this can be disabled, so save promptly `[documented]` (`deko3d` `Primer.md` "Queues"):

```c
if (dkQueueIsInErrorState(queue)) { /* save now; then dkQueueDestroy and exit */ }
```

The debug library (`-ldeko3dd`) adds parameter and state checking and, per the Primer, can print information about a GPU error through the debug callback `[documented]` (`deko3d` `README.md` and `Primer.md`). Use it for every development build. Typical causes are in [rendering](rendering.md): bad descriptors, memory a frame still reads that was overwritten, mismatched colour and depth target sizes, and missing barriers.

## Measure while you debug

The per-phase times printed by [first-game](first-game.md) (`begin`, `draw`, `present`, `loop`) show which CPU-side call a long frame spent its time in, and a few rare large values against a steady average point to hitches. They do not separate CPU rendering from GPU execution (a presented-frame interval includes waits and scheduling) and say nothing about GPU time; for that use GPU timestamps ([profiling](profiling-and-benchmarks.md)). Keep timing buckets exclusive and record the launch mode and clocks with each number.

## Pitfalls

- **Debugging the Album applet and the real target.** A bug that only appears under title override (or only under the Album) is usually memory; log the launch mode and the heap size.
- **Resolving addresses against the wrong build.** Rebuilding changes addresses; keep the `.elf` that matches the `.nro` you ran.
- **Logging in the hot path.** `nxlinkStdio` sockets are non-blocking in libnx, but a full buffer still costs time on the frame; log sparingly in release.
- **Trusting a GPU time derived from submission timestamps.** The Eidos port found nested buckets that "added up" to a false GPU-slow story; see the [case study](case-studies/case-study-eidos-ouroboros.md).

## Open questions

- Which fatal-screen codes and layouts are current on the latest firmware? Needs a deliberate fault on a console.
- Does a high-priority watchdog thread work as expected under Horizon scheduling? Needs a test program with a deliberate hang.
- What does the exception handler get for a fault inside a service call? Untested.

## Sources

- [noahc3 Homebrew Guide](https://github.com/noahc3/Homebrew-Guide) at `6d7b7bc` (2022-08-02, archived) — `homebrew_dev/app_dev.rst`: `nxlink`, fatal screens, `addr2line`.
- [switch-examples](https://github.com/switchbrew/switch-examples) at `669786898205b7beb25ff1731e72982e6d0397d3` — `exception-handler`, `network/nxlink_stdio`.
- [Atmosphere](https://github.com/Atmosphere-NX/Atmosphere) at `4e3f37038d9e82dde7054897213ab32b2ecf651f` — `docs/components/modules/creport.md`, `stratosphere/creport/source/creport_crash_report.cpp`, `docs/changelog.md`.
- [deko3d](https://github.com/devkitPro/deko3d) at `373bfd43d8b2d805fa2eb7d406dbb823032df958` — `Primer.md`, `README.md`.
- SwitchBrew [Error codes](https://switchbrew.org/wiki/Error_codes) (oldid 14808, edited 2026-07-13) — error-code lookup.
