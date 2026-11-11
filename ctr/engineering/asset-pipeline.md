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
| `-f` format | `rgba8888` (default; also `rgba`, `rgba8`), `rgb888` (`rgb`, `rgb8`), `rgba5551`, `rgb565`, `rgba4` (also `rgba444`), `la88` (`la`, `la8`), `hilo88` (`hilo`, `hilo8`), `l8` (`l`), `a8` (`a`), `la44` (`la4`), `l4`, `a4`, `etc1`, `etc1a4`, `auto-l8`, `auto-l4`, `auto-etc1` | `auto-*` picks the alpha variant only when the image has alpha |
| `-a`, `--atlas` | atlas | Pack all listed images into one texture; sub-images retrievable by index |
| `-m` filter | `point`, `box`, `triangle`, `cubic`, `lanczos`, `mitchell`, … | Generate a mipmap chain with that filter |
| `-z` compression | `auto` (default), `none`, `huff`, `lzss`/`lz10`, `lz11`, `rle` | Compresses the *file*; data is decompressed on load. Not the same as ETC1, which stays compressed in GPU memory |
| `-q` | `low`, `medium` (default), `high` | ETC1 encoder quality (build time only) |
| `-b`, `--border` | `none`, `transparent`, `edge` | 1-pixel border around atlas images to stop filtering bleed |
| `-t` | | Trim transparent edges of atlas images |
| `-c`, `-s` | | Cube map / skybox from a cross-shaped image |
| `-r` | | Raw image data only, no `.t3x` header |
| `-d`, `-H`, `-p`, `-i` | file names | Dependency file for `make`, generated C header with sub-texture indices, a preview image of the converted result, and an options file to include |

> Conflict: `tex3ds --help` (2.3.0) lists the short options `-a`, `-b` and `-c`, but the installed Windows binary rejected them with "unknown option" in a test conversion, while `--atlas`, `--border` and `--cubemap` worked. Use the long forms in scripts until this is understood. Not tested on other platforms.


[documented: `tex3ds --help`, version 2.3.0, re-read from the installed binary]. The help text lists the spellings above (`rgba4`/`rgba444`, not `rgba4444`); this page previously wrote `rgba4444`, which the help does not list.

A cube map (`-c`) or skybox (`-s`) is cut from one cross-shaped image laid out as `+Y` above `+Z`, with `-X +Z +X -Z` in the middle row and `-Y` below (the diagram in `tex3ds --help`). Failure mode: a wrong layout gives faces in the wrong order with no error. Inspect the `-p` preview image before shipping.

### Choosing a format

| Content | Format | Bytes per pixel |
| ------- | ------ | --------------- |
| Photographic or painted textures, large backgrounds | `etc1` / `etc1a4` | 0.5 / 1 |
| Opaque pixel art, UI backgrounds | `rgb565` | 2 |
| Sprites with hard edges (1-bit transparency) | `rgba5551` | 2 |
| Sprites and UI with soft edges | `rgba4`, or `rgba8888` if banding is visible | 2 / 4 |
| Glyphs, masks, light maps, single-channel data | `a8`, `l8`, `la88`, `a4`, `l4`, `la44` | 1, 1, 2, 0.5, 0.5, 1 |
| Two-channel data such as normal-map XY | `hilo88` | 2 |

ETC1 is lossy and blurs hard pixel edges; do not use it for pixel art or text. Remember that texture dimensions are powers of two: a 300×200 image occupies a 512×256 texture unless it shares an atlas.

### Atlases and padding

