# New 3DS extras: head tracking (QTM) and the video decoder (MVD)

Two services exist only on a **New 3DS**: **QTM** (`qtm:u`), which reports where the player's eyes are as seen by the inner camera, and **MVD** (`mvd:STD`), a hardware block that decodes H.264 video and converts image colour formats. An Old 3DS has neither: the services are absent, so a program that wants them must check first and have a fallback. This page shows how to detect them, how to open and close each correctly, what the data and buffers look like, and where the documentation is thin.

Both are optional features for a game that supports the Old 3DS as its baseline. Do not build a mandatory feature on them. (The **New 2DS XL** is a special case for QTM: it has no 3D display, and `qtm.h` says QTM is always unavailable on it.)

Background: [libctru: QTM](../homebrew/libctru.md#qtm-new-3ds-head-tracking) (what QTM does, with the barrier maths), [CTR services](../software/services.md), [I/O and peripherals](../hardware/io-and-peripherals.md) (MVD registers). Normal input is on [input and sensors](input-and-sensors.md); the camera on [cameras and Y2R](camera-and-y2r.md).

## Before you start

- Checked against libctru 2.7.1 source (`services/qtm.h/.c`, `qtmc.h`, `mvd.h/.c`) and the 3ds-examples `qtm/eye_position` and `mvd` programs ([source versions](source-versions.md)). The two examples below compile; **neither has been run on any console**, and none of this was tested on a New 3DS.
- The Old 3DS path of both examples is a clean "not available" result. That path is what a build can verify without a New 3DS; it has not been run on an Old 3DS either.

## Detecting what exists

| Check | Meaning |
| ----- | ------- |
| `APT_CheckNew3DS(&b)` | New 3DS family (all three New models) |
| `qtmCheckServicesRegistered()` | The `qtm:u` service is registered (`srvIsServiceRegistered`) |
| Result of `qtmInit` / `QTMU_Get*` | `0xC8A183EF`: QTM unavailable (always on the New 2DS XL); `0xC8A18008`: the camera is in use by the user |
| `QTMU_IsCurrentAppBlacklisted(&b)` | QTM was made unavailable for this title (rare, test titles); true on the New 2DS XL, but "do not rely on this for New 2DS XL detection" |
| `mvdstdInit(...)` result | MVD's own error; on an Old 3DS `srvGetServiceHandle("mvd:STD")` fails |

> Conflict: what `qtmCheckServicesRegistered()` returns. The comment on the function in `qtm.h` says "True on O3DS systems, false on N3DS systems". The same header's description says "QTM services are not present on O3DS, thus caller must call `qtmCheckServicesRegistered` to check if the services are registered", and the code returns whether `qtm:u` is registered (`srvIsServiceRegistered`). This page follows the code and the header description: **true where the service exists, i.e. on a New 3DS, false on an Old 3DS**; the one-line comment is a documentation slip. Not verified on hardware.

## QTM: head tracking

QTM follows the player's eyes using the inner camera (sampled at 320 x 240, 30 times a second) and the gyroscope, and makes the data available to any program. libctru wraps `qtm:u` as follows.

- `qtmInit(QTM_SERVICE_USER)` opens `qtm:u`; call `qtmCheckServicesRegistered()` first. `qtmExit()` closes it. Reference counted; the session limit is **3 sessions for all QTM services combined, 2 before system update 9.3.0**, and `qtm:c` counts too [documented: `qtm.h`]. `QTM_SERVICE_SYSTEM` (`qtm:s`) and `QTM_SERVICE_SYSTEM_PROCESS` (`qtm:sp`) exist for system modules; they also have commands for the barrier and calibration and re-enable automatic barrier control on exit, so a game should not use them.
- `QTMU_GetTrackingData(&d)` returns processed data, suitable for 3D scenes; `QTMU_GetRawTrackingData` returns the unprocessed camera-pixel coordinates; the `...Ex` versions take a prediction time in system ticks (at most one 30 FPS frame in the past, up to five in the future). On failure the output is zeroed, not left uninitialised.

`QtmTrackingData` fields [documented: `qtm.h`]:

| Field | Meaning |
| ----- | ------- |
| `headTracked` | An eye position was detected or predicted (`confidenceLevel > 0`); even when false QTM may guess from the gyroscope |
| `faceDetected`, `eyesDetected` | Whole face / eyes actually detected with confidence |
| `clamped` | Normalised eye coordinates were clamped after lens-distortion correction |
| `confidenceLevel` | 0 to 1 |
| `eyeCameraCoordinates[eye][2]` | Normalised, lens-corrected, centred on the camera; X in -1 to 1, Y in about -0.75 to 0.75 |
| `eyeWorldCoordinates[eye][2]` | The same multiplied by the tangent of half the field of view (64.9 degrees horizontal, 51.0 vertical on real hardware) |
| `dPitch`, `dYaw`, `dRoll` | Gyro rotation relative to the position at boot |
| `samplingTick` | When the measurement was made |

Helper functions compute the field of view (`qtmComputeFovX/Y`), the inverse aspect ratio, the head tilt angle (`qtmComputeHeadTiltAngle`, radians) and the eye-to-camera distance in millimetres (`qtmEstimateEyeToCameraDistance`, which assumes a 62 mm interocular distance). The header advises not hard-coding the camera's resolution, aspect ratio or field of view, because the data API is hardware-agnostic.

Manual control of the tracking infrared LED (`QTMU_EnableManualIrLedControl`, `QTMU_SetIrLedStatus`, `QTMU_DisableManualIrLedControl`) is also in `qtm:u`; the setting is cleared when the user closes the shell. Leaving the LED forced on is not something this wiki has measured.

```c
#include <3ds.h>
#include <stdio.h>

// New 3DS only: print the head-tracking confidence and eye midpoint. On an Old 3DS the
// program says so and does nothing else.
int main(void)
{
	gfxInitDefault();
	consoleInit(GFX_TOP, NULL);

	bool qtm = false;
	if (qtmCheckServicesRegistered()) {         // do this before qtmInit (qtm.h)
		Result rc = qtmInit(QTM_SERVICE_USER);  // qtm:u
		qtm = R_SUCCEEDED(rc);
		if (!qtm) printf("qtmInit failed: %08lX\n", (unsigned long)rc);
	} else {
		printf("QTM service not registered on this console.\n");
	}

	while (aptMainLoop()) {
		hidScanInput();
		if (hidKeysDown() & KEY_START) break;

		if (qtm) {
			QtmTrackingData d;
			Result rc = QTMU_GetTrackingData(&d);   // d is zeroed on failure
			if (R_SUCCEEDED(rc) && d.headTracked) {
				float mx = 0.5f * (d.eyeCameraCoordinates[QTM_EYE_LEFT][0] + d.eyeCameraCoordinates[QTM_EYE_RIGHT][0]);
				float my = 0.5f * (d.eyeCameraCoordinates[QTM_EYE_LEFT][1] + d.eyeCameraCoordinates[QTM_EYE_RIGHT][1]);
				printf("\x1b[4;1Hconfidence %.2f  eyes mid (%+.2f,%+.2f)   \n", d.confidenceLevel, mx, my);
			} else {
				printf("\x1b[4;1Hno head (rc=%08lX)                     \n", (unsigned long)rc);
			}
		}
		gfxFlushBuffers();
		gfxSwapBuffers();
		gspWaitForVBlank();
	}

	if (qtm) qtmExit();
	gfxExit();
	return 0;
}
```

Expected on a New 3DS (3D slider irrelevant): confidence near 1 with a face in view and eye midpoints near the screen centre; while the camera app or another program holds the camera, `rc=C8A18008`. On an Old 3DS the program prints "QTM service not registered" and idles. Not observed.

## MVD: the hardware video decoder

MVD is a block in the New 3DS SoC that decodes H.264 and converts colour formats ([registers](../hardware/io-and-peripherals.md)). libctru's `mvdstd*` functions drive it through `mvd:STD`.

- Two modes. `MVDMODE_COLORFORMATCONV` converts an image (input `MVD_INPUT_YUYV422`, output `MVD_OUTPUT_YUYV422`, `_BGR565` or `_RGB565`). `MVDMODE_VIDEOPROCESSING` takes H.264 NAL units (`MVD_INPUT_H264`).
- `mvdstdInit(mode, input, output, workBufSize, NULL)` opens the service and allocates a **work buffer with `linearAlloc`** (the size you pass plus a safety margin) that it frees in `mvdstdExit()`. For video use `MVD_DEFAULT_WORKBUF_SIZE` (0x9006C8, about 9 MiB, "what the New 3DS Internet Browser uses") or compute one with `mvdstdCalculateBufferSize`. Colour conversion needs no work buffer (size ignored). The linear heap is the scarce resource here ([memory and cache](memory-and-cache.md#how-much-memory-you-get)); **New 3DS:** has more application memory than an Old 3DS but the same rules apply.
- `mvdstdExit()` first loops on `MVDSTD_ControlFrameRendering(1)` while the result is "busy", then shuts the service down and frees the work buffer. Call it on every path after a successful init.
- Colour conversion: `mvdstdGenerateDefaultConfig(&cfg, inW, inH, outW, outH, inData, out0, out1)` then `mvdstdConvertImage(&cfg)`. Buffers are linear-heap pointers (virtual addresses; the library converts them).
- Video: feed one NAL unit at a time to `mvdstdProcessVideoFrame(buf, size, flag, &out)` (buffer in the linear heap, starting with the `00 00 01` prefix per the header comment), treat the results listed in `MVD_CHECKNALUPROC_SUCCESS` as success, and when a frame is ready call `mvdstdRenderVideoFrame(&cfg, true)` to write it to the output buffer. Output buffers can be changed with `mvdstdSetupOutputBuffers` / `mvdstdOverrideOutputBuffers` (the New 3DS browser does not use them).
- The 3ds-examples README warns that its video playback is "*very* *slow*" because every frame is shown right after rendering, with no buffering, and gives history about which launch methods could reach `mvd:STD` and that the exheader needs a kernel release version of at least 2.44 (system 8.0). Whether a `.3dsx` under today's Luma3DS can open `mvd:STD` was not tested here.

```c
#include <3ds.h>
#include <stdio.h>
#include <string.h>

#define W 320
#define H 240

// New 3DS only: convert one packed YUYV 4:2:2 image to RGB565 with the MVD block.
// Follows 3ds-examples/mvd (mvd_colorconvert); on an Old 3DS it returns without touching MVD.
static Result mvd_yuyv_to_rgb565(void)
{
	bool isNew = false;
	if (R_FAILED(APT_CheckNew3DS(&isNew)) || !isNew) return MAKERESULT(RL_PERMANENT, RS_NOTSUPPORTED, RM_APPLICATION, RD_NOT_IMPLEMENTED);

	const u32 inBytes = W * H * 2, outBytes = W * H * 2;
	u8 *in  = linearAlloc(inBytes);
	u8 *out = linearAlloc(outBytes);
	Result rc = (in && out) ? 0 : -1;
	if (R_SUCCEEDED(rc)) {
		memset(in, 0x80, inBytes);                      // flat grey test pattern
		GSPGPU_FlushDataCache(in, inBytes);             // CPU wrote it, MVD will read it
		rc = mvdstdInit(MVDMODE_COLORFORMATCONV, MVD_INPUT_YUYV422, MVD_OUTPUT_RGB565, 0, NULL);
		if (R_SUCCEEDED(rc)) {
			MVDSTD_Config cfg;
			// The last output pointer is only used for some output types (mvd.h); pass the same buffer.
			mvdstdGenerateDefaultConfig(&cfg, W, H, W, H, (u32 *)in, (u32 *)out, (u32 *)out);
			rc = mvdstdConvertImage(&cfg);
			mvdstdExit();                               // always pair with a successful init
			GSPGPU_InvalidateDataCache(out, outBytes);  // MVD wrote it, CPU will read it
		}
	}
	if (in) linearFree(in);
	if (out) linearFree(out);
	return rc;
}

int main(void)
{
	gfxInitDefault();
	consoleInit(GFX_TOP, NULL);
	Result rc = mvd_yuyv_to_rgb565();
	printf("mvd conversion: %08lX\nSTART exits.\n", (unsigned long)rc);
	while (aptMainLoop()) {
		hidScanInput();
		if (hidKeysDown() & KEY_START) break;
		gfxFlushBuffers();
		gfxSwapBuffers();
		gspWaitForVBlank();
	}
	gfxExit();
	return 0;
}
```

Expected: `mvd conversion: ...` with a non-success code on an Old 3DS (the program returns before touching MVD); on a New 3DS a success code if the service can be opened. Not observed.

## Technical details

| Item | Value | Source |
| ---- | ----- | ------ |
| QTM services | `qtm:u`, `qtm:s`, `qtm:sp`, plus `qtm:c` (hardware check, `qtmc.h`) | `qtm.c`, `qtm.h` |
| QTM session limit | 3 combined (2 before 9.3.0) | `qtm.h` |
| QTM result codes | `0xC8A18008` camera in use; `0xC8A183EF` QTM unavailable; `0xC8A18005` manual IR LED control not enabled; `0xC8A18009` unavailable (IR LED call) | `qtm.h` |
| QTM config blocks | status `0x180000`, calibration `0x180001` | `qtm.h` |
| Tracking cadence | inner camera 320x240 at 30 FPS plus gyro (about 10 ms samples) | `qtm.h` |
| MVD service | `mvd:STD` | `mvd.c` |
| MVD H.264 levels | 1.0 to 5.2 (`MVD_H264_LEVEL_*`, the calculator rejects levels above 5.2) | `mvd.h` |
| MVD work buffer | `linearAlloc(size + safety margin)`; freed by `mvdstdExit` | `mvd.c` |
| MVD statuses | OK `0x17000`, PARAMSET `0x17001`, BUSY `0x17002`, FRAMEREADY `0x17003`, INCOMPLETEPROCESSING `0x17004`, NALUPROCFLAG `0x17007` | `mvd.h` |

## Pitfalls

- **Assuming QTM or MVD exists.** Symptom: init fails or the program crashes on an Old 3DS. Fix: model check first, fallback path always present.
- **Treating zeroed tracking data as "centre".** The output is zeroed on failure. Test the result code and `headTracked`.
- **Relying on the 2DS-family detection from QTM.** The header says not to; use `CFGU_GetSystemModel` ([system information](system-info-and-power.md)).
- **Leaking the MVD work buffer.** It is linear memory; pair every successful `mvdstdInit` with `mvdstdExit`.
- **Passing non-linear memory to MVD.** NAL units "must be located in linearmem" (`mvd.h`).
- **Opening more QTM sessions than the limit.** Symptom: `qtmInit` fails. Fix: one session, closed with `qtmExit`.

## Open questions

- Latency and jitter of QTM data against a real head movement, with and without prediction; CPU cost of polling it each frame.
- Whether `mvd:STD` can be opened by a `.3dsx` under current Luma3DS; MVD throughput for H.264 at the sizes a 3DS screen needs.
- Whether camera use by the application blocks QTM (`0xC8A18008`) while its own capture runs.
- The exact set of New 3DS models on which `qtm:u` is registered (the New 2DS XL case).

## Verification status

| Item | Status |
| ---- | ------ |
| Both examples | Build with devkitARM r68-1 (GCC 16.1.0), libctru 2.7.1-1, `-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft -O2 -Wall -Wextra`, no warnings; linked with `3dsx.specs` |
| Old 3DS | not run |
| New 3DS | not run |
| Emulator | not run |

## Sources

- [libctru `qtm.h`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/include/3ds/services/qtm.h) and [`qtm.c`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/source/services/qtm.c) — services, data, result codes, session limit.
- [libctru `mvd.h`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/include/3ds/services/mvd.h) and [`mvd.c`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/source/services/mvd.c) — modes, formats, work buffer, init/exit.
- [3ds-examples `qtm/`](https://github.com/devkitPro/3ds-examples/tree/be2001fee08cf8ec7d3366e095e83f6f75da8942/qtm) and [`mvd/`](https://github.com/devkitPro/3ds-examples/tree/be2001fee08cf8ec7d3366e095e83f6f75da8942/mvd) — usage and the MVD README's caveats.
- [3dbrew: MVD Services](https://www.3dbrew.org/wiki/MVD_Services) — referenced by `mvd.h`; not re-read for this page.
