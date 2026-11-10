# Asset pipeline for 3DS homebrew

Games need images, fonts, shaders, sounds and level data, and on the 3DS almost none of them can be used in the form an artist hands over. Textures must be converted to the GPU's tiled layout and a size-appropriate format, shaders must be assembled, fonts rasterised into the console's font format, and everything packed so the program can load it quickly. The devkitPro tools do all of this at build time; this page explains which tool handles which asset, how the standard Makefile wires them together, and how to choose formats.

## Before you start

- Checked against tex3ds 2.3.0, picasso 2.7.2, 3dstools 1.3.1 (`3dsxtool`, `smdhtool`, `mkromfs3ds`), `mkbcfnt` from general-tools 1.4.4, libctru 2.7.1, citro2d 1.7.0, and the `templates/application` Makefile in `3ds-examples`.
- Converting at build time is the rule. Converting on the console costs CPU time, memory for both copies, and load-time hitches; do it only for content that is created at run time.

## Where assets go

| Asset | Source file | Tool (run by the Makefile) | Result | Loaded with |
| ----- | ----------- | -------------------------- | ------ | ----------- |
| Textures, sprite atlases, cube maps | `.png` etc. + a `.t3s` options file in `gfx/` | `tex3ds` | `.t3x` | `Tex3DS_TextureImport*` (citro3d), `C2D_SpriteSheetLoad*` (citro2d) |
| Vertex / geometry shaders | `.v.pica` (or several files listed in a `.shlist`) in `source/` | `picasso` | `.shbin`, linked in, with a `_shbin.h` header | `DVLB_ParseFile` |
| Fonts | `.ttf`/`.otf` | `mkbcfnt` | `.bcfnt` | `C2D_FontLoad` (or `C2D_FontLoadSystem` for the console's built-in fonts) |
| Arbitrary binary data | anything in `data/` | `bin2s` | linked into the program, with `<name>_<ext>.h` | the generated symbol |
| Files in a filesystem | the `romfs/` folder | `3dsxtool --romfs` (3DSX) or `makerom` (CIA) | RomFS image appended to / inside the program | `romfsInit()`, then `fopen("romfs:/…")` |
| Icon and title | `icon.png` (48×48), `APP_TITLE`, `APP_DESCRIPTION`, `APP_AUTHOR` | `smdhtool` | `.smdh` | read by the Homebrew Menu |

[documented: `3ds-examples/templates/application/Makefile`, `graphics/printing/custom-font/Makefile`]

### Linked-in versus RomFS

The template's `GFXBUILD` variable decides where converted textures go. Left at `$(BUILD)`, every `.t3x` is linked into the executable and available as a symbol; set to `$(ROMFS)/gfx`, they are placed in the RomFS and loaded by path.

- **Linked in**: simplest; the whole program, assets included, is loaded into memory at start-up and stays there. Fine for small games and for assets that are always needed.
- **RomFS**: assets stay on the SD card (inside the `.3dsx` or the installed title) until you open them, so memory holds only what is loaded. Use it once your assets approach a few megabytes, and always for anything level-specific. Load during transitions, never in the middle of a frame.

Under a `.3dsx` the RomFS is read from the file on the SD card, so read speed is SD speed. Fewer, larger sequential reads beat many small ones; pack small files into one archive with an index if you have hundreds of them [hypothesis on the magnitude — measure on your target].

## Textures with tex3ds

`tex3ds` converts images to **tiled** GPU textures ([glossary](../../glossary.md#tiling)): 8×8-pixel blocks instead of rows. The Makefile runs `tex3ds -i file.t3s -H file.h -d deps -o file.t3x`; the `.t3s` file holds options on its first line and image files after it. Two real examples from `3ds-examples`:

```text
--atlas -f rgba8888 -z auto
enemyBlack1.png
enemyBlack2.png
...
```

```text
-f auto-etc1 -z auto -m cubic
kitten.png
```

The first packs many sprites into one atlas texture; the second makes one compressed, mipmapped texture.

| Option | Choices | Notes |
| ------ | ------- | ----- |
| `-f` format | `rgba8888` (default), `rgb888`, `rgba5551`, `rgb565`, `rgba4444`, `la88`, `hilo88`, `l8`, `a8`, `la44`, `l4`, `a4`, `etc1`, `etc1a4`, `auto-l8`, `auto-l4`, `auto-etc1` | `auto-*` picks the alpha variant only when the image has alpha |
| `-a` | atlas | Pack all listed images into one texture; sub-images retrievable by index |
| `-m` filter | `point`, `box`, `triangle`, `cubic`, `lanczos`, `mitchell`, … | Generate a mipmap chain with that filter |
| `-z` compression | `auto` (default), `none`, `huff`, `lzss`/`lz10`, `lz11`, `rle` | Compresses the *file*; data is decompressed on load. Not the same as ETC1, which stays compressed in GPU memory |
| `-q` | `low`, `medium` (default), `high` | ETC1 encoder quality (build time only) |
| `-b` | `none`, `transparent`, `edge` | 1-pixel border around atlas images to stop filtering bleed |
| `-t` | | Trim transparent edges of atlas images |
| `-c`, `-s` | | Cube map / skybox from a cross-shaped image |
| `-r` | | Raw image data only, no `.t3x` header |

[documented: `tex3ds --help`, version 2.3.0]

### Choosing a format

| Content | Format | Bytes per pixel |
| ------- | ------ | --------------- |
| Photographic or painted textures, large backgrounds | `etc1` / `etc1a4` | 0.5 / 1 |
| Opaque pixel art, UI backgrounds | `rgb565` | 2 |
| Sprites with hard edges (1-bit transparency) | `rgba5551` | 2 |
| Sprites and UI with soft edges | `rgba4444`, or `rgba8888` if banding is visible | 2 / 4 |
| Glyphs, masks, light maps, single-channel data | `a8`, `l8`, `la88`, `a4`, `l4`, `la44` | 1, 1, 2, 0.5, 0.5, 1 |
| Two-channel data such as normal-map XY | `hilo88` | 2 |

ETC1 is lossy and blurs hard pixel edges; do not use it for pixel art or text. Remember that texture dimensions are powers of two: a 300×200 image occupies a 512×256 texture unless it shares an atlas.

### Atlases and padding

Put sprites drawn together into the same atlas so citro2d or your batcher can draw them in one call ([GPU rendering](gpu-rendering.md#2d-games-citro2d)). With bilinear filtering, use `-b edge` (or `transparent`) so neighbouring images do not bleed into each other at their borders; with nearest-neighbour pixel art at integer positions, no border is needed.

## Shaders with picasso

Shader source files ending in `.v.pica` in a source directory are assembled one per output file; to put several shaders (for example a vertex and a geometry shader) into one `.shbin`, list them in a `.shlist` file. The generated header declares `<name>_shbin` and `<name>_shbin_size`, which you pass to `DVLB_ParseFile` ([picasso and citro3d](../homebrew/picasso-and-citro3d.md)).

## Fonts

citro2d draws text from **BCFNT** fonts (the console's own font format). Either use the system font, `C2D_FontLoadSystem(CFG_REGION_USA)` (other regions give Chinese, Korean and Taiwanese glyph sets), which costs nothing to ship; or convert your own TrueType font with `mkbcfnt -s <points> -o out.bcfnt in.ttf`, optionally restricting it to the characters you need with `-w` (whitelist) or `-b` (blacklist) files to save memory [documented: `mkbcfnt --help`, `graphics/printing/custom-font`].

## Audio

There is no build-time audio converter in the 3DS toolchain. Three common choices:

- **Raw PCM** (16-bit signed, mono or stereo) loaded into linear memory: no CPU cost to play, large on disk. Fine for short sound effects.
- **Ogg Vorbis** or **Opus** decoded on the console with the `libvorbisidec` (integer-only Vorbis) or `opusfile` port libraries from devkitPro's package manager, streamed into `ndsp` wave buffers: small files, CPU cost while playing. See `audio/ogg-vorbis-decoding`, `audio/opus-decoding` and `audio/streaming` [documented: `3ds-examples`].
- **Tracker modules** with `libmodplug` (`audio/modplug-decoding`).

The DSP's native output rate is about 32,728 Hz ([audio](../hardware/audio.md)); `ndsp` resamples each channel from its own rate (`ndspChnSetRate`). Decoding on a core-1 thread on an Old 3DS is a common pattern; read [performance](threading.md) for its caveats.

## Data compression

libctru can decompress the same formats tex3ds writes, so you can compress your own data files with standard Nintendo-style headers: `decompress()` and `decompressV()` (`<3ds/util/decompress.h>`) handle LZSS (LZ10), LZ11, Huffman and RLE, from memory, a file descriptor or a `FILE*` via callbacks. The header is one type byte (`0x10` LZSS, `0x11` LZ11, `0x28` Huffman, `0x30` RLE, `0x00` uncompressed) followed by the 24-bit uncompressed size (or a 32-bit size plus padding when the type byte's top bit is set) [documented: `tex3ds --help`, libctru `decompress.h`].

Whether compression speeds up loading depends on SD-card read speed versus decompression CPU time, and differs between Old and New 3DS [hypothesis — measure both].

## Technical details

- `.t3x` files start with a header describing sub-textures (count, coordinates) followed by the texture data, optionally compressed; the format is defined by `tex3ds` and parsed by citro3d's `tex3ds.c`. Imported textures are allocated in linear memory unless the `vram` argument is true.
- `3dsxtool` appends the SMDH and RomFS to the `.3dsx` (`--smdh=`, `--romfs=` flags in the template).
- `bin2s` turns `data/foo.bin` into an object file defining `foo_bin`, `foo_bin_end` and `foo_bin_size` and a header `foo_bin.h`.

## Pitfalls

- **Images whose size is not a power of two** outside an atlas: tex3ds pads them; check you are not wasting most of a texture.
- **Using `-z` instead of ETC1 to save GPU memory:** file compression does not reduce memory once the texture is loaded.
- **Loading everything at start-up** into a 64 MB Old 3DS: use RomFS and load per level.
- **Forgetting `romfsInit()`** (or calling file functions before it): every `romfs:/` open fails.

## Open questions

- Measured SD read throughput for RomFS inside a `.3dsx` versus an installed `.cia`, Old vs New 3DS.
- Decompression speed of LZ11 vs Huffman vs none for typical game data on Old 3DS.

## Sources

- [tex3ds](https://github.com/devkitPro/tex3ds) — formats, options and compression (from the tool's `--help`).
- [3ds-examples](https://github.com/devkitPro/3ds-examples) — `templates/application/Makefile` (asset rules), `graphics/gpu/gpusprites` and `mipmap_fog` (`.t3s` files), `graphics/printing/custom-font` (BCFNT), `audio/*` (decoding and streaming).
- [libctru](https://github.com/devkitPro/libctru) — `util/decompress.h`, `romfs.h`.
- [citro2d](https://github.com/devkitPro/citro2d) — font and sprite-sheet loading.
- [picasso](https://github.com/devkitPro/picasso) — shader assembly.
