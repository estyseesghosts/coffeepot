# Controllers

The Switch 2's detachable controllers, called Joy-Con 2, are a revision of the original Switch's Joy-Con rather than a new accessory family. The internal parts and the wire protocol both changed, which is why original Switch controller software does not simply work.

## Where it fits

Input reaches a game through Horizon's input services and the runtime the game is running under. On a Switch 2 this may be a native title, a game running in [compatibility mode](../software/compatibility.md), or a program running inside another game's process; each sees a different set of capabilities. The hardware itself is described here; the display and storage side of the console is on the [devices page](devices.md).

## How it works

The Joy-Con 2 are identified on the bus by a new set of HID input report identifiers, and they carry their own radio, motion sensors and magnetometer. Unlike the original Joy-Con, they also support mouse input, whose exposure to software depends on the input API and execution environment.

## Technical details

### Hardware

[Reverse-engineered] Switchbrew documents the two controller board identifiers and their parts:

| Part | `BEE-JCL-MAIN-01` (left) | `BEE-JCR-MAIN-01` (right) |
| ---- | ------------------------- | -------------------------- |
| SoC | MediaTek MT3689BCA | MediaTek MT3689BCA |
| IMU | TDK InvenSense ICM-42670-P | TDK InvenSense ICM-42670-P |
| Compass | Asahi Kasei Microdevices AK09919C | Asahi Kasei Microdevices AK09919C |
| PMIC | Texas Instruments BQ25618 | Texas Instruments BQ25618 |
| Amplifier | Analog Devices MAX98388 | Analog Devices MAX98388 |
| NFC | — | NXP Semiconductors PN71602 |

The right-hand controller carries a near-field communication chip. The original Switch right Joy-Con also supports NFC.

### Input reports

[Reverse-engineered] ndeadly's controller research documents the HID input report identifiers the Switch 2 uses:

| Controller | Input report |
| ---------- | ------------ |
| Joy-Con 2 Left | `0x07` |
| Joy-Con 2 Right | `0x08` |
| Switch 2 Pro Controller | `0x09` |
| NSO GameCube Controller | `0x0A` |

The same research records feature flags for buttons, sticks, motion sensors, mouse data, rumble and magnetometer functionality. These flags are the practical way to ask what a given controller model can do, rather than assuming from the shape of the device.

### Safe mode

[Reverse-engineered] A Joy-Con 2 can be placed in a DFU-like mode with this sequence:

1. Hold Plus (right) or Minus (left).
2. Hold ZR (right) or ZL (left).
3. Hold Sync.
4. Release ZR or ZL while still holding Sync and Plus or Minus.
5. Release Plus or Minus while still holding Sync.
6. Release Sync.

Done correctly, the first and fourth player LEDs light up. In this mode the controller enumerates as a "Nintendo Safe Mode Device" when connected to a PC through the Charging Grip.

### Experimental access

[Reverse-engineered] The protocol documentation covers Bluetooth LE communication and pairing, HID commands and descriptors, input reports, controller flash and calibration memory layouts, and USB and Bluetooth packet captures.

Unlike most Switch 2 research, much of this can be studied with controllers attached to an ordinary computer. The `linux-switch2` project publishes a protocol document that separates established behavior from observations and unresolved fields, and Linux carries a `hid-nintendo` driver. That makes controller behavior the most accessible area of Switch 2 research to verify independently.

> Unknown: The relationship between the magnetometer data the controller reports and the compass calibration layout has not been characterised in the public sources this page draws on.

## Homebrew notes

Treat the controller model and its feature flags as data, not as a fixed contract. Ask the input system what a connected controller reports and adapt the interface rather than assuming mouse, motion or rumble are present, because the same runtime may be talking to an original Switch Joy-Con, a Switch 2 Joy-Con or a Pro Controller.

Handle disconnects and re-pairs explicitly: on a hybrid console a controller can be detached while the program is running, and a reconnect may present a different report identifier.

When the mouse mode is enabled, distinguish pointer motion from stick input in the input handling, and scale the interface accordingly; a pointer is not a substitute for an analogue stick and a UI built for one behaves badly with the other.

## Sources

- [SwitchBrew Joy-Con 2](https://switchbrew.org/wiki/Joy-Con_2) — board identifiers, component list and the safe-mode sequence.
- [ndeadly's Switch 2 controller research](https://github.com/ndeadly/switch2_controller_research) — protocol documents, input report identifiers, feature flags and packet captures.
- [linux-switch2 protocol notes](https://github.com/novakpetya/linux-switch2/blob/main/PROTOCOL.md) — an independent protocol implementation that separates established behaviour from observations.
- [Linux `hid-nintendo` driver](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-nintendo.c) — an independent implementation of the controller's HID behaviour.
