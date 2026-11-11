# Input and sensors: buttons, touch, sticks and motion

Reading the 3DS controls looks trivial (`hidScanInput()`, then `hidKeysDown()`), but a few details decide whether your program feels right: when input is sampled, what "pressed" means, which inputs exist on every console and which only on a New 3DS, and which sensors are off until you switch them on. This page covers the **HID** service (the system's *human interface device* service, `hid:USER`, which serves the buttons, Circle Pad, touch screen and motion sensors) and its companion **IRRST** (`ir:rst`, which serves the C-Stick and ZL/ZR on a New 3DS).

Background: [I/O and peripherals](../hardware/io-and-peripherals.md) (hardware), [CTR services](../software/services.md) (service names). Head tracking is not ordinary input: it is on [New 3DS: QTM and MVD](new-3ds-qtm-and-mvd.md).

## Before you start

- Checked against libctru 2.7.1 source (`services/hid.h`, `source/services/hid.c`, `irrst.c`, `include/3ds/os.h`) and the `input/read-controls` and `input/touch-screen` examples of 3ds-examples (see [source versions](source-versions.md)). The example compiles with devkitARM r68 and `-Wall -Wextra`; it has not been run on an Old 3DS or New 3DS (see [Verification status](#verification-status)).
- Old 3DS is the baseline: a program that needs ZL, ZR, the C-Stick or head tracking must still work without them.

## The one-frame loop

```c
while (aptMainLoop()) {
    hidScanInput();                    // exactly once per frame
    u32 down = hidKeysDown();          // pressed since the previous scan
    u32 held = hidKeysHeld();
    u32 up   = hidKeysUp();            // released since the previous scan
    touchPosition t;  hidTouchRead(&t);
    circlePosition c; hidCircleRead(&c);
    /* ... */
}
```

- `hidScanInput()` copies the newest entries out of the shared memory the HID service keeps updating, then computes *down* and *up* by comparing with the previous call. The other `hid*` functions only return what the last scan stored. Calling it twice in one frame makes the second call's `hidKeysDown()` empty, because the second scan sees no change [documented: `hid.c`].
- `hidInit()` is called for you before `main` and `hidExit()` after it ([application lifecycle](application-lifecycle.md#technical-details)); do not call them yourself unless you replaced the default startup.
- Sample once, near the top of the frame, and use the same values for the whole frame. Input latency then depends only on where in the frame you scan (see [frame pacing](frame-pacing.md)).

## What each input gives you

| Input | API | Value | Old 3DS | New 3DS |
| ----- | --- | ----- | ------- | ------- |
| Buttons (A, B, X, Y, L, R, START, SELECT, D-pad) | `hidKeysHeld/Down/Up` bits `KEY_A` ... | bit mask | yes | yes |
| Circle Pad | `hidCircleRead` | `circlePosition {s16 dx, dy}` | yes | yes |
| Circle Pad as digital keys | `KEY_CPAD_UP/DOWN/LEFT/RIGHT` (and `KEY_UP/DOWN/LEFT/RIGHT` = D-pad or Circle Pad) | bit mask | yes | yes |
| Touch screen (bottom) | `hidTouchRead`; `KEY_TOUCH` while touching | `touchPosition {u16 px, py}` | yes | yes |
| ZL, ZR | `KEY_ZL`, `KEY_ZR` | bit mask | no | yes |
| C-Stick | `hidCstickRead` (alias of `irrstCstickRead`); `KEY_CSTICK_*` | `circlePosition` | no | yes |
| Accelerometer | `hidAccelRead` | `accelVector {s16 x, y, z}` | yes (must enable) | yes (must enable) |
| Gyroscope | `hidGyroRead` | `angularRate {s16 x (roll), z (yaw), y (pitch)}` | yes (must enable) | yes (must enable) |
| Volume slider | `HIDUSER_GetSoundVolume` | `u8` 0-63 | yes | yes |
| 3D slider | `osGet3DSliderState()` | `float` 0.0-1.0 | yes | yes |
| Headset plugged in | `osIsHeadsetConnected()` | `bool` | yes | yes |

Bit positions: `KEY_A` is bit 0 ... `KEY_Y` bit 11, `KEY_ZL` bit 14, `KEY_ZR` bit 15, `KEY_TOUCH` bit 20, `KEY_CSTICK_*` bits 24-27, `KEY_CPAD_*` bits 28-31 [documented: `hid.h`].

Notes that are easy to miss:

- **`KEY_TOUCH` is not a HID key.** libctru sets it itself when the touch record says the screen is pressed [documented: `hid.h` comment "Not actually provided by HID"; `hid.c`]. When nothing touches the screen `px` and `py` are 0, so test `KEY_TOUCH`, not the coordinates.
- **Touch coordinates** are in the bottom screen's 320 x 240 pixel space; the touch-screen example draws with them directly [documented: `3ds-examples/input/touch-screen`].
- **Old 3DS has no ZL, ZR or C-Stick.** Their bits stay 0 and `hidCstickRead` returns (0, 0) because IRRST is never started (next section). Offer a fallback (for example L/R for ZL/ZR), never require them.
- **Motion sensors start disabled.** Call `HIDUSER_EnableAccelerometer()` / `HIDUSER_EnableGyroscope()` once, read with `hidAccelRead` / `hidGyroRead`, and disable them again when you stop using them (`HIDUSER_Disable*`). Whether leaving them enabled costs battery is not measured here. To convert gyro counts to degrees per second multiply by the float from `HIDUSER_GetGyroscopeRawToDpsCoefficient`. Dead zone, calibration and drift of the sensors are not documented in libctru and not measured here ([Open questions](#open-questions)).
- **Key repeat.** `hidKeysDownRepeat()` reports a press, then repeats after an initial delay and then at an interval, counted in *scans* (defaults 30 and 15, i.e. about 0.5 s and 0.25 s at 60 scans per second). Change them with `hidSetRepeatParameters(delay, interval)`; reading `hidKeysDownRepeat()` clears the repeat flag, so call it once per frame [documented: `hid.c`].

## Waiting for input instead of polling

A program that only reacts to input (a menu, a tool) can sleep until something changes:

- `hidWaitForEvent(HIDEVENT_PAD0, false)` blocks until the HID module updated the pad/touch region (also `HIDEVENT_PAD1`, `HIDEVENT_Accel`, `HIDEVENT_Gyro`, `HIDEVENT_DebugPad`). With `nextEvent = true` it first discards an already-signalled event and waits for a fresh one.
- `hidWaitForAnyEvent(nextEvents, cancelEvent, timeout)` waits on all HID events plus the IRRST event plus an optional handle of your own (handy to wake the wait from another thread) and returns the wait result, so a timeout is visible [documented: `hid.c`].

For a game, drive input from the frame loop instead; events add nothing when you scan every frame.

## C-Stick and ZL/ZR: the IRRST detour

These inputs come from a different service, `ir:rst`, not from HID, even on a New 3DS. libctru hides this: `hidInit()` calls a function `hidShouldUseIrrst()` and, if it returns true, `irrstInit()`; `hidScanInput()` then also runs `irrstScanInput()` and merges the keys.

- The default `hidShouldUseIrrst()` is a *weak* symbol: true if `APT_CheckNew3DS` succeeds and says New 3DS, so an Old 3DS never opens `ir:rst` [documented: `hid.c`]. **New 3DS:** the extra session and 0x98 bytes of mapped shared memory are used; you can define your own `bool hidShouldUseIrrst(void)` returning `false` to avoid them if you do not need the C-Stick or ZL/ZR.
- `irrstInit()` also calls `IRRST_Initialize(10, 0)` unless the loader passed an `ir:rst` handle in the environment (`envGetHandle("ir:rst")` is non-zero), in which case the loader's session is used and not shut down by you [documented: `irrst.c`]. The meaning of the two arguments is not documented in libctru.
- An Old 3DS with the Circle Pad Pro accessory is also served by `ir:rst` on the system side, but libctru's default does not start IRRST on an Old 3DS, so those inputs are not read unless you override `hidShouldUseIrrst()` and call `irrstInit()` yourself. Whether that works for the accessory is not verified here (> Unknown: no source in this wiki shows a working Circle Pad Pro read through libctru).

## Detecting the model, and degrading gracefully

```c
bool isNew = false;
if (R_FAILED(APT_CheckNew3DS(&isNew))) isNew = false;   // treat failure as Old 3DS
```

Use this to decide whether to show ZL/ZR/C-Stick hints. Keep game logic keyed to *actions* ("camera left"), and map several inputs to each action, so the Old 3DS and New 3DS paths are one code path.

## A complete example

Reads everything above, prints it on the top screen, and exits on START. It enables and later disables both motion sensors.

```c
#include <3ds.h>
#include <stdio.h>

int main(void)
{
	gfxInitDefault();
	consoleInit(GFX_TOP, NULL);

	bool isNew = false;
	if (R_FAILED(APT_CheckNew3DS(&isNew))) isNew = false; // on failure behave as an Old 3DS

	// Motion sensors are off until you ask for them. Leave them off if unused.
	bool accel = R_SUCCEEDED(HIDUSER_EnableAccelerometer());
	bool gyro  = R_SUCCEEDED(HIDUSER_EnableGyroscope());
	float dpsPerRaw = 0.0f;
	if (gyro && R_FAILED(HIDUSER_GetGyroscopeRawToDpsCoefficient(&dpsPerRaw))) dpsPerRaw = 0.0f;

	printf("Model: %s. Sensors: accel %s, gyro %s. START exits.\n",
	       isNew ? "New 3DS" : "Old 3DS", accel ? "on" : "off", gyro ? "on" : "off");

	while (aptMainLoop()) {
		hidScanInput();                 // once per frame, before any hidKeys*/hid*Read call
		u32 down = hidKeysDown();
		u32 held = hidKeysHeld();
		if (down & KEY_START) break;

		touchPosition touch;   hidTouchRead(&touch);
		circlePosition cpad;   hidCircleRead(&cpad);
		circlePosition cstick; hidCstickRead(&cstick); // stays 0,0 on an Old 3DS
		accelVector acc;       hidAccelRead(&acc);
		angularRate rate;      hidGyroRead(&rate);

		printf("\x1b[3;1Hheld=%08lX  touch=%s (%3u,%3u)   \n", (unsigned long)held,
		       (held & KEY_TOUCH) ? "yes" : "no ", touch.px, touch.py);
		printf("circle pad  %4d %4d   \n", cpad.dx, cpad.dy);
		printf("c-stick     %4d %4d %s\n", cstick.dx, cstick.dy, isNew ? "" : "(none)");
		printf("ZL/ZR       %d %d   \n", (held & KEY_ZL) != 0, (held & KEY_ZR) != 0);
		printf("accel       %6d %6d %6d   \n", acc.x, acc.y, acc.z);
		printf("gyro (dps)  %7.1f %7.1f %7.1f   \n", rate.x * dpsPerRaw, rate.y * dpsPerRaw, rate.z * dpsPerRaw);

		gfxFlushBuffers();
		gfxSwapBuffers();
		gspWaitForVBlank();
	}

	if (accel) HIDUSER_DisableAccelerometer();
	if (gyro)  HIDUSER_DisableGyroscope();
	gfxExit();
	return 0;
}
```

Expected on an Old 3DS: the model line says Old 3DS, `c-stick` shows `(none)` with zeros, ZL/ZR stay 0; moving the Circle Pad changes `dx`/`dy`; touching the bottom screen sets `touch=yes`. These are the behaviours the source implies; they have not been observed on hardware.

## Sleep, HOME and input

While your application is suspended it is not running, so it scans nothing. After returning from HOME or sleep the first scan may therefore show a large jump in touch and stick values, and a button held across the suspend may appear as a fresh press `[hypothesis]` (nothing in libctru handles it; confirm by logging the first scan after `APTHOOK_ONRESTORE`). Clear any "held" game state on `APTHOOK_ONRESTORE` / `ONWAKEUP` ([application lifecycle](application-lifecycle.md#what-each-transition-means-for-your-program)). The exact behaviour of `hidKeysDown()` on the first scan after resume has not been tested here.

## Technical details

| Item | Value | Source |
| ---- | ----- | ------ |
| HID service names tried | `hid:USER`, then `hid:SPVR` | `hid.c` `hidInit` |
| HID shared memory mapped | 0x2B0 bytes, read-only, virtual address from `mappableAlloc` | `hid.c` |
| IRRST service / mapped size | `ir:rst` / 0x98 bytes | `irrst.c` |
| HID events | 5: PAD0, PAD1, Accel, Gyro, DebugPad (handles returned by `HIDUSER_GetHandles`, command `0xA`) | `hid.h`, `hid.c` |
| Shared-memory sections (word offsets) | Pad at 0; touch at 42 (byte 0xA8); accelerometer at 66; gyroscope at 86. Word 4 of each section is the newest entry index. Pad, touch, accelerometer keep 8 entries (index clamped to 7); gyroscope keeps 32 (clamped to 31) | `hid.c` `hidScanInput` |
| "Entry valid" test | For index 0, the two 64-bit timestamps at the start of the section must differ and be non-negative | `hidCheckSectionUpdateTime` |
| Pad entry | Key mask at word `10 + 4*id`, Circle Pad (dx, dy) at word `13 + 4*id` | `hid.c` |
| Touch entry | At word `50 + 2*id`: x,y in the first word; a non-zero second word means touching | `hid.c` |
| Key-repeat defaults | delay 30, interval 15 scans | `hid.c` |
| Volume slider | `HIDUSER_GetSoundVolume`, 0-63 | `hid.h` |
| 3D slider, headset, Wi-Fi strength | Read from the shared config page: `osGet3DSliderState`, `osIsHeadsetConnected`, `osGetWifiStrength` (0-3) | `os.h` |
| New 3DS check | `APT_CheckNew3DS` (`apt.h`); `PTMSYSM_CheckNew3DS` also exists but is a system service | `apt.h`, `ptmsysm.h` |

The shared-memory layout is libctru's reading of the service's memory; the full structure is documented by 3dbrew (HID Shared Memory), which this page does not copy.

## Pitfalls

- **Scanning twice per frame.** Symptom: `hidKeysDown()` is always empty in the second place you call it. Fix: scan once, pass the results around.
- **Using `px, py` to detect touch.** Symptom: a phantom touch at (0, 0). Fix: test `KEY_TOUCH`.
- **Requiring ZL/ZR or the C-Stick.** Symptom: the game cannot be played on an Old 3DS. Fix: every action needs an Old 3DS input.
- **Reading sensors you never enabled.** Symptom: zeros from `hidAccelRead` / `hidGyroRead`. Fix: call `HIDUSER_Enable*` first.
- **Treating the Circle Pad like a D-pad with a hard threshold.** Symptom: unintended drift or missed diagonals. Fix: apply your own dead zone; libctru does not [documented: `hid.c` copies the raw values]. The size of the dead zone to use is not established here.
- **Not handling an `ir:rst` failure on a New 3DS.** `hidInit()` returns the error of `irrstInit()`; the default startup ignores it, so the C-Stick silently stays at 0 ([application lifecycle](application-lifecycle.md#technical-details)).

## Open questions

- Latency from physical press to the value seen by `hidScanInput()`, and its jitter, on Old 3DS and New 3DS. Experiment: a program that shows a flash on a button edge and a high-speed camera, or a tick-stamped log of `hidWaitForEvent(HIDEVENT_PAD0)` against V-blank.
- Size of the Circle Pad dead zone, its resting offset between units, and gyroscope bias. Experiment: log raw values with no input on several consoles.
- Whether `KEY_*` state is cleaned up across sleep and HOME exactly as described above.
- Whether the Circle Pad Pro can be read through `hidShouldUseIrrst()` overrides on an Old 3DS.

## Verification status

| Item | Status |
| ---- | ------ |
| The example | Builds with devkitARM r68-1 (GCC 16.1.0), libctru 2.7.1-1, `-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft -O2 -Wall -Wextra`, no warnings; linked with `3dsx.specs` |
| Old 3DS | not run |
| New 3DS | not run |
| Emulator | not run |

## Sources

- [libctru `hid.h`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/include/3ds/services/hid.h) and [`hid.c`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/source/services/hid.c) — key bits, structures, init, shared-memory parsing, key repeat, `hidShouldUseIrrst`.
- [libctru `irrst.h`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/include/3ds/services/irrst.h) and [`irrst.c`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/source/services/irrst.c) — C-Stick/ZL/ZR service session and shared memory.
- [libctru `os.h`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/include/3ds/os.h) — 3D slider, headset and Wi-Fi strength accessors.
- [3ds-examples `input/`](https://github.com/devkitPro/3ds-examples/tree/be2001fee08cf8ec7d3366e095e83f6f75da8942/input) — read-controls and touch-screen examples.
- [3dbrew: HID Services](https://www.3dbrew.org/wiki/HID_Services) and [HID Shared Memory](https://www.3dbrew.org/wiki/HID_Shared_Memory) — referenced by `hid.h` for the full memory layout (not re-read for this page).
