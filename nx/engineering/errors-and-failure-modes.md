# Switch homebrew failure modes: from symptom to first check

This page is for a developer who has a symptom (black screen, crackle, freeze, a program that will not start) and needs to know where to look first. The short answer: record the same small report for every failure, match the symptom to a subsystem below, and use a different diagnostic for a CPU crash, a GPU error and a hang, because they leave different evidence. The tools themselves (nxlink, crash dumps, `addr2line`) are on [debugging](debugging.md).

## Before you start

- Toolchain: `libnx` 4.12.0-1, `deko3d` 0.5.0-1 ([source versions](source-versions.md)). This is an index of where to look, built from the other pages and the headers; none of its **fault reproducers** has been written, so no diagnostic artifact below was produced on a console by this wiki.
- Everything here is for homebrew under Atmosphere; error screens belong to the system software and can change with firmware.

## Capture the same report every time

A failure report should hold: the operation attempted; the native result code and its module and description; the firmware and library versions; whether the program was an application or an applet; the last successful initialization step; the relevant sizes and configuration; and whether it reproduces.

```c
if (R_FAILED(rc)) {
    printf("operation failed: rc=0x%08x module=%u description=%u\n", rc, R_MODULE(rc), R_DESCRIPTION(rc));
}
```

`R_FAILED`, `R_MODULE` and `R_DESCRIPTION` are in `libnx` `result.h` `[documented]`. Look the pair up on SwitchBrew's [Error codes](https://switchbrew.org/wiki/Error_codes). Errors surfaced through the C library appear as `errno`; some wrappers translate service failures, and for filesystem calls `fsdevGetLastResult()` returns the underlying native result `[documented]` (`fs_dev.h`). `deko3d` creation calls do not return a `Result`; their errors arrive through the debug callback ([renderer](deko3d-renderer.md)).

## Symptom to first investigation

| Symptom | Look first at |
| ------- | ------------- |
| NRO does not start | Loader and executable format; startup requirements; the title-override path ([Atmosphere and running](../homebrew/atmosphere-and-running.md)) |
| Fails right after start | The result of each service initialization; which call was last to succeed |
| Black renderer or missing shader | Is `romfsInit` successful; the path `romfs:/shaders/…`; DKSH file exists and loads; was it built with `uam`; binding state ([renderer](deko3d-renderer.md)) |
| GPU hang or abort | `dkQueueIsInErrorState`; invalid resource access; command memory reused too early; barriers ([synchronization](gpu-memory-and-synchronization.md)); run with `-ldeko3dd` |
| Corrupted textures | Image format, pitch, block-linear conversion, upload and staging lifetime |
| Crackle or silence | Late buffer production, buffers rewritten while queued, 0x1000 alignment, format ([audio](audio.md)) |
| Works under Album, not under title override | Different process environment, memory and permissions ([memory](memory-management.md)) |
| Works under title override, not under Album | The applet memory limit |
| Freezes on Home or resume | A lock or wait held across suspend, timers, exit lock ([lifecycle](lifecycle-and-threading.md)) |
| Terminated shortly after a quit request | `appletUnlockExit` missing; cleanup longer than 15 s |
| Save corrupt or progress lost | Serialization, ignored write errors, incomplete state ([storage](storage-and-saves.md)) |
| Works in the emulator, fails on the console | Timing, GPU behaviour, services the emulator stubs, memory limits |
| Out of memory | Launch mode, heap size, a `DkMemBlock` per resource |
| A service call fails | Service available on this firmware? Permission? Arguments? See below |

### Telling service failures apart

A request can fail because the service is unavailable, the command does not exist on that firmware, the process lacks access, the arguments are invalid, or the service itself errored. The module and description answer most of that; a `libnx` result module (`Module_Libnx`, e.g. incompatible system version) means the wrapper refused it before sending. Use the supported wrappers rather than building IPC by hand ([services and IPC](../software/services-and-ipc.md)).

## CPU crash, GPU error, hang: three workflows

| Failure | Evidence available | Where |
| ------- | ------------------ | ----- |
| CPU exception | Registers, fault address, program counter; Atmosphère's crash report on the SD card; an optional handler in your program | [debugging](debugging.md) |
| GPU error | `dkQueueIsInErrorState` is true; the only legal call is `dkQueueDestroy`; the system may kill the process a few seconds later `[documented]` (`Primer.md` "Queues"). The debug library can print what happened through its callback | [debugging](debugging.md#gpu-errors) |
| Deadlock or infinite loop | Often nothing: no exception, no report | Your own logging: a frame counter and phase name printed periodically from a higher-priority thread |

Do not rely on one universal crash handler. A handler that writes a file from inside a fault depends on services that may be broken; Atmosphère's report is the dependable record.

## Resolve an address to a source line

1. Keep the exact `.elf` that produced each released `.nro`; rebuilding changes addresses.
2. Take the program counter from the crash report and subtract the load base to get an offset into your program.
3. `aarch64-none-elf-addr2line -e program.elf -f -p -C -a 0xOFFSET`.

A stripped build has no lines, and the wrong `.elf` resolves to a plausible but wrong function. The walk from report to source line has not been reproduced on a console.

## A fault suite to write

Separate programs that deliberately read an invalid address; raise a fatal diagnostic; deadlock two workers; starve a lower-priority thread; use a destroyed graphics resource (isolated, never in normal examples); fail a required file lookup; exhaust a bounded pool; fail audio initialization. Each should record which diagnostic artifacts the console actually produced, rather than relying on fatal-screen descriptions from older tutorials.

## Pitfalls

- **Printing only "failed".** Print the hex result and its module and description.
- **Debugging the emulator.** Bring-up on an emulator proves bring-up, not game frames.
- **Resolving against the wrong build.** Keep matching `.elf` files.
- **Testing GPU faults in the main example.** Isolate them.

## Open questions

- Which fatal-screen codes and layouts apply to current firmware? Needs deliberate faults on a console.
- What do the exception handler and Atmosphère's report contain for a fault inside a service call? Untested.

## Sources

- [libnx](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/include/switch/result.h) at `b156b4774fb2df7b93402169509266da7547fc4c` — `result.h`, `runtime/devices/fs_dev.h`, `services/applet.h`.
- [deko3d](https://github.com/devkitPro/deko3d/blob/373bfd43d8b2d805fa2eb7d406dbb823032df958/Primer.md) at `373bfd43d8b2d805fa2eb7d406dbb823032df958` — `Primer.md` "Queues" (error state), debug callback.
- [Atmosphere](https://github.com/Atmosphere-NX/Atmosphere) at `4e3f37038d9e82dde7054897213ab32b2ecf651f` — `docs/components/modules/creport.md`.
- SwitchBrew [Error codes](https://switchbrew.org/wiki/Error_codes) (oldid 14808) — code lookup.
