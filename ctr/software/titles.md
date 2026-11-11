# CTR titles

On the 3DS, anything installable is a **title**: a game, a demo, DLC, an update, a system app, a system module, even the firmware itself. Each has a 64-bit **title ID** that says what kind of thing it is, a version number, a metadata file listing its contents, and — for downloads — a ticket that carries the right to use it.

This model was inherited from the Wii and the DSi. It lets the system install, update and delete software uniformly, and it is what the eShop and system updates deliver.

## Behavior

Titles live either on the internal [NAND](nand-filesystem.md) (system titles and applications) or on the [SD card](sd-filesystem.md) (applications only). Installing writes the new content files and metadata beside the old; once installation is finalised the system's title database switches over to the new version.

A title's rights are expressed by a **ticket**.  Tickets for eShop purchases are tied to one console and account; system titles use a *common ticket* (`cetk`) shared by every console. The ticket holds the title's encryption key, itself encrypted (see [title metadata and tickets](title-metadata-and-tickets.md)).

## Technical details

### Title ID layout

`0xCCCCABCDLLLLLLRR`

| Part | Meaning |
| ---- | ------- |
| `CCCC` | Platform: `0004` = 3DS (the same field uses 5 = Wii U, 3 = DSi, 1 = Wii) |
| `ABCD` | Content category (bit mask, see below) |
| `LLLLLL` | Unique ID (for TWL titles, `SSLLLL`, where `SS` is the old DSi ID) |
| `RR` | Variation (e.g. `02` = system title for core version 2, `03` = SAFE_MODE system title) |

**Content categories** (`ABCD` bit mask):

| Category | Mask |
| -------- | ---- |
| Normal | `0x0` |
| DlpChild | `0x1` |
| Demo | `0x2` |
| Contents | `0x3` |
| AddOnContents (DLC) | `0x4` |
| Patch | `0x6` |
| CannotExecution | `0x8` |
| System | `0x10` |
| RequireBatchUpdate | `0x20` |
| NotRequireUserApproval | `0x40` |
| NotRequireRightForMount | `0x80` |
| CanSkipConvertJumpId | `0x100` |
| TWL | `0x8000` |

TWL titles carry over the DSi category masks, so TWL system titles have `0x8001`. Bit `0x4000` appears reserved. System titles (CTR and TWL) are eligible for system updates.

**Unique ID ranges** (known for developer titles, retail appears to follow): System `0x0–0x2FF`, Application `0x300–0xF7FFF`, Evaluation `0xF8000–0xFEFFF`, Prototype `0xFF000–0xFF3FF`, Developer `0xFF400–0xFF7FF`. The high nibble of the low 32 bits of the program ID (mask `0xF0000000`; unique-ID mask `0xF00000`) relates to the hardware a title may run on and is normally zero; 3DS code tends to clear it when handling program IDs. **New 3DS:** a value of `0x2` (program-ID low `0x20000000`) means the title runs only on a New 3DS; an Old 3DS never runs those titles.

Prototype-range unique IDs appear on the Home Menu after install without the usual gift fanfare. 3dbrew calls the Evaluation and Developer ranges uncertain (`?`). Regions are not stored in the title ID. Region ordering appears sequential (JPN, USA, EUR, then KOR, CHN, and TWN is 3DS-only).

**Common title ID prefixes** (a data-only example is the [country/region archive](country-region-data.md), `0004009B00010402`): `00040010` system applications, `0004001B` system data archives, `00040030` system applets, `0004009B` shared data archives, `000400DB` system data archives, `00040130` system modules, `00040138` system firmware; `00040001` Download Play titles; `00048005`, `0004800F` and `00048004` are TWL (DSi) system applications, system data archives and DSiWare ports. The full list is on the [title list](title-list.md).

The terms "Title ID", "Partition ID" and "Media ID" are interchangeable on 3dbrew. The variation byte is used mainly by non-Normal categories; for Download Play children it indexes up to 255 children per unique ID.

### Version numbers

