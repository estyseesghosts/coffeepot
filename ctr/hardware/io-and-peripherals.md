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
- SD slot, 3.5 mm headphone jack, game card slot. **New 3DS:** the SD slot takes microSD, which cannot be changed without taking the back cover off (Copetti). The New 3DS head-tracking feature depends on the extra ARM11 core ([CPU](cpu.md#new-3ds-extra-cores)).

### The auxiliary microcontroller (MCU)

The MCU is a small processor that runs its own firmware (the SoC talks to it over I²C; it uses the RL78 instruction set per 3dbrew, Copetti describes it as an NEC/Renesas 78K0R-family chip). It exclusively controls (Copetti): LCD power, LED indicators, power management and the battery fuel gauge/charging, the Wi-Fi EEPROM, the real-time clock, the 3D slider, the Wi-Fi switch and the accelerometer (via its own I²C master bus). It also monitors the HOME and power buttons and the volume slider, which saves the main processors from polling (3dbrew lists the HOME button, Wi-Fi switch, 3D slider and volume slider; Copetti adds the power button). It also drives the LEDs and power supplies through its I²C link to the power-management chip (PMIC).

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

Physical ranges from 3dbrew's *IO Registers* page. The "Old 3DS" column is the page's own availability column (header `Old3DS`): `No` is read here as "not present on an Old 3DS", i.e. New 3DS only. "A9/A11" says which processor's bus reaches the block (`A9` = ARM9 only, `A11` = ARM11 only, `A9/A11` = both).

ARM11 user mapping: `physical = virtual - 0x1EC00000 + 0x10100000` (kernel mappings vary per build). Registers from `0x10200000` up, including the GPU and LCD, are not visible to the ARM9; registers below `0x10100000` are not visible to the ARM11 bus.

| Address | Block | Bus | Old 3DS | Used by | Notes |
| ------- | ----- | --- | ------- | ------- | ----- |
| `0x10000000` | CONFIG9 | A9 | Yes | Boot9, Process9 | |
| `0x10001000` | IRQ | A9 | Yes | Boot9, Process9, Kernel9 | ARM9 interrupt masking |
| `0x10002000` | NDMA | A9 | Yes | Boot9, Process9 | AHB DMA engine |
| `0x10003000` | TIMER | A9 | Yes | Boot9, Process9 | |
| `0x10004000` / `0x10005000` | CTRCARD | A9 | Yes | Process9 | Game card reader ([game cards](gamecard.md)) |
| `0x10006000` / `0x10007000` | EMMC | A9 | Yes | Boot9, Process9, NewKernel9Loader | SD(IO) controllers 1 and 3; controller 3 is normally mapped to the ARM11 |
| `0x10008000` | PXI | A9 | Yes | Boot9, Process9 | ARM9 side of the [PXI](../glossary.md#pxi) channel |
| `0x10009000` | AES | A9 | Yes | Boot9, Process9, NewKernel9Loader | [Security hardware](security-hardware.md) |
| `0x1000A000` | SHA | A9 | Yes | Boot9, Process9, NewKernel9Loader | |
| `0x1000B000` | RSA | A9 | Yes | Boot9, Process9 | |
| `0x1000C000` | XDMA | A9 | Yes | Boot9, Kernel9 | CoreLink DMA-330 r0p0, AXI bus master, two channels, 32-bit bus width |
| `0x1000D800` | SPICARD | A9 | Yes | Process9 | Game card save SPI |
| `0x10010000` | CONFIG | A9 | Yes | Process9 | |
| `0x10011000` | PRNG | A9 | Yes | Boot9, Process9 | Entropy source for seeding random-number generators |
| `0x10012000` | OTP | A9 | Yes | Boot9, Kernel9, NewKernel9Loader | "Top secret"; hidden after boot |
| `0x10018000` | ARM7 | A9 | Yes | TwlProcess9 | Sets up the ARM7 core for AGB/TWL mode |
| `0x10100000` | TMIO SD(IO) controller 3 | A11/A9 | Yes | | NWM references this controller but has no access to it |
| `0x10101000` | HASH | A11/A9 | Yes | Filesystem services | Same as SHA, except the FIFO is at `0x10301000` |
| `0x10102000` | Y2R | A11/A9 | Yes | Camera services | Colour conversion |
| `0x10103000` | CSND | A11/A9 | Yes | TwlBg, Codec, CSND and DSP services | Sound hardware ([audio](audio.md)) |
| `0x10110000` | LgyFb bottom screen | A11/A9 | Yes | TwlBg | Legacy output framebuffer and upscaling-filter setup |
| `0x10111000` | LgyFb top screen | A11/A9 | Yes | TwlBg | As above |
| `0x10120000` | Camera | A11/A9 | Yes | Camera services | |
| `0x10121000` | Camera | A11/A9 | Yes | Camera services | "Mirror of `0x10120000`?" |
| `0x10122000` | WIFI | A11/A9 | Yes | NWM services | Wi-Fi SDIO bus registers |
| `0x10123000` | ? | A11/A9 | Yes | NWM services | "WIFI?" |
| `0x10130000` | MVD | A11/A9 | No | MVD services | |
| `0x10131000` | MVD | A11/A9 | No | MVD services | |
| `0x10132000` | MVD | A11/A9 | Yes | MVD services | |
| `0x10140000` | CONFIG11 | A11/A9 | Yes | Process9, Boot11, Kernel11, TwlBg, DSP / SPI / NWM services (NWM until 11.4.x) | System configuration |
| `0x10141000` | PDN | A11/A9 | Yes | Process9, Boot11, Kernel11, TwlBg, Codec / NWM / SPI / PDN services | Power management |
| `0x10142000` | SPI | A11/A9 | Yes | TwlBg, SPI services | |
| `0x10143000` | SPI | A11/A9 | Yes | TwlBg, dmnt module | "Debugger related?" |
| `0x10144000` | I2C | A11/A9 | Yes | Boot11, Kernel11, TwlBg, I2C services | 3DS I2C interface (MCU, cameras, LCD) |
| `0x10145000` | I2S | A11/A9 | Yes | TwlBg, AgbBg, Codec services | Sound input/output lines |
| `0x10146000` | HID | A11/A9 | Yes | Boot9, Boot11, Kernel11, TwlBg, HID and dlp services | Digital buttons ("see PAD") |
| `0x10147000` | GPIO | A11/A9 | Yes | Boot11, TwlBg, GPIO services, DSP services (v0) | |
| `0x10148000` | I2C | A11/A9 | Yes | TwlBg, I2C services | 3DS I2C interface (gyroscope, IR) |
| `0x10160000` | SPI | A11/A9 | Yes | Boot9, TwlBg, SPI services | |
| `0x10161000` | I2C | A11/A9 | Yes | Boot11, TwlBg, I2C services | TWL I2C interface (MCU, cameras) |
| `0x10162000` | MIC | A11/A9 | Yes | MIC services | |
| `0x10163000` | PXI | A11/A9 | Yes | Boot11, Kernel11, TwlBg, PXI services | ARM11 side of PXI |
| `0x10164000` | NTRCARD | A11/A9 | Yes | Boot9, Process9 | DS-mode card reader |
| `0x10165000` | MP | A11/A9 | Yes | MP services | |
| `0x10170000`, `0x10171000` | MP | A11/A9 | Yes | MP services | NTR (DS) Wi-Fi registers, see GBATek; second is a mirror |
| `0x10172000`, `0x10173000` | ? | A11/A9 | Yes | ? | NTR Wi-Fi, unused? |
| `0x10174000`, `0x10175000` | MP / ? | A11/A9 | Yes | MP services (first) | NTR Wi-Fi RAM |
| `0x10176000`, `0x10177000` | MP | A11/A9 | Yes | ? | NTR Wi-Fi registers (mirrors) |
| `0x10178000`-`0x10180000` | MP | A11/A9 | Yes | MP services | NTR Wi-Fi WS1 region |
| `0x10200000` | CDMA | A11 | Yes | Boot11, Kernel11 | CoreLink DMA-330 r0p0, eight channels. **New 3DS:** used only by the boot ROM |
| `0x10201000` | FCRAM configuration | A11 | Yes | TwlBg, Kernel11 (dead code) | |
| `0x10202000` | LCD | A11 | Yes | TwlBg, Kernel11, GSP services | |
| `0x10203000` | DSP | A11 | Yes | DSP services | "See the DSi XpertTeak section in no$gba help" |
| `0x10204000` | ? | A11 | No | ? | |
| `0x10206000` | CDMA | A11 | No | NewKernel11 | DMA-330 r1p2, eight channels; the DMA engine the New 3DS ARM11 kernel actually uses |
| `0x10207000` | MVD | A11 | No | MVD services | "New 3DS only?" |
| `0x1020F000` | AXI | A11 | Yes | TwlBg, GSP services | CoreLink NIC-301 r1p2 |
| `0x10300000`-`0x10340000` | AHB (or AXI?) FIFOs | A11 | Yes | | Pages correspond to the devices at `0x10100000`-`0x10140000` but hold FIFOs, not registers: HASH FIFO at `0x10301000`, LgyFb scaler data FIFOs at `0x10310000` (top) and `0x10311000` (bottom). Needed for DMA |
| `0x10400000` | GPU | A11 | Yes | Boot11, Kernel11, GSP services | See [GPU](gpu.md) |

Rows marked `No` are not on the Old 3DS: the MVD windows at `0x10130000`, `0x10131000` and `0x10207000`, the DMA engine at `0x10206000` and the unknown block at `0x10204000`. **New 3DS:** these are present.

### New 3DS MVD

> **New 3DS:** the MVD exists only on the New 3DS; the Old 3DS has no such block.

The decoder block **MVD** performs H.264 and MJPEG decoding and YUV→RGB conversion, with its own DMA. 3dbrew's IO Registers page lists MVD register windows at `0x10130000`, `0x10131000`, `0x10132000` and `0x10207000`; its availability column marks all but `0x10132000` as absent on the Old 3DS (see the conflict below). Per Copetti it is used only by the web browser for accelerated video.

> Conflict: MVD register windows. 3dbrew's IO Registers table marks `0x10130000` and `0x10131000` as absent on the Old 3DS ("No" in its `Old3DS` column), but marks `0x10132000` as present ("Yes"), while describing `0x10207000` as "New 3DS only?". Copetti describes the MVD only as a New 3DS block. This page follows Copetti and the "No" rows for "New 3DS only" and treats `0x10132000` as unclear (the "Yes" may be an error on 3dbrew). Unresolved: a read of those windows on an Old 3DS and a New 3DS would settle it.

> Conflict: MCU instruction set. 3dbrew's Hardware page says the MCU uses the RL78 ISA; Copetti describes an NEC/Renesas 78K0R-family chip. The 78K0R is the predecessor family of the RL78, so the two may describe the same part loosely. This page follows 3dbrew, which names the ISA directly, but no teardown or datasheet was checked. Unresolved.

### Wi-Fi

The Wi-Fi module is an Atheros AR6014 (Old 3DS; the Foxconn J27H023.01 and AR6014G-AL1C variants appear in other columns of 3dbrew's table, see [models](models.md)). It is the "new" SDIO-attached module first used in the DSi (3dbrew describes it as based on the Atheros AR6002), with a separate proprietary DS-mode Wi-Fi path. The module has a small SPI flash that can also hold a boot image (see [boot process](../software/boot-process.md)).

3dbrew's Wi-Fi module pinout (a photographed board): the SDIO interface (CLK, CMD, D0 to D3) is marked red; the proprietary DS-mode Wi-Fi lines (pins unknown) yellow; an I²C EEPROM (SCL, SDA) blue; and the SPI flash (CLK, CS#, SI, SO, WP#, NC) purple. The flash's raw ID (command `0x9F`, RDID) reads `20 58` on one board and `62 62` on another (3dbrew's table; the model mapping is on [models](models.md)).

> Unknown: 3dbrew does not say which Wi-Fi module the New 3DS uses (the cell is `??`).

### NAND and SD controllers

3dbrew has NAND pinout photographs for the normal, XL, 2DS, New 3DS and New 3DS XL boards. Dumping the NAND over those pins "has been successful, but the image is encrypted" (see [NAND filesystem](../software/nand-filesystem.md)). The internal SDIO controller accepts SD cards up to 2.199 TB (32-bit sector number); Nintendo recommends cards up to 32 GB, and a 2 TB card has been tested. Larger cards lengthen boot time (measured numbers on [models](models.md#capacity-notes)). Format cards as FAT32 with a 64 KB unit for the best compatibility.

### Board test points

3dbrew documents test points (stub page "TP Nets") and a procedure for tapping the lower screen's video signal, covered on [video capture](video-capture.md). They are mainly of interest for hardware modders.

## Homebrew notes

Homebrew reaches peripherals through the same services as retail software (`libctru` wraps `hid`, `cam`, `i2c` etc.). Direct register access needs kernel privilege ([exploits](../homebrew/exploits.md)). Hardware hacking uses the Wi-Fi flash FIRM boot path, the NAND pinout and bootrom glitching described in the [boot process](../software/boot-process.md) and [flaws](../homebrew/exploits.md#hardware-and-boot-rom).

## Open questions

- Many I/O blocks are listed with unknown contents (`?`).

## Sources

- [3dbrew: IO Registers](https://www.3dbrew.org/wiki/IO_Registers) — register block map.
- [3dbrew: Hardware](https://www.3dbrew.org/wiki/Hardware) — MCU details, I²C bus membership, Wi-Fi pinout, SDIO controller, NAND pinouts.
- [3dbrew: TP Nets](https://www.3dbrew.org/wiki/TP_Nets) and [Video Capture](https://www.3dbrew.org/wiki/Video_Capture) — test points; see [video capture](video-capture.md).
- [Rodrigo Copetti, Nintendo 3DS Architecture: I/O](https://www.copetti.org/writings/consoles/nintendo-3ds/) — external interfaces, internal buses, MCU, New 3DS additions, MVD.

See also [board test points](tp-nets.md) for measured pads on the Old 3DS board and [screen video capture points](video-capture.md).
