# CTR displays

The 3DS has two screens. The bottom one is an ordinary 320×240 touchscreen. The top one is special: it can show a separate image to each eye without glasses, which is what gives the console its name. For stereoscopic output, games normally provide left- and right-eye images from slightly different viewpoints. With stereo disabled, the upper screen normally displays one image; rendering work can be shared between eye views, so stereo does not necessarily double the total frame cost.

On top of that, DS and Game Boy Advance games reach the same screens through a separate "legacy" path, because their graphics chips work completely differently from the 3DS [GPU](gpu.md).

## Where it fits

The [GPU](gpu.md) renders images into memory; the display controllers (called **[PDC](../glossary.md#pdc)** in the register map) read finished *LCD framebuffers* out of [memory](memory.md) and feed them to the screens line by line. The [microcontroller](io-and-peripherals.md#the-auxiliary-microcontroller-mcu) controls LCD power and backlight; the 3D slider is read by the same MCU.

## How it works

### The 3D screen

The top panel is physically 800×240 pixels, but each pixel is half-width. A **parallax barrier** (a layer of opaque slits in front of the backlight) makes the left eye see every other pixel column and the right eye see the rest.

- **3D mode:** all 800 columns are used as two 400×240 images, one per eye.
- **2D mode:** pairs of columns are treated as single pixels, showing one 400×240 image.
- The panel supports a single 800×240 image in wide mode. Copetti reports no commercial games using that mode; homebrew can use it [reported: Copetti, 3DS Architecture; date and test models unspecified].

The barrier needs extra brightness (shorter battery life), and the effect only works if the player holds the console square to their face. A slider ("3D depth slider") scales how different the two views are. **New 3DS:** a front camera and infrared LED track the player's head to keep the image aligned (this takes one CPU core; see [CPU](cpu.md) and the next section). The 2DS and New 2DS XL have no 3D screen at all.

### Head tracking and "Super Stable 3D" (New 3DS)

> **New 3DS:** this section applies only to the New 3DS (and not to the New 2DS XL, which has no 3D screen).

A system service called **[QTM](../glossary.md#qtm)** moves the parallax barrier to follow the viewer. `libctru` documents it (QTM services `qtm:u`, `qtm:s`, `qtm:c`):

- The inner camera samples at 320×240 and 30 frames per second; the gyroscope predicts the eye positions between camera samples, effectively doubling the tracking rate. An infrared LED lights the scene in low light.
- The barrier is a pattern of 12 positions per interocular-distance unit, set through a TI TCA6416A I²C-to-parallel expander; its pattern and polarity are dithered by software roughly every 10 ms (one gyroscope sample).
- Nintendo's "Super Stable 3D" works out the midpoint of the viewer's eyes in camera pixels, rotates it by a calibrated angle, converts it to millimetres of horizontal deviation using the camera's field of view and the calibrated eye-to-screen distance, and shifts the barrier by that amount (rounded with a ±0.01 bias to avoid flicker). All calculations assume a 62 mm eye separation. It is not applied in 2D mode.
- QTM is absent on the Old 3DS and may be unavailable on the New 2DS XL (no 3D screen), so programs must check that the services exist before calling them.

### Framebuffers and swapping

A frame travels through three kinds of buffer: the GPU render target, the *LCD framebuffer* (a copy in the format and orientation the panel wants) and the display controller. Games usually keep two LCD framebuffers per screen. While the panel scans one, the GPU/transfer engine fills the other; the swap is triggered at [V-blank](../../glossary.md#vblank) to avoid tearing.

### DS and GBA games

The DS/GBA picture-processing units live in the SoC. Their scanlines go to the **[LgyFb](../glossary.md#lgyfb)** block (optionally upscaled) and then to the framebuffer, with the ARM11 and its DMA moving data. This adds a small, negligible delay.

## Technical details

### Screen summary

| Screen | Native size | Notes |
| ------ | ----------- | ----- |
| Top | 800×240 physical; 400×240 usable per eye | Parallax barrier on the Nintendo 3DS and 3DS XL. **New 3DS:** also on the New Nintendo 3DS and 3DS XL. The 2DS and New 2DS XL have a non-3D top panel |
| Bottom | 320×240 | Resistive touch overlay |

Panel sizes (top / bottom): Nintendo 3DS 3.53 in / 3.00 in; 3DS XL 4.88 / 4.18; 2DS 3.53 and 3.00 cropped from a single panel. **New 3DS:** New Nintendo 3DS 3.88 / 3.33; New Nintendo 3DS XL 4.88 / 4.18. See [models](models.md).

Frame timing: the 3DS refresh is derived from the pixel clock; one documented timing is (268111856 ÷ 24) ÷ (450+1) ÷ (494+1) ≈ 50.04 Hz when `VTotal` is set to 494.

### LCD framebuffer registers

Top-screen registers start at physical `0x10400400`, bottom at `0x10400500`. All must be accessed with 32-bit operations. The values describe the panel's physical orientation (the first pixel is top-left when the console is held with the bottom screen in the left hand).

| Offset | Name | Meaning |
| ------ | ---- | ------- |
| `0x00` | HTotal | Total scanline width; HClock = PClock ÷ (HTotal+1) |
| `0x04`, `0x0C`, `0x10`, `0x14`, `0x18`, `0x60` | H timing | Image start, front porch, HSync, back porch, left border, display start/end |
| `0x1C`, `0x40` | H/V interrupt timing | Low u16 asserts, high u16 deasserts; GSP sets the H interrupt so it never fires |
| `0x24` | VTotal | VClock = PClock ÷ (HTotal+1) ÷ (VTotal+1) |
| `0x4C` | Overscan colour | 24-bit fill colour for areas outside the image |
| `0x50`, `0x54` | HCount, VCount | Beam position counters |
| `0x68`, `0x6C` | Framebuffer A (first, second) | Top screen: left-eye buffers |
| `0x70` | Format | Bits 2–0 colour format; 5–4 interlace mode; 6 alternative pixel mode; 9–8 DMA burst size (0 = 32 B, 1 = 64 B, 2 = 128 B; 3 unsupported, FCRAM bursts at most 128 B and the screen goes black) |
| `0x74` | PDC control | Bit 0 enable; bits 8/9/10 mask H-blank/V-blank/error IRQs; 16 output enable |
| `0x78` | Select/status | Bit 0 next framebuffer; 4 current (3dbrew: "?"); 8 reset FIFO (?); 16/17/18 H-blank/V-blank/error IRQ status, write 1 to acknowledge |
| `0x80`, `0x84` | Colour lookup table | Index write-only, auto-increment; RGB8 `0x00BBGGRR` |
| `0x90` | Stride | Bytes between rows (multiple of 8); zero repeats the first row; negative flips vertically |
| `0x94`, `0x98` | Framebuffer B (first, second) | Top screen: right-eye buffers |

**Colour formats** (bits 2–0): 0 `RGBA8`, 1 `RGB8`, 2 `RGB565`, 3 `RGB5A1`, 4 `RGBA4`. Components are stored most-significant-bit first with little-endian pixels. Formats 5–7 are blocked by the GSP service but behave as pixel-doubled `RGBA8` outside userland.

**Interlace modes** (bits 5–4): 0 A (none), 1 AA (scanline doubling), 2 AB (interleave A and B), 3 BA. The bottom screen always uses mode A; the top screen uses AB in 3D mode and A in 2D mode. The GSP service enables the parallax barrier only when interlacing bit 5 = 1 and alternative-mode bit 6 = 0.

#### Timing registers in detail

All pixel and scanline timing fields are 12 bits (each half of a two-field register too) `[reverse-engineered: 3dbrew, authors not stated]`. The horizontal events fire when `HCount` equals the register value, in this order (values may wrap through `HTotal`): `0x10 < 0x14 <= 0x60.low <= 0x04 <= 0x60.high <= 0x08 <= 0x0C <= 0x10`, and `0x18 <= 0x60.low`. Each match latches the controller into a new mode; if two events coincide, a fixed latch order decides. The modes, in order: HSync (moves the LCD to the next line); back porch (no pixels, at least 16 pixel clocks or the picture glitches); left border (solid colour); image (pixel data fetched by DMA from video memory or main RAM); right border / image end; an unknown synchronisation mode that seems broken; front porch (no pixels, at least 68 clocks or the screen loses sync).

| Offset | 3dbrew name | Notes |
| ------ | ----------- | ----- |
| `0x04` | HStart | when the image starts in the visible region (register `0x60`) |
| `0x08` | HBR | right-border start (?); no visible effect but must hold a valid value |
| `0x0C` | HPF | front porch: image blanked, no pixels pushed; a single red dot is shown just before entering it (cause unknown) |
| `0x10` | HSync | the sync pulse; must last at least one pixel clock for the LCD to register it |
| `0x14` | HPB | back porch (?); must be at least one larger than HSync or HSync never triggers; display blank, nothing pushed |
| `0x18` | HBL | left-border trigger; enables pixel pushing. If smaller than the back porch the back porch has zero length. May be lower than HSync. Must be <= the low half of `0x60`, else no pixels are pushed |
| `0x1C` | H interrupt timing | low u16 asserts the PDC interrupt line when `HCount` equals it, the high u16 probably deasserts it. Low must be smaller than high; if low < `HTotal` then high must also be < `HTotal`; low >= `HTotal` disables the interrupt. GSP sets low = `HTotal`, so the H-sync interrupt never fires |
| `0x20` | ? | two unknown u16 fields |
| `0x28` | ? | seems to set the vertical blanking interval: lower than `VTotal - VDisp` cuts off the top lines; higher pushes the image down and shows the overscan colour; higher than `HTotal` makes the GPU skip vertical synchronisation of pixel data |
| `0x30` | ? | total vertical scanlines in the pixel buffer; if less than `VDisp`, the last two scanlines repeat interlaced |
| `0x34` | VDisp (?) | vertical scanlines displayed (top screen only, it seems); if less than `VTotal`, the remaining lines are not refreshed and fade out |
| `0x38` | vertical data offset (?) | a high enough value shifts the picture up; at or above some threshold (less than one scanline shown) the screen loses sync |
| `0x40` | V interrupt timing | like `0x1C` but compared against `VCount` and limited by `VTotal`; the interrupt that fires is VSync, and it seems to fire at `HCount` == 0 always |
| `0x44`, `0x48` | ? | `0x44` is like `0x10`; in `0x48` bit 0 seems to disable HSync and bit 8 VSync, other bits not writable |
| `0x5C` | ? | low u16 image width (including some offset?), high u16 image height (unused?) |
| `0x60` | HDisp | low u16 image start (border to pixel data), high u16 image end (pixel data to border) |
| `0x64` | ? | low u16 unknown, high u16 total framebuffer height (scanlines blitted regardless of framebuffer height) |

Setting `VTotal` to 494 lowers the refresh to about 50.04 Hz, as noted above.

The two eye framebuffers of the top screen are both displayed regardless of the 3D slider; with the slider off, the left and right addresses are normally set equal. If they differ with the 3D effect disabled, the LCD seems to alternate between left and right each frame `[reverse-engineered: 3dbrew]`. AB/BA interlacing alternates scanlines of A and B (the stride is added to the internal scanline pointer after each line and the other framebuffer is selected); AA reads the A pointer for both. 3D mode is decided by software: per 3dbrew, the GSP service enables stereoscopy only when interlace bit 5 = 1 and bit 6 = 0 (GSP mode 2), mode 1 is the full 240×800 output with both off, and bit 6 alone (bit 5 = 0) selects 2D (mode 0, the default). Bit 6 behaves differently per panel: on the bottom screen it doubles every pixel; on the top screen it appears to receive two pixels per clock; on a 2DS it only shifts the bottom framebuffer two pixels right. Interlacing and scan-doubling cannot both be enabled in userland but work on bare metal.

### Transfer engine

The transfer engine at `0x10400C00` converts GPU render targets (8×8-tiled) into LCD framebuffers (linear) and back, with optional vertical flip, colour conversion and 1×1/2×1/2×2 box-filter downscale (used for anti-aliasing and mipmaps). It also has a plain `TextureCopy` mode. Flags, constraints (minimum widths/heights and alignments) and the working format conversions are in [GPU external registers](gpu-external-registers.md#transfer-engine).

### Framebuffer memory (system applets)

Top-screen buffers are 240×400×3 bytes for each eye (the panel is scanned rotated by 90°). See the VRAM layout on the [memory page](memory.md).

## Homebrew notes

Homebrew normally uses `libctru`'s `gfx` API (which wraps the GSP service) rather than programming the PDC; that API treats the panels as portrait screens rotated 90° counter-clockwise, so the top screen is 240 pixels wide and 400 tall ([libctru](../homebrew/libctru.md)). Direct register work is needed only for bare-metal or exploit-driven code (running with ARM11 kernel privileges). The video-capture test points on the board (documented on 3dbrew) allow capturing the raw screen signals with hardware.

## Open questions

- Several display registers (`0x20`, `0x28`–`0x38`, `0x44`, `0x48`, `0x5C`, `0x64`) are only partly understood.
- How the controller selects true interleaving vs line skipping for 3D is unknown (3dbrew: other PDC registers decide whether both framebuffers are treated as 240×400 or 240×800).

## Sources

- [3dbrew: GPU/External Registers](https://www.3dbrew.org/wiki/GPU/External_Registers) — PDC registers, timing order and latch modes, framebuffer formats, interlace modes, transfer engine (full register map: [GPU external registers](gpu-external-registers.md)).
- [3dbrew: Hardware](https://www.3dbrew.org/wiki/Hardware) — resolutions and screen sizes.
- [3dbrew: Memory layout](https://www.3dbrew.org/wiki/Memory_layout) — VRAM framebuffer addresses.
- [3dbrew: Video Capture](https://www.3dbrew.org/wiki/Video_Capture) — screen capture test points.
- [libctru `qtm.h` and `gfx.h`](https://github.com/devkitPro/libctru) — QTM head-tracking and barrier control, portrait-orientation note for the framebuffer API.
- [Rodrigo Copetti, Nintendo 3DS Architecture: Displays](https://www.copetti.org/writings/consoles/nintendo-3ds/) — parallax barrier, 2D/3D modes, head tracking.
- [Copetti, Nintendo 3DS Architecture: Graphics](https://www.copetti.org/writings/consoles/nintendo-3ds/) — framebuffer swap process and the legacy (DS/GBA) path.
