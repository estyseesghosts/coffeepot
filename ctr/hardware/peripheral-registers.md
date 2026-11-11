# Peripheral control registers

The 3DS has hardware registers for reading buttons, controlling input/output pins, communicating with peripheral chips and managing power. This page is a low-level reference for system programmers and emulator authors. Ordinary homebrew uses operating-system services and libraries to access these devices; the physical addresses below are not portable user-mode pointers or a substitute for those services.

## Where it fits

The register blocks sit alongside the other devices in the [input/output map](io-and-peripherals.md#io-register-blocks). The [kernel and services](../software/services.md) mediate access, and practical input handling is covered by [input and sensors](../engineering/input-and-sensors.md). See [system information and power](../engineering/system-info-and-power.md) and [application lifecycle](../engineering/application-lifecycle.md) for supported application-level behavior.

The addresses in this page are **physical addresses** (locations in hardware memory, before [address translation](../../glossary.md#mmu)); widths are in bytes. Bus visibility does not imply a game process has permission or a mapping to use a register. The tables describe Old 3DS; differences and New 3DS-only devices are stated explicitly. The saved 3dbrew revisions do not state the model and firmware used for each individual finding, so descriptions are `[reverse-engineered: 3dbrew]`, not measurements by this wiki.

## How it works

The button block exposes active-low digital inputs: a pressed button clears its bit. General-purpose input/output (**GPIO**) pins expose device signals separately. The inter-integrated circuit (**[I²C](../../glossary.md#i2c-spi)**) controllers exchange bytes with external devices such as the auxiliary microcontroller; the power-management block tracks wake sources and controls clocks, resets and legacy-mode interfaces. These raw interfaces are distinct from the higher-level operating-system APIs.

## Technical details

### Digital buttons

3dbrew's HID Registers page lists these 16-bit registers on the Old 3DS `[reverse-engineered: 3dbrew HID Registers]`:

| Address | Register | Function |
| ------- | -------- | -------- |
| `0x10146000` | `HID_PAD` | Active-low button state: `0` means pressed, `1` released. |
| `0x10146002` | `HID_PAD_CNT` | Button selection and interrupt control, used by `TwlBg`. |

For both registers, button bits 0–11 are A, B, Select, Start, Right, Left, Up, Down, R, L, X and Y, respectively. In `HID_PAD_CNT`, bits 12–13 are unused, bit 14 enables interrupt `0x5B`, and bit 15 selects the OR (`0`) or AND (`1`) condition for selected buttons. This description does not assign meanings to the remaining `HID_PAD` bits. **New 3DS:** C-Stick and ZL/ZR are exposed separately through the input services; see [input and sensors](../engineering/input-and-sensors.md).

### General-purpose input/output

GPIO1, GPIO2 and GPIO3 begin at `0x10147000`, `0x10147010` and `0x10147020` respectively. The table describes the saved 3dbrew GPIO revision `[reverse-engineered: 3dbrew GPIO Registers]`:

| Address | Width | Register | Meaning |
| ------- | ----- | -------- | ------- |
| `0x10147000` | 1 | `GPIO1_DATA` | Pin values. |
| `0x10147010` | 1 | `GPIO2_DATA` | Pin values. |
| `0x10147011` | 1 | `GPIO2_DIR` | Direction per pin: `0` input, `1` output. |
| `0x10147012` | 1 | `GPIO2_INTCFG` | Edge per pin: `0` falling, `1` rising. |
| `0x10147013` | 1 | `GPIO2_INTEN` | Interrupt enabled per pin when set. |
| `0x10147014` | 2 | `GPIO2_DATA2` | Extra pin; not interrupt-configurable. |
| `0x10147020` | 2 | `GPIO3_DATA` | Pin values. |
| `0x10147022` | 2 | `GPIO3_DIR` | Direction, same encoding as GPIO2. |
| `0x10147024` | 2 | `GPIO3_INTCFG` | Edge, same encoding as GPIO2. |
| `0x10147026` | 2 | `GPIO3_INTEN` | Interrupt enables, same encoding as GPIO2. |
| `0x10147028` | 2 | `GPIO3_DATA2` | Extra pin; not interrupt-configurable. |

3dbrew identifies signals for the touch screen, shell, headphones, gyroscope, infrared and microcontroller, but marks several pin functions as uncertain. Only GPIO2/3 pin interrupts are configurable. Its saved page says active-low pins should use falling-edge interrupts and outputs should not have interrupts enabled; this is hardware reference information, not an instruction for applications to reconfigure pins.

The adjacent legacy real-time clock (**RTC**) block includes `RTC_CNT` at `0x10147100` (2 bytes) and time registers at `0x10147120` and `0x10147124`. The saved GPIO page describes byte-wise bit-swapped time values and labels some register widths uncertain. Consult it for the detailed latch and alarm fields rather than assuming a normal byte order `[reverse-engineered: 3dbrew GPIO Registers]`.

### I²C controllers and devices

Three controllers share a layout at different bases `[reverse-engineered: 3dbrew I2C Registers]`:

| Controller | Base physical address |
| ---------- | --------------------- |
| I²C1 | `0x10161000` |
| I²C2 | `0x10144000` |
| I²C3 | `0x10148000` |

At each base, `DATA` is an 8-bit register at `+0`, `CNT` an 8-bit register at `+1`, `CNTEX` a 16-bit register at `+2`, and `SCL` a 16-bit register at `+4`. In `CNT`, bit 0 marks stop/last byte, bit 1 start/first byte, bit 2 pause after error, bit 4 acknowledge status/control, bit 5 read (`1`) or write (`0`), bit 6 interrupt enable, and bit 7 start/busy. For bit 4 the source distinguishes a write in a data-read operation from a read in a data-write operation. `CNTEX` bits 0–1 and `SCL` fields have undocumented meanings; 3dbrew gives typical values but does not explain them.

The 3dbrew device table lists **8-bit write address bytes**, not 7-bit device addresses: the read address sets bit 0 and the 7-bit address is the listed value shifted right by one. For example, its bus 2 microcontroller entry uses write byte `0x4A`, corresponding to 7-bit address `0x25`. The same byte can appear on different buses; include the bus in any device identification. The saved table says bus 3 gyroscope device 9 changed from write byte `0xA6` to `0xD6` with system version 8.0.0-18. **New 3DS:** the NFC device (bus 2, `0xEE`) and QTM head-tracking device (bus 1, `0x40`) are New 3DS-only; device 17 (`0x54`, bus 3) relates to additional New 3DS input, with support differing by firmware. These bytes are examples of the source's notation, not application-level service IDs.

The auxiliary microcontroller's device-3 register map includes raw 3D-slider byte `0x08`, volume-slider byte `0x09`, battery temperature `0x0A`, integer and fractional battery percentages `0x0B`/`0x0C`, and a system-voltage reading `0x0D`. The source interprets percentage as `reg[0x0B] + reg[0x0C]/256` and voltage as `reg[0x0D]*5/256` volts, with measured load-side voltage lower than a direct battery measurement; the source does not specify model and test conditions. Treat these as 3dbrew's reverse-engineered interpretation, not a calibrated measurement on this wiki. Use the relevant supported services for applications.

### Sleep, wake and legacy input

| Physical address | Width | Register | Documented source interpretation |
| ---------------- | ----- | -------- | -------------------------------- |
| `0x10141000` | 2 | `PDN_SLEEP_CNT` | Bit 0 enters sleep; bit 15 reports a VRAM low-power state, with power-down versus self-refresh uncertain. |
| `0x10141008` | 4 | `PDN_WAKE_ENABLE` | Bitmask of enabled wake conditions. |
| `0x1014100C` | 4 | `PDN_WAKE_REASON` | Same layout, recording wake conditions; acknowledgement semantics are marked uncertain. |
| `0x10141100` | 2 | `LGY_MODE` | Legacy-mode state and enable. |
| `0x1014110A` | 2 | `LGY_PAD_CNT` | Legacy key condition; copied to `HID_PAD_CNT` when GBA mode sleeps. |
| `0x10141110` | 2 | `LGY_HIDEMU_MASK` | Selects button bits to override for the legacy CPU. |
| `0x10141112` | 2 | `LGY_HIDEMU_PAD` | Replacement active-low button values for selected bits. |
| `0x10141200` | 4 | `PDN_GPU_CNT` | Reset controls and clock enable; bit 16 enables clocks for the described GPU blocks, VRAM and LCD. |
| `0x10141210` | 2 | `PDN_FCRAM_CNT` | Bit 0 reset, bit 1 clock request, bit 2 clock acknowledgement. |
| `0x10141230` | 1 | `PDN_DSP_CNT` | Bit 0 reset and bit 1 clock enable. |

For the wake masks, 3dbrew lists bit 1 as the button condition, bit 3 shell open, bit 4 headphones not plugged in, bit 26 microcontroller interrupt, bit 30 touch and bit 31 game-card insert/removal. It marks Wi-Fi bit 8 and shell GPIO bit 19 as tentative and says the list is incomplete. The page describes selected wake conditions as ORed and raising interrupt `0x58` `[reverse-engineered: 3dbrew PDN Registers]`.

For legacy input, selected mask bits substitute the corresponding value in `LGY_HIDEMU_PAD`; unselected bits continue to reflect `HID_PAD`. 3dbrew describes this mechanism being used to prevent menu button presses from reaching a DS/GBA program. The GPIO and card-detect override pairs are separate (`0x10141114`/`0x10141116` and `0x10141118`/`0x10141119`). Do not conflate the legacy guest's overridden view with the physical input register.

**New 3DS:** 3dbrew lists `PDN_MVD_CNT` at `0x10141240` and extra SoC/power controls at `0x10141300`, `0x10141304` and `0x10141310`–`0x10141313`; they are not Old 3DS controls. Its saved page says the SoC-mode register reads zero on Old 3DS. See [New 3DS extras](../engineering/new-3ds-qtm-and-mvd.md) and [CPU](cpu.md#new-3ds-extra-cores).

## Homebrew notes

Use the [input service](../engineering/input-and-sensors.md), [system-information APIs](../engineering/system-info-and-power.md) and [application lifecycle](../engineering/application-lifecycle.md) for normal homebrew. Direct access to these registers depends on privileged mappings, and their reset, clock or power effects can disrupt the running system. The source does not provide a complete per-firmware mapping, register-availability matrix or a safe generic direct-access procedure.

## Sources

- [3dbrew: HID Registers, revision 21444](https://www.3dbrew.org/w/index.php?title=HID_Registers&oldid=21444) — button bits and interrupt control.
- [3dbrew: GPIO Registers, revision 21467](https://www.3dbrew.org/w/index.php?title=GPIO_Registers&oldid=21467) — GPIO registers, pin functions and legacy RTC.
- [3dbrew: I2C Registers, revision 23702](https://www.3dbrew.org/w/index.php?title=I2C_Registers&oldid=23702) — controller fields, device table and microcontroller register descriptions.
- [3dbrew: PDN Registers, revision 21471](https://www.3dbrew.org/w/index.php?title=PDN_Registers&oldid=21471) — sleep, wake, legacy input and hardware clock/reset controls.
