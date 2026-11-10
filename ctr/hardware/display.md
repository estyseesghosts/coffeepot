# CTR displays

The 3DS has two screens. The bottom one is an ordinary 320×240 touchscreen. The top one is special: it can show a separate image to each eye without glasses, which is what gives the console its name. Games have to cooperate by drawing every frame twice, from slightly different viewpoints.

On top of that, DS and Game Boy Advance games reach the same screens through a separate "legacy" path, because their graphics chips work completely differently from the 3DS [GPU](gpu.md).

## Where it fits

The [GPU](gpu.md) renders images into memory; the display controllers (called **[PDC](../glossary.md#pdc)** in the register map) read finished *LCD framebuffers* out of [memory](memory.md) and feed them to the screens line by line. The [microcontroller](io-and-peripherals.md#the-auxiliary-microcontroller-mcu) controls LCD power and backlight; the 3D slider is read by the same MCU.

## How it works

### The 3D screen

The top panel is physically 800×240 pixels, but each pixel is half-width. A **parallax barrier** (a layer of opaque slits in front of the backlight) makes the left eye see every other pixel column and the right eye see the rest.

- **3D mode:** all 800 columns are used as two 400×240 images, one per eye.
- **2D mode:** pairs of columns are treated as single pixels, showing one 400×240 image.
- The panel can in principle show a true 800×240 image; no commercial game used it.

The barrier needs extra brightness (shorter battery life), and the effect only works if the player holds the console square to their face. A slider ("3D depth slider") scales how different the two views are. Starting with the New 3DS, a front camera and infrared LED track the player's head to keep the image aligned (this takes one CPU core; see [CPU](cpu.md)).

### Framebuffers and swapping

A frame travels through three kinds of buffer: the GPU render target, the *LCD framebuffer* (a copy in the format and orientation the panel wants) and the display controller. Games usually keep two LCD framebuffers per screen. While the panel scans one, the GPU/transfer engine fills the other; the swap is triggered at [V-blank](../../glossary.md#vblank) to avoid tearing.

### DS and GBA games

The DS/GBA picture-processing units live in the SoC. Their scanlines go to the **[LgyFb](../glossary.md#lgyfb)** block (optionally upscaled) and then to the framebuffer, with the ARM11 and its DMA moving data. This adds a small, negligible delay.

## Technical details

### Screen summary

| Screen | Native size | Notes |
| ------ | ----------- | ----- |
| Top | 800×240 physical; 400×240 usable per eye | Parallax barrier on 3DS / 3DS XL / New 3DS / New 3DS XL; 2DS and New 2DS XL have a non-3D top panel |
| Bottom | 320×240 | Resistive touch overlay |

Panel sizes: 3DS 3.53 in top / 3.00 in bottom; 3DS XL 4.88 / 4.18; 2DS 3.53 and 3.00 cropped from a single panel; New 3DS 3.88 / 3.33; New 3DS XL 4.88 / 4.18. See [models](models.md).

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
| `0x70` | Format | Bits 2–0 colour format; 5–4 interlace mode; 6 alternative pixel mode; 9–8 DMA burst size (0 = 32 B, 1 = 64 B, 2 = 128 B) |
| `0x74` | PDC control | Bit 0 enable; bits 8/9/10 mask H-blank/V-blank/error IRQs; 16 output enable |
| `0x78` | Select/status | Bit 0 next framebuffer; 4 current; 16/17/18 IRQ acknowledge |
| `0x80`, `0x84` | Colour lookup table | Index write-only, auto-increment; RGB8 `0x00BBGGRR` |
| `0x90` | Stride | Bytes between rows (multiple of 8); zero repeats the first row; negative flips vertically |
| `0x94`, `0x98` | Framebuffer B (first, second) | Top screen: right-eye buffers |

**Colour formats** (bits 2–0): 0 `RGBA8`, 1 `RGB8`, 2 `RGB565`, 3 `RGB5A1`, 4 `RGBA4`. Components are stored most-significant-bit first with little-endian pixels. Formats 5–7 are blocked by the GSP service but behave as pixel-doubled `RGBA8` outside userland.

**Interlace modes** (bits 5–4): 0 A (none), 1 AA (scanline doubling), 2 AB (interleave A and B), 3 BA. The bottom screen always uses mode A; the top screen uses AB in 3D mode and A in 2D mode. The GSP service enables the parallax barrier only when interlacing bit 5 = 1 and alternative-mode bit 6 = 0.

### Transfer engine

The transfer engine at `0x10400C00` converts GPU render targets (8×8-tiled) into LCD framebuffers (linear) and back, with optional vertical flip, colour conversion and 1×1/2×1/2×2 box-filter downscale (used for anti-aliasing and mipmaps). It also has a plain `TextureCopy` mode. Constraints (minimum widths/heights and alignments) are listed on 3dbrew's GPU external registers page.

### Framebuffer memory (system applets)

Top-screen buffers are 240×400×3 bytes for each eye (the panel is scanned rotated by 90°). See the VRAM layout on the [memory page](memory.md).

## Homebrew notes

Homebrew normally uses `libctru`'s `gfx` API (which wraps the GSP service) rather than programming the PDC. Direct register work is needed only for bare-metal or exploit-driven code (running with ARM11 kernel privileges). The video-capture test points on the board (documented on 3dbrew) allow capturing the raw screen signals with hardware.

## Open questions

- Several display registers (`0x20`, `0x28`–`0x38`, `0x44`, `0x48`, `0x5C`, `0x64`) are only partly understood.
- How the controller selects true interleaving vs line skipping for 3D is unknown.

## Sources

- [3dbrew: GPU/External Registers](https://www.3dbrew.org/wiki/GPU/External_Registers) — PDC registers, framebuffer formats, interlace modes, transfer engine.
- [3dbrew: Hardware](https://www.3dbrew.org/wiki/Hardware) — resolutions and screen sizes.
- [3dbrew: Memory layout](https://www.3dbrew.org/wiki/Memory_layout) — VRAM framebuffer addresses.
- [3dbrew: Video Capture](https://www.3dbrew.org/wiki/Video_Capture) — screen capture test points.
- [Rodrigo Copetti, Nintendo 3DS Architecture: Displays](https://www.copetti.org/writings/consoles/nintendo-3ds/) — parallax barrier, 2D/3D modes, head tracking.
- [Copetti, Nintendo 3DS Architecture: Graphics](https://www.copetti.org/writings/consoles/nintendo-3ds/) — framebuffer swap process and the legacy (DS/GBA) path.