Put sprites drawn together into the same atlas so citro2d or your batcher can draw them in one call ([GPU rendering](gpu-rendering.md#2d-games-citro2d)). With bilinear filtering, use `-b edge` (or `transparent`) so neighbouring images do not bleed into each other at their borders; with nearest-neighbour pixel art at integer positions, no border is needed.

### Repacking an unusual source layout

Some ports must load assets in their original layout at runtime. The Sonic CD 11 3DS research describes rearranging a 16 × 16384 indexed tileset into a 512 × 512 direct-colour atlas: for integer source coordinates `(sx, sy)`, use `dx = sx + 16 * (sy / 512)` and `dy = sy % 512`. A 512 × 512 RGBA5551 atlas uses 512 KiB for pixels alone. [reported: project renderer, revision and test model unspecified; see the [case study](case-studies-rendering-tradeoffs.md#sonic-cd-convert-a-software-renderer-to-gpu-layers)]

Expand indices through the active palette, then tile the resulting texture for the GPU. Validate the first and last pixel of each 512-row section and compare tile addresses and orientation against the original. Keep the original palette or a way to reconstruct it: when it changes, invalidate all converted pixels that depend on it. Include source, destination and any temporary buffers in the peak load budget.

## Shaders with picasso

Shader source files ending in `.v.pica` in a source directory are assembled one per output file; to put several shaders (for example a vertex and a geometry shader) into one `.shbin`, list them in a `.shlist` file. The generated header declares `<name>_shbin` and `<name>_shbin_size`, which you pass to `DVLB_ParseFile` ([picasso and citro3d](../homebrew/picasso-and-citro3d.md)).

## Fonts

citro2d draws text from **BCFNT** fonts (the console's own font format). Either use the system font, `C2D_FontLoadSystem(CFG_REGION_USA)` (other regions give Chinese, Korean and Taiwanese glyph sets), which costs nothing to ship; or convert your own TrueType font with `mkbcfnt -s <points> -o out.bcfnt in.ttf`, optionally restricting it to the characters you need with `-w` (whitelist) or `-b` (blacklist) files to save memory. Those files are whitespace-separated lists of Unicode codepoints. `mkbcfnt` accepts several input fonts and gives lower-numbered inputs priority, which is how a Latin font can be combined with a CJK fallback [documented: `mkbcfnt --help` (general-tools 1.4.4), `graphics/printing/custom-font`]. Expected result: one `.bcfnt` whose size grows with the number of glyphs; if text shows missing glyphs, the codepoint was filtered out or no input font contains it.

## Audio

There is no build-time audio converter in the 3DS toolchain. Three common choices:

- **Raw PCM** (16-bit signed, mono or stereo) loaded into linear memory: no CPU cost to play, large on disk. Fine for short sound effects.
- **Ogg Vorbis** or **Opus** decoded on the console with the `libvorbisidec` (integer-only Vorbis) or `opusfile` port libraries from devkitPro's package manager, streamed into `ndsp` wave buffers: small files, CPU cost while playing. See `audio/ogg-vorbis-decoding`, `audio/opus-decoding` and `audio/streaming` [documented: `3ds-examples`].
- **Tracker modules** with `libmodplug` (`audio/modplug-decoding`).

The DSP's native output rate is about 32,728 Hz ([audio](../hardware/audio.md)); `ndsp` resamples each channel from its own rate (`ndspChnSetRate`). Decoding on a core-1 thread on an Old 3DS is a common pattern; read [performance](threading.md) for its caveats.

## Data compression

libctru can decompress the same formats tex3ds writes, so you can compress your own data files with standard Nintendo-style headers: `decompress()` and `decompressV()` (`<3ds/util/decompress.h>`) handle LZSS (LZ10), LZ11, Huffman and RLE, from memory, a file descriptor or a `FILE*` via callbacks. The header is one type byte (`0x10` LZSS, `0x11` LZ11, `0x28` Huffman, `0x30` RLE, `0x00` uncompressed) followed by the 24-bit uncompressed size (or a 32-bit size plus padding when the type byte's top bit is set) [documented: `tex3ds --help`, libctru `decompress.h`].

Whether compression speeds up loading depends on SD-card read speed versus decompression CPU time, and may differ between Old 3DS and New 3DS [hypothesis — measure Old 3DS first, then New 3DS].

## Streaming and load hitches

A game can be fine in steady play and still stutter when it loads: opening a file, reading it from the SD card, decompressing and converting it all take time, and on an Old 3DS they take it from the same single core that runs your game. These are the rules this wiki carries, mostly from the author's unpublished notes (`old-3ds-guide-revised.md`, section 10, written for a port) and therefore `[hypothesis]` until measured on your own game:

- **Keep file work out of the frame.** No `fopen`, blocking SD read, texture decode or allocation in the part of the loop that must meet a deadline, and none inside audio refill or GPU callbacks. Load from a worker thread ([threading](threading.md)) and hand finished data over with an event.
- **Few large sequential reads beat many small ones.** Pack related assets into one file with an index of offsets and lengths, and read whole blocks. Whether compressing the pack helps depends on SD read speed versus CPU decompression and has to be measured on hardware (see Data compression above).
- **Time the stages separately**: wait on the file, decompress, decode, swizzle, cache flush, upload. A single "load time" number hides which one to fix. Log from memory and write the log out later, not synchronously per asset.
- **Test cold and warm.** The first run after boot reads from the card; a repeat may be served from caches. Report both, and report the transition frame, not only the average.
- **Prefetch what a transition predicts** (the next map's textures, the next menu's art) but never let prefetching evict something the current frame needs, and never hold a global lock across an SD read.
- **Fail visibly.** A missing or corrupt asset should produce a clear log line and a fallback, not a crash several frames later.
- **Bound the memory.** Know what the loaded set costs in linear memory and VRAM before you add a feature ([memory and cache](memory-and-cache.md#size-arithmetic)); a texture cache that is nearly full can turn a handful of loads into seconds of work.

No measured SD throughput, decompression speed or prefetch gain exists on this wiki; the experiments are listed under Open questions.

## Asset production for constrained hardware

Run-time optimisation is limited by what the art asks the hardware to do, so many savings are decided in asset production, before any engine code. This section collects the rules this wiki can support; none of the budgets below is measured, so they are things to set and test, not known-good numbers. Scene-side counterparts are in [3D scene optimisation](3d-scene-optimisation.md) and [stereo engineering](stereo-engineering.md).

**Convert offline, always.** Do swizzling (tiling), format conversion, atlas packing, mipmap generation, shader assembly and compression in the build (`tex3ds`, `picasso`, `mkbcfnt`, above). On the console they cost CPU time on the same core as the game and need memory for the source and result [documented: this page, above]. Models follow the same rule: convert vertex data offline to the exact attribute formats and orders the shader and draw call expect (packed 8-bit or 16-bit attributes where the art tolerates them), so loading is a copy into linear memory. The vertex-format options are in [GPU rendering](gpu-rendering.md); no 3DS model-conversion tool is covered by this wiki, so a model converter is code you write (the file format is yours) [hypothesis on the saving; measure load time].

**Set budgets before production.** Choose, per scene, the VRAM and linear memory you will spend on textures, meshes and render targets, and the triangle and draw counts you can submit per frame, and give them to artists as numbers. How to compute the memory side, and the per-format sizes, is in [memory and cache](memory-and-cache.md#size-arithmetic); this page does not repeat the table. Budget triangles and draws from a measurement on your own scene at your target frame rate [hypothesis: no wiki measurement exists]. Remember render targets and depth buffers share the 6 MiB of VRAM with textures ([GPU rendering](gpu-rendering.md#stereoscopic-3d-and-wide-mode)).

**Bake effects instead of computing them.** Lighting, ambient occlusion and shadows for static geometry go into vertex colours or a lightmap texture; reflections and highlights can be painted; painted flat backdrops can replace distant geometry. The options and costs are in [3D scene optimisation](3d-scene-optimisation.md#baked-lighting-shadows-and-vertex-colours). Baked results cost build time and storage, and cannot react to run-time changes.

**Stage loading and evict by lifetime.** Group assets by how long they live: permanent (UI font, common atlas), per-level, per-room, per-frame. Allocate each group from its own region or pool, load a group during a transition on a worker thread, and free the whole group at once instead of tracking assets one by one. This keeps fragmentation down, and makes the memory use of each scene a known number [hypothesis on fragmentation benefit; see [memory and cache](memory-and-cache.md#buffer-lifetime) for lifetimes of GPU-visible buffers]. The loading rules (large sequential reads, no file work in the frame, timing the stages) are in Streaming and load hitches above. The review that prompted this section (coffeepot4.md section 9) asserts that offline conversion, staged loading and lifetime-based eviction should be part of the architecture from the start; that is advice, not a measured result.

### Pixel art against 3D textures: choosing the format

The size and format table is under Choosing a format above and in [memory and cache](memory-and-cache.md#size-arithmetic). The quality trade is the part to decide in art production:

| Asset | Format to try first | Why | Check |
| ----- | ------------------- | --- | ----- |
| Pixel-art sprites and tiles, hard edges | `rgba5551` (1-bit alpha) or `rgba4` (soft alpha); `rgb565` if opaque | ETC1 works on 4x4 blocks with a shared base colour and blurs hard single-pixel edges | Zoom to the actual output scale and compare against the source image `[hypothesis]` |
| Pixel art with more than 16 colours per tile and gradients | `rgba8888` if it fits the budget | 16-bit formats band | Look at gradients and anti-aliased outlines |
| 3D environment textures, painted or photographic | `etc1` / `etc1a4` (`auto-etc1`), with mipmaps | Quarter or eighth of the memory of 16-bit formats; block artefacts are less visible in noisy art | Check on the console screen at distance, and with the texture magnified |
| Textures with sharp text or fine lines | `rgba4` or `rgba8888`, not ETC1 | Block compression smears thin lines | Render at the largest size shown |
| Alpha cut-outs (foliage, fences) | `etc1a4` (4-bit alpha) or `rgba5551` | Alpha precision differs; edge quality differs | Look at the edge against a bright and a dark background |
| Light maps, masks | `l8`, `a8`, `l4`, `a4` | One channel is enough | Banding on gradients in `l4` |
| UI and HUD in stereo | Format that keeps text sharp | Parallax offsets expose edge errors | Test on a 3D screen ([stereo engineering](stereo-engineering.md)) |

The ETC1 claim (lossy 4x4 block compression, blurs hard edges) comes from the format's design and this page's earlier guidance; the quality verdict for your art is a visual judgement, not a measurement. Use `-q high` for final ETC1 builds (build time only) and compare against `-q low`. Quality is subjective and screen-dependent; the 3DS LCD is small and low-resolution, so artefacts that show on a PC monitor may not show on the console, and the reverse `[hypothesis]`.

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
- How much a background prefetch thread costs the main thread on an Old 3DS (it shares the CPU and the SD bus) compared with loading on demand, measured on the first frame after a scene change.


## Sources

- [tex3ds](https://github.com/devkitPro/tex3ds) — formats, options and compression (from the tool's `--help`).
- [3ds-examples](https://github.com/devkitPro/3ds-examples) — `templates/application/Makefile` (asset rules), `graphics/gpu/gpusprites` and `mipmap_fog` (`.t3s` files), `graphics/printing/custom-font` (BCFNT), `audio/*` (decoding and streaming).
- [libctru](https://github.com/devkitPro/libctru) — `util/decompress.h`, `romfs.h`.
- [citro2d](https://github.com/devkitPro/citro2d) — font and sprite-sheet loading.
- [picasso](https://github.com/devkitPro/picasso) — shader assembly.
- coffeepot4.md (external review, private), section 9 — the offline-conversion and eviction advice, treated as a secondary source.
- Unpublished author notes `old-3ds-guide-revised.md` (section 10, file I/O and asset streaming) — the streaming rules above; labelled `[hypothesis]` here.