A 16-bit version: 6-bit major (matches the NCCH remaster version), 6-bit minor, 4-bit micro. Dev Menu installs CTR applications to NAND with variation `02`; variation is ignored for TWL titles (carried over from the DSi ID and used for region lock). Normally SAFE_MODE ARM11 CXI titles cannot be launched on retail, because the system core version does not match the CXI exheader's. The version's micro part is called "build" in some contexts; the CVer title's RomFS build field has always been zero regardless of the TMD, so firmware-version prediction must account for that [documented: 3dbrew Titles]. Example: v2069 = `0b000010 000001 0101` = v2.1.5.

### Storage layout

| Item | NAND | SD |
| ---- | ---- | -- |
| Title data | `nand/title` | `sdmc/Nintendo 3DS/<ID0>/<ID1>/title` |
| Save data | `nand/data/<ID0>/sysdata` | per-title `data` directory |

Per-title structure: `<Title ID High>/<Title ID Low>/{00000000.ctx, content/{<ContentID>.app, <ContentID>.tmd, cmd/<ContentID>.cmd}}`; SD titles add `data/00000001.sav`. DLC titles (`0004008C`) use a different layout with an index separator. Title data on the SD card is encrypted with a console-unique key; NAND title data is cleartext. The initial counter for each file is derived from its path and never changes. When a newer version installs, the new `.app`, `.tmd`, `.cmd` (and `.ctx` on NAND) are written beside the old ones; on finalising, the entry in `import.db` moves to `title.db`, the system switches to the new files, and then the old files are deleted. The `<ContentID>.tmd` filename's content ID starts at `00000000` and rises by 1 per update; the file is redundant after install (data lives in `title.db` and `.cmd`) but is kept. The decrypted TMD is also served by Nintendo's CDN with a certificate chain appended; stripping the chain gives the plain TMD. Unlike TMDs, CDN NCCH files are only available encrypted [documented: 3dbrew Titles].

### Title metadata (TMD)

The **TMD** lists a title's contents (content ID, index, type, size, SHA-256) and carries the title version and save size. It is signed with RSA-4096, RSA-2048 or ECDSA plus SHA-256, and the content with index `0000` is the main executable or archive. Full layout, signature table, content flags and certificates are on [title metadata and tickets](title-metadata-and-tickets.md).

### Tickets

A ticket holds the title's 16-byte **titlekey**, encrypted with one of six common keys (the index is at ticket offset `0xB1`), plus ticket ID, console ID, title ID and limits. The titlekey is decrypted with AES-128-CBC using the big-endian title ID as IV, and then decrypts CDN content with the content index as IV. Layout, `cetk` common tickets and certificates are on [title metadata and tickets](title-metadata-and-tickets.md).

### Files in the title directory

| File | Meaning |
| ---- | ------- |
| `<ContentID>.app` | An NCCH file ([file formats](file-formats.md)); the content ID is the `u32` from the TMD. One title can have several: the executable (CXI) plus extra CFAs such as the manual and the Download Play child container. The content index decides the role (index `0000` executable, `0001` manual, `0002` DLP child; not valid for DLC). Deleting the manual's `.app` makes the manual fail to open; deleting the executable's makes the title not start and the 3D banner disappear (the banner is read from the executable each time Home Menu loads, unlike the icon and name which are cached). |
| `<ContentID>.tmd` | The TMD. Content ID starts at `00000000` and rises by 1 per installed update. |
| `cmd/<ContentID>.cmd` | Content-meta file: indexes the installed `.app` files. Content ID starts at `00000001` and rises by 1 each time `.app` files are added or removed. It is part of the title DRM: `title.db` stores the `.cmd` content ID but not a hash of the file. |
| `00000000.ctx` | Download state file (see below). |
| `data/00000001.sav` | The title's save game (SD titles). Renaming it makes Home Menu hang when launching the title; modifying it causes the usual save corruption errors. |

**`.cmd` layout** (cleartext form; the file is encrypted on the card). The AES-CMACs are used only for SD titles, NAND Download Play titles and non-system DSiWare; for other titles they are all zero.

