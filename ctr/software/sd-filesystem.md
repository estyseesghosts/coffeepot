# CTR SD filesystem

The 3DS uses its SD card (**New 3DS:** microSD) for photos, music, videos and — above all — downloaded games and their save data. The card is formatted normally ([FAT](../../glossary.md#fat)), but the folder the 3DS uses for software is encrypted with keys derived from the console, so a card's games only work on the console that installed them.

## Behavior

On the card the 3DS creates a `Nintendo 3DS` folder. Inside it, one sub-folder named after the console (`<ID0>`) and one named after the card (`<ID1>`) hold everything that console has installed on that card. Moving a card to a different console does not give that console access to the old content.

Photos and videos taken with the camera app (and browser image downloads) go to `DCIM`; software, save data and DSiWare exports go inside `Nintendo 3DS/<ID0>/<ID1>/`. 3D video (added in system version 3.0.0-5) is stored as `.avi` with MJPEG frames.

## Technical details

```text
sdmc
├── DCIM
├── Nintendo 3DS
│   ├── <ID0>
│   │   └── <ID1>
│   │       ├── backups
│   │       ├── dbs
│   │       ├── extdata
│   │       ├── title
│   │       └── Nintendo DSiWare
│   └── Private
└── private
    └── Nintendo 3DS
        └── app
```

### Encryption

Everything under `Nintendo 3DS/<ID0>/<ID1>` is AES-128-CTR encrypted with console-unique key slots initialised from `nand/private/movable.sed`. The counter (CTR) for each file:

1. Take the UTF-16 path relative to `Nintendo 3DS/<ID0>/<ID1>` (starting with `/`, including a null terminator).
2. Compute its SHA-256 hash.
3. CTR byte `i` = hash byte `i` XOR hash byte `16+i`, for `i = 0…15`.

The counter is fixed per file and never changes when the file is rewritten, so XORing two versions of the same file (for example newly created extdata and a later copy) reveals plaintext where the newer version overwrote zeroes. Files under `nand/data/<ID0>` use the same key slot, but only for MACs.

### Directory names

- `<ID0>`: the first `0x10` bytes of a SHA-256 hash (the same ID0 appears under `nand/data`).
- `<ID1>`: derived from the SD card's CID register: rotate the CID left by 8 bits, then shuffle the eight 16-bit words (input index → output index: 6→0, 7→1, 4→2, 5→3, 2→4, 3→5, 0→6, 1→7).

### Directories

| Path | Contents |
| ---- | -------- |
| `DCIM` | Photos and videos (also browser image downloads) |
| `backups` | SD title save-data backups |
| `dbs` | Encrypted title databases for SD titles |
| `extdata` | Extra save data created by titles |
| `title` | SD-installed [titles](titles.md), encrypted |
| `Nintendo DSiWare` | Exported DSiWare |
| `Private/<Title ID Low>` | Cleartext data: `00020400` (Camera, `phtcache.bin`; to make a picture visible rename it to eight digits plus `.mpo` and put it in `DCIM`), `00020500` (Sound, `voice/XX/*.m4a`, XX = 01-10) |
| `/private/Nintendo 3DS/app/<Game Code>/` | Some apps store cleartext data here under their four-letter game code; Flipnote Studio 3D (`JKZP`) keeps numbered `.kwz` files and a `!!.lst` |

Save data for titles on the SD card is stored under each title's `data` folder ([titles](titles.md), [savegames](savegames.md)).

### Capacity

Nintendo recommends cards up to 32 GB; larger cards work (2 TB has been tested) with FAT32 and a 64 KB cluster being the most compatible, but boot gets slower as capacity grows ([models](../hardware/models.md)).

## Homebrew notes

The SD card is user-writable, and its encryption depends on a console key (`movable.sed`), which made it a recurring [exploit](../homebrew/exploits.md) channel: in 2018 *Seedminer* showed `movable.sed` could be brute-forced from a value obtainable in userland, which let users inject crafted DSiWare onto the card; the *miner* exploit family and `bannerbomb3` build on that. Homebrew is typically loaded from the card as `.3dsx` files ([development workflow](../homebrew/development-workflow.md)).

## Sources

- [3dbrew: SD Filesystem](https://www.3dbrew.org/wiki/SD_Filesystem) — layout, encryption counter derivation, ID1 derivation.
- [3dbrew: Titles](https://www.3dbrew.org/wiki/Titles) — SD title directory structure.
- [3dbrew: Flash Filesystem](https://www.3dbrew.org/wiki/Flash_Filesystem) — `nand/data/<ID0>` MACs.
- [3dbrew: Hardware](https://www.3dbrew.org/wiki/Hardware) — SD capacity, formatting.
- [Hacks Guide Wiki: History of 3DS Hacking](https://wiki.hacks.guide/wiki/3DS:History_of_3DS_Hacking) — SD-based exploit families.
