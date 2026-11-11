# CTR system call (SVC) reference

A **[system call](../../glossary.md#syscall)** (on ARM, a **supervisor call** or **[SVC](../glossary.md#svc)**) is how a program asks the [kernel](kernel.md) to do something privileged: allocate memory, start a thread, wait for an event, send a message to a [service](services.md). This page is the full table of 3DS system calls, the structures and enumerations they use, and the details of the multi-purpose calls (`KernelSetState`, `GetSystemInfo`, `GetProcessInfo`, `GetHandleInfo`). The ones Luma3DS adds are at the end.

The kernel page covers what the calls mean in practice (threads, scheduling, synchronization). Field-level layouts of the kernel objects they act on are on [kernel objects](kernel-objects.md). Most programs call these through [libctru](../homebrew/libctru.md) wrappers (`svcCreateThread` and so on), not by writing `svc` instructions.

## Behavior

- A process may only execute system calls enabled in its **SVC access-control mask** (copied from the exheader's kernel capability descriptors). Ordinary applications only have access to SVCs up to `0x3D`, and not all of those; calling a disabled one panics the kernel.
- The argument lists below are those of the official userland wrapper functions. The actual register order passed through the `svc` instruction is documented separately by 3dbrew as the "Kernel ABI".
- A **stubbed** call only returns the error `0xF8C007F4` ("invalid type"), which is also what `KernelSetState` returns for an unknown type. Stubbed calls are marked in the notes column.
- The same call can exist in three kernels, shown in the table as `NF11` (the ARM11 kernel of `NATIVE_FIRM`, the normal system), `NF9` (the ARM9 kernel of `NATIVE_FIRM`) and `TF11` (the ARM11 kernel of `TWL_FIRM`, used for DS/DSi software). See [firmware](firmware.md) for the FIRM types. Everything else on this page concerns `NF11`.

## Technical details

### Call table

`Y` = implemented, `N` = not implemented, `N?` = not confirmed. Calls not listed (for example `0x3F`-`0x46`, `0x5B`-`0x5F`, `0x6E`-`0x6F`, `0x7E`-`0xFE`) have no entry in the 3dbrew table.

| ID | NF11 | NF9 | TF11 | Signature | Notes |
| -- | ---- | --- | ---- | --------- | ----- |
| `0x01` | Y | N | N | `Result ControlMemory(u32* outaddr, u32 addr0, u32 addr1, u32 size, MemoryOperation operation, MemoryPermission permissions)` | Outaddr is usually the same as the input addr0. |
| `0x02` | Y | N | N | `Result QueryMemory(MemoryInfo* info, PageInfo* out, u32 Addr)` |  |
| `0x03` | Y | N | N | `void ExitProcess(void)` |  |
| `0x04` | Y | N | N | `Result GetProcessAffinityMask(u8* affinitymask, Handle process, s32 processorcount)` |  |
| `0x05` | Y | N | N | `Result SetProcessAffinityMask(Handle process, u8* affinitymask, s32 processorcount)` |  |
| `0x06` | Y | N | N | `Result GetProcessIdealProcessor(s32 *idealprocessor, Handle process)` |  |
| `0x07` | Y | N | N | `Result SetProcessIdealProcessor(Handle process, s32 idealprocessor)` |  |
| `0x08` | Y | Y | Y | `Result CreateThread(Handle* thread, func entrypoint, u32 arg, u32 stacktop, s32 threadpriority, s32 processorid)` |  |
| `0x09` | Y | Y | Y | `void ExitThread(void)` |  |
| `0x0A` | Y | Y | Y | `void SleepThread(s64 nanoseconds)` |  |
| `0x0B` | Y | Y | Y | `Result GetThreadPriority(s32* priority, Handle thread)` |  |
| `0x0C` | Y | Y | Y | `Result SetThreadPriority(Handle thread, s32 priority)` |  |
| `0x0D` | Y | N | N | `Result GetThreadAffinityMask(u8* affinitymask, Handle thread, s32 processorcount)` |  |
| `0x0E` | Y | N | N | `Result SetThreadAffinityMask(Handle thread, u8* affinitymask, s32 processorcount)` | Replaced with a stub in ARM11 NATIVE_FIRM kernel beginning with 8.0.0-18. |
| `0x0F` | Y | N | N | `Result GetThreadIdealProcessor(s32* processorid, Handle thread)` |  |
| `0x10` | Y | N | N | `Result SetThreadIdealProcessor(Handle thread, s32 processorid)` | Replaced with a stub in ARM11 NATIVE_FIRM kernel beginning with 8.0.0-18. |
| `0x11` | Y | N | N | `s32 GetCurrentProcessorNumber(void)` |  |
| `0x12` | Y | N | N | `Result Run(Handle process, StartupInfo* info)` | This starts the main() thread. Buf+0 is main-thread priority, Buf+4 is main-thread stack-size. |
| `0x13` | Y | Y | Y | `Result CreateMutex(Handle* mutex, bool initialLocked)` |  |
| `0x14` | Y | Y | Y | `Result ReleaseMutex(Handle mutex)` |  |
| `0x15` | Y | Y | Y | `Result CreateSemaphore(Handle* semaphore, s32 initialCount, s32 maxCount)` |  |
| `0x16` | Y | Y | Y | `Result ReleaseSemaphore(s32* count, Handle semaphore, s32 releaseCount)` |  |
| `0x17` | Y | Y | Y | `Result CreateEvent(Handle* event, ResetType resettype)` |  |
| `0x18` | Y | Y | Y | `Result SignalEvent(Handle event)` |  |
| `0x19` | Y | Y | Y | `Result ClearEvent(Handle event)` |  |
| `0x1A` | Y | Y | Y | `Result CreateTimer(Handle* timer, ResetType resettype)` |  |
| `0x1B` | Y | Y | Y | `Result SetTimer(Handle timer, s64 initial_nanoseconds, s64 interval)` |  |
| `0x1C` | Y | Y | Y | `Result CancelTimer(Handle timer)` |  |
| `0x1D` | Y | Y | Y | `Result ClearTimer(Handle timer)` |  |
| `0x1E` | Y | N | N | `Result CreateMemoryBlock(Handle* memblock, u32 addr, u32 size, MemoryPermission mypermission, MemoryPermission otherpermission)` |  |
| `0x1F` | Y | N | N | `Result MapMemoryBlock(Handle memblock, u32 addr, MemoryPermission mypermissions, MemoryPermission otherpermission)` |  |
| `0x20` | Y | N | N | `Result UnmapMemoryBlock(Handle memblock, u32 addr)` |  |
| `0x21` | Y | Y | Y | `Result CreateAddressArbiter(Handle* arbiter)` |  |
| `0x22` | Y | Y | Y | `Result ArbitrateAddress(Handle arbiter, u32 addr, ArbitrationType type, s32 value, s64 nanoseconds)` |  |
| `0x23` | Y | Y | Y | `Result CloseHandle(Handle handle)` |  |
| `0x24` | Y | Y | Y | `Result WaitSynchronization1(Handle handle, s64 timeout_nanoseconds)` |  |
| `0x25` | Y | Y | Y | `Result WaitSynchronizationN(s32* out, Handle* handles, s32 handlecount, bool waitAll, s64 timeout_nanoseconds)` |  |
| `0x26` | Y | N | N | `Result SignalAndWait(s32* out, Handle signal, Handle* handles, s32 handleCount, bool waitAll, s64 nanoseconds)` | Stubbed |
| `0x27` | Y | Y | Y | `Result DuplicateHandle(Handle* out, Handle original)` |  |
| `0x28` | Y | Y | Y | `s64 GetSystemTick(void) (This returns the total CPU ticks elapsed since the CPU was powered-on)` |  |
| `0x29` | Y | N | N | `Result GetHandleInfo(s64* out, Handle handle, HandleInfoType type)` |  |
| `0x2A` | Y | Y | Y | `Result GetSystemInfo(s64* out, SystemInfoType type, s32 param)` |  |
| `0x2B` | Y | Y | Y | `Result GetProcessInfo(s64* out, Handle process, ProcessInfoType type)` |  |
| `0x2C` | Y | Y | Y | `Result GetThreadInfo(s64* out, Handle thread, ThreadInfoType type)` |  |
| `0x2D` | Y | N | N | `Result ConnectToPort(Handle* out, const char* portName)` |  |
| `0x2E` | Y | N | N | `Result SendSyncRequest1(Handle session)` | Stubbed |
| `0x2F` | Y | N | N | `Result SendSyncRequest2(Handle session)` | Stubbed |
| `0x30` | Y | N | N | `Result SendSyncRequest3(Handle session)` | Stubbed |
| `0x31` | Y | N | N | `Result SendSyncRequest4(Handle session)` | Stubbed |
| `0x32` | Y | N | N | `Result SendSyncRequest(Handle session)` |  |
| `0x33` | Y | N | N | `Result OpenProcess(Handle* process, u32 processId)` |  |
| `0x34` | Y | N | N | `Result OpenThread(Handle* thread, Handle process, u32 threadId)` |  |
| `0x35` | Y | N | Y | `Result GetProcessId(u32* processId, Handle process)` |  |
| `0x36` | Y | N | N | `Result GetProcessIdOfThread(u32* processId, Handle thread)` |  |
| `0x37` | Y | Y | Y | `Result GetThreadId(u32* threadId, Handle thread)` |  |
| `0x38` | Y | N | N | `Result GetResourceLimit(Handle* resourceLimit, Handle process)` |  |
| `0x39` | Y | N | N | `Result GetResourceLimitLimitValues(s64* values, Handle resourceLimit, LimitableResource* names, s32 nameCount)` |  |
| `0x3A` | Y | N | N | `Result GetResourceLimitCurrentValues(s64* values, Handle resourceLimit, LimitableResource* names, s32 nameCount)` |  |
| `0x3B` | Y | N | N | `Result GetThreadContext(ThreadContext* context, Handle thread)` | Stubbed |
| `0x3C` | Y | Y | Y | `Break(BreakReason reason)`; also `Break(BreakReason debugReason, const void* croInfo, u32 croInfoSize)` | The second form is used for LOAD_RO/UNLOAD_RO |
| `0x3D` | Y | Y | Y | `OutputDebugString(void const, int)` | Does nothing on non-debug units. |
| `0x3E` | Y | N | N | `ControlPerformanceCounter(unsigned long long, int, unsigned int, unsigned long long)` |  |
| `0x47` | Y | N | N | `Result CreatePort(Handle* portServer, Handle* portClient,  const char* name, s16 maxSessions)` | Setting name=NULL creates a private port not accessible from svcConnectToPort. |
| `0x48` | Y | N | N | `Result CreateSessionToPort(Handle* session, Handle port)` |  |
| `0x49` | Y | N | N | `Result CreateSession(Handle* sessionServer, Handle* sessionClient)` |  |
| `0x4A` | Y | N | N | `Result AcceptSession(Handle* session, Handle port)` |  |
| `0x4B` | Y | N | N | `Result ReplyAndReceive1(s32* index, Handle* handles, s32 handleCount, Handle replyTarget)` | Stubbed. |
| `0x4C` | Y | N | N | `Result ReplyAndReceive2(s32* index, Handle* handles, s32 handleCount, Handle replyTarget)` | Stubbed. |
| `0x4D` | Y | N | N | `Result ReplyAndReceive3(s32* index, Handle* handles, s32 handleCount, Handle replyTarget)` | Stubbed. |
| `0x4E` | Y | N | N | `Result ReplyAndReceive4(s32* index, Handle* handles, s32 handleCount, Handle replyTarget)` | Stubbed. |
| `0x4F` | Y | N | N | `Result ReplyAndReceive(s32* index, Handle* handles, s32 handleCount, Handle replyTarget)` |  |
| `0x50` | Y | Y | Y | `Result BindInterrupt(Interrupt name, Handle eventOrSemaphore, s32 priority, bool isLevelHighActive)` |  |
| `0x51` | Y | Y | Y | `Result UnbindInterrupt(Interrupt name, Handle eventOrSemaphore)` |  |
| `0x52` | Y | Y | Y | `Result InvalidateProcessDataCache(Handle process, void* addr, u32 size)` |  |
| `0x53` | Y | Y | Y | `Result StoreProcessDataCache(Handle process, void const* addr, u32 size)` |  |
| `0x54` | Y | Y | Y | `Result FlushProcessDataCache(Handle process, void const* addr, u32 size)` |  |
| `0x55` | Y | Y | Y | `Result StartInterProcessDma(Handle* dma, Handle dstProcess, void* dst, Handle srcProcess, const void* src, u32 size, const DmaConfig* config)` |  |
| `0x56` | Y | Y | Y | `Result StopDma(Handle dma)` |  |
| `0x57` | Y | Y | Y | `Result GetDmaState(DmaState* state, Handle dma)` |  |
| `0x58` | Y | Y | Y | `RestartDma(Handle, void *, void  const*, unsigned int, signed char)` |  |
| `0x59` | Y | N? | N | `SetGpuProt(s8 input_flag). Implemented with 11.3.0-X, see below.` |  |
| `0x5A` | Y | N? | N | `SetWifiEnabled(s0 input_flag). Implemented with 11.4.0-X, see below.` |  |
| `0x60` | Y | N | N | `Result DebugActiveProcess(Handle* debug, u32 processID)` |  |
| `0x61` | Y | N | N | `Result BreakDebugProcess(Handle debug)` |  |
| `0x62` | Y | N | N | `Result TerminateDebugProcess(Handle debug)` |  |
| `0x63` | Y | N | N | `Result GetProcessDebugEvent(DebugEventInfo* info, Handle debug)` |  |
| `0x64` | Y | N | N | `Result ContinueDebugEvent(Handle debug, u32 flags)` |  |
| `0x65` | Y | N | N | `Result GetProcessList(s32* processCount, u32* processIds, s32 processIdMaxCount)` |  |
| `0x66` | Y | N | N | `Result GetThreadList(s32* threadCount, u32* threadIds, s32 threadIdMaxCount, Handle domain)` |  |
| `0x67` | Y | N | N | `Result GetDebugThreadContext(ThreadContext* context, Handle debug, u32 threadId, u32 controlFlags)` |  |
| `0x68` | Y | N | N | `Result SetDebugThreadContext(Handle debug, u32 threadId, const ThreadContext* context, u32 controlFlags)` |  |
| `0x69` | Y | N | N | `Result QueryDebugProcessMemory(MemoryInfo* blockInfo, PageInfo* pageInfo, Handle debug, u32 addr)` |  |
| `0x6A` | Y | N | N | `Result ReadProcessMemory(void* buffer, Handle debug, u32 addr, u32 size)` |  |
| `0x6B` | Y | N | N | `Result WriteProcessMemory(Handle debug, void const* buffer, u32 addr, u32 size)` |  |
| `0x6C` | Y | N | N | `Result SetHardwareBreakPoint(s32 registerId, u32 control, u32 value)` |  |
| `0x6D` | Y | N | N | `GetDebugThreadParam(s64* unused, u32* out, Handle kdebug, u32 threadId, DebugThreadParameter param)` |  |
| `0x70` | Y | N | N | `Result ControlProcessMemory(Handle KProcess, unsigned int Addr0, unsigned int Addr1, unsigned int Size, unsigned int Type, unsigned int Permissions)` |  |
| `0x71` | Y | N | N | `Result MapProcessMemory(Handle process, u32 startAddr, u32 size)` |  |
| `0x72` | Y | N | N | `Result UnmapProcessMemory(Handle process, u32 startAddr, u32 size)` |  |
| `0x73` | Y | N | N | `Result CreateCodeSet(Handle* handle_out, struct CodeSetInfo, u32 code_ptr, u32 ro_ptr, u32 data_ptr)` |  |
| `0x74` | Y | N | N | `Result RandomStub()` | Stubbed |
| `0x75` | Y | N | N | `Result CreateProcess(Handle* handle_out, Handle codeset_handle, u32 arm11kernelcaps_ptr, u32 arm11kernelcaps_num)` |  |
| `0x76` | Y | N | N | `TerminateProcess(Handle)` |  |
| `0x77` | Y | N | N | `Result SetProcessResourceLimits(Handle KProcess, Handle KResourceLimit)` |  |
| `0x78` | Y | N | N | `Result CreateResourceLimit(Handle *KResourceLimit)` |  |
| `0x79` | Y | N | N | `Result SetResourceLimitLimitValues(Handle res_limit, LimitableResource* resource_type_list, s64* resource_list, u32 count)` |  |
| `0x7A` | Y | N | Y | `AddCodeSegment (unsigned int Addr, unsigned int Size)` | Stubbed on NATIVE_FIRM beginning with 2.0.0-2. Used during TWL_FIRM boot. |
| `0x7B` | Y | Y | N | `Backdoor(unsigned int CodeAddress)` | This is used on ARM9 NATIVE_FIRM. No ARM11 processes have access to it without some form of kernelhax, and this was removed on 11.0.0-33 (for ARM11). |
| `0x7C` | Y | Y | Y | `KernelSetState(unsigned int Type, ...)` | The type determines the args to be passed |
| `0x7D` | Y | N | N | `Result QueryProcessMemory(MemInfo *Info, unsigned int *Out, Handle KProcess, unsigned int Addr)` |  |
| `0xFF` | Y | Y | Y | `Stop point` | The svcaccesscontrol mask doesn't apply for this SVC. This svc doesn't check the "debug mode enabled" flag either. Does nothing if there is no KDebug object associated to the current process. Stubbed on ARM9 NATIVE_FIRM. |

Additional notes from 3dbrew:

- `0xFF` (stop point) is used by debuggers; it needs no entry in the access-control mask, does not check the "debug mode" flag and does nothing if the current process has no `KDebug` object.
- Stubbed calls named in the notes: `SignalAndWait` (`0x26`), `SendSyncRequest1-4` (`0x2E`-`0x31`), `GetThreadContext` (`0x3B`), `ReplyAndReceive1-4` (`0x4B`-`0x4E`), `RandomStub` (`0x74`). `Set{Thread}AffinityMask` (`0x0E`), `SetThreadIdealProcessor` (`0x10`) and `AddCodeSegment` (`0x7A`) became stubs on `NATIVE_FIRM` at the versions noted.
- Thread-priority and CPU calls on the Old 3DS and New 3DS: see [kernel](kernel.md#threads-priorities-and-cores).

### Types and enumerations

| Name | Values |
| ---- | ------ |
| `ResetType` (events, timers) | 0 `ONESHOT`, 1 `STICKY`, 2 `PULSE` |
| `BreakReason` | 0 `PANIC`, 1 `ASSERT`, 2 `USER`; user-break events also carry 3 `LOAD_RO`, 4 `UNLOAD_RO` (used by the [RO service](cro-modules.md) so a debugger learns about module loads) |
| `StartupInfo` (`Run`) | s32 priority, u32 stack size, s32 argc, s16* argv, s16* envp (`argc`, `argv`, `envp` are ignored) |
| `ArbitrationType` | 0 `SIGNAL`, 1 `WAIT_IF_LESS_THAN`, 2 `DECREMENT_AND_WAIT_IF_LESS_THAN`, 3 `WAIT_IF_LESS_THAN_TIMEOUT`, 4 `DECREMENT_AND_WAIT_IF_LESS_THAN_TIMEOUT` |
| `DebugThreadParameter` | 0 `PRIORITY`, 1 `SCHEDULING_MASK_LOW`, 2 `CPU_IDEAL`, 3 `CPU_CREATOR` |
| `ThreadContext` (`0xCC` bytes) | `+0x0` `CpuRegisters` (r0-r12, sp, lr, pc, cpsr), `+0x44` `FpuRegisters` (d0-d15, fpscr, fpexc). The caller must adjust `pc` for exceptions taken in Thumb mode |
| Debug thread-context flags | bit 0 CPU general registers (r0-r12), bit 1 CPU special registers (sp, lr, pc, cpsr), bit 2 FPU general registers (d0-d15), bit 3 FPU special registers (fpscr, fpexc) |
| `MemOp` (Luma3DS) | `1` free, `2` reserve, `3` commit, `4` map, `5` unmap, `6` protect; `+0x100` APPLICATION, `+0x200` SYSTEM, `+0x300` BASE region; `+0x10000` linear (mask `0xFF` op, `0xF00` region) [documented: Luma3DS `kernel.h`] |
| `MemPerm` | bit 0 read, bit 1 write, bit 2 execute; `0x10000000` don't-care |
| `LimitableResource` | 0 priority, 1 commit, 2 thread, 3 event, 4 mutex, 5 semaphore, 6 timer, 7 shared memory, 8 address arbiter, 9 CPU time (see [kernel](kernel.md#resource-limits-and-arbitration)) |

Special handles: `0xFFFF8000` is the currently running thread and `0xFFFF8001` the current process. Handles are valid only in the process that created them; the [IPC](../../glossary.md#ipc) functions are the way to copy one to another process.

When setting the CPSR through `SetDebugThreadContext` the kernel stores `ctx->cpsr = (ctx->cpsr & 0x7F0FDFF) | (userCtx->cpsr & 0xF80F0200)`, so a debugger cannot change processor mode or interrupt-disable bits.

Timer and event reset types: a `STICKY` object stays signalled and wakes waiters until a thread explicitly resets it; a `ONESHOT` object wakes exactly one thread and then resets itself; a `PULSE` timer resets after waking one thread and is started again immediately. It is unknown whether `PULSE` is valid for events.

### Interrupts and DMA

`svcBindInterrupt` registers an event or semaphore in the global interrupt table for an interrupt ID. Interrupts 0-14 and 16-31 can never be bound, whatever the process's exheader interrupt flags say, and the flags are not checked when binding interrupt 15. The "level-high active / manual clear" argument must be false when binding a semaphore (otherwise `0xD8E007EE`, invalid combination). Binding an ID that already has an object returns `0xD8E007F0`. Applications wait for the interrupt with `WaitSynchronization` on the handle. The CTR SDK calls `svcBreak` when `svcStartInterProcessDma` returns an error (except a few values), so on retail a system module whose DMA request fails terminates [documented: 3dbrew SVC].

### Debugging calls

`DebugActiveProcess` needs the target process's exheader kernel flag "Enable debug", or the calling process needs "Force debug"; it also only works when a kernel-state debug flag is non-zero (zero on retail). `svcSetHardwareBreakPoint` writes the ARM debug unit registers: `registerId` 0-5 are breakpoints (BRP0-5), `0x100`-`0x101` are watchpoints (WRP0-1), anything else is an error. The kernel sets bit 15 of the DSCR to enable monitor-mode debugging; when bit 21 of the control value is set (breakpoint type "context ID"), the kernel loads the target process's context ID from the `KDebug` handle passed as `value`. It then disables the (B/W)RP, writes the value to (B/W)VR and the control value to (B/W)CR [reverse-engineered: 3dbrew SVC].

#### Debug events

`svcGetProcessDebugEvent` takes the first queued `KEventInfo` of a process's `KDebug` and returns it as `DebugEventInfo` (`0x28` bytes): u32 event type, u32 thread ID (not used by all events), u32 flags (bit 0 = `svcContinueDebugEvent` must be called), u8[4] leftover flags (always 0 here), u32[6] event-specific data. The kernel-side layout is on [kernel objects](kernel-objects.md#debug-objects).

| Event | ID | Notes |
| ----- | -- | ----- |
| ATTACH PROCESS | 0 | Data: u64 program ID, char[8] process name, u32 process ID, u32 "other" flag (always 0) |
| ATTACH THREAD | 1 | Emitted for each thread when attaching and for each thread later created in the process |
| EXIT THREAD | 2 | Reason: 0 none, 1 terminate, 2 exit process, 3 terminate process |
| EXIT PROCESS | 3 | Non-blocking. Reason: 0 none, 1 terminate, 2 debug terminate |
| EXCEPTION | 4 | See exception types below |
| DLL LOAD, DLL UNLOAD | 5, 6 | Completely removed from the kernel but referenced by the DMNT module; early system development apparently used dynamic libraries |
| SCHEDULE IN, SCHEDULE OUT | 7, 8 | Non-blocking; the kernel has handling code but nothing signals them |
| SYSCALL IN, SYSCALL OUT | 9, 10 | Same |
| OUTPUT STRING | 11 | Data: string address, size |
| MAP | 12 | Non-blocking; same note. Data: address, size, `MemoryPermission`, `MemoryState` |

All events except the non-blocking ones stop every thread of the debugged process until `svcContinueDebugEvent`. Attaching signals ATTACH PROCESS, then ATTACH THREAD for each open thread, then ATTACH BREAK.

Exception types: 0 `UNDEFINED INSTRUCTION`, 1 `PREFETCH ABORT`, 2 `DATA ABORT`, 3 `UNALIGNED DATA ACCESS`, 4 `ATTACH BREAK`, 5 `STOP POINT`, 6 `USER BREAK`, 7 `DEBUGGER BREAK`, 8 `UNDEFINED SYSCALL`. Data for the first, second, third, fourth and last types is one u32 of fault information (Fault Address Register for data aborts and unaligned access, the attempted SVC ID for `UNDEFINED SYSCALL`, otherwise 0). `STOP POINT` carries the stop-point type (0 = `svc 0xFF`, 1 = breakpoint, 2 = watchpoint) and a fault address for watchpoints. `USER BREAK` carries the break reason and, for `LOAD_RO`/`UNLOAD_RO`, the module info. `DEBUGGER BREAK` carries the s32[4] IDs of the threads that were running on each core (-1 if none; only the first two are meaningful on the Old 3DS).

> **New 3DS:** the four-entry `DEBUGGER BREAK` array exists because the New 3DS has four cores; on the Old 3DS only entries 0 and 1 are used.

### KernelSetState types

`KernelSetState` is the catch-all privileged call; its first argument selects the operation. Anything needing cross-core synchronisation uses software-generated interrupt #6. Availability: NF11 = `NATIVE_FIRM` ARM11 kernel, TF11 = `TWL_FIRM` ARM11 kernel.

| Type | NF11 | TF11 | Arguments and effect |
| ---- | ---- | ---- | -------------------- |
| 0 | Y | N | `u64 firmTitleID`. Initialises the program ID for FIRM launch and triggers it. The upper 32 bits mean something special on the New 3DS (0 when using New 3DS `pm`); the upper half is always set to `0x40138`. The New 3DS kernel forces the New 3DS `NATIVE_FIRM` when the ID given is 2 and disables the extra New 3DS cache hardware before the firmlaunch function |
| 1 | Y | Y | Does nothing |
| 2 | Y | Y | Powers down the GPU and syncs with Process9 (waits for `PXI_SYNC11` to be 1). New 3DS: disables the extra cache hardware first, if enabled |
| 3 | Y | N | `0, void* address` or `1`. Manages the `0x1000`-byte buffer handed to the launched FIRM: with 1 it is copied to the start of FCRAM (3dbrew gives the address as `0xE0000000`); with 0 the kernel buffer is mapped at the given process address |
| 4 | N | Y | Unmaps(?) ranges `0x00300000..0x04300000`, `0x08000000..0x0FE00000`, `0x10000000..0xF8000000` by writing physical-address-based entries (base `0x80000000`) to the L1 MMU table |
| 5 | Y | Y | One u32: power-state change. 0 = shutdown/reboot: all ARM11 cores enter an infinite `WFI`/branch loop (used by kernel panic and `PTM`) |
| 6 | Y | N | `u32 what, u64 val`; needs a non-zero `UNITINFO` for `what` 1 and 2. 1 = enable `ERR:F`-style register dumps on user-mode CPU/VFP exceptions when `val` is non-zero (the user handler, its stack pointer and the exception-info pointer are contiguous in the thread's TLS at offset `0x40`, or in the main thread's TLS if the handler is NULL; a stack or buffer value of 1 means `sp_usr - 0x5c`). 2 = call kernel panic when `svcBreak` is used by a non-attached process (set up by `NS` on dev units from config block `0x000F0000`, default 0). 3 = change the scheduling/preemption mode, only when no threads are being preempted (else `0xC8A01414`); see `KResourceLimit` on 3dbrew |
| 7 | Y | N | Hard reboot through the MCU: writes value 4 to register `0x20` of device `0x4A` on the second I2C bus; does not return. New 3DS: disables the extra cache hardware first |
| 8 | Y | N | Hangs the ARM9 using a path similar to firmlaunch; used by `PTM` on shutdown/reboot |
| 9 | Y (after system version 4.5.0) | ? | `u64 titleID`. Stored in a global; a process created with a non-zero equal title ID gets `KProcessHwInfo+0x32` ("currently running app") set. Used by `NS` depending on the NS CFA contents |
| 10 | Y (New 3DS kernel only) | ? | `u32 config`: `ConfigureNew3DSCPU`. Only bits 0-1 are used: bit 0 enables the higher core clock, bit 1 the additional L2 cache. The code runs on all ARM11 cores through a function-pointer handler and also programs MPCore private-memory registers. See [CPU](../hardware/cpu.md#new-3ds-extra-cores) |

> **New 3DS:** type 10 exists only on the New 3DS kernel. Type 0 (`firmTitleID` of 2 selects the New 3DS `NATIVE_FIRM`) and types 2 and 7 also handle the New 3DS cache hardware.

### GetSystemInfo, GetProcessInfo, GetHandleInfo

`GetSystemInfo(out, type, param)`:

| Type | Param | Result |
| ---- | ----- | ------ |
| 0 | 0 | Used memory in APPLICATION, SYSTEM and BASE combined |
| 0 | 1, 2, 3 | Used memory in the APPLICATION, SYSTEM or BASE region |
| 2 | unused | FCRAM memory used by the kernel |
| 25 | unused | Number of threads launched directly by the kernel (no longer exists in some kernel versions?) |
| 26 | unused | Number of processes launched directly by the kernel: normally 5 on `NATIVE_FIRM`/`SAFE_MODE_FIRM` (`sm`, `fs`, `pm`, `loader`, `pxi`) |

`GetProcessInfo(out, process, type)` (output in `r1`/`r2`, low and high word):

| Type | Since | Result |
| ---- | ----- | ------ |
| 0 | | Private (code, data, regular heap) plus shared memory, plus total supervisor-mode stack size plus the page-rounded external handle table: the physical memory used, minus TLS, main-thread stack and linear memory |
| 1, 3 | | "Related unused field" variants of types 0 and 2 plus stack and handle-table sizes |
| 2 | | Private (code, data, heap) memory plus stack and handle-table sizes |
| 4 | | Handles currently in use |
| 5 | | Highest number of handles open at once |
| 6 | | Always 0 (reads `KProcess+0x234`) |
| 7 | | Number of threads |
| 8 | | Maximum threads the process can open (always 0) |
| 9-18 | 8.0.0-18 | Only returns error `0xD8E007ED` |
| 19 | stub 8.0.0-18, implemented 11.3.0 | Memory region of the process: low word = exheader kernel flags `& 0xF00`, high word 0 |
| 20 | 8.0.0-18 | Low word = `0x20000000 - <process linear virtual base>`: the amount to add to a linear virtual address to get its physical address |
| 21-23 | 8.0.0-18, **New 3DS** only | Virtual-to-physical offset, base virtual address and size of the QTM static memory block reserved in the exheader (`0x800000` is quoted as the exheader size); `0` plus error `0xE0E01BF4` if the block does not exist |

`GetHandleInfo(out, handle, type)`: type 0 returns the creation time in ticks of the `KProcess` (garbage from `r5`/`r6` if the handle is not a process); type 1 returns the object's reference count (minus the one the call itself adds); type 2 is unimplemented and returns whatever `r5`/`r6` held; `0x32107` returns 0. `GetThreadInfo` always returns an error, only distinguishing thread handles: `0xD8E007ED` (bad enum) if the handle is a thread, `0xD8E007F7` (bad handle) if not.

### SetGpuProt and SetWifiEnabled

- `svc 0x59 SetGpuProt` was added in system version 11.3.0 and is used by the GSP module from then on. It always returns 0. With `input_flag` not equal to 1 it uses 0 internally; if the stored state already matches it returns at once, otherwise it records the flag after running. The GSP module passes 0 for the APPLICATION memory region and 1 for others. The effect is a write of `0x100 | val` to `PDN` register base `+0x140`. For `input_flag` 1 `val` is fixed (**Old 3DS:** `0x3`; **New 3DS:** `0x460`); otherwise it depends on the kernel's APPMEMTYPE: Old 3DS types 2, 3, 4 give `0x3`, `0x5`, `0x6` and any other type `0x7`; New 3DS type 7 gives `0x490` and any other type `0x4F0`. The same register is set during kernel boot since 3.0.0-5 (Old 3DS `0x103`, New 3DS `0x550`) [reverse-engineered: 3dbrew SVC]. 3dbrew does not say what the register bits mean, only that this is GPU memory protection.
- `svc 0x5A SetWifiEnabled` (11.4.0 and later) sets or clears bit 0 of `CFG11_WIFICNT`, what `NWM` used to do directly [reverse-engineered: 3dbrew SVC; Luma3DS `SetWifiEnabled.c` implements exactly this].

> Conflict: 3dbrew documents `SetGpuProt` as writing the `PDN` register; Luma3DS's replacement (`k11_extension/source/svc/SetGpuProt.c`) is an empty function that just returns 0, so under Luma3DS the call does not change GPU protection. Both are true for their firmware: this page treats the 3dbrew description as stock behaviour and the Luma3DS version as the behaviour on a CFW system.

### Backdoor

`svc 0x7B Backdoor` saves the supervisor-mode SP and LR on the user stack, sets the SVC-mode SP to the user SP and calls the given function in supervisor mode; afterwards it restores SP and LR and returns. It does not disable IRQs, so an interrupt or context switch while SP points at the user stack crashes the ARM11 kernel and hangs the system [reverse-engineered: 3dbrew SVC]. See the conflict about its availability on [kernel](kernel.md#homebrew-notes).

### Kernel error codes

The standard 3DS result layout applies (see [kernel](kernel.md#kernel-errors)).

| Code | Meaning |
| ---- | ------- |
| `0x09401BFE` | Timeout in `WaitSynchronization*` (also the "object not ready" result for timeout 0) |
| `0xC8601801` | No free object in a class's pool (`KEvent`, `KMutex`, `KTimer`, `KSemaphore`, `KAddressArbiter`, `KThread`) |
| `0xC8601802` | No free `KSharedMemory` object |
| `0xC8601809` | No free `KSession` |
| `0xC860180A` | Not enough free memory for the allocation |
| `0xC920181A` | The session was closed by the other process |
| `0xD0401834` | Maximum connections to a port exceeded |
| `0xD8609013` | Unknown, probably resource-limit related (Luma3DS returns it when its 64-entry language-emulation table is full) |
| `0xD88007FA` | No `KObjectName` matches the port name given to the SVC |
| `0xD8E007ED` | Value outside the enumeration being used |
| `0xD8E007F1` | Misaligned address |
| `0xD8E007F7` | Handle not in the process's handle table, or the object type does not match what the SVC needs |
| `0xD9000402` | Invalid memory permissions for `svcStartInterProcessDma` buffers |
| `0xD9001814` | Failed unprivileged load or store (wrong memory permissions) |
| `0xD9001BF7` | The kernel got a pointer to an object of the wrong type |
| `0xD92007EA` | `svcCreateMemoryBlock` by a process of the application memory type with `addr` 0 |
| `0xE0E01BF5` | Invalid address |
| `0xF8C007F4` | Invalid `KernelSetState` type or parameters; also returned by every stubbed SVC |

### Luma3DS additions and replacements

Luma3DS's `k11_extension` builds a second table of 256 entries (`alteredSvcTable`): a copy of the `0x7E` official entries with the hooks below, plus new numbers. These exist only under [Luma3DS](../homebrew/luma3ds.md) [documented: `k11_extension/source/svc.c` and `svc/*.c`]. The signatures are the wrapper-level forms from the headers.

| ID | Name | Effect |
| -- | ---- | ------ |
| `0x01` | `ControlMemory` hook | Calls the stock call with an extra flag that is true when the caller is process ID 1 (the parameter is named `isLoader` in the source) |
| `0x03` | `ExitProcess` hook | If the process has the plugin "signal on exit" flag, signals the plugin exit event and unlocks threads locked by Rosalina, then calls the stock call |
| `0x08` | `CreateThread` hook (**New 3DS** only) | If the "redirect application threads" option is on, redirection is not disabled, the requested processor is 1 (syscore) and the process is in the APPLICATION region, the thread is created on core 2 instead. Old 3DS: not hooked |
| `0x29`-`0x2C` | `GetHandleInfo`, `GetSystemInfo`, `GetProcessInfo`, `GetThreadInfo` hooks | Types `>= 0x10000` are Luma-specific, other types go to the stock call (below) |
| `0x2D` | `ConnectToPort` hook | Validates and records the port name for every new session (limited to 11 characters; names too long return `0xE0E0181E`); zeroes the output handle on failure |
| `0x32` | `SendSyncRequest` hook | Intercepts a fixed list of IPC command headers (for example `cfg:` region/language queries for language emulation, `err:f` Throw, `ndm:u` workarounds); other requests take a fast path to the stock call |
| `0x3C` | `Break` hook | Uses the stock `Break` only if the process has a debugger attached, otherwise Luma's own version |
| `0x59` | `SetGpuProt` | Replaced by a function that returns 0 |
| `0x5A` | `SetWifiEnabled` | Sets/clears `CFG11_WIFICNT` bit 0 |
| `0x76` | `TerminateProcess` hook | Signals the plugin exit event (15 s timeout) before the stock call when the process has the "signal on exit" flag |
| `0x7B` | `Backdoor` | Reinstated so ARM11 userland with the SVC enabled can run code in supervisor mode |
| `0x7C` | `KernelSetState` hook | Adds types `0x10000`-`0x10008` and `0x10080` (below); type 10 additionally honours "lock/unlock faster speed" bits (bit 2 locks it on, bit 3 unlocks it) |
| `0x80` | `CustomBackdoor(void *function, ...)` | Like `Backdoor`, but passes up to three more arguments to the function in `r0`-`r2` |
| `0x90` | `convertVAToPA(addr, writeCheck)` | Translates a virtual address to physical with the CP15 address-translation operation (privileged read check, or write check if `writeCheck` is true); returns 0 on failure |
| `0x91`-`0x94` | cache helpers | `flushDataCacheRange`, `flushEntireDataCache`, `invalidateInstructionCacheRange`, `invalidateEntireInstructionCache` |
| `0xA0` | `MapProcessMemoryEx(dstProc, vaDst, srcProc, vaSrc, size, flags)` | Maps another process's memory into the destination, using memory state `0x5806` (shared) or, with flag bit 0, `0xBB05` (private) |
| `0xA1` | `UnmapProcessMemoryEx(proc, dst, size)` | Reverse of `0xA0` |
| `0xA2` | `ControlMemoryEx(...)` | `ControlMemory` with an extra "is loader" argument |
| `0xA3` | `ControlMemoryUnsafe(out, addr, size, op, perm)` | Commits (optionally linear) or frees memory in the current process without the stock checks; allocations are zeroed and cache-flushed |
| `0xB0` | `ControlService(op, arg1, arg2)` | Op 0: obtain a client session by service or global port name (`0x09401BFE` if the service is not initialised yet); op 1: get the port name of a client or server session handle |
| `0xB1` | `CopyHandle(out, outProc, handle, inProc)` | Copies a handle from one process's table into another's |
| `0xB2` | `TranslateHandle(outKernelAddr, outClassName, handle)` | Returns the kernel address and class name of the object behind a handle (see [kernel objects](kernel-objects.md#class-tokens-kclasstoken)) |
| `0xB3` | `ControlProcess(process, op, arg2, arg3)` | Op 0 list all handles (optionally filtered by class token); 1 make all of the process's memory RWX; 2 get an event signalled when its memory layout changes; 3 signal on exit; 4 virtual to physical address; 5 lock or unlock the process's threads (optionally through a predicate) |

Luma-specific information types (documented by comments in the source as unstable, "do not use these, except 0, 1 and 0x200"):

- `GetSystemInfo` type `0x10000`: params 0 (CFW version), 1 (commit hash), `2`-`0xA` (config format version, config bitfields, boot configuration, splash duration, volume-slider and LED overrides, MCU flags), `0x10`-`0x12` (autoboot settings), `0x80` (APPLICATION region size), `0x100`-`0x117` (Rosalina and screen-filter settings), `0x180`-`0x181` (plugin loader), `0x200`-`0x203` (release/New 3DS/SD-init/SD-mode flags), `0x300`-`0x301` (size of the kernel extension, size of SYSTEM memory taken for it), `0x400`-`0x4FF` (the launched path, 8 bytes at a time). Unknown params return `0xF8C007F4`. **New 3DS:** type `0x10001` reports the current clock rate (`(((CFG11_MPCORE_CLKCNT >> 1) & 3) + 1) * 268` MHz), the higher clock rate available and whether the L2 cache is enabled; on the Old 3DS it returns `0xF8C007F4`.
- `GetProcessInfo` types `0x10000`-`0x10009`: process name, title ID, `.text`/`.rodata`/`.data` sizes in bytes, their load addresses, translation table base and MMU table size/physical address.
- `GetThreadInfo` type `0x10000`: the thread's TLS address. `GetHandleInfo` types `0x10000` (context ID) and `0x10001` (class token) [documented: Luma3DS].
- `KernelSetState` types: `0x10000` toggle Rosalina thread locks (1 = application/applet/DSP threads, 2 = GSP, 4 = HID/IR), `0x10001` register a per-title language/region emulation entry (64 entries), `0x10002` enable or disable SVC-entry/exit debug events for a process (up to 3 processes), `0x10003` enable monitor-mode debugging on all cores, `0x10004`-`0x10006` clear/set hardware watchpoints 0 and 1 with a context ID, `0x10007` register the plugin signalling event, `0x10008` LED brightness override changed, `0x10080` disable thread redirection.
- Luma delivers SYSCALL IN/OUT debug events by piggybacking on the OUTPUT STRING event type with the pseudo-addresses `0xFFFFFFFE` and `0xFFFFFFFF` and the SVC ID as the size, because the real SYSCALL events are non-blocking and unused.

## Homebrew notes

Homebrew normally uses the wrappers in [libctru](../homebrew/libctru.md). Anything that needs privileged calls (`Backdoor`, `KernelSetState`, the debugging calls) needs either a [kernel exploit](../homebrew/exploits.md) or [Luma3DS](../homebrew/luma3ds.md) with the right exheader capabilities. `svcGetSystemTick` returns the CPU tick count since power-on and is the usual precise timer.

## Open questions

- The `NF9` and `TF11` columns come from 3dbrew and have not been rechecked against firmware.
- Meaning of the `PDN +0x140` bits written by `SetGpuProt`.
- Whether `PULSE` is a valid reset type for events.

## Sources

- [3dbrew: SVC](https://www.3dbrew.org/wiki/SVC) — call table, types, debug events, `KernelSetState`, `GetSystemInfo`/`GetProcessInfo`/`GetHandleInfo`, `svc 0x59`/`0x5A`, `Backdoor`, kernel error codes.
- [3dbrew: Multi-threading](https://www.3dbrew.org/wiki/Multi-threading) — `ArbitrationType`, `StartupInfo`, thread calls.
- [3dbrew: KEventInfo](https://www.3dbrew.org/wiki/KEventInfo) — kernel-side debug event layout.
- [Luma3DS `k11_extension`](https://github.com/LumaTeam/Luma3DS) — `svc.c`, `svc/*.c`, `include/kernel.h` for the replaced and added calls.
