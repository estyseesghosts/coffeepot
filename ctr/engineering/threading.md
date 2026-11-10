# Threads and multiple cores on the 3DS

A 3DS program starts with one thread on the application core, and most homebrew never needs more. Extra threads help when work can run *beside* the main loop — audio decoding, asset loading, geometry preparation — but the 3DS's cores are not equal: on an [Old 3DS](../glossary.md#old-new-3ds) the second core (the **[syscore](../glossary.md#appcore-syscore)**) is shared with the operating system and only lends your program a time slice, and on a New 3DS a third core is fully usable while the fourth is not. This page explains which cores you can use and how, how the scheduler treats priorities, which synchronisation primitives libctru offers, and a worker-thread pattern that avoids the usual stalls and shutdown hangs.

Background: [kernel scheduling](../software/kernel.md#scheduling), [CPU](../hardware/cpu.md). Measuring the effect: [performance](performance.md).

## Before you start

- Checked against libctru 2.7.1 and Luma3DS v13.5 source. Code fragments use real API names; they have not been run on hardware for this page.
- The scheduling details marked `[reverse-engineered]` come from comments by TuxSH in Luma3DS's process-manager reimplementation, which their author describes as untested.

## Which cores you can use

| Core | Old 3DS | New 3DS | Notes |
| ---- | ------- | ------- | ----- |
| 0 (application core) | Yours | Yours | Always available; the main thread runs here |
| 1 (system core) | One thread, after `APT_SetAppCpuTimeLimit` | Same | Time-sliced with system modules |
| 2 | — | Yours, if the exheader allows it (`.3dsx` under Luma3DS: yes) | Not time-sliced with system modules [reverse-engineered] |
| 3 | — | Not for applications | |

[documented: libctru `thread.h`; Luma3DS `hbldr.c`]

`threadCreate(entry, arg, stack_size, prio, core_id, detached)` takes `core_id` 0–1 (Old 3DS) or 0–3 (New 3DS), `-1` for "any core in the process's affinity" or `-2` for the exheader's default; it returns `NULL` on failure — always check. Detect the model at run time with `APT_CheckNew3DS(&isNew)`.

### Core 1: borrowing the system core

1. Call `APT_SetAppCpuTimeLimit(percent)` once, early, before creating the thread; read back the granted value with `APT_GetAppCpuTimeLimit(&p)`.
2. Create **one** thread with `core_id` 1. libctru documents that, with a time limit set, "it is possible to create a single thread on this core".
3. Without step 1, the kernel refuses application threads on core 1 (since system version 2.1) [reverse-engineered: Luma3DS `pm/source/reslimit.c`].

For a `.3dsx` under Luma3DS the maximum is 89% [documented: `hbldr.c`]. How the share is applied [reverse-engineered]: in the "single" scheduling mode used for applications, core 1's time is cut into 25 ms slices; your thread gets *percent*% of each slice, system modules share (90 − *percent*)%, and 10% is left for other threads. Practical consequences:

- Work handed to a core-1 thread can wait up to a slice before it starts. Use core 1 for throughput (decoding ahead, building next frame's data with a frame of slack), not for anything that must finish within a few milliseconds.
- A large share slows the system modules your program talks to. A third-party measurement on an Old 3DS saw `DSP_FlushDataCache` rise from ~0.1 ms to 1–5 ms at 80% [measured by a third party: libctru issue #556]. Prefer the kernel's `svcFlushProcessDataCache` from code that runs while a share is active ([memory and cache](memory-and-cache.md#the-functions)), and tune the percentage by measurement.

Two well-known ports illustrate a common placement: the Super Mario 64 3DS port runs its audio thread on core 1 on Old 3DS and core 2 on New 3DS [documented: its README]. That is a design choice for that program, not a rule; measure your own.

### Core 2 on New 3DS

A `.3dsx` may create threads on core 2 because Luma3DS's generated exheader sets the "access core 2" flag (`0x2000`) [documented: `hbldr.c`, `thread.h`]. Use it for your main worker on New 3DS, with a core-1 or core-0 fallback on Old 3DS.

## Priorities and scheduling

- Userland priorities run from `0x18` to `0x3F`; **lower numbers mean higher priority**. The main thread usually has `0x30`; read it with `svcGetThreadPriority(&p, CUR_THREAD_HANDLE)` and set workers relative to it [documented: `thread.h`].
- On each core the kernel runs the highest-priority ready thread. A thread keeps the core until it blocks, sleeps or makes a call that reschedules; a higher-priority thread becoming ready can preempt it ([kernel](../software/kernel.md#scheduling)). Threads of equal priority are not time-sliced against each other.
- Consequences: a high-priority thread that never blocks starves everything below it on its core, including the main loop; and a worker at the same priority as the main thread on core 0 only runs when the main thread blocks (for example while `C3D_FrameBegin` waits for the GPU).
- Never wait by spinning or by `svcSleepThread(0)` in a loop; block on a synchronisation object.

## Synchronisation primitives

libctru provides lightweight user-mode primitives built on the kernel's address arbiter (a futex-like wait on a memory word), plus the kernel objects themselves [documented: `synchronization.h`]:

| Primitive | Use |
| --------- | --- |
| `LightLock` | Mutex for short critical sections (`LightLock_Init/Lock/TryLock/Unlock`) |
| `RecursiveLock` | Mutex the same thread may take repeatedly |
| `CondVar` | Wait for a condition while holding a `LightLock` (`CondVar_Wait`, `CondVar_WaitTimeout`, `CondVar_Signal`, `CondVar_Broadcast`) |
| `LightEvent` | Signal another thread to wake (`RESET_ONESHOT` or `RESET_STICKY`; `LightEvent_Wait`, `WaitTimeout`, `Signal`, `Clear`, `Pulse`) |
| `LightSemaphore` | Counted resource (`Acquire`, `TryAcquire`, `Release`) |
| `__ldrex`/`__strex` (and `h`/`b` variants), C11 `<stdatomic.h>` | Lock-free counters and flags |
| `__dmb()`, `__dsb()`, `__isb()` | Memory barriers (CP15 operations on ARMv6K) |
| Kernel events, mutexes, semaphores, timers (`svcCreateEvent`, …) | Waiting on several objects at once (`svcWaitSynchronizationN`), or waiting together with service handles |

The libctru primitives do not implement priority inheritance ([kernel](../software/kernel.md#synchronization)): keep critical sections short when threads of different priorities share a lock. Standard `pthread`, C11 threads and `std::thread` are supported experimentally since libctru 2.3.0.

## A worker-thread pattern

A job queue with one worker, woken by an event, with a bounded shutdown:

```c
#include <3ds.h>

#define QUEUE_LEN 16
typedef struct { void (*fn)(void *); void *arg; } Job;

static Job queue[QUEUE_LEN];
static int q_head, q_tail;            // protected by q_lock
static LightLock q_lock;
static LightEvent q_wake;             // RESET_ONESHOT
static volatile bool q_quit;
static Thread worker;

static void worker_main(void *unused)
{
    while (true) {
        LightEvent_Wait(&q_wake);
        for (;;) {
            LightLock_Lock(&q_lock);
            if (q_head == q_tail) { LightLock_Unlock(&q_lock); break; }
            Job job = queue[q_head];
            q_head = (q_head + 1) % QUEUE_LEN;
            LightLock_Unlock(&q_lock);
            job.fn(job.arg);                       // run outside the lock
        }
        if (q_quit) break;
    }
}

bool jobs_start(int core, int prio)
{
    LightLock_Init(&q_lock);
    LightEvent_Init(&q_wake, RESET_ONESHOT);
    worker = threadCreate(worker_main, NULL, 32 * 1024, prio, core, false);
    return worker != NULL;                         // caller falls back to another core on failure
}

bool jobs_push(void (*fn)(void *), void *arg)
{
    LightLock_Lock(&q_lock);
    int next = (q_tail + 1) % QUEUE_LEN;
    bool ok = next != q_head;                      // false when full: caller decides what to do
    if (ok) { queue[q_tail] = (Job){ fn, arg }; q_tail = next; }
    LightLock_Unlock(&q_lock);
    if (ok) LightEvent_Signal(&q_wake);
    return ok;
}

void jobs_stop(void)
{
    q_quit = true;
    LightEvent_Signal(&q_wake);
    threadJoin(worker, U64_MAX);
    threadFree(worker);
}
```

Design notes:

- The worker blocks in `LightEvent_Wait` when there is nothing to do, so it costs no CPU time and does not hold back system modules on core 1.
- Jobs report completion through their own mechanism (a flag the main loop checks, or a second event); never let the main loop wait on a job it needs *this* frame if the worker is on core 1.
- Keep thread stacks modest: libctru allocates each thread's stack, together with its control block and thread-local storage, from the application heap with `memalign` [documented: `source/thread.c`].

## Thread lifetime

[documented: libctru `source/thread.c`]

- A thread ends by returning from its function or calling `threadExit(rc)` — never `svcExitThread` directly. libctru marks the thread *finished* just before the kernel exit call.
- `threadJoin(t, timeout_ns)` returns at once if the thread is already marked finished; otherwise it waits on the kernel thread handle.
- `threadFree(t)` releases the handle and the memory, including the stack, and does nothing if the thread has not finished. Never free a thread whose join timed out.
- A detached thread (`detached = true`, or `threadDetach`) frees itself; do not join or free it.
- Before tearing down anything a worker uses (GPU buffers, audio, files), stop it, wait for it, and only then release the resources.
- Only one thread should run `aptMainLoop()` and react to HOME, sleep and exit requests.

## Technical details

- `threadCreate` rounds the stack size up to 8 bytes (more if the TLS alignment requires) and copies initialised TLS data into the new thread's block; standard input/output streams are inherited.
- Core-1 CPU-time limit (kernel resource limit type 9): 0 forbids application threads on core 1; 1–89 is a percentage; values of 10000 and above exempt applets; the kernel reschedules when the value is set below 90 ([kernel](../software/kernel.md#resource-limits-and-arbitration)).
- `LightLock` and the other light primitives use `syncArbitrateAddress` → `svcArbitrateAddress` on a process-wide arbiter created by libctru.

## Pitfalls

- **Creating the core-1 thread before `APT_SetAppCpuTimeLimit`**: `threadCreate` returns `NULL`.
- **Two application threads on core 1**: the second creation fails.
- **Assuming core 2 exists**: always check the model and the return value.
- **A worker at higher priority than the main thread that polls**: the main loop stalls.
- **Sharing a cache line between threads on different cores** that both write it: slow, because the line moves between the cores' caches.
- **Freeing a thread after a timed-out join**: use-after-free of its stack.

## Open questions

- Measured scheduling delay for a core-1 thread at several time-limit percentages (a thread recording gaps in `svcGetSystemTick`).
- Whether threads on core 2 see any interference from system activity.

## Sources

- [libctru](https://github.com/devkitPro/libctru) — `thread.h`, `source/thread.c`, `synchronization.h`, `source/synchronization.c`, `services/apt.h`.
- [Luma3DS](https://github.com/LumaTeam/Luma3DS) — `sysmodules/loader/source/hbldr.c` (3DSX core access and CPU-time descriptor), `sysmodules/pm/source/reslimit.c` (scheduling and CPU-time notes).
- [libctru issue #556](https://github.com/devkitPro/libctru/issues/556) — DSP flush latency under a core-1 share.
- [sm64-port (3DS branch) README](https://github.com/thearst3rd/sm64-port/blob/3ds-port/README.md) — core placement of the audio thread in a real port.
