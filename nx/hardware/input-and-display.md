# NX Input and Display

You can play with controllers attached to the sides, held wirelessly, or as one wired pad — plus a touch screen in handheld mode. Games do not talk to the controller hardware directly; they read the latest button, stick and motion state from a shared snapshot the system refreshes continuously. The picture goes to the built-in screen when portable and to a TV through the dock, with sound from the built-in speakers, headphone jack or the TV.

## Where it fits

Buttons, sticks, motion and touch are collected by the [operating system](../software/os.md) HID service into a snapshot in shared [memory](memory.md) that the game program (each running program is a **[process](../../glossary.md#process)**) polls each frame. Finished pictures (each a **[framebuffer](../../glossary.md#framebuffer)**) produced by the [GPU](gpu.md) are handed to the display driver, which scans them out during the pause between screen refreshes (the **[vertical blank](../../glossary.md#vblank)**). Detachable controllers also carry a small touch-tag reader and motion sensors managed by their own firmware.

## How it works

Each frame the game reads buttons, analog sticks, gyroscope/accelerometer and touch from the HID snapshot instead of sending a request per button (cross-process requests are called **[IPC](../../glossary.md#ipc)**). Motion controls and the right-controller touch-tag reader appear as extra HID devices with their own calibration; rumble and LED patterns are commands sent back through the same service. Display mode — handheld resolution versus TV resolution — is picked from system-supported configurations negotiated with the dock, and the game resizes its framebuffers and render targets to match. Audio mixing is done in software and sent to the codec / HDMI sink; there is no separate music chip to program.

## Technical details

| Controller | Hardware inside | Notes |
| ---- | ----- | ----- |
| Joy-Con left (internal name Sakyo) | Broadcom BCM20734 SoC; STMicroelectronics LSM6DS3H motion sensor; TI BQ24072 power IC; 4 Mb Macronix flash. No separate microcontroller or NFC part is listed | [reverse-engineered] (SwitchBrew Joy-Con, "Hardware", oldid 13113) |
| Joy-Con right (Ukyo) | The same parts plus an STMicroelectronics **[microcontroller](../../glossary.md#mcu)** (listed as STM32P411) and an ST21NFCB NFC controller | [reverse-engineered] (same page) |
| Motion sensing | STMicroelectronics LSM6DS3H inertial module in each Joy-Con; the Pro Controller was not checked | [reverse-engineered] (SwitchBrew Joy-Con) |
| Touch-tag reader | NFC controller in the right Joy-Con only, for figures and cards | [reverse-engineered] (SwitchBrew Joy-Con; the NFC/NFP service pages were not saved) |

| HID path | Value |
| ---- | ----- |
| Delivery | Shared-memory snapshot, polled by the game; not one IPC per button per frame |
| Snapshot region size | 0x40000 bytes |
| Motion rate | Higher-rate sensor sampling decimated into the polled snapshot |

The 0x40000-byte shared-memory size is read from the code: `libnx` maps the HID shared-memory handle with `shmemLoadRemote(..., 0x40000, Perm_R)` [documented] (`libnx` `nx/source/services/hid.c` line 61). Polling the snapshot once per frame instead of sending IPC for each button is the documented design of `padUpdate` and the `PadState` API (`nx/include/switch/runtime/pad.h`). The SwitchBrew HID page was not saved.

| Output | Behavior |
| ---- | -------- |
| Handheld | Built-in touch display; lower GPU clock and render resolution typical |
| Docked | HDMI through the dock; higher GPU clock and render resolution typical; audio follows HDMI when present |
| Capture | System screenshot / video capture handled by the OS display service |

Exact panel and HDMI mode tables live with the OS display service; this page only records the handheld-versus-docked split. Programs learn which they are in through the applet service: `appletGetOperationMode()` returns `AppletOperationMode_Handheld` or `AppletOperationMode_Console` and an applet hook reports changes [documented] (`libnx` `nx/include/switch/services/applet.h`; see [performance and porting](../engineering/performance-and-porting.md)). The `deko_basic` example uses 1280x720, with 1920x1080 as the commented alternative [documented] (`switch-examples` `graphics/deko3d/deko_basic/source/main.c`).

Audio (brief): software-mixed PCM through the system audio service to speakers, 3.5 mm jack, USB audio or HDMI. No elaboration here — formats and latency belong with a future audio page.

## Homebrew notes

Use the libnx pad API: `padConfigureInput`, `padInitializeDefault`, then `padUpdate` once per frame and `padGetButtons` / `padGetButtonsDown` to read it (see [libnx and deko3d](../homebrew/deko3d-and-libnx.md)). The older `hidScanInput` / `hidKeysHeld` calls were removed in libnx 4.1.3. Whether the system's motion calibration is applied for you or must be applied by the game was not checked; the `sixaxis` example in `switch-examples` `hid/` is the place to read it. Expect different display sizes handheld versus docked — query the current mode and reallocate framebuffers on mode change.

## Sources

- SwitchBrew [Joy-Con](https://switchbrew.org/wiki/Joy-Con) (oldid 13113, edited 2025-04-26) — Sakyo and Ukyo hardware lists.
- [libnx](https://github.com/switchbrew/libnx) at `b156b4774fb2df7b93402169509266da7547fc4c` — `nx/source/services/hid.c` (shared-memory size), `nx/include/switch/runtime/pad.h`, `nx/include/switch/services/applet.h`.
- [switch-examples](https://github.com/switchbrew/switch-examples) at `669786898205b7beb25ff1731e72982e6d0397d3` — `hid/` examples, `deko_basic` resolution constants.
- SwitchBrew HID and display-services pages — referenced by earlier text; not saved with the research sources and not re-verified here.
