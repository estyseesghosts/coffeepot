# CTR GPU external registers

Besides the command-driven [internal registers](gpu-internal-registers.md), the 3DS [GPU](gpu.md) has a small block of registers that the ARM11 CPU can read and write directly. They handle the jobs around rendering: filling memory, copying and converting finished images into the screen format, profiling, starting a command list, and telling the two [display controllers](display.md) where the screen images are. Normal games reach them only through the `gsp::Gpu` service; this page is for people working at the register level (bare-metal code, emulators, exploit payloads).

Everything here is the same on the Old 3DS and the New 3DS as far as the sources show. The one model flag is bit 2 of the hardware ID register ("new model"); see the unknown in the map.

## Where it fits

The registers sit in the ARM11's I/O space at physical `0x10400000`. The GSP module, a system service, owns them; applications ask it to do work with GX commands such as "process command list", "display transfer", "texture copy" and "memory fill". The screen-scanout registers (`0x400`, `0x500`) are documented on [displays](display.md#lcd-framebuffer-registers).

## Technical details

### Map

`GSPGPU:WriteHWRegs` takes these addresses relative to `0x1EB00000`.

| User VA | Physical | Length | Name | Notes |
| ------- | -------- | ------ | ---- | ----- |
| `0x1EF00000` | `0x10400000` | 4 | Hardware ID | bit 2: "new model" |
| `0x1EF00004` | `0x10400004` | 4 | unknown | |
| `0x1EF00010` | `0x10400010` | 16 | Memory fill 1 "PSC0" | GX command 2 |
| `0x1EF00020` | `0x10400020` | 16 | Memory fill 2 "PSC1" | GX command 2 |
| `0x1EF00030` | `0x10400030` | 4 | VRAM bank control | bits 8-11: bank i disabled; other bits unused |
| `0x1EF00034` | `0x10400034` | 4 | GPU busy | bit 26 PSC0, 27 PSC1, 30 PPF, 31 P3D |
| `0x1EF00050` | `0x10400050` | 4 | unknown | `0x22221200` is written on GPU init |
| `0x1EF00054` | `0x10400054` | 4 | unknown | `0xFF2` written on GPU init |
| `0x1EF00064` | `0x10400064` | 0xC | Hotspot profiling | see below |
| `0x1EF000C0` | `0x104000C0` | 4 | Backlight control | write 0 to let the backlights turn off, `0x20000000` to force them always on |
| `0x1EF00400` | `0x10400400` | 0x100 | Framebuffer setup "PDC0" (top screen) | [displays](display.md#lcd-framebuffer-registers) |
| `0x1EF00500` | `0x10400500` | 0x100 | Framebuffer setup "PDC1" (bottom screen) | same layout |
| `0x1EF00C00` | `0x10400C00` | ? | Transfer engine ("DMA") | see below |
| `0x1EF01000-0x1EF01C00` | `0x10401000-0x10401C00` | | Window onto the internal registers | normally written through the command list (the `GPUREG_CMDBUF_*` registers) rather than directly |
| `0x1EF01000` | `0x10401000` | 4 | unknown | 0 written on init and before a command list is used |
| `0x1EF01080` | `0x10401080` | 4 | unknown | `0x12345678` on init |
| `0x1EF010C0` | `0x104010C0` | 4 | unknown | `0xFFFFFFF0` on init |
| `0x1EF010D0` | `0x104010D0` | 4 | unknown | 1 on init |
| `0x1EF014??` | `0x104014??` | 0x14 | "PPF"? | |
| `0x1EF018E0` | `0x104018E0` | 0x14 | Command list "P3D" | see below |

> Unknown: which console model the hardware-ID "new model" bit is set on, and the values it takes, are not stated by 3dbrew. Reading the register on an Old 3DS and a New 3DS would settle it.

### Memory fill

Two units (PSC0 at `0x...010`, PSC1 at `0x...020`) fill VRAM with a value, like `memset`. Registers, offset X from the unit base:

| Offset | Meaning |
| ------ | ------- |
| +0 | Buffer start physical address >> 3 |
| +4 | Buffer end physical address >> 3 |
| +8 | Fill value |
| +C | Control: bit 0 start/busy, bit 1 finished, bits 8-9 fill width (0 = 16-bit, 1 or 3 = 24-bit, 2 = 32-bit) |

Setting bit 0 starts the fill and aborts any fill still running on that unit. On completion the hardware clears bit 0, sets bit 1 and raises the PSC0/PSC1 interrupt. The addresses must be inside VRAM. GX `SetMemoryFill` uses these; `citro3d` clears render targets with it (`C3D_FrameBufClear` -> `GX_MemoryFill` with width `cfs << 8`, citro3d `framebuffer.c`).

### Hotspot profiling

Registers that sample which stage of the GPU is busy over fixed intervals, meant for finding bottlenecks.

| Register | Bits | Meaning |
| -------- | ---- | ------- |
| `0x1EF00064` | bit 0 | Enable: write 1 to start measuring |
| `0x1EF00068` | bits 0-15 | Interval count (number of intervals recorded in a row) |
| `0x1EF00068` | bits 16-31 | Interval length - 1, in GPU clock cycles |
| `0x1EF0006C` | all | Result FIFO: read 4 times; each 32-bit word holds two 16-bit counters |

Interval count 0 measures until the FIFO is read at least once. Beyond `0xFFFF` intervals the counters wrap. There are 8 counters; each interval increments the counter of one stage, so the sum equals the interval count. The GPU clock is 268 MHz (see [GPU](gpu.md#summary)). For interval length < 3 the first stage always seems to be the one counted. What happens on reading the FIFO before a fixed-length measurement completes is untested. 3dbrew's mapping of counters to stages is an educated guess from testing `[reverse-engineered: 3dbrew, author not stated]`:

| Word | Low half (bits 0-15) | High half (bits 16-31) |
| ---- | -------------------- | ---------------------- |
| 0 | Array reads (depends on input primitives and attribute sizes) | Vertex shader (grows with more shader instructions) |
| 1 | Primitive setup / culling | Rasterizer (number and size of triangles) |
| 2 | Texture reads | Lighting calculations |
| 3 | Colour combiners (enabled TexEnv stages) | Framebuffer operations (area covered) |

### Transfer engine

Registers at `0x1EF00C00`; used by GSP for `DisplayTransfer` and `TextureCopy`. `DisplayTransfer` registers are used when TextureCopy mode is off, and by default it works in tiled-to-linear mode.

| Address | Meaning |
| ------- | ------- |
| `0x1EF00C00` | Input physical address >> 3 |
| `0x1EF00C04` | Output physical address >> 3 |
| `0x1EF00C08` | DisplayTransfer output width (bits 0-15) and height (16-31) |
| `0x1EF00C0C` | DisplayTransfer input width and height |
| `0x1EF00C10` | Transfer flags (below) |
| `0x1EF00C14` | unknown; GSP writes 0 here before writing `0x1EF00C18` for a DisplayTransfer |
| `0x1EF00C18` | Bit 0 starts the transfer; on completion bit 0 clears and bit 8 sets |
| `0x1EF00C1C` | unknown |
| `0x1EF00C20` | TextureCopy total bytes |
| `0x1EF00C24` | TextureCopy input line width (bits 0-15) and gap (16-31), in 16-byte units |
| `0x1EF00C28` | TextureCopy output line width and gap |

| Flag bit | Meaning |
| -------- | ------- |
| 0 | Flip the image vertically |
| 1 | Linear-to-tiled (overrides tiled-to-linear) |
| 2 | Required when the output is narrower than the input, so the hardware crops lines; otherwise the output is mis-aligned |
| 3 | TextureCopy mode (overrides all other modes) |
| 5 | Tiled-to-tiled (overrides the two above) |
| 8-10 | Input colour format |
| 12-14 | Output colour format |
| 16 | 32x32 block tiling instead of 8x8 (output dimensions must be multiples of 32, even when cropping) |
| 24-25 | Box-filter downscale: 0 none, 1 2x1, 2 2x2, 3 invalid |
| 4, 6-7, 11, 15, 17-23, 26-31 | Not writable |

The colour format values are the framebuffer values (0 RGBA8, 1 RGB8, 2 RGB565, 3 RGB5A1, 4 RGBA4).

**Tiled to linear** (render target to LCD framebuffer) constraints: output dimensions not larger than input; width >= 64, height >= 16; width aligned to 16 bytes for RGB8 transfers, 8 bytes otherwise; with downscale, input and output dimensions should be equal (else output is glitched) and width / 2 must also meet the alignment. Which conversions complete (3dbrew's table, results as observed):

| Input -> output | Result |
| --------------- | ------ |
| RGBA8 -> any of RGBA8, RGB8, RGB565, RGB5A1, RGBA4 | works, interrupt fires |
| RGB8 -> RGB8 | works, interrupt fires |
| RGB8 -> RGBA8, RGB565, RGB5A1, RGBA4 | no interrupt |
| RGB565, RGB5A1, RGBA4 -> RGB565, RGB5A1, RGBA4 | works, interrupt fires |
| RGB565, RGB5A1, RGBA4 -> RGBA8 or RGB8 | no interrupt |

**Tiled to tiled** is officially always used with 2x2 downscale (other configurations are glitched); it serves anti-aliasing and mipmap generation. Output not larger than input; width >= 64, height >= 32; width aligned to 64 bytes for RGB8/RGBA8, 128 bytes otherwise. Conversion results are the same as above.

**TextureCopy** (flag bit 3) copies raw data with no format conversion and a configurable gap between lines; every control bit except the dimensions and bit 2 (needed when gaps are used) is ignored. The size register gives the total bytes; the hardware loops over lines until that many are copied, skipping `gap` bytes after each line read (gaps do not count towards the total). Line width and gap are stored divided by 2 (a 16-byte unit, with the low 4 bits stripped from a bit count). Example from 3dbrew, left half of a 32x32 RGB8 texture: line width = (16 * 24) >> 4 = 24, gap = line width, size = 16 * 32 * 3 = 1536. With tiled data remember a tile's contents are sequential in memory. Junk values can hang the GPU: contiguous mode needs size >= 16; gap mode needs size >= 192 and a non-zero line width.

### Command list (P3D)

| Address | Meaning |
| ------- | ------- |
| `0x1EF018E0` | Buffer size in bytes >> 3 |
| `0x1EF018E8` | Buffer physical address >> 3 |
| `0x1EF018F0` | Bit 0 = 1 starts command processing; it appears to reset to 0 when done |

These are what GX command 1 ("process command list") uses. That the GSP module programs exactly these three registers is the 3dbrew description; the wiki's own [command pipeline page](../engineering/pica200-command-pipeline.md#frame-end-gx-and-the-gsp) marks the exact GSP behaviour as unverified.

### Initial values (top screen)

3dbrew records the values `nngxInitialize` writes to the top-screen display controller (`0x1EF00400` block), which are a known-good scanout timing: `0x400 = 0x1C2`, `0x404 = 0xD1`, `0x408 = 0x1C1`, `0x40C = 0x1C1`, `0x410 = 0`, `0x414 = 0xCF`, `0x418 = 0xD1`, `0x41C = 0x1C501C1`, `0x420 = 0x10000`, `0x424 = 0x19D`, `0x428 = 2`, `0x42C = 0x1C2`, `0x430 = 0x1C2`, `0x434 = 0x1C2`, `0x438 = 1`, `0x43C = 2`, `0x440 = 0x1960192`, `0x444 = 0`, `0x448 = 0`, `0x45C = 0x19000F0`, `0x460 = 0x1C100D1`, `0x464 = 0x1920002`, `0x470 = 0x80340`, `0x49C = 0`. Also written: `0x468`, `0x46C`, `0x494`, `0x498` = `0x18300000` (the first two later changed by GSP when it updates the framebuffer), `0x478 = 1` (reads back 0), `0x474 = 0x10501`. The meaning of these registers is on [displays](display.md#lcd-framebuffer-registers).

## Homebrew notes

Homebrew does not touch these directly: `libctru` wraps them as `GX_MemoryFill`, `GX_DisplayTransfer`, `GX_TextureCopy` and `GX_ProcessCommandList` ([libctru](../homebrew/libctru.md)). Bare-metal or exploit code writes them itself, then must respect the alignment and dimension constraints above: wrong values can leave the GPU stuck.

## Open questions

- Purpose of the unknown registers (`0x04`, `0x50`, `0x54`, `0x1000`-`0x10D0`, `0x1C`, `0x20`, `0x14xx`).
- Whether the conversions marked "no interrupt" produce valid output (3dbrew records only the interrupt).
- Model differences in any of the above.

## Sources

- [3dbrew: GPU/External Registers](https://www.3dbrew.org/wiki/GPU/External_Registers) — address map, memory fill, hotspot profiling, transfer engine, command list, init values.
- citro3d `source/framebuffer.c` — how memory fills and display transfers are driven from user code.
