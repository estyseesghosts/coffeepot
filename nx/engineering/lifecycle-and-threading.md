# Horizon lifecycle, scheduling and threads

This page is for a developer whose program must survive being interrupted (Home menu, sleep, docking, exit requests) and whose work is spread over threads. The short answer: treat the program as a state machine rather than a loop that always runs, decide in advance what to pause, reset and rebuild on each transition, keep exit cleanup bounded, and choose thread priorities and cores deliberately, because the kernel will not do it for you.

## Before you start

- Toolchain: `libnx` 4.12.0-1 ([source versions](source-versions.md)). API facts are `[documented]` from `libnx` at `b156b4774fb2df7b93402169509266da7547fc4c`: `nx/include/switch/services/applet.h`, `kernel/thread.h`.
- Verification: the hook and lock fragments compile in `nx/engineering/examples/api-check`. The first-game and renderer examples use focus state, and the first game's self-test ran in Eden, which never sends Home, sleep or dock events. **Nothing on this page has been tested on a console**, and there is no scheduler experiment yet.
- Read [Horizon OS](../software/os.md) for what the OS is and [services and IPC](../software/services-and-ipc.md) for the applet service.

## Lifecycle as states

`Starting → Running → (Unfocused or Suspended) → Resuming → Running → Exiting`

A main loop cannot assume that two iterations are consecutive moments of play. By default the system suspends the whole process when Home opens or the console sleeps, so the main thread may not run between two lines of code. The focus-handling mode changes that (`appletSetFocusHandlingMode`) `[documented]` (`applet.h`):

| Mode | Behaviour |
| ---- | --------- |
| `SuspendHomeSleep` (default) | Suspend only for the Home menu and sleep |
| `NoSuspend` | Never suspend when out of focus; the program must pause itself |
| `SuspendHomeSleepNotify` | As the default, but still receive the `OnFocusState` hook |
| `AlwaysSuspend` | Suspend whenever out of focus, whatever the reason |

Hooks registered with `appletHook` are called from `appletMainLoop` for operation mode (handheld/docked), performance mode, focus state, exit request and resume. Keep hook bodies small: set a flag and act in the main loop.

## What to do on a transition

| Transition | Always | Depends on your engine |
| ---------- | ------ | ---------------------- |
| Lose focus / suspend | Stop advancing the game; stop submitting graphics | Pause or stop audio; avoid starting long blocking work |
| Resume / regain focus | Reset the frame clock (do not count the gap as one frame); refresh controller state | Re-check services and files that may have changed; restart audio |
| Handheld ↔ docked | Re-read the operation mode | Recreate size-dependent render targets; change resolution or budgets |
| Exit requested | `appletMainLoop` returns false; leave the loop | Save, stop audio, destroy graphics, within the deadline |

A variable-timestep game that uses the elapsed time across a long suspension advances everything at once; a fixed-step game that replays every missed step enters a catch-up loop. Pick an explicit policy: the first-game program clamps the step to 0.1 s and resets its clock on focus changes, and the cap-the-catch-up fragment is on [performance and porting](performance-and-porting.md#threads-and-cores).

### Exit locks

`appletLockExit` delays exit so the program can clean up after `appletMainLoop` returns false. Once exit is requested there is a **15 second timeout**, after which the process is forcibly terminated if it is still locked, and `appletUnlockExit` **must be called before `main` returns** `[documented]` (`applet.h`). So: lock, run bounded cleanup (a save, closing services), unlock. An exit lock is not a way to finish arbitrary work. `api-check` shows the sequence.

## Threads

`threadCreate(thread, entry, arg, stack, stack_size, prio, cpuid)` then `threadStart` (a thread is created, then started; the SVCs are `0x08` and `0x09`, see [services and IPC](../software/services-and-ipc.md)).

- Priority is `0x00` to `0x3F`, smaller meaning higher priority. `0x2C` is the usual main-thread priority. `0x3B` is a special priority on cores 0 to 2 that enables preemptive multithreading (`0x3F` on core 3) `[documented]` (`thread.h`).
- The core ID selects where the thread is created (0 to 3, or -2 for the process default). It is a placement, not exclusive use of that core.
- Ports report that a game gets three cores (0 to 2) and that threads at other priorities are not time-sliced against each other, so a busy higher-priority thread starves lower ones on its core, with a 10 ms slice at `0x3B` in SA2's notes `[reported]` (`sa2-nx` `docs/porting-guide.md`, `nfsmw-nx` `docs/platform-notes.md`). The header documents the special case; the "only" and the slice length are the ports' observations, and the full scheduler behaviour depends on firmware. Test before relying on it.
- Core layouts from ports are examples: SA2 uses core 0 for the game thread and core 1 for rendering; melonDS puts audio on core 0 and emulation on cores 1 to 2 `[reported]`/`[documented]` per [performance and porting](performance-and-porting.md#threads-and-cores).

### A three-thread workload

Main (input, simulation), a render worker (command or geometry preparation) and an audio worker (bounded blocks). Start with everything on one thread, measure, then split one task at a time, measuring again. More threads can slow a program: queues, cache traffic, scheduling and uneven work all cost. Use queues with clear ownership, events or condition variables for waking, and a shutdown path that joins every thread (`threadWaitForExit`, `threadClose`). A high-priority thread that never blocks starves the rest.

## Scheduler experiment (to write)

Record per run: priority, core, current core where observable, productive work done, wake-up latency from an event, and behaviour with a competing busy thread. Cover several priorities including `0x3B`, and compare across firmware and models. Do not generalise from one configuration.

## Pitfalls

- **Freeze on Home or resume.** A lock or wait held across the transition, or timers not reset.
- **Forgetting `appletUnlockExit`.** The process is terminated after 15 seconds.
- **Assuming a thread keeps running.** In the default mode it does not.
- **Blocking I/O on the game thread.** USF4 froze for seconds when a log send blocked while holding a mutex `[reported]`; use non-blocking sockets or a worker.
- **A busy high-priority thread.** Starves system-like background work on its core.

## Open questions

- What exactly happens to threads, timers and audio across suspend on each focus mode? Needs the runnable lifecycle test below.
- What is the time-slice behaviour at each priority, per firmware and model? Needs the scheduler experiment.
- How should a game behave when a controller disconnects mid-play? `switch-examples` `hid/` has the pieces; not worked through here.

A lifecycle test should respond to Home, sleep/resume, dock changes, controller loss and exit, show which state is rebuilt and which timers reset, and log every transition with a timestamp.

## Sources

- [libnx](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/include/switch/services/applet.h) at `b156b4774fb2df7b93402169509266da7547fc4c` — `services/applet.h` (focus modes, hooks, `appletLockExit`), `kernel/thread.h` (priorities, core selection).
- SwitchBrew [SVC](https://switchbrew.org/wiki/SVC) (oldid 15416) — `CreateThread`, `StartThread`, `SetThreadPriority`, `SetThreadCoreMask`.
- [sa2-nx](https://github.com/ChanseyIsTheBest/sa2-nx) at `baf523b`, [nfsmw-nx](https://github.com/StevensND/nfsmw-nx) at `20f8dc1` — scheduling observations `[reported]`; [usf4-nx](https://github.com/Thorhax/usf4-nx) at `30ffae0` — blocking-log freeze `[reported]`.
- `nx/engineering/examples/api-check`, `nx/engineering/examples/first-game` — the code fragments.
