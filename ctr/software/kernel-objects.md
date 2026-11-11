# CTR kernel objects and data structures

The ARM11 [kernel](../../glossary.md#kernel) keeps track of everything it manages (processes, threads, events, memory blocks, sessions) as **kernel objects**: small structures in kernel memory, each belonging to a named class such as `KProcess` or `KThread`. Programs never see these structures; they hold [handles](../../glossary.md#handle) that point at them. This page is the field-level reference for those structures, as reverse engineered by the 3dbrew community and as declared in the [Luma3DS](../homebrew/luma3ds.md) kernel extension.

Read it if you are writing a kernel exploit payload, a debugger, an emulator, or a Luma3DS-style patch. If you only want to know what threads, processes and system calls do from a program's point of view, start with the [kernel page](kernel.md) and the [SVC reference](svc-reference.md).

All layouts below are for the ARM11 kernel (`NATIVE_FIRM`) unless stated. Sizes and offsets describe the Old 3DS; where the New 3DS differs the difference is stated. Everything here is reverse engineered, so field names are community names, not Nintendo's.

## Where it fits

The kernel lives in [BASE-region](../hardware/memory.md#fcram-regions-appmemtype) memory and in the [AXI WRAM](../glossary.md#axi-wram) kernel pages. User programs reach objects only through handles in a per-process handle table (see [kernel](kernel.md#processes-and-threads)). Two helper families underlie most objects: the intrusive linked list (`KLinkedList`, `KLinkedListNode`) and the object pools (`KObjectList`, `KObjectLink`).

## Technical details

### Class tokens (`KClassToken`)

Every class has a 5-byte token: a pointer to the class name string (offset `0x0`) and a one-byte flag value (offset `0x4`). The kernel checks the token to make sure an object of a given class may be used for the requested operation. Bit 0 means "can synchronize" (the class derives from `KSynchronizationObject`, so `WaitSynchronization` works on it) and bit 1 means "is interruptible". The other bits are unnamed in the source, but the values below are unique per class, and Luma3DS's `svcControlProcess` uses them to filter a process's handles by type [documented: Luma3DS `kernel.h`, `ControlProcess.c`].

| Token | Class | Token | Class |
| ----- | ----- | ----- | ----- |
| `0x00` | `KAutoObject` | `0x95` | `KServerSession` |
| `0x01` | `KSynchronizationObject` | `0x98` | `KAddressArbiter` |
| `0x1F` | `KEvent` | `0xA5` | `KClientSession` |
| `0x2F` | `KSemaphore` | `0xA8` | `KPort` |
| `0x35` | `KTimer` | `0xB0` | `KSharedMemory` |
| `0x39` | `KMutex` | `0xC5` | `KProcess` |
| `0x4D` | `KDebug` | `0xC8` | `KResourceLimit` |
| `0x55` | `KServerPort` | `0x59` | `KDmaObject` |
| `0x65` | `KClientPort` | `0x68` | `KCodeSet` |
| `0x70` | `KSession` | `0x8D` | `KThread` |

The two 3dbrew sources agree on every value. Luma3DS reads the token through `GetClassToken` on kernels whose minor version is 46 or higher and, on older kernels, takes the low byte of what `GetClassName` returns instead [documented: Luma3DS `kernel.h`]. Which retail firmware versions that corresponds to is not stated in the sources.

### Base structures

| Structure | Size | Layout |
| --------- | ---- | ------ |
| `KAutoObject` | `0x8` | `+0x0` vtable pointer, `+0x4` reference count |
| `KSynchronizationObject` | `0x14` | `KAutoObject`, then a `KLinkedList` of the threads waiting on it |
| `KLinkedList` | `0xC` | `+0x0` node count, `+0x4` first node, `+0x8` last node |
| `KLinkedListNode` | `0xC` | `+0x0` next, `+0x4` previous, `+0x8` data pointer ("key") |
| `KObjectMutex` | `0x8` | `+0x0` owning `KThread*`; `+0x4` s16 "try-acquire" counter (incremented at the start of each try-acquire loop and decremented again; an error is detected if it is above 0); `+0x6` s16 context-switch counter (incremented before each context switch while trying to acquire the mutex, then decremented). Provides thread-safety for `KProcessHwInfo`, `KProcessHandleTable`, the interrupt table and similar |
| `KRecursiveLock` | `0x8` | `+0x0` owning `KThread*`, `+0x4` s32 lock count. Methods `Lock`, `Unlock` and `LocalUnlock`; `LocalUnlock` never raises the rescheduling software-generated interrupt #8 (the scheduler's cross-core signal). Used for critical sections |
| `KObjectLink` | `0x10` | `+0x0` next available object, `+0x4` first object, `+0x8` kernel-heap allocation size for this class, `+0xC` object size. Tracks and allocates objects of one class |
| `KObjectList` | `0x28` | `+0x0` count, `+0x4` first and `+0x8` last `KLinkedListNode*`, `+0xC` `KObjectMutex`, `+0x14` u16 total objects ever created, `+0x16` alignment, `+0x18` `KObjectLink` |
| `KObjectName` | `0x10` | `+0x0` char[12] name, `+0xC` `KClientPort*` of the parent `KPort`. The list searched by `ConnectToPort`; no match gives `0xD88007FA` |

### Memory objects

| Structure | Size | Layout |
| --------- | ---- | ------ |
| `KBlockInfo` | `0x8` | `+0x0` start of a memory section in FCRAM (kernel virtual address), `+0x4` page count |
| `KMemoryBlock` | `0x14` | `+0x0` base address, `+0x4` size in pages, `+0x8` permissions (rwx), `+0xC` state flags (lowest byte is the memory type, the second byte holds properties), `+0x10` zero |
| `KThreadLocalPage` | `0x18` | `+0x0` page used for thread-local regions, `+0x4` owner `KProcess*`, `+0x8` number of threads, `+0xC` char[8] usage map, `+0x14` zero |

Details that do not fit a table:

- **`KBlockInfo`.** The `KBlockInfo` objects start with the `fs` process's `KProcess` on the kernel heap and continue in order. Each process gets three or more: `.text` pointer and page count, `.rodata`, then `.data`; an application that does not fit in one free area gets several per section. Destroying them updates a **page reference count** table: the first `0x20000` bytes of base memory are `0x8000` words, one word per FCRAM page, in order. Counts are decremented as blocks are freed and pages reaching 0 are released [reverse-engineered: 3dbrew KBlockInfo]. (`0x8000` pages of `0x1000` bytes is 128 MB, the Old 3DS FCRAM size; whether the New 3DS table is larger is not stated.)
- **`KMemoryBlock` state byte.** Luma3DS names the types (`MemState`): 0 free, 1 reserved, 2 I/O, 3 static, 4 code, 5 private, 6 shared, 7 continuous, 8 aliased, 9 alias, 0xA alias code, 0xB locked. This is the same `MemoryState` that `QueryMemory` reports [documented: Luma3DS `kernel.h`]. Luma uses state `0xBB05` for private and `0x5806` for shared mappings and `0x3907` for linear allocations.
- **`KThreadLocalPage`.** The usage map has one char per thread-local region: `0x200` bytes per region times 8 regions is one `0x1000` page. A char of **1 means unused** and **0 means in use** (the reverse of what one might guess). A process with more than 8 threads has several `KThreadLocalPage` objects chained with `KLinkedListNode`s.

### Process structures

`KProcess` is a `KSynchronizationObject` (waiting on a process wakes when it exits) followed by an interrupt-event part and the `KProcessHwInfo` block. Its fields in memory order, as declared by Luma3DS [documented: Luma3DS `kernel.h`]:

| Order | Field | Notes |
| ----- | ----- | ----- |
| 1 | `KSynchronizationObject`, `KSendableInterruptEvent` | `KProcessHwInfo` starts at `KProcess+0x1C` |
| 2 | `KProcessHwInfo` | See below; its size depends on the model and kernel |
| 3 | total thread-context size, `KLinkedList` of `KThreadLocalPage`s | |
| 4 | ideal processor, `KDebug*`, `KResourceLimit*`, status | Status: 1 available, 2 closing, 3 exited |
| 5 | affinity mask, thread count, maximum thread count | |
| 6 | `svcAccessControlMask[4]` | The 128-bit allowed-SVC mask copied from the exheader; see [kernel](kernel.md#behavior) |
| 7 | `interruptFlags[4]`, kernel flags, handle-table size, kernel release version | Kernel flags bits `0xF00` hold the memory-region flag (see `GetProcessInfo` type 19 in the [SVC reference](svc-reference.md)) |
| 8 | `KCodeSet*`, process ID, creation time, main thread | |
| 9 | `interruptEnabledFlags[4]`, `KProcessHandleTable` | The handle table holds 40 inline descriptors plus a pointer for larger tables |
| 10 | Luma3DS custom fields: `customFlags`, `onMemoryLayoutChangeEvent` | Appended by Luma3DS; not part of the stock kernel |

Luma3DS keeps three layouts of the process structures and picks one at run time: **N3DS** (New 3DS kernel), **O3DS8x** (Old 3DS kernel from system version 8.0.0-18, minor version 44 or higher) and **O3DSPre8x** (earlier Old 3DS kernels). Only offsets inside `KProcessHwInfo` and the structures that embed it shift between them.

**`KProcessHwInfo`** (starts at `KProcess+0x1C`; `0x4C` bytes on the New 3DS per 3dbrew) [reverse-engineered: 3dbrew KProcessHwInfo]:

| Offset (New 3DS) | Type | Description |
| ------ | ---- | ----------- |
| `0x0` | `KObjectMutex` | Mutex |
| `0x8` | u32[cores] | "TLB entries for this process need invalidating on core i" flags: 4 entries on the New 3DS |
| `0x18` | u32 | `KMemoryBlock` count |
| `0x1C`, `0x20` | `KLinkedListNode*` | First and last node of the `KMemoryBlock` list |
| `0x24`, `0x28` | u32 | Unknown |
| `0x2C` | void* | Translation table base |
| `0x30` | u8 | Context ID |
| `0x31` | bool | A global TLB flush is needed |
| `0x32` | bool | The process is the currently running game/app (set via `KernelSetState` type 9, see the [SVC reference](svc-reference.md#kernelsetstate-types)) |
| `0x34` | u32 | Unknown |
| `0x38` | void* | End of userland memory for the process |
| `0x3C` | void* | Linear memory start for the process (since 8.0.0-18) |
| `0x40` | u32 | Unknown (since 8.0.0-18) |
| `0x44` | u32 | MMU table size |
| `0x48` | u32* | Pointer to the MMU table in kernel FCRAM virtual memory |

**Old 3DS:** the Old 3DS kernel has two ARM11 cores, so the per-core array at `0x8` has 2 entries (8 bytes, not 16) and every later offset is 8 lower; from 8.0.0-18 the structure then has the same fields as above (size `0x44`), before 8.0.0-18 it lacks the two fields at `0x3C` and `0x40` (size `0x3C`). These Old 3DS sizes are derived from Luma3DS's `KProcessHwInfoO3DS8x` and `KProcessHwInfoO3DSPre8x` declarations; 3dbrew only lists the New 3DS layout and says offsets above `0x8` apply to the New 3DS.

> Conflict: 3dbrew's `KProcessHwInfo` page says "offsets above 0x8 are only for N3DS" and gives only the New 3DS size. Luma3DS's `kernel.h` declares an Old 3DS layout with the same field order and a smaller per-core array. This page follows Luma3DS for the Old 3DS layout because it is compiled code that runs on retail Old 3DS kernels, and 3dbrew for field meanings.

**`KCodeSet`** (Luma3DS): the object, then three `{load address, page count, block list}` descriptors for `.text`, `.rodata` and `.data`, then the three page counts, an 8-byte process name, two unknown u16 values and the 64-bit title ID. This matches the `CodeSetInfo` structure that `CreateCodeSet` receives (see [kernel](kernel.md#processes-and-threads)).

### Thread structures

**`KThread`** fields in memory order [documented: Luma3DS `kernel.h`]: `KSynchronizationObject` and three interrupt-event bases (timeable, sendable, schedulable); `schedulingMask`, `shallTerminate` (3dbrew puts the "marked for termination" flag at `KThread+0x36`); `KDebugThread*`; base priority; the object being waited on and the wait result; `KObjectMutex*`; arbitration address; the `KLinkedList` of objects still to wait for (used by `WaitSynchronizationN`); mutex list and owned-mutex list; dynamic priority; core ID; preemption timer; `isAlive`/`isEnded`; affinity mask; owner `KProcess*`; thread ID; `svcRegisterStorage` and `endOfThreadContext` (3dbrew places these at `KThread+0x88` and `+0x8C`, matching this order); ideal processor; user and kernel thread-local-storage pointers; thread-list node; highest priority.

**`KThreadContext`** (`0x1000` bytes, one page per thread in the `0xFF4XX000` region): the kernel addresses it from the end of the page (`page_end - N`). Per 3dbrew [reverse-engineered: 3dbrew KThreadContext]:

| Offset (page end minus) | Type | Description |
| ----------------------- | ---- | ----------- |
| `0xF38` (`0xC8`) | u32[4] | SVC access-control data copied from the owner process |
| `0xF48` (`0xB8`) | u8[4] | Debug info read by the SVC handler; see below |
| `0xF50` (`0xB0`) | u32[10] | Supervisor-mode registers `r4-r11`, `r13`, `r14` (this is where `KThread+0x88` points) |
| `0xF78` | f64[16] | VFP registers as 16 double-precision registers |
| `0xFF8` | u32 | `FPEXC` |
| `0xFFC` | u32 | `FPSCR` |

The four debug bytes: byte 0 bit 0 = reschedule, bit 1 = switch context (if non-zero, and depending on `shallTerminate`, thread-exit debug events are sent after valid SVCs other than `0xFF`; cleared after handling), bit 2 = lock debug features. Byte 1 is the exheader's "allow debug" flag: if set, `{r0-r7, r12, lr_svc}` are pushed (always done for invalid or unauthorised SVCs and for SVC `0xFF`), which `GetThreadContext`/`SetThreadContext` need outside exception handling; if `lr_svc` is set to 1 in this frame the registers are reloaded from it (used by `svcBreak` and SVC `0xFF`). Byte 2 is unknown. Byte 3 is the ID of the SVC being handled (reset to 0 afterwards).

Context switching proceeds in this order: load `FPEXC`; save `LR` in `r1` (the return into the main scheduling function); load `r4-r11`, `SP`, `LR`; branch to `r1`, now running in the new thread's context; disable interrupts; if the core's `KScheduler` says another switch is needed, run the scheduler again and repeat, otherwise exit.

On `NATIVE_FIRM` these pages are in FCRAM; on `TWL_FIRM` they are in AXI WRAM (for `TWL_FIRM` v6704 the first thread's page is at physical `0x1FF93000`, the next at `0x1FF92000`, and so on). The first page, `0xFF401000`, belongs to the first created thread and `0xFF403000` to the second.

**`KScheduler`** (Luma3DS): an interrupt-event base, a switch-attempt counter, flags (`contextSwitchNeeded`, `contextSwitchStartedDuringInterrupt`, `triggerCrossCoreInterrupt`, `postInterruptReschedulingNeeded`), core number, managed-thread count, two 32-bit bitfields of non-empty priority queues (high and low), the scheduler thread, and **64 per-priority thread lists** (one for each priority 0-63). The scheduler is therefore priority-queue based, which fits the SCHED_FIFO description in [kernel](kernel.md#scheduling).

**`KResourceLimit`** (Luma3DS): `KAutoObject`, ten maximum values, ten current values (indexed by the resource types listed in [kernel](kernel.md#resource-limits-and-arbitration)), a `KObjectMutex` and a `KPreemptionTimer`.

### Per-core state

**`KCoreContext`**: `0x9000` bytes (effective size `0x6000`), one per core, the current core's copy aliased at `0xFFFF1000` [reverse-engineered: 3dbrew KCoreContext].

| Offset | Type | Description |
| ------ | ---- | ----------- |
| `0x0` | u32[0x400] | Unmapped |
| `0x1000` | u32[0x1000] | L1 translation table (TTB1); the unused part serves as stack for exception modes other than supervisor |
| `0x5000` | u32[0x400] | Unused |
| `0x6000` | `KThreadContext` | Context of the scheduler's thread |
| `0x7000` | u32[0x400] | Unused |
| `0x8000` | `KCoreObjectContext` | The objects currently in use (below) |

`KCoreObjectContext` (`0x1000` bytes):

| Offset | Type | Description |
| ------ | ---- | ----------- |
| `0x0` | `KThread*` | Current thread on this core |
| `0x4` | `KProcess*` | Current process |
| `0x8` | `KScheduler*` | Current scheduler |
| `0xC` | `KSchedulableInterruptEventLinkedList*` | This core's interrupt-event list |
| `0x10` | `KThread*` | Last thread to take an exception on this core (used when dumping FPU registers) |
| `0x14` | u8[4] | Padding? |
| `0x18` | `KThread` | Scheduler thread instance |
| `0xC8` | `KScheduler` | Scheduler instance |
| `0x2F0` | `KSchedulableInterruptEventLinkedList` | List instance |
| `0x300` | u8[0x874] | Unused? |
| `0xB74` | u32[0x123] | Unknown stack |

Because the current core's context is aliased at `0xFFFF1000`, its pointers appear at `0xFFFF9000` (current thread), `0xFFFF9004` (process), `0xFFFF9008` (scheduler), `0xFFFF900C` (interrupt-event list) and `0xFFFF9010` (last thread with an exception). The ARM9 kernel keeps its current thread, process and scheduler at `0x08000040`, `0x08000044` and `0x08000048`.

### Interrupt handling objects

| Structure | Size | Description |
| --------- | ---- | ----------- |
| `KSchedulableInterruptEventLinkedList` | `0x10` | One per core. When an interrupt handler of a `KSchedulableInterruptEvent` returns anything other than NULL or 1, the event is appended here. A kernel thread of priority 0 (created by the kernel main function) is then scheduled, pops the first event, runs its second virtual function, unschedules itself and switches back. Fields: `+0x0` first event, `+0x4` last event, `+0x8` unused, `+0xC` handling `KThread*` |
| `KInterruptEventMailbox` | `0x14` | Exactly two instances. Mailbox 1 holds `KProcess` pointers (viewed as `KSendableInterruptEvent`) and is served by a thread of priority 0. Mailbox 2 holds `KThread` pointers and DMA-related events and is served by a thread of priority 16. The serving thread runs the first virtual function of every queued event, removing each from the list; when the list is empty it sets the "busy" flag and unschedules itself. Fields: `+0x0` mailbox ID, `+0x4` first event, `+0x8` last event, `+0xC` busy flag (and not empty), `+0xD` padding, `+0x10` handling `KThread*` |

`svcBindInterrupt` attaches a user event or semaphore to one of the global interrupt table entries (see the [SVC reference](svc-reference.md#interrupts-and-dma)). Luma3DS's declarations add `KBaseInterruptEvent`, `KFIQInterruptEvent`, `KTLBOperationsInterruptEvent` and a cache-maintenance interrupt event with 2 per-core asynchronous parts on the Old 3DS and 4 on the New 3DS, and an interrupt manager with 2 (Old 3DS) or 4 (New 3DS) arrays of 32 private interrupts plus 96 public events [documented: Luma3DS `kernel.h`].

### Debug objects

**`KDebugThread`** (`0x18` bytes, one per debugged thread) [reverse-engineered: 3dbrew KDebugThread]:

| Offset | Type | Description |
| ------ | ---- | ----------- |
| `0x0` | `KThread*` | The linked thread |
| `0x4` | u8 | `svcBreak` was used with a type other than LOAD_RO/UNLOAD_RO |
| `0x5` | u8 | The thread has been locked by `KDebug` code |
| `0x6` | u8 | The thread shall be locked (set by `KDebug` code) |
| `0x7` | u8 | SCHEDULE IN/OUT events: 0 = in, 1 = out |
| `0x8` | u8 | Thread is attached |
| `0x9` | u8 | SYSCALL IN/OUT events: 0 = in, 1 = out |
| `0xA` | u8[2] | Padding |
| `0xC` | u32 | Thread exit reason |
| `0x10` | u32 | ID of the creator thread |
| `0x14` | u32 | Fault Address Register, when applicable |

**`KEventInfo`** (`0x38` bytes) is the kernel's queued debug event; `svcContinueDebugEvent` converts it into the user-visible `DebugEventInfo` (`0x28` bytes, in the [SVC reference](svc-reference.md#debug-events)) [reverse-engineered: 3dbrew KEventInfo].

| Offset | Type | Description |
| ------ | ---- | ----------- |
| `0x0` | u32 | Event type |
| `0x4` | u32 | Current `KThread` ID |
| `0x8` | u32 | Flags. In all observed cases bit 0 means `svcContinueDebugEvent` must be called for this event (EXIT PROCESS events need it even with the bit clear) |
| `0xC` | u8 | ATTACH PROCESS/THREAD: 1 if attached by `svcDebugActiveProcess` (always 1 for processes), else 0 |
| `0xD` | u8 | Equal to bit 0 of the field at `0x8` in all observed cases |
| `0xE` | bool | Ignore continue |
| `0xF` | u8 | The event was handled and should be deleted |
| `0x10` | union | Event-specific data (slightly different from `DebugEventInfo`) |

Event-specific data, `KEventInfo` layout (the user-visible layout and the event/exception ID tables are in the [SVC reference](svc-reference.md#debug-events)):

| Event | Fields |
| ----- | ------ |
| ATTACH PROCESS | `KProcess*` process |
| ATTACH THREAD | u32 creator thread ID (0 if attached by `svcDebugActiveProcess`); void* thread-local storage; u32* entry point (the parent process's `.text` load address) |
| EXIT THREAD / PROCESS | u32 exit reason |
| EXCEPTION | u32 exception type; u32 exception address; u32 category (4 = DEBUGGER BREAK, 3 = USER BREAK, 2 = STOP POINT, 1 = DATA ABORT / UNALIGNED DATA ACCESS / UNDEFINED SYSCALL, 0 otherwise); type-specific union |
| ... UNDEFINED INSTRUCTION / PREFETCH ABORT / DATA ABORT / UNALIGNED DATA ACCESS / UNDEFINED SYSCALL / STOP POINT | u32 fault information (Fault Address Register for watchpoints, data aborts and unaligned access; attempted SVC ID for UNDEFINED SYSCALL; otherwise 0); u32 stop-point type when applicable (0 = `svc 0xFF`, 1 = breakpoint, 2 = watchpoint) |
| ... USER BREAK | u32 break reason; u32[2] user-provided parameters, or 0 |
| ... DEBUGGER BREAK | s32[4] IDs of the attached process's threads running on each core when `svcBreakDebugProcess` was called, or -1 (only the first 2 are meaningful on the Old 3DS) |
| SCHEDULE IN/OUT | u64 clock tick; u32 CPU ID; u32[5] unknown; u32 event info (apparently 0) |
| SYSCALL IN/OUT | u64 clock tick; u32 syscall ID |
| OUTPUT STRING | u32 string address; u32 string size |
| MAP | u32 mapped address; u32 mapped size; u32 `MemoryPermission`; u32 `MemoryState` |

**`KDebug`** (Luma3DS) is the object behind a debug handle: it is a `KSynchronizationObject` (waiting on it wakes on a debug event), holds two `KLinkedList`s of queued events, the owner `KProcess*`, the debugged thread, lists of debugged threads and process threads, and a set of state flags (event signalled, paused, process ended, pending `svcBreak`, and so on) [documented: Luma3DS `kernel.h`].

## Homebrew notes

Luma3DS's `svcControlProcess` and `svcTranslateHandle` expose parts of this world to userland ([kernel](kernel.md#homebrew-notes)): `TranslateHandle` returns the kernel address of the object a handle refers to plus its class name, and `ControlProcess` can list a process's handles filtered by class token. Kernel exploits and `Backdoor` payloads locate the current thread and process through the fixed `0xFFFF9000` pointers described above.

## Open questions

- Several fields are unnamed in both sources (`KProcessHwInfo` `0x24`, `0x28`, `0x34`, `0x40`; `KCoreObjectContext` padding and stacks).
- Offsets of `KProcess` and `KThread` fields are not tabulated by either source beyond the few anchors quoted here; they vary with model and firmware (Luma3DS selects offsets at run time).
- Whether the `KBlockInfo` page reference-count table grows on the New 3DS is not stated.

## Sources

- [3dbrew: Kernel](https://www.3dbrew.org/wiki/Kernel) and the kernel object pages [KBlockInfo](https://www.3dbrew.org/wiki/KBlockInfo), [KClassToken](https://www.3dbrew.org/wiki/KClassToken), [KCoreContext](https://www.3dbrew.org/wiki/KCoreContext), [KDebugThread](https://www.3dbrew.org/wiki/KDebugThread), [KEventInfo](https://www.3dbrew.org/wiki/KEventInfo), [KInterruptEventMailbox](https://www.3dbrew.org/wiki/KInterruptEventMailbox), [KLinkedList](https://www.3dbrew.org/wiki/KLinkedList), [KLinkedListNode](https://www.3dbrew.org/wiki/KLinkedListNode), [KMemoryBlock](https://www.3dbrew.org/wiki/KMemoryBlock), [KObjectLink](https://www.3dbrew.org/wiki/KObjectLink), [KObjectList](https://www.3dbrew.org/wiki/KObjectList), [KObjectMutex](https://www.3dbrew.org/wiki/KObjectMutex), [KObjectName](https://www.3dbrew.org/wiki/KObjectName), [KProcessHwInfo](https://www.3dbrew.org/wiki/KProcessHwInfo), [KRecursiveLock](https://www.3dbrew.org/wiki/KRecursiveLock), [KSchedulableInterruptEventLinkedList](https://www.3dbrew.org/wiki/KSchedulableInterruptEventLinkedList), [KThreadContext](https://www.3dbrew.org/wiki/KThreadContext), [KThreadLocalPage](https://www.3dbrew.org/wiki/KThreadLocalPage) — all field tables unless a row cites Luma3DS.
- [3dbrew: Memory layout](https://www.3dbrew.org/wiki/Memory_layout) — thread-context pages, per-core pointers.
- [Luma3DS `k11_extension/include/kernel.h`](https://github.com/LumaTeam/Luma3DS) — class declarations, process-structure variants, memory states, scheduler and resource-limit layouts.
