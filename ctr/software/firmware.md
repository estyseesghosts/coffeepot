# CTR firmware

Firmware is the software that brings a 3DS to life and decides what mode it is in. The 3DS has four firmware images, because the same hardware can act as a 3DS, a Nintendo DSi / DS, a Game Boy Advance, or a bare-bones updater. Each image is a signed bundle with programs for each processor, and the console only starts one that passes the [boot ROM](../../glossary.md#boot-rom)'s checks.

Only one of the four lives in a raw NAND partition. The two `FIRM` partitions of the [NAND](../../glossary.md#nand), `firm0` and `firm1`, hold the bootable `NATIVE_FIRM` (two copies, so a corrupted one can fall back to the other). `TWL_FIRM`, `AGB_FIRM` and `SAFE_MODE_FIRM` are not stored in those partitions: they are firmware system titles in the NAND filesystem (title ID high word `00040138`, see [titles](titles.md)), which `NATIVE_FIRM` loads and boots into when the console needs another mode.

3dbrew's title list has a `00040138` "System Firmware" category and states that `SAFE_MODE_FIRM` is, in its initial versions, exactly `NATIVE_FIRM` apart from core-version fields, and that on retail it is only used to run the System Updater; a launched GBA Virtual Console title is handled by `AGB_FIRM` [documented: 3dbrew Title list]. The exact title-ID list per firmware is not reproduced here.

## Behavior

| Image | Mode | Main processor | Notes |
| ----- | ---- | -------------- | ----- |
| `NATIVE_FIRM` | Normal 3DS | [ARM11](../hardware/cpu.md) | Runs [Horizon](os.md); the only image the boot ROM can start from a power cycle. Stored in the `firm0` and `firm1` NAND partitions |
| `TWL_FIRM` | DS / DSi | ARM9 + ARM7 | Firmware title (not a `firm` partition). Disables 3DS-only features; ARM11 emulates relocated DS hardware. "TWL" is the DSi's codename |
| `AGB_FIRM` | Game Boy Advance | ARM7 | Firmware title. ARM11 and ARM9 keep working in the background. "AGB" is the GBA's codename |
| `SAFE_MODE_FIRM` | Maintenance | ARM11 | Firmware title. An early revision of `NATIVE_FIRM` (no newer than 3.0 on Old 3DS or 8.1 on New 3DS); mainly runs the System Updater |

After a cold boot into `NATIVE_FIRM`, the OS can choose to reboot into another image — for example when a DS game is launched or when the user starts a system update. Leaving TWL or AGB mode runs a routine that reboots back into `NATIVE_FIRM`.

### Updates

Updates come online or from a game card. Cards carry system updates without the matching user apps; network updates include everything. Version names encode a mix of the two if both were used. Installing an update reboots into `SAFE_MODE_FIRM`, where the System Updater applies it. Online and cartridge updates both include system titles; the title list on 3dbrew shows which titles each firmware version changed (see [titles](titles.md)).

System version numbers look like `11.17.0-50`: major, minor and micro numbers plus a build number. The community history lists 11.17 as the version released in May 2023 ([history](../homebrew/history.md)); the sources used here do not state whether it is the final version.

## Technical details

### Where the images live

The NAND begins with an [NCSD](file-formats.md) header listing up to eight partitions. Boot9 parses all eight and tries every partition that is an actual FIRM (filesystem type `0x3`, crypt type `0x2`), in the order listed, until one verifies.

| Offset | Size | Name | Description |
| ------ | ---- | ---- | ----------- |
| `0x0B130000` | `0x400000` | `firm0` | Firmware partition (NCSD index 2) |
| `0x0B530000` | `0x400000` | `firm1` | Backup copy, normally identical (NCSD index 3) |

The full NAND layout is on the [NAND filesystem page](nand-filesystem.md).

### FIRM header facts

- The header begins with the magic `FIRM`; a priority field (offset `+4`) lets the boot ROM skip a partition if an equal-or-higher-priority FIRM has already been found.
- The image is split into sections, each with a load address, size and SHA-256 hash; the boot ROM refuses sections that hit its memory-range blacklist.
- The header is signed with RSA-2048 (verified with the key stored in Boot9 data for NAND FIRM).
- A FIRM image contains separate binaries for the ARM9 and ARM11 (and ARM7 for TWL/AGB images). New 3DS firmware adds an extra loader stage, **arm9loader**, that decrypts the ARM9 binary using keys derived from the console's OTP — a design that was found vulnerable in 2015 ([history](../homebrew/history.md)).

### Other boot sources

If no NAND FIRM verifies, Boot9 also tries a FIRM image stored in the Wi-Fi module's SPI flash (offset `0x400`), and, if the shell is closed and a key combination is held, a DS cartridge (the [ntrboot](../homebrew/boot9strap.md#ntrboot) mechanism). Details are on the [boot process page](boot-process.md).

### Contents of NATIVE_FIRM

`NATIVE_FIRM` contains Kernel9 and Process9 for the ARM9 and Kernel11 plus a handful of essential ARM11 modules (`sm`, `fs`, `pm`, `loader`, `pxi`) that start before the rest of the system; "SAFE_MODE" versions of these exist in `SAFE_MODE_FIRM`. The public keys used to check title signatures are stored here.

## Homebrew notes

Firmware is the battleground for permanent homebrew: attacks target the boot ROM, the extra loader stage, signature checks, or downgrade paths. See [boot9strap](../homebrew/boot9strap.md) (the standard way in today), [custom firmware](../homebrew/custom-firmware.md) and the [exploit catalogue](../homebrew/exploits.md).

## Open questions

- The binary layout of the FIRM header beyond the points above is not in the sources used here.
- Region-specific firmware differences and a full version timeline are not covered; 3dbrew's *Update Data* pages list per-version title changes.

## Sources

- [Rodrigo Copetti, Nintendo 3DS Architecture: Operating System](https://www.copetti.org/writings/consoles/nintendo-3ds/) — the four firmware images, update paths.
- [3dbrew: Bootloader](https://www.3dbrew.org/wiki/Bootloader) — NAND FIRM parsing, header magic/priority, other boot sources.
- [3dbrew: Flash Filesystem](https://www.3dbrew.org/wiki/Flash_Filesystem) — `firm0`/`firm1` offsets.
- [3dbrew: Title list](https://www.3dbrew.org/wiki/Title_list) — firmware title (`00040138`) and SAFE_MODE notes.
- [3dbrew: Update Data](https://www.3dbrew.org/wiki/Update_Data) — per-version update contents.
- [3dbrew: Memory layout](https://www.3dbrew.org/wiki/Memory_layout) — TWL/AGB memory differences.
