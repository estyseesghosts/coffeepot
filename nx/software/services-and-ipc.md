# NX services and IPC

Almost everything a Switch game does — open a window, draw, play sound, read input, open files, use the network — is a request to another program. The game (a **[process](../../glossary.md#process)**) sends a message and waits for a reply (this message-passing is **[inter-process communication](../../glossary.md#ipc)**, or **[IPC](../../glossary.md#ipc)**). A central directory hands out connections to named background services; the game then talks over short-lived message channels.

This page covers the message system, the small privileged doorway calls underneath it, and which services games actually use. File formats and filesystems are summarized only as service names; storage layout lives on the [firmware page](firmware.md) and process isolation on the [OS page](os.md).

## Behavior

At startup a title asks the service directory (`sm:`) once for each service name it needs (`vi:u`, `hid`, `audout:u`, and so on) and keeps the returned session for the life of the program. Looking a name up once and reusing the session — instead of reconnecting per request — is the expected pattern [documented] (`libnx/include/switch/services/sm.h`, `libnx/source/services/sm.c`).

Two dialects carry the messages: CMIF for almost all services (session plus domain objects) and TIPC for exactly two services, `sm:` itself and `pgl`, which use a simpler typed-message form [reverse-engineered] (SwitchBrew IPC page). Either way the game packs a header plus send, receive, and exchange buffers into a message: send buffers carry arguments the service reads, receive buffers carry areas the service fills in, and exchange buffers do both [reverse-engineered] (SwitchBrew IPC page). Large or steady streams avoid messages entirely: controller input, for example, is published through a fixed-size shared-memory block (`0x40000` bytes) that the game reads directly after setup instead of polling over IPC [reverse-engineered] (SwitchBrew HID page).

Underneath IPC sit the kernel's own doorways (a **[system call](../../glossary.md#syscall)**). Ordinary code in the unprivileged level (called **[userland](../../glossary.md#userland)**) cannot touch hardware or other processes directly; it calls a numbered kernel function through a libnx wrapper, and the title's capability metadata decides which numbers are legal [documented] (`libnx/source/kernel/svc.s`). A typical frame therefore stacks three layers: game code calls libnx, libnx emits IPC over a session, and IPC transport uses a handful of syscalls to move the message and wait for the reply.

Numbered kernel references (a **[handle](../../glossary.md#handle)**) are the discipline that holds this together: every session, event, shared-memory block, and thread is a handle, and every handle the game no longer needs must be closed or it leaks kernel resources [documented] (`libnx/include/switch/kernel/svc.h`: `svcCloseHandle`). Waiting on several handles at once (for example a frame event plus an audio event plus an exit flag) goes through a single wait call rather than one thread per event.

## Technical details

Message types:

| Dialect | Used for | Notes |
| ------- | -------- | ----- |
| CMIF | All services except the two below; supports sessions and domain sub-objects. | Header carries command ID, buffer counts, and handle-transfer flags [reverse-engineered] (SwitchBrew IPC / CMIF pages). |
| TIPC | `sm:` and `pgl` only. | Simpler typed form; do not use CMIF framing against these two [reverse-engineered] (SwitchBrew IPC page). |

Buffer roles:

| Role | Direction | Typical use |
| ---- | --------- | ----------- |
| Send | Game to service | Command structs, paths, descriptors. |
| Receive | Service to game | Output structs, read data. |
| Exchange | Both | In-place transform buffers. |

All rows [reverse-engineered] (SwitchBrew IPC page).

Selected kernel doorways games and libnx actually use (full list lives on SwitchBrew SVC):

| Number | Name | What it does |
| ------ | ---- | ------------ |
| `0x01` | `SetHeapSize` | Sets the process heap size; backs `malloc` regions. |
| `0x04` | `MapMemory` | Maps memory into the process address space. |
| `0x08` | `CreateThread` | Starts a thread (entry function, argument, stack, priority, core). |
| `0x11` | `SignalEvent` | Wakes waiters on an event handle. |
| `0x18` | `WaitSynchronization` | Sleeps until one of several handles is signaled, with timeout. |
| `0x1F` | `ConnectToNamedPort` | Connects to a named kernel port; how `sm:` sessions begin. |
| `0x20` / `0x21` | `SendSyncRequest` (with/without user buffer) | Sends the packed IPC message and waits for the reply. |

Numbers and roles [reverse-engineered] (SwitchBrew SVC page) as wrapped [documented] (`libnx/source/kernel/svc.s`, `libnx/include/switch/kernel/svc.h`). Call them through libnx; raw numbers are listed so agent code can match traces and stubs.

Game-relevant services (hot path first; filesystem and network last on purpose):

| Service | Provides | libnx header (under `include/switch/services/`) |
| ------- | -------- | ----------------------------------------------- |
| `sm:` | Service directory: look up every other name once at init. | `sm.h` |
| `appletOE` / `am` | Lifecycle: foreground, overlays, suspend, exit. | `applet.h`, `am.h` |
| `vi:u` | Display: open layers and submit frames. | `vi.h` |
| `nvdrv` services | GPU command submission (retail NVN path; homebrew uses deko3d instead). | `nvdrv.h` |
| `hid` | Input via `0x40000`-byte shared memory after setup; no steady-state IPC. | `hid.h` |
| `audren:u`, `audout:u`, `audin:u` | Audio renderer, audio output, audio input. | `audren.h`, `audout.h`, `audin.h` |
| `pcv`, `psm`, `time` | Clocks/voltage, power/battery state, wall and system clocks. | `pcv.h`, `psm.h`, `time.h` |
| `fsp-srv`, `ncm`, `ns`, `ldr:ro` | Files, installed content, title launch info, read-only loader queries. Keep off the frame path; open handles early or on worker threads. | `fs.h`, `ncm.h`, `ns.h`, `ldr.h` |
| `bsd:u`, `nifm`, `ldn` | Sockets, network availability, local wireless. Never block gameplay on them; poll or defer. | `bsd.h`, `nifm.h`, `ldn.h` |

Service names [reverse-engineered] (SwitchBrew Services page); headers [documented] (`libnx/include/switch/services/`). Exact header filenames move with libnx releases — confirm against the pinned libnx commit before generating includes.

Handle and session rules:

- Look up each `sm:` name once at startup; reconnecting per call wastes a port connection and a session [documented] (`libnx/source/services/sm.c`).
- Close what you open: sessions, events, shared-memory handles, and transferred handles each need `svcCloseHandle` on their owner [documented] (`libnx/include/switch/kernel/svc.h`).
- Prefer one `WaitSynchronization` with a list of handles rather than a thread per event [documented] (same header).
- Read `hid` state from shared memory in the steady state; reserve IPC for setup and configuration changes [reverse-engineered] (SwitchBrew HID page).

> Unknown: per-service domain-object lifetime tables across firmware versions. SwitchBrew documents the CMIF domain mechanism; this wiki does not yet pin per-service object graphs.

## Homebrew notes

Homebrew uses the same directory and the same dialects: connect to `sm:`, open sessions, and exchange CMIF messages through libnx wrappers. The two common mistakes are reconnecting per frame (slow and handle-hungry) and blocking the render loop on filesystem or network services. Open `fsp-srv` / `bsd:u` handles on worker threads, keep `vi:u`, `hid` shared memory, and audio on the hot path, and close every session on exit so the homebrew loader reclaims cleanly.

Retail graphics go through NVN over `nvdrv`; homebrew normally uses the homebrew GPU library (deko3d) instead of reimplementing NVN framing — the service underneath is the same, the framing library differs.

## Sources

- [SwitchBrew wiki](https://switchbrew.org/) — IPC, CMIF, TIPC, SVC, Services, HID, Filesystem services pages for dialects, buffer roles, syscall numbers, `0x40000`-byte HID shared memory, and service names.
- [libnx](https://github.com/switchbrew/libnx) — `source/kernel/svc.s`, `include/switch/kernel/svc.h` for syscall wrappers and handle discipline [documented]; `include/switch/services/` and `source/services/` for session setup and per-service wrappers [documented].
