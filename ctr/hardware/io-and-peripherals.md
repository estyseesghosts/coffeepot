# CTR input/output and peripherals

The 3DS packs a lot of small hardware around its processors: buttons, a circle pad, a touchscreen, motion sensors, cameras, wireless, storage slots, and sliders for 3D depth and volume. Most of it is wired through a handful of serial buses, and many chores (power, LEDs, battery, the 3D slider) are handled by a tiny separate computer called the auxiliary microcontroller.

**New 3DS:** adds a second analog stick (the C-Stick), two extra shoulder buttons, an NFC reader, an infrared head-tracking LED and a microSD slot (instead of full-size SD), and removes the physical Wi-Fi switch.

## Where it fits

Peripheral chips hang off the [SoC](../../glossary.md#soc) over [I²C and SPI](../../glossary.md#i2c-spi) buses and a few FIFO-based storage links. The [ARM9](cpu.md) owns most of the hardwired security-related I/O; the ARM11 reaches nearly everything through [services](../software/services.md) (for example `hid:USER`, `i2c::MCU`, `cam:u`, `ptm:u`). Screens are covered on the [display page](display.md), sound on the [audio page](audio.md), and the cartridge slot on the [gamecard page](gamecard.md).

## How it works

### What the player can touch

- Digital buttons and a circle pad; sliders for 3D depth and volume; a Wi-Fi switch. **New 3DS:** adds the C-Stick and two extra shoulder buttons, and has no Wi-Fi switch.
- A resistive touchscreen on the bottom panel.
- A gyroscope and an accelerometer.
- One front camera and two rear cameras (the rear pair allow stereoscopic photos).
- An infrared transceiver for accessories (only one accessory at a time). **New 3DS:** also an NFC reader and an infrared LED for head tracking.
- SD slot, 3.5 mm headphone jack, game card slot. **New 3DS:** the card slot takes microSD.

### The auxiliary microcontroller (MCU)

The MCU is a small processor that runs its own firmware (the SoC talks to it over I²C; it uses the RL78 instruction set per 3dbrew, Copetti describes it as an NEC/Renesas 78K0R-family chip). It exclusively controls: LCD power, LED indicators, power management and the battery fuel gauge/charging, the Wi-Fi EEPROM, the real-time clock, the 3D slider, the Wi-Fi switch and the accelerometer (via its own I²C master bus). It also monitors the HOME button and the volume slider.

### Buses

| Bus | Devices |
| --- | ------- |
| SPI (four buses, mostly ARM9-accessible) | Game card save flash, DS power management, parts of Wi-Fi, touchscreen, sound codec, microphone, circle pad |
| I²C (SoC is master) | MCU, cameras (front also in DSi mode, one rear in DSi mode), LCDs, infrared transceiver, gyroscope (**New 3DS:** also NFC and the head-tracking module) |
| I²C (MCU is master) | Fuel gauge, accelerometer (slave address `0x18`), PMIC |
| HID block | Digital buttons, read through a 16-bit register on both ARM9 and ARM11 buses |
| High-speed FIFO links (about 16 MB/s) | Internal eMMC and the SD slot |

Some peripherals are wired twice: once the DS/DSi way (for backward compatibility) and once the 3DS way.

## Technical details

### I/O register blocks

Physical ranges (ARM11 user mapping: `physical = virtual − 0x1EC00000 + 0x10100000`; registers from `0x10200000` up, including the GPU and LCD, are not visible to the ARM9):

| Address | Block | Used by (selection) |
| ------- | ----- | ------------------- |
| `0x10000000` | CONFIG9 | Boot9, Process9 |
| `0x10001000` | ARM9 IRQ | |
| `0x10002000` | NDMA | AHB DMA engine |
| `0x10003000` | Timers | |
| `0x10004000`/`5000` | CTRCARD | Process9 (cartridge reader) |
| `0x10006000`/`7000` | eMMC (SD controllers 1 and 3) | Boot9, Process9 |
| `0x10008000` | PXI (ARM9 side) | |
| `0x10009000`, `0x1000A000`, `0x1000B000` | AES, SHA, RSA engines | See [security hardware](security-hardware.md) |
| `0x1000C000` | XDMA (DMA-330) | |
| `0x10011000` | PRNG | |
| `0x10012000` | OTP | Top secret; hidden after boot |
| `0x10018000` | ARM7 control | TWL/AGB firmware |
| `0x10102000` | Y2R (colour conversion) | Camera services |
| `0x10103000` | CSND | Sound |
| `0x10110000`/`1000` | LgyFb bottom/top | Legacy framebuffer |
| `0x10120000` | Cameras | |
| `0x10122000` | Wi-Fi SDIO | `nwm` services |
| `0x10130000`, `0x10131000`, `0x10132000` | MVD registers | New 3DS video decode block (MVD services); 3dbrew lists several MVD windows, see below |
| `0x10140000` | CONFIG11 | System configuration |
| `0x10141000` | PDN | Power management |
| `0x10142000`, `0x10143000`, `0x10160000` | SPI | |
| `0x10144000`, `0x10148000`, `0x10161000` | I²C | 3DS bus (MCU, cameras, LCD), gyro+IR, TWL bus |
| `0x10145000` | I2S | Sound lines |
| `0x10146000` | HID | Buttons |
| `0x10147000` | GPIO | |
| `0x10162000` | Microphone | |
| `0x10163000` | PXI (ARM11 side) | |
| `0x10164000` | NTRCARD | Cartridge |
| `0x10170000…` | DS-mode Wi-Fi | |
| `0x10200000` | CDMA | ARM11 |
| `0x10202000` | LCD registers | |
| `0x10203000` | DSP | |
| `0x10206000` | CDMA (**New 3DS** kernel's DMA-330 r1p2) | on the New 3DS the original CDMA at `0x10200000` is used only by the boot ROM |
| `0x10207000` | MVD registers | 3dbrew: "New 3DS only?" |
| `0x1020F000` | AXI interconnect (NIC-301) | |
| `0x10400000` | GPU | See [GPU](gpu.md) |

### New 3DS MVD

> **New 3DS:** the MVD exists only on the New 3DS; the Old 3DS has no such block.

The decoder block **MVD** performs H.264 and MJPEG decoding and YUV→RGB conversion, with its own DMA. 3dbrew's IO Registers page lists MVD register windows at `0x10130000`, `0x10131000`, `0x10132000` and `0x10207000`; which of them exist only on the New 3DS is not stated unambiguously there (the `0x10207000` entry is marked "New 3DS only?"). Per Copetti it is used only by the web browser for accelerated video.

> Conflict: MVD register windows. 3dbrew's IO Registers table lists `0x10130000` and `0x10131000` with a "No" in its availability column, `0x10132000` with "Yes" and `0x10207000` as "New 3DS only?"; the column meaning is not defined in the extracted page. Copetti describes MVD only as a New 3DS block. This page follows Copetti for "New 3DS only" and treats the individual windows as unclear. Unresolved: a per-window read from New and Old 3DS hardware would settle it.

> Conflict: MCU instruction set. 3dbrew's Hardware page says the MCU uses the RL78 ISA; Copetti describes an NEC/Renesas 78K0R-family chip. The 78K0R is the predecessor family of the RL78, so the two may describe the same part loosely. This page follows 3dbrew, which names the ISA directly, but no teardown or datasheet was checked. Unresolved.

### Wi-Fi

The Wi-Fi module is an Atheros AR6014 (Old 3DS; the source does not give the New 3DS part), the same family as the DSi's AR6002/AR6013 but with a different chip, so settings are synced across DS and 3DS modes at boot. The module has a small SPI flash that can also hold a boot image (see [boot process](../software/boot-process.md)).

### Board test points

3dbrew documents test points and a procedure for capturing the raw screen video of both LCDs (stub pages "TP Nets", "Video Capture"). They are mainly of interest for hardware modders.

## Homebrew notes

Homebrew reaches peripherals through the same services as retail software (`libctru` wraps `hid`, `cam`, `i2c` etc.). Direct register access needs kernel privilege ([exploits](../homebrew/exploits.md)). Hardware hacking uses the Wi-Fi flash FIRM boot path, the NAND pinout and bootrom glitching described in the [boot process](../software/boot-process.md) and [flaws](../homebrew/exploits.md#hardware-and-boot-rom).

## Open questions

- Many I/O blocks are listed with unknown contents (`?`).

## Sources

- [3dbrew: IO Registers](https://www.3dbrew.org/wiki/IO_Registers) — register block map.
- [3dbrew: Hardware](https://www.3dbrew.org/wiki/Hardware) — MCU details, I²C bus membership, Wi-Fi pinout, SDIO controller.
- [3dbrew: TP Nets](https://www.3dbrew.org/wiki/TP_Nets) and [Video Capture](https://www.3dbrew.org/wiki/Video_Capture) — test points.
- [Rodrigo Copetti, Nintendo 3DS Architecture: I/O](https://www.copetti.org/writings/consoles/nintendo-3ds/) — external interfaces, internal buses, MCU, New 3DS additions, MVD.