| Offset | Size | Description |
| ------ | ---- | ----------- |
| `0x0` | 4 | The `.cmd` content ID (start of the header) |
| `0x4` | 4 | Number of AES-CMACs and content IDs in the first list (X) |
| `0x8` | 4 | Number of content IDs in the second list (Y) |
| `0xC` | 4 | CMD format version, usually 1 |
| `0x10` | `0x10` | AES-CMAC over the first `0x10` bytes |
| `0x20` | `4 * X` | Installed content IDs in content-index order; missing contents are `0xFFFFFFFF` |
| `0x20 + 4*X` | `4 * Y` | Installed content IDs in ID order |
| `0x20 + 4*(X+Y)` | `0x10 * X` | AES-CMAC of each content in the first list |

X follows the highest installed content index: a title with 5 contents of which only 1 and 3 are installed still has 3 CMACs, the second unused. For SD contents each CMAC is made by taking the NCCH header without signature (`0x100`-`0x1FF`), appending the content index and content ID (both `u32`), computing SHA-256, then an AES-CMAC with the SD/NAND AES-CMAC key. TWLNAND contents use the same process, even for SRL contents, with the NAND-dbs keyslot.

**`.ctx` files.** On the SD card, `00000000.ctx` exists only while an eShop title is being downloaded and is deleted when the download finishes (it is about 1.5 KB). It contains an `AP0000000000000000` certificate signed by the CTCert, followed by signed data that is probably an ECDSA public key. Just after purchase the area past the header is all `0xFF`; during download it is filled with the console's APcert, which is how the "APcert infoleak" in [system flaws](system-flaws.md) works. NAND `.ctx` files use a completely different format (CTXT) and exist only for titles whose installation is not yet finalised, for example an unfinalised system update.

### Directory names

On SD, `<ID0>` is the first `0x10` bytes of a SHA-256 hash (the same name appears under `nand/data`) and `<ID1>` is generated from the SD card CID ([SD filesystem](sd-filesystem.md)).

### DLC titles

All DLC for one application is installed under the same title ID (the `0004008C` high word), but add-ons are usually separate purchases, so the directory layout lets any combination be installed. DLC can receive updates, usually more DLC and/or fixes; individual DLC content can only be managed from inside the application.

| Item | Meaning |
| ---- | ------- |
| `<IndexSeparator>` directories | DLC NCCH contents are split by content index, starting at `00000000`, one directory per 256 contents. All are created at install time, so some may be empty. |
| `<ContentID>.tmd` | Not modified by the 3DS; lists every DLC content, installed or not. |
| `<ContentID>.cmd` | Lists the installed DLC contents; its content ID changes each time DLC is installed or removed. |

### Installing other-model system titles

When already-downloaded Old 3DS system titles are finalised onto a New 3DS NAND image running New 3DS `NATIVE_FIRM`, the New 3DS version of those titles (title-ID low mask `0x20000000`) is deleted. 3dbrew does not know where this is implemented (possibly `NIM`, `AM` or `AMXPXI`).

> Unknown: the behaviour is documented only for Old 3DS titles on a New 3DS NAND; the reverse direction is not described.

## Homebrew notes

Installing unsigned or modified titles requires signature checks to be disabled ([custom firmware](../homebrew/custom-firmware.md)); installers such as FBI or the dev menu handle CIA files ([file formats](file-formats.md)). Ticket forging and tools around `movable.sed` are covered under [exploits](../homebrew/exploits.md). The [title list](title-list.md) documents the system titles and [update data](update-data.md) the changes in early system updates.

## Open questions

- The title-ID structure is "not documented fully yet" (3dbrew).
- Whether the unique-ID ranges are enforced on retail units is unknown.

## Sources

- [3dbrew: Titles](https://www.3dbrew.org/wiki/Titles) — title ID, versions, directory structure.
- [3dbrew: Title metadata](https://www.3dbrew.org/wiki/Title_metadata) — TMD layout.
- [3dbrew: Ticket](https://www.3dbrew.org/wiki/Ticket) — ticket layout and keys.
- [3dbrew: Title list](https://www.3dbrew.org/wiki/Title_list) — title ID prefixes and system titles ([title list](title-list.md)).
- [3dbrew: Update Data](https://www.3dbrew.org/wiki/Update_Data) — per-update title changes ([update data](update-data.md)).
