# Cameras and hardware colour conversion (CAM and Y2R)

The 3DS has three cameras: one on the inside of the lid and two on the outside, side by side, which is how it takes 3D photos. Homebrew reaches them through the **CAM** service (`cam:u`), which delivers still or video frames into a buffer you provide. Camera frames usually arrive in a video colour format (YUV); a second block, **Y2R** (`y2r:u`, "YUV to RGB"), converts them to RGB in hardware, optionally tiled for use as a GPU texture. This page shows the capture sequence, how to size and own the buffers, how to convert, and what can go wrong.

Both services exist on every model. Their performance on an Old 3DS is not measured on this wiki.

Background: [I/O and peripherals](../hardware/io-and-peripherals.md) (camera hardware), [CTR services](../software/services.md). Cache rules: [memory and cache](memory-and-cache.md#keeping-caches-coherent). The related head-tracking camera use is on [New 3DS: QTM and MVD](new-3ds-qtm-and-mvd.md).

## Before you start

- Checked against libctru 2.7.1 source (`services/cam.h`, `cam.c`, `y2r.h`, `y2r.c`) and the 3ds-examples `camera/image` and `camera/video` programs ([source versions](source-versions.md)). Both examples compile; neither has been run on hardware.
- Camera access conflicts with the system camera applet and with other users of the camera ("camera in use by user" is a QTM result code, `0xC8A18008`, per `qtm.h`); expect `camInit()` and captures to fail while the camera is busy elsewhere.

## Taking one picture

The sequence, as the 3ds-examples do it [documented: `camera/image/source/main.c`]:

1. `camInit()`: opens `cam:u` and calls `CAMU_DriverInitialize()`; `camExit()` calls `CAMU_DriverFinalize()` and closes the session. Both are reference counted.
2. Configure each camera by `SELECT_*` mask: `CAMU_SetSize`, `CAMU_SetOutputFormat`, `CAMU_SetFrameRate`, `CAMU_SetNoiseFilter`, `CAMU_SetAutoExposure`, `CAMU_SetAutoWhiteBalance` (and optionally effect, contrast, photo mode, flip, trimming).
3. `CAMU_GetMaxBytes(&maxBytes, w, h)` then `CAMU_SetTransferBytes(port, maxBytes, w, h)`: the largest DMA transfer size for that image size. This value also becomes the `transferUnit` argument of `CAMU_SetReceiving`.
4. `CAMU_Activate(select)`, `CAMU_ClearBuffer(port)`, `CAMU_StartCapture(port)`.
5. `CAMU_SetReceiving(&event, dst, port, imageBytes, transferUnit)` returns an event handle; wait on it (always with a timeout), then close the handle.
6. `CAMU_StopCapture(port)` and `CAMU_Activate(SELECT_NONE)` to release the sensors.

Naming: cameras are `SELECT_OUT1` (outer camera 1), `SELECT_IN1` (inner camera 1) and `SELECT_OUT2` (outer camera 2); combinations such as `SELECT_OUT1_OUT2` are provided. Ports are `PORT_CAM1`, `PORT_CAM2`, `PORT_BOTH`. In the 3D-photo example `SELECT_OUT1` is received on `PORT_CAM1` and `SELECT_OUT2` on `PORT_CAM2`, and the two are started together and synchronised with `CAMU_SynchronizeVsyncTiming(SELECT_OUT1, SELECT_OUT2)` [documented: `camera/image`]. Which port the inner camera uses is not stated in the headers (> Unknown: confirm in 3dbrew's CAM services page or by experiment).

Each camera has two **contexts** (A and B) holding size, output format, flip and effect, so you can switch between two configurations (`CAMU_SwitchContext`) without reconfiguring. Settings such as frame rate, exposure and white balance are per camera, not per context.

## Image sizes and formats

| `CAMU_Size` | Pixels | Note |
| ----------- | ------ | ---- |
| `SIZE_VGA` | 640 x 480 | |
| `SIZE_QVGA` (= `SIZE_CTR_BOTTOM_LCD`) | 320 x 240 | Bottom screen size |
| `SIZE_QQVGA` | 160 x 120 | |
| `SIZE_CIF` / `SIZE_QCIF` | 352 x 288 / 176 x 144 | |
| `SIZE_DS_LCD` / `SIZE_DS_LCDx4` | 256 x 192 / 512 x 384 | Nintendo DS screen size |
| `SIZE_CTR_TOP_LCD` | 400 x 240 | Top screen size |

Output formats: `OUTPUT_YUV_422` (2 bytes per pixel, packed) or `OUTPUT_RGB_565` (2 bytes per pixel). Frame rates: fixed (`FRAME_RATE_5` ... `FRAME_RATE_30`, including 8.5) or ranges such as `FRAME_RATE_30_TO_10` that let the camera slow down in dim light [documented: `cam.h`]. `CAMU_SetSize` with a custom size and crop is `CAMU_SetDetailSize`.

An image buffer for `imageBytes` bytes can be ordinary heap memory; the examples use `malloc`. It is DMA-written by the camera module while your CPU has cached lines for it, so after the receive event fires, invalidate the range before reading (it is not the same as flushing; see [memory and cache](memory-and-cache.md#keeping-caches-coherent)). The examples do not do this; whether it matters in practice is not tested here (`[hypothesis]`). The safe form needs a 32-byte aligned buffer with a size that is a multiple of 32, which is why the example below uses `memalign`.

## Colour conversion with Y2R

When the camera outputs YUV (`OUTPUT_YUV_422`, which is packed "YUYV": Y0 U Y1 V), Y2R turns it into RGB:

- `y2rInit()` / `y2rExit()` open `y2r:u` and run `Y2RU_DriverInitialize` / `Y2RU_DriverFinalize`; both are reference counted.
- Describe the conversion with one `Y2RU_ConversionParams` and `Y2RU_SetConversionParams`: input format (planar 4:2:2 or 4:2:0 at 8 or 16 bits, or packed `INPUT_YUV422_BATCH`), output format (`OUTPUT_RGB_32`, `_24`, `_16_555`, `_16_565`), rotation (0/90/180/270), block alignment, input width and height, the colour coefficients and the alpha value.
- `BLOCK_8_BY_8` writes the output as 8x8 tiles in Z-order, which is the GPU texture layout, so the result can be bound as a texture without a CPU swizzle ([tiling](../../glossary.md#tiling)). Height must then be a multiple of 8.
- Width must be a multiple of 8 and at most 1024; height should be a multiple of 8 [documented: `y2r.h`].
- Coefficient presets are BT.601 and BT.709 with PC ranges and with TV ranges (`COEFFICIENT_ITU_R_BT_601_SCALING` etc.). `CAMU_GetSuitableY2rStandardCoefficient` returns the preset that matches the camera [documented: `cam.h`].
- Source and destination are DMA'd: `Y2RU_SetSendingYUYV` (or `...Y`, `...U`, `...V` for planar) and `Y2RU_SetReceiving`, each with `(buffer, image_size, transfer_unit, transfer_gap)`; `transfer_unit` must divide `image_size`, and `transfer_unit + transfer_gap` must be less than 32768. The destination does not need to be in the linear heap [documented: `y2r.h`].
- `Y2RU_StartConversion()` starts it. Completion is the event from `Y2RU_GetTransferEndEvent()` (enabled with `Y2RU_SetTransferEndInterrupt(true)`; it is on by default) or polling `Y2RU_IsBusyConversion`.

Two warnings from the header: with a one-line `transfer_unit` on a 400x240 image the end event can fire right after the conversion started, and 2, 4 or 8 lines avoids it (4 or 8 reportedly gives the best speed); and the event "sometimes (but rarely) isn't triggered", so always wait with a timeout [documented: `y2r.h`].

## Complete example: one frame from the outer camera

Captures a 320x240 RGB565 frame from the outer-left camera, prints the centre pixel and exits on START. It releases the sensor on every error path.

```c
#include <3ds.h>
#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>

#define W 320
#define H 240

// Capture one RGB565 QVGA frame from the outer-left camera into dst (W*H u16).
static Result capture_frame(u16 *dst)
{
	const u32 imageBytes = W * H * sizeof(u16);
	Handle done = 0;
	u32 maxBytes = 0;
	bool active = false, capturing = false;
	Result rc;

	if (R_FAILED(rc = CAMU_SetSize(SELECT_OUT1, SIZE_QVGA, CONTEXT_A))) goto out;
	if (R_FAILED(rc = CAMU_SetOutputFormat(SELECT_OUT1, OUTPUT_RGB_565, CONTEXT_A))) goto out;
	if (R_FAILED(rc = CAMU_SetFrameRate(SELECT_OUT1, FRAME_RATE_30))) goto out;
	if (R_FAILED(rc = CAMU_SetNoiseFilter(SELECT_OUT1, true))) goto out;
	if (R_FAILED(rc = CAMU_SetAutoExposure(SELECT_OUT1, true))) goto out;
	if (R_FAILED(rc = CAMU_SetAutoWhiteBalance(SELECT_OUT1, true))) goto out;
	if (R_FAILED(rc = CAMU_GetMaxBytes(&maxBytes, W, H))) goto out;
	if (R_FAILED(rc = CAMU_SetTransferBytes(PORT_CAM1, maxBytes, W, H))) goto out;

	if (R_FAILED(rc = CAMU_Activate(SELECT_OUT1))) goto out;
	active = true;
	if (R_FAILED(rc = CAMU_ClearBuffer(PORT_CAM1))) goto out;
	if (R_FAILED(rc = CAMU_StartCapture(PORT_CAM1))) goto out;
	capturing = true;
	if (R_FAILED(rc = CAMU_SetReceiving(&done, dst, PORT_CAM1, imageBytes, (s16)maxBytes))) goto out;

	// 1 s timeout: never wait forever on a device event.
	rc = svcWaitSynchronization(done, 1000ULL * 1000 * 1000);
	// The camera DMA wrote dst behind the CPU's back: drop stale cache lines before reading.
	// dst is 32-byte aligned and imageBytes a multiple of 32, so no neighbouring data is lost.
	if (R_SUCCEEDED(rc)) svcInvalidateProcessDataCache(CUR_PROCESS_HANDLE, (u32)dst, imageBytes);
out:
	if (capturing) CAMU_StopCapture(PORT_CAM1);
	if (done) svcCloseHandle(done);
	if (active) CAMU_Activate(SELECT_NONE);   // release the sensor
	return rc;
}

int main(void)
{
	gfxInitDefault();
	consoleInit(GFX_TOP, NULL);

	u16 *frame = memalign(32, W * H * sizeof(u16));   // whole cache lines, see capture_frame
	Result rc = frame ? camInit() : -1;       // opens cam:u and runs CAMU_DriverInitialize
	if (R_SUCCEEDED(rc)) {
		rc = capture_frame(frame);
		if (R_SUCCEEDED(rc)) printf("centre pixel RGB565 = %04X\n", frame[(H / 2) * W + W / 2]);
		camExit();
	}
	if (R_FAILED(rc)) printf("camera error %08lX\n", (unsigned long)rc);
	printf("Press START to exit.\n");

	while (aptMainLoop()) {
		hidScanInput();
		if (hidKeysDown() & KEY_START) break;
		gfxFlushBuffers();
		gfxSwapBuffers();
		gspWaitForVBlank();
	}
	free(frame);
	gfxExit();
	return 0;
}
```

## Complete example: Y2R conversion

Converts a synthetic mid-grey packed-YUYV image to RGB565 in linear memory and prints the first output pixel. It shows the cache rules for both directions (flush what the CPU wrote, invalidate what the hardware wrote).

```c
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 320   // multiple of 8, at most 1024 (y2r.h)
#define H 240   // multiple of 8 (required for BLOCK_8_BY_8, preferred otherwise)

// Convert one packed YUYV 4:2:2 image into RGB565 using the Y2R hardware.
static Result yuyv_to_rgb565(const u8 *yuyv, u16 *rgb)
{
	const u32 inBytes = W * H * 2, outBytes = W * H * 2;
	Handle done = 0;
	Result rc = y2rInit();
	if (R_FAILED(rc)) return rc;

	Y2RU_ConversionParams p;
	memset(&p, 0, sizeof p);
	p.input_format = INPUT_YUV422_BATCH;
	p.output_format = OUTPUT_RGB_16_565;
	p.rotation = ROTATION_NONE;
	p.block_alignment = BLOCK_LINE;
	p.input_line_width = W;
	p.input_lines = H;
	p.standard_coefficient = COEFFICIENT_ITU_R_BT_601;
	p.alpha = 0xFF;

	if (R_FAILED(rc = Y2RU_SetConversionParams(&p))) goto out;
	if (R_FAILED(rc = Y2RU_SetTransferEndInterrupt(true))) goto out;
	if (R_FAILED(rc = Y2RU_GetTransferEndEvent(&done))) goto out;   // do not close this handle (y2r.h)
	if (R_FAILED(rc = Y2RU_SetSendingYUYV(yuyv, inBytes, W * 2, 0))) goto out;
	// 4 lines per DMA transfer: y2r.h notes that 1 line can fire the end event too early.
	if (R_FAILED(rc = Y2RU_SetReceiving(rgb, outBytes, W * 2 * 4, 0))) goto out;
	if (R_FAILED(rc = Y2RU_StartConversion())) goto out;

	// y2r.h: the event "sometimes (but rarely) isn't triggered", so always use a timeout.
	rc = svcWaitSynchronization(done, 1000ULL * 1000 * 1000);
	if (R_FAILED(rc)) Y2RU_StopConversion();
out:
	y2rExit();
	return rc;
}

int main(void)
{
	gfxInitDefault();
	consoleInit(GFX_TOP, NULL);

	u8 *yuyv = linearAlloc(W * H * 2);
	u16 *rgb = linearAlloc(W * H * 2);
	if (yuyv && rgb) {
		for (u32 i = 0; i < W * H * 2; i += 4) {   // Y0 U Y1 V : mid grey
			yuyv[i] = 128; yuyv[i + 1] = 128; yuyv[i + 2] = 128; yuyv[i + 3] = 128;
		}
		GSPGPU_FlushDataCache(yuyv, W * H * 2);      // CPU wrote it; hardware will read it
		Result rc = yuyv_to_rgb565(yuyv, rgb);
		GSPGPU_InvalidateDataCache(rgb, W * H * 2);  // hardware wrote it; CPU will read it
		printf("rc=%08lX first pixel=%04X\n", (unsigned long)rc, rgb[0]);
	}
	printf("Press START to exit.\n");
	while (aptMainLoop()) {
		hidScanInput();
		if (hidKeysDown() & KEY_START) break;
		gfxFlushBuffers();
		gfxSwapBuffers();
		gspWaitForVBlank();
	}
	if (yuyv) linearFree(yuyv);
	if (rgb) linearFree(rgb);
	gfxExit();
	return 0;
}
```

Expected: `rc=00000000` and a first pixel of about mid grey in RGB565 (around `8410`). These are what the API contract implies; they have not been observed on hardware.

## Technical details

| Item | Value | Source |
| ---- | ----- | ------ |
| Camera service | `cam:u`; init opens it then runs `CAMU_DriverInitialize` (command `0x39`); exit runs `CAMU_DriverFinalize` (`0x3A`) | `cam.c` |
| Y2R service | `y2r:u`; init runs `Y2RU_DriverInitialize`; exit `Y2RU_DriverFinalize` | `y2r.c` |
| `CAMU_SetReceiving` | Command `0x07`; passes the destination address, port, size and transfer unit plus the current-process handle; returns the completion event | `cam.c` |
| `Y2RU_SetConversionParams` | Command `0x29`; sends the 28-byte params struct in the command buffer | `y2r.c` |
| Y2R colour formula | `R = trunc(rgb_Y*Y + r_V*V + 0.75 + r_offset)`, `G = trunc(rgb_Y*Y - g_U*U - g_V*V + 0.75 + g_offset)`, `B = trunc(rgb_Y*Y + b_U*U + 0.75 + b_offset)`; coefficients 0-4 are unsigned 2.8 fixed point, offsets signed 11.5 | `y2r.h` |
| Y2R input line width / lines | width multiple of 8, up to 1024; lines multiple of 8 (required for `BLOCK_8_BY_8`) | `y2r.h` |
| Calibration | `CAMU_GetStereoCameraCalibrationData`, `CAMU_GetImageQualityCalibrationData`: structs in `cam.h`; the 3ds-examples leave using the stereo data as a TODO | `cam.h`, `camera/image` |
| Shutter sound | `CAMU_PlayShutterSound(SHUTTER_SOUND_TYPE_NORMAL/MOVIE/MOVIE_END)` | `cam.h` |

`cam.h` also exposes register-level calls (`CAMU_WriteRegisterI2c`, `CAMU_WriteMcuVariableI2c`) that write the camera sensor's registers; they are not covered here and can change hardware behaviour.

## Pitfalls

- **Waiting on the receive or conversion event without a timeout.** Symptom: the program hangs when the camera is busy or when the Y2R event is not delivered. Fix: wait with a timeout and treat expiry as failure (stop capture, release).
- **Forgetting `CAMU_Activate(SELECT_NONE)`.** Symptom: the camera stays powered or other programs cannot use it. Fix: release on every exit path, as in the example.
- **One-line `transfer_unit` for Y2R.** Symptom: the end event fires too early. Fix: 2, 4 or 8 lines (`y2r.h`).
- **Using a buffer after `camExit()` / `y2rExit()` or while a transfer is pending.** The hardware writes into your memory until the event fires or you stop; free the buffer only afterwards.
- **Reading a camera buffer without invalidating the cache.** Symptom: stale or partly old pixels `[hypothesis]`. Fix: invalidate a 32-byte aligned range after the event.
- **Assuming a frame arrives at the nominal rate.** Auto-exposure ranges slow the rate in the dark; schedule from the receive events, not from V-blank.

## Open questions

- Time from `CAMU_StartCapture` to the first usable frame (auto exposure convergence), and per-frame latency, on an Old 3DS and a New 3DS.
- Whether the invalidate in the cam example is required for correct pixels, and whether a buffer in the linear heap behaves differently from one in the ordinary heap.
- Y2R throughput for 320x240 and 400x240 images on an Old 3DS versus the CPU doing the same conversion, and the best `transfer_unit`.
- Which `PORT_*` the inner camera uses, and whether the inner and an outer camera can capture at the same time.
- Camera behaviour with the lid closed or during sleep.

## Verification status

| Item | Status |
| ---- | ------ |
| Both examples | Build with devkitARM r68-1 (GCC 16.1.0), libctru 2.7.1-1, `-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft -O2 -Wall -Wextra`, no warnings; linked with `3dsx.specs` |
| Old 3DS | not run |
| New 3DS | not run |
| Emulator | not run |

## Sources

- [libctru `cam.h`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/include/3ds/services/cam.h) and [`cam.c`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/source/services/cam.c) — enums, structs, init/exit, receiving command.
- [libctru `y2r.h`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/include/3ds/services/y2r.h) and [`y2r.c`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/source/services/y2r.c) — formats, formula, transfer rules and warnings.
- [3ds-examples `camera/`](https://github.com/devkitPro/3ds-examples/tree/be2001fee08cf8ec7d3366e095e83f6f75da8942/camera) — still-image and video capture sequence.
- [libctru `qtm.h`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/include/3ds/services/qtm.h) — the "camera in use" result code.
- [3dbrew: Camera Services](https://www.3dbrew.org/wiki/Camera_Services) — referenced for the port and context model; not re-read for this page.
