# CTR kernel and system calls

A **[kernel](../../glossary.md#kernel)** is the privileged heart of the operating system. The 3DS actually has two: **Kernel11** on the [ARM11](../hardware/cpu.md) (which runs games and apps) and **Kernel9** on the ARM9 (which does security and storage). Both are small and follow the **[microkernel](../../glossary.md#microkernel)** idea: they only manage memory, schedule [threads](../../glossary.md#thread), and pass messages; everything else is done by ordinary processes.

This page describes the ARM11 kernel as programs see it: processes, threads, synchronization objects and the system call list. The surrounding system is on the [OS page](os.md); the way programs request things from other processes is on the [services page](services.md).

## Behavior

- Every program runs as a **[process](../../glossary.md#process)** with its own address space, handle table and system-call permissions. A process can only call the system calls its **kernel capability descriptors** allow (official software copies them from its title's metadata, the *exheader*). Calling a forbidden one panics the kernel; `svcBreak` on retail kills only the calling process (`BreakReason` values: 0 PANIC, 1 ASSERT, 2 USER) [documented: 3dbrew SVC].
- Processes cannot touch each other's memory directly. When a service call passes a buffer, the kernel maps that memory into the receiving process.
- Handles (object references) are per-process; `0xFFFF8001` always means "the current process".
- Low-level system information appears in a read-only **configuration memory** page mapped into every process (see [memory](../hardware/memory.md)).

### Scheduling

Each core has its own `KScheduler`. 3dbrew states that little about it is confirmed; the Citra team's reverse engineering (below) adds detail. The first core (the *appcore*) is **mostly cooperative**: a thread loses the core when it blocks or makes certain system calls, but the scheduler may also preempt a lower-priority thread to let a higher-priority one run in specific cases. A game can still deadlock itself with careless threading. The *syscore* has been hypothesised to be preemptive but this is unconfirmed. The application's share of the syscore is set as a CPU-time percentage through the `APT` service (`APT_SetAppCpuTimeLimit`); Copetti describes it as roughly 30%, but the actual maximum comes from the title's exheader — up to 89%, the value Luma3DS gives `.3dsx` homebrew — and is 0 (no application threads on the syscore) until the application requests a share. Luma3DS's process-manager notes (TuxSH, marked as untested) describe two preemption modes: in the "single" mode used by applications, the syscore's time is cut into 25 ms slices, the one application thread allowed there gets its percentage of each slice, system modules share (90 − percentage)%, and 10% is left for other threads; CPU-time preemption exists only on the syscore. Practical consequences are on [performance](../engineering/threading.md). How the ARM9 and the two extra New 3DS cores are scheduled is unknown.

Per Copetti, the New 3DS's fourth core is used solely for head tracking, and the third core is idle unless the title's metadata asks for it (see [CPU](../hardware/cpu.md#new-3ds-extra-cores)). Because the system cannot assume games are written for it, the faster clock and extra memory are off unless requested.

## Technical details

### Kernel objects (selected)

| Object | Size | Purpose (per 3dbrew) |
| ------ | ---- | -------------------- |
| `KCoreContext` | `0x9000` (effective `0x6000`) | One per core; the current core's is aliased at `0xFFFF1000` |
| `KThread` / `KThreadContext` | page (`0x1000`) | Each thread gets a page at `0xFF4xx000` for its supervisor-mode stack and saved context; the page is in FCRAM on `NATIVE_FIRM`, AXI WRAM on `TWL_FIRM` |
| `KProcess`, `KProcessHwInfo` | | `KProcessHwInfo` begins at `KProcess+0x1C` (`0x4C` bytes on New 3DS) |
| `KSchedulableInterruptEventLinkedList` | `0x10` | One per core; interrupt events whose handlers need a thread get queued here |
| `KInterruptEventMailbox` | `0x14` | Two instances; the first handles process-signalled events (thread priority 0) |
| `KLinkedList`, `KObjectList`, `KObjectLink`, `KObjectMutex`, `KRecursiveLock` | | Helper structures (a recursive lock's unlock avoids triggering the reschedule SGI #8) |
| `KEventInfo`, `KDebugThread` | `0x38`, `0x18` | Debug events |

Per-core "current" pointers live at fixed kernel addresses: `0xFFFF9000` current thread, `0xFFFF9004` current process, `0xFFFF9008` scheduler, `0xFFFF900C` interrupt-event list; the ARM9 equivalents are at `0x08000040–0x08000048`.

### Processes and threads

`CreateCodeSet` copies the `.text`, `.rodata` and `.data/.bss` segments (the `CodeSetInfo` structure gives addresses and page counts) into freshly allocated memory; `CreateProcess` applies the exheader's capability descriptors (memory mappings, allowed SVCs, CPU clock and L2 configuration); `Run` creates the main thread. Core affinity is managed through `Get/SetThreadAffinityMask` and `Get/SetThreadIdealProcessor`.

### Synchronization

Mutexes, semaphores, events, timers, and **address arbiters** (futex-like waits on a memory word) are kernel objects. `WaitSynchronization1/N` and `SignalAndWait` wait on them. `libctru` adds user-space helpers (critical sections without priority inheritance) described on 3dbrew's *CTR API* page.

### System call list

System calls are numbered; `NF ARM11` marks calls on the normal ARM11 firmware. Highlights:

| Group | Calls (id) |
| ----- | ---------- |
| Memory | `ControlMemory` (0x01), `QueryMemory` (0x02), `CreateMemoryBlock` (0x1E), `MapMemoryBlock` (0x1F), `UnmapMemoryBlock` (0x20), `ControlProcessMemory` (0x70), `MapProcessMemory` (0x71) |
| Threads/processes | `ExitProcess` (0x03), `CreateThread` (0x08), `ExitThread` (0x09), `SleepThread` (0x0A), `Get/SetThreadPriority` (0x0B/0x0C), affinity/ideal-processor (0x04–0x07, 0x0D–0x10), `GetCurrentProcessorNumber` (0x11), `Run` (0x12) |
| Sync | `CreateMutex` (0x13), `CreateSemaphore` (0x15), `CreateEvent` (0x17), `CreateTimer` (0x1A), `CreateAddressArbiter` (0x21), `ArbitrateAddress` (0x22), `WaitSynchronization1/N` (0x24/0x25) |
| Handles/info | `CloseHandle` (0x23), `DuplicateHandle` (0x27), `GetSystemTick` (0x28), `GetHandleInfo` (0x29), `GetSystemInfo` (0x2A), `GetProcessInfo` (0x2B), `GetThreadInfo` (0x2C) |
| IPC | `ConnectToPort` (0x2D), `SendSyncRequest` (0x32), `CreatePort` (0x47), `CreateSession` (0x49), `AcceptSession` (0x4A), `ReplyAndReceive` (0x4F) |
| Interrupts/DMA/cache | `BindInterrupt` (0x50), `Invalidate/Store/FlushProcessDataCache` (0x52–0x54), `StartInterProcessDma` (0x55), `StopDma` (0x56) |
| Debugging | `DebugActiveProcess` (0x60) … `WriteProcessMemory` (0x6B), `SetHardwareBreakPoint` (0x6C), `GetProcessList` (0x65), `GetThreadList` (0x66) |
| Process creation | `CreateCodeSet` (0x73), `CreateProcess` (0x75), `TerminateProcess` (0x76), resource-limit calls (0x77–0x79) |
| Privileged | `Backdoor` (0x7B, runs a function in kernel mode; see the Conflict below), `KernelSetState` (0x7C), `QueryProcessMemory` (0x7D), `SetGpuProt` (0x59, added 11.3.0), `SetWifiEnabled` (0x5A, added 11.4.0) |
| Stubs | A stubbed call only returns an error (`0xF8C007F4`). 3dbrew marks `SignalAndWait` (0x26), `SendSyncRequest1-4` (0x2E-0x31), `GetThreadContext` (0x3B), `ReplyAndReceive1-4` (0x4B-0x4E) and `RandomStub` (0x74) as stubbed; `Set{Thread}AffinityMask` (0x0E) and `SetThreadIdealProcessor` (0x10) became stubs on NATIVE_FIRM from 8.0.0-18; `AddCodeSegment` (0x7A) on NATIVE_FIRM from 2.0.0-2 |

`0x3D OutputDebugString` does nothing on non-debug units, and `0x3E ControlPerformanceCounter` is listed without notes. `svc 0x59 SetGpuProt` enables/disables the GPU's memory protection.

### Waiting, signalling and rescheduling (Citra reverse-engineering notes)

- Every kernel object that can be waited on keeps a **waitlist** of threads blocked on it. When the object is signalled, the kernel takes the highest-priority waiter, checks whether it can run, appends it to the scheduler queue and removes it from the waitlist, repeating for every thread that is ready.
- Each thread also keeps a list of objects it is waiting on, used only by `WaitSynchronizationN`. A thread is queued only when *all* objects in its list are ready (wait-all mode).
- `WaitSynchronization1` with timeout 0: if the object is signalled, it is acquired with **no reschedule**; if not, it returns error `0x09401BFE` immediately. With a timeout above 0, an unready object adds the thread to the waitlist and suspends it; a ready object is acquired with no reschedule.
- `WaitSynchronizationN` in wait-any mode acquires the first ready object in order (no reschedule for timeout 0 or −1); in wait-all mode it first checks that all are ready. If a thread that cannot acquire everything is marked for termination (offset `0x36` in the `KThread`), it returns `0xD920060D` after being rescheduled; otherwise it joins every object's waitlist and suspends (in both wait modes `WaitSynchronizationN` clears the thread's own list and adds the thread to each object's waitlist; only wait-all repopulates the thread's list). After suspension the kernel removes the thread from the waitlists again — behavior the authors speculate is a coroutine-like return that sets the call's result.
- A reschedule attempt happens: at the end of a system call if the thread is marked for termination or its process has a debug event; after `ExitThread`, `SleepThread`, `ExitProcess`, `TerminateProcess`, `CreateThread`, `SetThreadPriority`; after a process or thread that others waited on is destroyed; after a timeout wakes a thread; when `WaitSynchronization1`/`N` has to block; after a sync request to a client session and after `ReplyAndReceive` replies to a server session; when a client or server session is closed (waiters wake with an error); when a thread resumes after a signal; after `ReleaseMutex` brings the lock count to 0; and after `SetResourceLimitValues` with type CPU_TIME (`9`) and a value below `0x5A` (90).
- The notes mark as unverified: address arbiter handling, memory-mapping calls, DMA transfers and `Map`/`UnmapProcessMemory`; `ReplyAndReceive` is still TODO.

### Resource limits and arbitration

Resource limit types: 0 priority, 1 commit (allocatable memory), 2 threads, 3 events, 4 mutexes, 5 semaphores, 6 timers, 7 shared-memory objects, 8 address arbiters, 9 CPU time (a percentage, regular until it reaches 90). Address-arbiter actions: signal (`−1` wakes all), wait if less than, decrement-and-wait if less than, and timeout variants of both. `svcCreateThread` takes a processor ID: 0 is the application core, 1 the system core, `−1` all cores, `−2` the default from the exheader.

### Kernel errors

Two kernel results quoted above: `0x09401BFE` is the timeout/not-ready result of the wait calls, `0xD920060D` the result for a thread marked for termination [reverse-engineered: Citra notes]. Kernel results use the standard 3DS error-code layout; see 3dbrew's SVC page for a table.

## Homebrew notes

> Conflict: 3dbrew's SVC table says `Backdoor` (0x7B) is used on ARM9, that ARM11 processes cannot call it without some form of kernelhax, and that it was removed from the ARM11 kernel in 11.0.0-33. Luma3DS's `k11_extension/source/svc.c` (`buildAlteredSvcTable`) nevertheless installs its own `Backdoor` at 0x7B. Both are consistent: stock ARM11 firmware from 11.0.0-33 has no userland Backdoor, but under Luma3DS the call exists again [documented: Luma3DS source at the pinned commit]. This page reports both and treats availability as "only under CFW" for current firmware.

The `Backdoor` (0x7B) call and kernel exploits turn userland control into full ARM11 kernel access (see [exploits](../homebrew/exploits.md)). `libctru` wraps the system calls and `svcGetSystemTick` is used for precise timing. Luma's `buildAlteredSvcTable` replaces these official calls with hooks: 0x01 ControlMemory, 0x03 ExitProcess, 0x08 CreateThread (New 3DS only), 0x29-0x2D (GetHandleInfo, GetSystemInfo, GetProcessInfo, GetThreadInfo, ConnectToPort), 0x32 SendSyncRequest, 0x3C Break, 0x59 SetGpuProt, 0x5A SetWifiEnabled, 0x76 TerminateProcess, 0x7B Backdoor, 0x7C KernelSetState. It adds custom calls: 0x80 CustomBackdoor; 0x90-0x94 cache/address helpers (`convertVAToPA`, data-cache range/entire flush, instruction-cache range/entire invalidate); 0xA0-0xA3 (`MapProcessMemoryEx`, `UnmapProcessMemoryEx`, `ControlMemoryEx`, `ControlMemoryUnsafe`); 0xB0-0xB3 (`ControlService`, `CopyHandle`, `TranslateHandle`, `ControlProcess`) [documented: Luma3DS `k11_extension/source/svc.c`]. Custom firmware extends the kernel: Luma3DS injects `k11_extension` to hook and add supervisor calls ([Luma3DS](../homebrew/luma3ds.md)).

## Open questions

- The scheduler's exact policies are only partly known; the Citra notes flag the address-arbiter, memory-mapping and DMA cases as unverified.
- Behavior of `CreateCodeSet`/`CreateProcess`/`Run` on 3dbrew is marked unconfirmed.

## Sources

- [3dbrew: Kernel](https://www.3dbrew.org/wiki/Kernel) and kernel object pages: [KThreadContext](https://www.3dbrew.org/wiki/KThreadContext), [KCoreContext](https://www.3dbrew.org/wiki/KCoreContext), [KProcessHwInfo](https://www.3dbrew.org/wiki/KProcessHwInfo), [KSchedulableInterruptEventLinkedList](https://www.3dbrew.org/wiki/KSchedulableInterruptEventLinkedList), [KInterruptEventMailbox](https://www.3dbrew.org/wiki/KInterruptEventMailbox), [KRecursiveLock](https://www.3dbrew.org/wiki/KRecursiveLock) — kernel structures.
- [3dbrew: SVC](https://www.3dbrew.org/wiki/SVC) — system call list.
- [3dbrew: Multi-threading](https://www.3dbrew.org/wiki/Multi-threading) — processes, threads, synchronization, capability descriptors.
- [3dbrew: CTR API](https://www.3dbrew.org/wiki/CTR_API) — critical sections.
- [3dbrew: Memory layout](https://www.3dbrew.org/wiki/Memory_layout) — thread context pages, kernel pointers.
- Citra project wiki, "Kernel synchronization" (scheduler, waitlists, rescheduling cases) — the `citra-emu/citra` repository that hosted it has since been removed from GitHub, so no live link could be verified.
- [libctru `svc.h`, `thread.h`, `apt.h`](https://github.com/devkitPro/libctru) — resource limit types, arbitration types, processor IDs, application CPU time limit calls.
- [Luma3DS `k11_extension`](https://github.com/LumaTeam/Luma3DS) — the supervisor calls Luma adds or replaces ([Luma3DS](../homebrew/luma3ds.md)).
- [Rodrigo Copetti, Nintendo 3DS Architecture: Operating System](https://www.copetti.org/writings/consoles/nintendo-3ds/) — scheduler behavior per core and the 30% lending rule.
