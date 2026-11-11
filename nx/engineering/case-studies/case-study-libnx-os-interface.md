# Case study: libnx OS interface (documented behaviour)

[libnx](../../glossary.md#libnx) is the community C library every Switch homebrew NRO links: thin AArch64 syscall wrappers plus clients for ~100 services. Source facts are `[documented]` with the file; console numbers are `[reported]`, never Coffeepot measurements.

## Before you start

- Evidence: `[documented]` = in the named file; `[reported]` = third-party claim with source and conditions. Coffeepot built no libnx tree for this page.
- Tree: `switch/libnx` (`kernel/`, `services/`, `sf/`, `runtime/`), `switch-examples/` (`hid/`, `audio/`, `fs/`, `applet/`, `network/`, `power/`, `templates/`, `jit/`), plus the SVC/HIPC/Kernel/Services SwitchBrew pages in the same folder.
- Companions: [NX CPU](../../hardware/cpu.md), [NX GPU](../../hardware/gpu.md), [libnx and deko3d](../../homebrew/deko3d-and-libnx.md).
- Command IDs shift per Horizon release; re-check headers at your pinned commit.

Jargon: the **[CPU](../../../glossary.md#cpu)** runs code; the **[kernel](../../../glossary.md#kernel)** schedules it; a **[process](../../../glossary.md#process)** is one isolated program; a **[thread](../../../glossary.md#thread)** is one scheduled sequence in it; a **[syscall](../../../glossary.md#syscall)** asks the kernel for privileged work; **[IPC](../../../glossary.md#ipc)** is messaging between processes; a **[handle](../../../glossary.md#handle)** names a kernel object. The family is **[NX](../../glossary.md#nx)** on **[Tegra X1](../../glossary.md#tegra-x1)**; **[deko3d](../../glossary.md#deko3d)** is the homebrew GPU library; **[title override](../../glossary.md#title-override)** gives homebrew full game memory.

## What makes libnx tick

| Layer | Approach |
| ----- | -------- |
| SVC wrappers (`kernel/svc.h`, `svc.s`) | One `svc` stub per call (`svcCreateThread`, `svcWaitSynchronization`, `svcMapSharedMemory`, JIT map) [documented]. Policy lives in `kernel/thread.c`, `services/`. |
| Service sessions (`services/sm.h`, `sf/`) | `smGetService` once, then `serviceDispatch*` over CMIF/TIPC blobs from `sf/` [documented]. Cached globals + `XxxExit`; per-frame reconnect is overhead. |
| Dialects (`sf/cmif.h`, `sf/tipc.h`) | CMIF (descriptors + PID) for most; TIPC fast path for `nv`, `sm`, some `vi` [documented]. Wrong dialect errors, not silent corruption. |
| Handles (`kernel/*.h`, `services/*/`) | Each copied handle needs one `svcCloseHandle`, usually in `XxxExit` [documented]. Leaks exhaust the table; later connects fail. |
| Init/exit (`runtime/crt0.s`) | `__libnx_init`: `smInit` → `appletInit` → service inits; exit reverses [documented]. Early calls hit dead sessions. |
| Applet gating (`services/applet/`) | `appletMainLoop` polls focus; on inactive stop submitting and suspend audio/I/O [documented]. Ignored = suspended mid-frame. |

## How it uses the Cortex-A57

| Mechanism | Detail |
| --------- | ------ |
| Threads on 3 app cores | `threadCreate(fn, arg, stack, prio, core)` → `svcCreateThread` + affinity; cores 0–2 usable, 3 reserved [documented]. Pin polling and main loop apart. |
| Mutex fast path | `mutexLock` spins `ldaxr`/`stlxr`, kernel-waits only on contention [documented]. Uncontended = no syscall. |
| Events | `eventWait` parks with nanosecond timeout; re-check predicate after wake [documented]. Early fire loses the wake. |
| Memory pages | `virtmem`: 2 MB blocks aligned, 4 KB otherwise [documented]. One build: ~4% faster at +47 MB with text+rodata on 2 MB [reported; SA2 docs, City Escape, 1020 MHz]. |
| JIT limits | `jit/`: map + permission flip + icache invalidate; never writable-and-executable; needs loader JIT pool [documented]. No pool in applets. |
| HID shared memory | `hidScanInput` polls the 0x40000-byte HID region via shared memory, not per-button IPC [documented]. Once per frame. |
| Audio pump | `audout` examples feed 48 kHz PCM from one thread; sharing its core causes underrun crackle [documented pattern; crackle [reported], conditions unstated]. |

## How it touches the GPU indirectly

libnx draws nothing. `viOpenDisplay` → `viCreateLayer` yields a window plus an `nv` channel that [deko3d](../../glossary.md#deko3d) binds as its swapchain target [documented in `services/vi.h`, `services/nv.h`]. Underneath are `nvidia` ioctls tunnelled through `nv` [documented]. libnx owns setup (resolution, vsync event); deko3d owns pacing — a leaked layer or unflushed fence freezes the swapchain with the GPU idle.

## What it does well

| Strength | Detail |
| -------- | ------ |
| Whole OS surface | ~100 clients (`hid`, `audren`, `fs`, `vi`, `nv`, `bsd`, `nifm`, `pcv`, `psm`), uniform init/exit names [documented]. |
| Examples map | Each `switch-examples/` folder demos one family (applet, romfs, sockets, sine audio) [documented]. Start from the nearest. |
| Debug path | `nxlink` routes `stdout` over TCP; `printf` works before any framebuffer [documented]. |
| Asset story | `romfs` packs data into the NRO (`romfs:/`) with no install step [documented]. |

## What it does poorly (costs and traps)

| Cost / trap | Detail |
| ----------- | ------ |
| Per-frame `sm` reconnect | Connect + close per frame wastes syscalls + round-trip; connect once, reuse [documented; figures [reported], conditions unstated]. |
| Blocking I/O on sim thread | `fs`/`bsd` block; inline calls stall the frame on slow media or Wi-Fi [documented]. Move to a loader thread + event. |
| Wrong-rate audio | 44.1 kHz into 48 kHz plays ~8% fast; resample or negotiate [documented; chipmunk posts [reported]]. |
| Applet-mode OOM | Album heap is small; assets fitting under [title override](../../glossary.md#title-override) abort from Album [documented mechanism; MB [reported] per version]. |
| Priority pitfalls | Top priorities starve system threads (watchdog kills); equals time-slice in 10 ms slices [documented]. Audio just above workers. |

## Lesson: hot/warm/cold services and symptom fixes

Hot (per frame): `hid` poll, `vi` vsync wait, `audout` append, `applet` poll. Warm (per change): `fs` opens, `nifm` checks, `psm`/`pcv` reads. Cold (once): `sm` connect, `nv` open, `vi` layer create, romfs mount. Main loop owns hot, audio thread owns pump, loader thread owns warm/blocking + event. Init cold → hot; exit reverses.

| # | Symptom to fix |
| - | -------------- |
| 1 | Per-frame `smGetService` jitter — connect once, close at exit; zero `sm` traffic after init [documented]. |
| 2 | Asset stalls frame — bulk `fs`/socket work to loader thread, `eventFire` on done [documented]. |
| 3 | Crackling/fast audio — one `audout` thread at 48 kHz, buffers ≥2 vsyncs [documented]. |
| 4 | Home-press freeze — poll `appletMainLoop` each frame; suspend submit + audio on focus loss [documented]. |
| 5 | Album-only crash — check heap, downscale for applet mode, document override need [documented mechanism]. |
| 6 | Worker deadlock — audit mutex order, short `ldaxr`/`stlxr` sections, no blocking IPC under frame mutex [documented]. |

## Open questions

- Applet heap and JIT pool sizes at current pins unverified; re-check `jit/` notes per release.
- `nv` fence-wait vs swapchain depth on Erista/Mariko has no first-party number here; needs GPU-side measurement.
- TIPC accept-lists change per release; `sf/tipc.h` needs a per-version audit.

## Sources

- `switch/libnx`: `kernel/svc.h`, `kernel/svc.s`, `kernel/thread.c`, `services/sm.h`, `services/vi.h`, `services/nv.h`, `services/applet/`, `sf/cmif.h`, `sf/tipc.h`, `runtime/crt0.s` [documented].
- `switch-examples/`: `hid/`, `audio/`, `fs/`, `applet/`, `network/`, `power/`, `templates/`, `jit/` [documented].
- SVC / HIPC / Kernel / Services SwitchBrew HTMLs in the same folder — numbers, descriptors, scheduler and handle semantics.
- SA2 port docs (`docs/cpu-rounds.md`) — 2 MB-block figure and conditions [reported].
