# NX Input and Display

You can play with controllers attached to the sides, held wirelessly, or as one wired pad — plus a touch screen in handheld mode. Games do not talk to the controller hardware directly; they read the latest button, stick and motion state from a shared snapshot the system refreshes continuously. The picture goes to the built-in screen when portable and to a TV through the dock, with sound from the built-in speakers, headphone jack or the TV.

## Where it fits

Buttons, sticks, motion and touch are collected by the [operating system](../software/os.md) HID service into a snapshot in shared [memory](memory.md) that the game program (each running program is a **[process](../../glossary.md#process)**) polls each frame. Finished pictures (each a **[framebuffer](../../glossary.md#framebuffer)**) produced by the [GPU](gpu.md) are handed to the display driver, which scans them out during the pause between screen refreshes (the **[vertical blank](../../glossary.md#vblank)**). Detachable controllers also carry a small touch-tag reader and motion sensors managed by their own firmware.

## How it works

Each frame the game reads buttons, analog sticks, gyroscope/accelerometer and touch from the HID snapshot instead of sending a request per button (cross-process requests are called **[IPC](../../glossary.md#ipc)**). Motion controls and the right-controller touch-tag reader appear as extra HID devices with their own calibration; rumble and LED patterns are commands sent back through the same service. Display mode — handheld resolution versus TV resolution — is picked from system-supported configurations negotiated with the dock, and the game resizes its framebuffers and render targets to match. Audio mixing is done in software and sent to the codec / HDMI sink; there is no separate music chip to program.

## Technical details

| Controller | Hardware inside | Notes |
| ---- | ----- | ----- |
| Joy-Con left / right | Broadcom BCM20734 Bluetooth radio; STMicro STM32 **[microcontroller](../../glossary.md#mcu)**; laser-trimmed sticks | [reverse-engineered] (SwitchBrew Joy-Con hardware notes; community teardowns) |
| Motion sensing | STMicro LSM6DS3H inertial module (gyro + accelerometer) in each Joy-Con / Pro Controller | [reverse-engineered] (SwitchBrew HID/Joy-Con notes; driver code) |
| Touch-tag reader | NFC reader in the right Joy-Con for figures and cards | [reverse-engineered] (SwitchBrew NFC / NFP service notes) |

| HID path | Value |
| ---- | ----- |
| Delivery | Shared-memory snapshot, polled by the game; not one IPC per button per frame |
| Snapshot region size | 0x40000 bytes |
| Motion rate | Higher-rate sensor sampling decimated into the polled snapshot |

The 0x40000-byte shared-memory size and poll-don't-call-per-frame behavior are [reverse-engineered] (SwitchBrew HID notes; libnx `hid` implementation).

| Output | Behavior |
| ---- | -------- |
| Handheld | Built-in touch display; lower GPU clock and render resolution typical |
| Docked | HDMI through the dock; higher GPU clock and render resolution typical; audio follows HDMI when present |
| Capture | System screenshot / video capture handled by the OS display service |

Exact panel and HDMI mode tables live with the OS display service; this page only records the handheld-vs-docked split and that games pick from system-offered modes [reverse-engineered] (SwitchBrew display / capture notes).

Audio (brief): software-mixed PCM through the system audio service to speakers, 3.5 mm jack, USB audio or HDMI. No elaboration here — formats and latency belong with a future audio page.

## Homebrew notes

Use libnx `hid` polling (`hidScanInput`, shared-memory reads) every frame; never open an IPC session per button, and always apply the system motion calibration rather than raw sensor values. Expect different display sizes handheld versus docked — query the current mode and reallocate framebuffers on mode change.

## Sources

- [SwitchBrew — HID / shared memory](https://switchbrew.org/wiki/HID) — 0x40000-byte polled snapshot, device map, motion handling.
- [SwitchBrew — Joy-Con](https://switchbrew.org/wiki/Joy-Con) — BCM20734, STM32 microcontroller, LSM6DS3H motion module, right-controller NFC reader.
- [SwitchBrew — Display / capture services](https://switchbrew.org/wiki/Display_services) — handheld vs. docked modes, framebuffer handoff, system capture.
- [libnx — HID input API](https://github.com/switchbrew/libnx) — polling model and calibration helpers for homebrew (`hid.c`).
