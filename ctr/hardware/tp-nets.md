# CTR board test points (TP nets)

The 3DS main board has numbered solder pads called test points (TPs). Each is wired to one electrical signal such as a power rail, a button line, the headphone output or the NAND data lines. 3dbrew measured many of them with an oscilloscope and recorded the voltage, signal type and frequency. This is hardware-modding material; the [I/O and peripherals](io-and-peripherals.md) page links to it too.

## Behavior

A test point lets you probe or tap a signal without desoldering chips, for example to read button states, capture the screen signal ([display](display.md)) or reach the NAND lines. The table lists only what was measured; many pads are unidentified.

## Technical details

3dbrew measured the signals with a PicoScope 2206B at least at 2 MS/s, plus or minus 5 V, 8-bit, 1:10, DC, referenced to the square GND pad at the bottom left. The measurements are for the **Old 3DS**; 3dbrew's page has no New 3DS section.

> Unknown: pad numbers and signals on a New 3DS board are not documented by the source.

Legend. Presence: S = static (always), O = device on, A = active (not standby), R = when required. Type: A analogue, D digital, P power, G ground; driver: PP push-pull, OC open collector, PU/PD pull up/down; waveform: R rectangular, T triangular.

| TP | Net | Voltage | Presence / type | Domain | Frequency | Notes |
| -- | --- | ------- | --------------- | ------ | --------- | ----- |
| 1 | GND | 0 V | S / G | Charger | | |
| 2 | +1.2V | 1.2 V | O / P | ? | | |
| 3, 6 | +2.8V | 2.8 V | O / P | ? | | |
| 4 | Uchrg | 3-5 V | S / P | Charger port | | |
| 5, 7, 10, 11 | +1.8V | 1.8 V | O or S / P | ? | | |
| 8 | Ubat | 3.5-3.8 V | S / P | Battery | | |
| 9 | +3.9V | 3.6-3.9 V | S / P | ? | | 3.9 V off, about 3.6 V active |
| 12 | +4V | 3.7-4 V | O / P | ? | | 4 V standby, about 3.75 V active |
| 41 / 42 | Bottom screen lights source / sink | 10 V / 0.2 V | A / P | ? | | |
| 46 | Enable? Supply? | 3.2 V | A / ? | ? | | High when displays active, low during boot |
| 48-51 | Headphone GND / plug detect / left / right | 2.2 Vpp (48, 50, 51), 1 V (49) | O or A | Headphone | about 33 kHz (48) | Left and right referenced to TP48, AC |
| 58 | WLAN button | 1.8 V | O / OC | Side buttons | | |
| 67, 70 | System LED blue / red low-side driver | 2.2-3 V | O / P+T | LEDs | 0 Hz, 2 kHz | Static when active, PWM in standby (70 uncertain) |
| 68, 69 | Circle pad Y / X | 0.2-1.5 V | A / A | Circle pad | | Y: up above down; X: left above right |
| 80-92 | START, SELECT, R, L, DPAD-U, X, DPAD-L, Y, A, DPAD-R, DPAD-D, B (84 is a +1.8V rail) | 1.8 V | O / OC | Main buttons | | |
| 96 | +3.2V | 3.2 V | O / P? | ? | | |
| 131 / 132 | Top / bottom screen PWM | 1.8 V | A / D | ? | 1166.7 kHz | Duty cycle 2.7%-25% (top), 5%-54% (bottom), darkest to brightest |
| 144 / 146 | IR I2C SCL / SDA | 3.2 Vp | R / OC | IR I2C | 390 kHz | Address `0x9A` write, `0x9B` read |
| 149 / 150 | ? | 2 Vp | A | ? | 47.6 kHz / 1.52 MHz | Perhaps I2S word select / clock |
| 152 | ? | 1.8 Vp | A / D | ? | 16.7 MHz | Perhaps a clock |
| 189-191 | VIDEO-CLK / HSYNC / VSYNC | 1.8 Vp | A / D | Video signal | 11.1 MHz / 24.77 kHz / about 59.8 Hz | At the Home screen |
| 213-216 | TOUCH-Y?/X? | 0 or 2 V, 2 Vp | O | Touch | | Labels uncertain |
| 249-253 (254?) | NAND-DAT0..3, NAND-CMD (NAND-CLK?) | | O? / PP? | NAND | | Related to the [NAND](../software/nand-filesystem.md) |

Unlisted numbers (gaps in the table) are not documented. TP93 and TP94 are static 0 V, possibly ground, digital or analogue.

## Homebrew notes

Not software-accessible; useful for hardware reading of NAND and capturing screen video (3dbrew's Video Capture page, out of this source set).

## Open questions

- Domains marked `?` and the New 3DS equivalents are unknown.

## Sources

- [3dbrew: TP Nets](https://www.3dbrew.org/wiki/TP_Nets) — all measurements above.
