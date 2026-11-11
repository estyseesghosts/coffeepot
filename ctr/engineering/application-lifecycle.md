# Application lifecycle: HOME, sleep, resume and exit

A 3DS program does not own the console. The user can press HOME, close the lid, press POWER or open another applet such as the software keyboard at any moment, and the system expects your program to cooperate. libctru turns these events into one call, `aptMainLoop()`, plus optional callbacks ("hooks"). This page explains what happens on each transition, what your program must do about graphics, audio, threads, files and network, and gives a complete example that logs every event.

The short answer: call `aptMainLoop()` once per frame from one thread, treat it as a call that may block for a long time, pause your other threads from hooks, and do not use files or services inside the exit hook.

## The whole thing in 10 lines

1. Call `aptMainLoop()` once per frame, from one thread only. It is where HOME, sleep and "close" are acted on.
2. It may block for minutes (HOME menu, lid closed). Your other threads keep running while it blocks.
3. Register `aptHook()` callbacks to learn about `ONSUSPEND`/`ONRESTORE` (HOME, applets) and `ONSLEEP`/`ONWAKEUP` (lid).
4. In those hooks only flip flags and park workers; do not touch the GPU, files or services.
5. After `ONRESTORE`, redraw everything; do not assume the screen or your GPU state survived.
6. When `aptMainLoop()` returns `false`, stop, save, and return from `main`.
7. Save **before** leaving the loop: `ONEXIT` runs after the file service is closed.
8. `ONEXIT` only fires when the system ordered the close, not when you quit on START.
9. Use `aptSetHomeAllowed(false)` only around short critical sections, and set it back.
10. Nothing on this page has been run on a console; see [Verification status](#verification-status).

## Before you start

- Checked against libctru 2.7.1 at the commit pinned on [source versions](source-versions.md) (`62b7c42c88b1ee837f513358409d2e5c8f9001a9`), devkitARM r68. Source links below are at that commit.
- Applies to Old and New 3DS. Nothing here was run on hardware or an emulator; see [Verification status](#verification-status).
- The service that tracks all this is the **Applet manager** (**[APT](../glossary.md#apt)**), a system [service](../software/services.md) that tells each running program whether it is in front, suspended or ordered to close. The library layer is [libctru](../homebrew/libctru.md); the graphics service is [GSP](../glossary.md#gsp).

## The model in one picture

At any time your program is either *active* (it owns the screens) or *not active* (the HOME menu or an applet owns them). The system moves it between the two with commands; libctru receives them on a private helper thread and records them as flags. Your program only acts on them when it calls `aptMainLoop()`.

| What happens | What `aptMainLoop()` does | Hooks fired (in order) | Returns |
| ------------ | ------------------------ | ---------------------- | ------- |
| Nothing | Returns immediately | none | `true` |
| User presses HOME (and HOME is allowed, see [Choosing what the user may do](#choosing-what-the-user-may-do)) | Jumps to the HOME menu and blocks until you are brought back | `ONSUSPEND`, then `ONRESTORE` after return | `true` (or `false` if the system cancelled you meanwhile) |
| POWER button pressed | Same jump to the HOME menu | as above | as above |
| Lid closed / sleep (if `aptSetSleepAllowed(true)`, the default) | Blocks until the console wakes | `ONSLEEP`, then `ONWAKEUP` | `true` |
| System orders the program to close (for example "close software" in HOME, or shutdown) | Nothing blocks; `aptShouldClose()` becomes true | none yet | `false` |
| You launch a library applet such as the software keyboard | Blocks inside your call (`swkbdInputText`) until the applet ends | `ONSUSPEND`, then `ONRESTORE` | n/a |
| Program ends (`main` returns) after the system ordered close | `aptExit()` from `__appExit` | `ONEXIT` | n/a |

Sources for the table: `aptMainLoop`, `aptHandleSleep`, `aptJumpToHomeMenu`, `aptLaunchLibraryApplet`, `aptExit` and the notification switch in `aptEventHandler` [documented: `libctru/source/services/apt.c`; `libctru/include/3ds/services/apt.h`].

## The main loop

```c
while (aptMainLoop()) {
    // read input, update, draw, present
}
```

Rules that follow from the source:

- Call it from **one thread**, at the start of every frame. The hooks and the blocking happen on whichever thread calls it [documented: `apt.c`, `aptMainLoop` is just `aptHandleSleep(); aptHandleJumpToHome(); return !aptShouldClose();`].
- While it is blocked (HOME menu showing, console asleep) **your main thread does not run, but every other thread you created does**. Background threads must stop themselves; see [Pausing a worker](#pausing-a-worker-thread).
- A `false` return means "leave now", either because the system ordered a close or because a cancel arrived while you were suspended (`FLAG_ORDERTOCLOSE`/`FLAG_CANCELLED` in `aptShouldClose`) [documented: `apt.c`]. Finish your loop, release resources in reverse order and return from `main`.
- Calling `aptMainLoop()` less often than the event rate does not lose events, but the HOME menu will not appear until you call it. A program that stops calling it for seconds looks frozen when HOME is pressed. Frame-loop structure: [frame pacing](frame-pacing.md).

## Choosing what the user may do

| Call | Effect |
| ---- | ------ |
| `aptSetHomeAllowed(true)` | Allows HOME presses. Call it explicitly if you rely on HOME working: `aptInit` starts with only the sleep flag set and adds the HOME flag only when `osGetSystemCoreVersion() == 2`, although the comment above that line says HOME is allowed by default except in safe mode [documented: `apt.c` `aptInit`; the code and the comment can be read as disagreeing, which this page has not resolved] |
| `aptSetHomeAllowed(false)` | HOME presses are remembered as rejected (`aptCheckHomePressRejected()` returns true) and no jump happens; use it to show a "HOME is disabled" icon [documented: `apt.h`, `apt.c` APTSIGNAL_HOMEBUTTON case] |
| `aptSetSleepAllowed(false)` | The sleep query is answered "reject", so closing the lid does not sleep the console while you are active [documented: `apt.c` APTSIGNAL_SLEEP_QUERY case] |

Disabling HOME is hostile to users except during short critical sections (for example while committing a save). Re-enable it afterwards. When your program is *not* active, sleep is accepted regardless of this setting [documented: `apt.c`].

## What each transition means for your program

### HOME menu (suspend and restore)

Order of events inside `aptJumpToHomeMenu` [documented: `apt.c`]:

1. `ONSUSPEND` hook runs (on your thread, before anything is released).
2. libctru saves the VRAM system area, captures the screens for the HOME menu's thumbnail, puts the DSP to sleep (`aptDspSleep`), and releases the GPU right (`GSPGPU_ReleaseRight`).
3. `APT_JumpToHomeMenu`, then it waits for a wake-up command.
4. On wake-up it re-acquires the GPU right (`GSPGPU_AcquireRight`), restores the VRAM system area, and runs `ONRESTORE`. **All three steps are skipped** when the wake-up command is plain `APTCMD_WAKEUP` or `APTCMD_WAKEUP_CANCEL`, or when the transition is a cancelled library applet (`TR_CANCELLIB`) [documented: `aptWaitForWakeUp`, one `if` guards all three]. `ONRESTORE` runs *before* libctru calls `aptDspWakeup()`, so inside the hook the DSP is still asleep.

What your code must do:

| Subsystem | During the transition | Your job |
| --------- | --------------------- | -------- |
| Graphics (GSP) | The GPU right is released, so you must not submit GPU work or touch the framebuffers | `ONSUSPEND`: stop submitting. After `ONRESTORE`, render a full new frame rather than assuming the previous presented image is on screen. Whether your own VRAM render targets keep their contents is not established here ([open question](#open-questions)) |
| Audio | libctru's own DSP hook puts the DSP to sleep and resumes it. With ndsp the library saves and restores DSP state for you [documented: `apt.c` weak `aptDspSleep` overridden by `source/ndsp/ndsp.c`; see [audio](audio.md#technical-details)] | Pause your own mixer/producer thread; do not queue buffers while suspended. Audio you generate yourself outside ndsp has no such protection |
| Threads | Only the thread inside `aptMainLoop()` is blocked | Park workers from the hook (below). Never call `aptMainLoop()` from a second thread |
| Files | No source indicates the file system is suspended | Treat suspend as a possible last chance: write your save before HOME is reachable if losing progress matters, because the user can also close the software from the HOME menu while you are suspended and `ONEXIT` is the only later notification. See [filesystem and saves](filesystem-and-saves.md) |
| Network | Not examined in the libctru files read for this page | Assume sockets may be dead after resume and be ready to reconnect `[hypothesis]`; a test is in [open questions](#open-questions) |

### Sleep (lid closed)

Order [documented: `apt.c`, `aptEventHandler` and `aptHandleSleep`]:

1. `APTSIGNAL_SLEEP_QUERY`: libctru answers accept or reject from `aptSetSleepAllowed`, or rejects if the program was ordered to close.
2. `APTSIGNAL_SLEEP_ENTER`: libctru puts the DSP to sleep immediately (so this happens even if you are blocked or slow) and sets a "should sleep" flag.
3. Your next `aptMainLoop()` call runs `ONSLEEP`, tells APT it is done (`APT_ReplySleepNotificationComplete`), then blocks.
4. `APTSIGNAL_SLEEP_WAKEUP` wakes it; the LCD is un-blacked and `ONWAKEUP` runs.

Differences from HOME: the GPU right is **not** released and the VRAM system area is not saved by libctru on this path (these calls appear only in `aptJumpToHomeMenu` and the applet launchers) [documented: `apt.c`]. Do not conclude from that that GPU state is guaranteed intact; it only means libctru does no extra work.

Because the sleep reply is sent right after `ONSLEEP` returns, **keep `ONSLEEP` short**. Anything slow (a large save) delays the system's sleep acknowledgement; behaviour on a very slow hook was not examined (`[hypothesis]`: the system may time out or show a delay).

### Exit

The system sets "order to close" (`APTSIGNAL_ORDERTOCLOSE`, or the shutdown/POWER-hold path). `aptMainLoop()` then returns `false`. Your `main` should tear down and return; libctru's `__appExit` then runs `userAppExit`, **unmounts all archives, closes the FS service**, closes HID, and only then calls `aptExit()` [documented: `libctru/source/system/appExit.c`]. Inside `aptExit`, the `ONEXIT` hook runs if and only if the system ordered a close [documented: `apt.c`].

Two consequences that surprise people:

- **`ONEXIT` runs after `fsExit()` and `archiveUnmountAll()`**. Do not open or flush files in it. Save before you leave the loop.
- If *you* leave the loop (for example on START) there may be no `ONEXIT` at all. A hook is a notification of a system event, not a destructor.

### Software keyboard and other library applets

`swkbdInputText()` calls `aptLaunchLibraryApplet(APPID_SOFTWARE_KEYBOARD, ...)`, which behaves like the HOME jump: `ONSUSPEND`, save VRAM, release the GPU right, run the applet, wait for wake-up, then restore and `ONRESTORE` [documented: `libctru/source/applets/swkbd.c`, `apt.c`]. Your thread is blocked inside the `swkbdInputText` call, not in `aptMainLoop`. Consequences:

- Workers must be parked by the same `ONSUSPEND`/`ONRESTORE` hooks; one mechanism covers HOME, sleep and applets.
- The call returns a button id, or `SWKBD_BUTTON_NONE` for special outcomes; check `swkbdGetResult()` [documented: `swkbd.h`]. If the user pressed HOME inside the keyboard and the system cancelled you, expect `aptMainLoop()` to return `false` soon after.
- Sleep is temporarily disabled while the library applet is being launched (`aptSetSleepAllowed(false)` inside the launcher) and restored afterwards [documented: `apt.c`].

### `.3dsx` versus `.cia`

How the program is started changes how it ends [documented: `apt.c`, `aptExit`]:

| Launch method | Exit path | Result |
| ------------- | --------- | ------ |
| `.3dsx` started by a launcher that sets `RUNFLAG_APTREINIT` in the system run flags (that the Homebrew Menu does so is `[hypothesis]`: not read here) | APT is finalized without closing the process | Control returns to the loader, which can launch another `.3dsx` |
| Installed `.cia` (no run flags set) | `APT_PrepareToCloseApplication`, then the process is closed | Returns to the HOME menu |
| `.3dsx` started by Luma3DS's built-in 3DSX loader | Luma sets `RUNFLAG_APTCHAINLOAD` in the process's `_prm` run flags, so `aptInit` calls `aptSetChainloaderToSelf()`; leaving without a system close then performs an application jump | The launching title (normally the Homebrew Menu) starts again [documented: Luma3DS `sysmodules/loader/source/3dsx.c`; libctru `apt.c` `aptInit`/`aptExit`] |
| Either, after `aptSetChainloader*` | Application jump to another title | The other title starts; if the HOME menu is not running, libctru uses a Luma3DS-provided workaround (`srvPublishToSubscriber(0x3000)`) [documented: `apt.c`] |

If the system ordered the close, `ONEXIT` runs first in all cases. A `.cia` has its own title ID and (optionally) save data; a `.3dsx` has neither, so persistent data goes on the SD card ([filesystem and saves](filesystem-and-saves.md)). Building each format: [development workflow](../homebrew/development-workflow.md); the container formats: [file formats](../software/file-formats.md). Homebrew-launch background: [libctru](../homebrew/libctru.md).

## Pausing a worker thread

Hooks run on the thread that is about to block, so they are the right place to tell workers to stop. The pattern: an atomic "paused" flag, a sticky `LightEvent` the worker waits on, and `ONEXIT` or normal teardown to release it. The complete example below uses exactly this. General worker design and the cost of core placement: [threading](threading.md#a-worker-thread-pattern).

Rules:

- A hook must not call anything that can block on the thing being suspended (no `gfx*` submission, no waiting on the worker to acknowledge from the same thread).
- Pausing is cooperative: a worker in the middle of a long CPU loop does not stop until it checks the flag. Check it at least every few milliseconds.
- Workers that use services (FS, sockets, DSP) should finish or abandon the current request, then park, rather than being parked mid-request `[hypothesis]`: the libctru sources read do not say what the services do to in-flight requests during suspend.

## A complete example

Frame counter on the top screen, a background worker that counts, hooks that print every event, the software keyboard on **A**, HOME toggle on **B**, exit on **START**. It uses no audio so it has no ndsp dependency.

```c
#include <3ds.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

// ---- shared state between the main thread and the worker ----
static atomic_bool g_quit;       // set when the program is leaving
static atomic_bool g_paused;     // set between SUSPEND/SLEEP and RESTORE/WAKEUP
static atomic_uint g_work_done;  // progress counter the worker bumps
static LightEvent  g_resume;     // signalled when the worker may continue

static const char *hook_name(APT_HookType h)
{
	switch (h) {
	case APTHOOK_ONSUSPEND: return "ONSUSPEND";
	case APTHOOK_ONRESTORE: return "ONRESTORE";
	case APTHOOK_ONSLEEP:   return "ONSLEEP";
	case APTHOOK_ONWAKEUP:  return "ONWAKEUP";
	case APTHOOK_ONEXIT:    return "ONEXIT";
	default:                return "?";
	}
}

// Hooks run on the thread that called aptMainLoop() (or aptExit() for ONEXIT).
// Keep them short. ONEXIT runs after fsExit(): no files, no sdmc:.
static void on_apt(APT_HookType hook, void *param)
{
	unsigned *frame = param;
	if (hook == APTHOOK_ONEXIT) {
		atomic_store(&g_quit, true);
		LightEvent_Signal(&g_resume);
		return; // only touch plain memory here
	}
	printf("[%06u] hook %s\n", *frame, hook_name(hook));
	switch (hook) {
	case APTHOOK_ONSUSPEND:
	case APTHOOK_ONSLEEP:
		atomic_store(&g_paused, true);
		LightEvent_Clear(&g_resume);
		break;
	case APTHOOK_ONRESTORE:
	case APTHOOK_ONWAKEUP:
		atomic_store(&g_paused, false);
		LightEvent_Signal(&g_resume);
		break;
	default:
		break;
	}
}

// Background worker: does CPU-only work and parks itself while paused.
static void worker(void *arg)
{
	(void)arg;
	while (!atomic_load(&g_quit)) {
		if (atomic_load(&g_paused)) {
			LightEvent_Wait(&g_resume);
			continue;
		}
		atomic_fetch_add(&g_work_done, 1);
		svcSleepThread(10 * 1000 * 1000LL); // 10 ms
	}
}

int main(void)
{
	gfxInitDefault();
	consoleInit(GFX_TOP, NULL);
	LightEvent_Init(&g_resume, RESET_STICKY);

	unsigned frame = 0;
	aptHookCookie cookie;
	aptHook(&cookie, on_apt, &frame);

	Thread t = threadCreate(worker, NULL, 16 * 1024, 0x30, -2, false);
	if (!t) printf("threadCreate failed\n");

	while (aptMainLoop()) {          // may block here during HOME/sleep
		hidScanInput();
		u32 down = hidKeysDown();
		if (down & KEY_START) break; // leave through the normal path
		if (down & KEY_A) {
			SwkbdState kb;
			char text[64] = "";
			swkbdInit(&kb, SWKBD_TYPE_NORMAL, 2, 60);
			SwkbdButton b = swkbdInputText(&kb, text, sizeof text);
			printf("keyboard button=%d text=\"%s\"\n", (int)b, text);
		}
		if (down & KEY_B) aptSetHomeAllowed(!aptIsHomeAllowed());
		if (aptCheckHomePressRejected()) printf("HOME press rejected\n");

		printf("\x1b[1;1Hframe %06u  worker %08u", frame, (unsigned)atomic_load(&g_work_done));
		frame++;

		gfxFlushBuffers();
		gfxSwapBuffers();
		gspWaitForVBlank();
	}

	atomic_store(&g_quit, true);
	LightEvent_Signal(&g_resume);
	if (t) { threadJoin(t, U64_MAX); threadFree(t); }
	aptUnhook(&cookie);
	gfxExit();
	return 0;
}
```

Note that `main` calls `aptUnhook(&cookie)` before returning (the hook's `param` points at a local variable, so it must not outlive `main`). That also means this program never sees its own `ONEXIT`; the `ONEXIT` branch only shows the pattern for a program that keeps the hook registered with static state.

What to expect (from reading the source, **not observed**): pressing HOME prints `hook ONSUSPEND`, the worker counter stops, and after returning to the program `hook ONRESTORE` prints and the counter resumes. Closing the lid prints `ONSLEEP` then, after opening, `ONWAKEUP`. Pressing **A** prints `ONSUSPEND`/`ONRESTORE` around the keyboard. Pressing **B** then HOME prints `HOME press rejected`. Hook output printed during the transition is drawn to the console buffer, which only reaches the screen once you present a frame after restoring.

How it fails: if you forget `LightEvent_Signal(&g_resume)` before `threadJoin`, the worker blocks forever when you exit while paused. The console `printf` inside a hook is acceptable in a demo; in real code log to a ring buffer and print it from the main loop.

### Verification status

Compiled and linked (not run) with devkitARM r68-1 (GCC 16.1.0) and libctru 2.7.1-1, `-Wall -Wextra -O2 -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft -specs=3dsx.specs`, no warnings. It was built directly with `arm-none-eabi-gcc` into an ELF, not through the template `Makefile`, and was not converted to `.3dsx`. Emulator: not run. Old 3DS: not run. New 3DS: not run. Recorded on [source versions](source-versions.md#verification-status-of-code-on-the-wiki) by the maintainer who merges this page.

## Technical details

| Item | Value | Source |
| ---- | ----- | ------ |
| Hook types | `APTHOOK_ONSUSPEND`=0, `ONRESTORE`, `ONSLEEP`, `ONWAKEUP`, `ONEXIT`; `APTHOOK_COUNT` | `apt.h` |
| Hook storage | Singly linked list; `aptHook` pushes the new cookie to the front, so the most recently registered hook runs first. Callbacks are invoked in list order | `apt.c` `aptHook`, `aptCallHook` |
| Hook thread | The caller of `aptMainLoop()` / `aptLaunch*Applet()`; `ONEXIT` on the thread running `aptExit()` | `apt.c` |
| APT event helper thread | Created by `aptInit` with `threadCreate(aptEventHandler, 0, 0x1000, 0x31, -2, true)`: stack 4 KiB, priority `0x31`, core from the exheader default | `apt.c` |
| Notification signals handled | HOME button (two kinds), sleep query/enter/cancel/wakeup, order to close, POWER button (set and clear), try-sleep | `apt.c` `aptEventHandler` |
| Parameter commands handled by the helper | `APTCMD_DSP_SLEEP`, `APTCMD_DSP_WAKEUP`; everything else is relayed to the waiting thread | `apt.c` |
| DSP hooks | `aptDspSleep`, `aptDspWakeup`, `aptDspCancel` are `weak` no-ops in `apt.c`; ndsp provides the real ones | `apt.c`; [audio](audio.md) |
| Close conditions | `FLAG_ORDERTOCLOSE` or `FLAG_CANCELLED` | `apt.c` `aptShouldClose` |
| Jump-to-HOME conditions | HOME button state set, `FLAG_SHOULDHOME`, or `FLAG_POWERBUTTON` | `apt.c` `aptShouldJumpToHome` |
| Exit order in `__appExit` | `userAppExit`, `archiveUnmountAll`, `fsExit`, `hidExit`, `aptExit`, `srvExit` | `source/system/appExit.c` |
| Start order in `__appInit` | `srvInit`, `aptInit`, `hidInit`, `fsInit`, `archiveMountSdmc`, `userAppInit` | `source/system/appInit.c` |

All files are under `https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/`.

## Troubleshooting

| Symptom | Likely cause | Check |
| ------- | ------------ | ----- |
| Pressing HOME does nothing | `aptSetHomeAllowed(false)` was left on, or `aptMainLoop()` is not being called | `aptIsHomeAllowed()`, `aptCheckHomePressRejected()`; is the loop running every frame? |
| HOME menu appears seconds late | Main loop stalled in long work | [frame pacing](frame-pacing.md); split long jobs across frames |
| Worker thread keeps running (heat, battery) while HOME is shown | Worker not parked from `ONSUSPEND`/`ONSLEEP` | Pause flag as in the example |
| Console never sleeps on lid close | `aptSetSleepAllowed(false)`, or a library applet launch in progress | `aptIsSleepAllowed()` |
| Hang on exit | Worker blocked on an event nobody signals | Signal the resume event before `threadJoin` |
| Save missing after quitting from the HOME menu | Save attempted in `ONEXIT` (FS already closed) | Save before the loop ends |
| Garbage or stale image after returning from HOME or the keyboard | Presented the previous frame without redrawing | Draw a full frame after `ONRESTORE` |
| `aptMainLoop()` returns `false` right after the keyboard | User pressed HOME in the applet and the system cancelled you | Treat as exit |

## Not covered yet

Interrupting the program during a GPU transfer, a file write or an audio refill (the experiment listed for this guide) has not been done; see [Open questions](#open-questions). Continuous audio is covered only by pointer to [audio](audio.md). Local wireless (UDS) and input/sensor behaviour across sleep are not covered yet: see the service catalogue entry in [CTR services](../software/services.md) and [networking](networking.md#not-covered-yet).

## Pitfalls

- **Calling `aptMainLoop()` from two threads.** Symptom: hooks and blocking happen on the wrong thread, workers deadlock. Fix: one thread owns it; others read atomics.
- **Saving in `ONEXIT`.** Symptom: the write fails or nothing is saved. Cause: `fsExit` already ran. Fix: save before leaving the loop, when `aptShouldClose()` first turns true.
- **Assuming `ONEXIT` always fires.** Symptom: cleanup missing after START. Fix: do cleanup in `main` after the loop; use the hook only for system-ordered closes if needed.
- **Workers that never park.** Symptom: heat, battery drain, or crashes on resume when a worker uses GSP/DSP while suspended. Fix: the pause flag above.
- **Long work in `ONSLEEP`.** Symptom: slow or inconsistent sleep. Fix: keep it to flag changes and small writes to memory.
- **Disabling HOME and forgetting to re-enable it.** Symptom: no way back to the menu. Fix: scope `aptSetHomeAllowed(false)` to the critical section.
- **Treating the console buffer as a log for suspend events.** The text appears only after the next present; use a file written from the main loop or [3dslink logging](debugging.md#logging-over-the-network-3dslink).

## Open questions

- Do the contents of VRAM render targets, textures and the GPU command state survive a HOME suspend? Experiment: render a known pattern into an offscreen target, suspend with HOME, resume, read it back and compare a checksum (Old and New 3DS).
- Do TCP sockets opened with `socInit` survive HOME and sleep? Experiment: open a connection to a PC echo server, ping once per second, press HOME/close the lid for 10 s and 60 s, record the first error code after resume.
- How long may `ONSLEEP` take before the system complains? Experiment: busy-wait 100 ms, 1 s, 5 s in the hook and observe whether the console still sleeps and wakes.
- Is the order of hook callbacks stable across libctru versions? The documentation is silent; the list order was read from the pinned commit only.
- Behaviour of in-flight FS and socket requests from a worker thread during suspend is not documented in the files read.
- Everything labelled "expect" in the example is derived from source and untested.

## Sources

- [libctru `apt.h` at the pinned commit](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/include/3ds/services/apt.h) — hook types, `aptMainLoop`, HOME/sleep policy functions, chainloader functions.
- [libctru `apt.c`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/source/services/apt.c) — event handler thread, hook calls, sleep, HOME jump, applet launch, `aptExit` paths.
- [libctru `appInit.c`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/source/system/appInit.c) and [`appExit.c`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/source/system/appExit.c) — service start and shutdown order.
- [libctru `swkbd.c`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/source/applets/swkbd.c) and [`swkbd.h`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/include/3ds/applets/swkbd.h) — keyboard launch and result codes.
- Wiki pages: [audio](audio.md), [threading](threading.md), [frame pacing](frame-pacing.md), [testing on hardware](testing-on-hardware.md#what-to-test-beyond-it-runs), [libctru](../homebrew/libctru.md).
