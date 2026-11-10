# CTR titles

On the 3DS, anything installable is a **title**: a game, a demo, DLC, an update, a system app, a system module, even the firmware itself. Each has a 64-bit **title ID** that says what kind of thing it is, a version number, a metadata file listing its contents, and — for downloads — a ticket that carries the right to use it.

This model was inherited from the Wii and the DSi. It lets the system install, update and delete software uniformly, and it is what the eShop and system updates deliver.

## Behavior

Titles live either on the internal [NAND](nand-filesystem.md) (system titles and applications) or on the [SD card](sd-filesystem.md) (applications only). Installing writes the new content files and metadata beside the old; once installation is finalised the system's title database switches over to the new version.

A title's rights are expressed by a **ticket**. Tickets for eShop purchases are tied to one console and account; system titles use a *common ticket* (`cetk`) shared by every console. The ticket holds the title's encryption key, itself encrypted.

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

**Unique ID ranges** (known for developer titles, retail appears to follow): System `0x0–0x2FF`, Application `0x300–0xF7FFF`, Evaluation `0xF8000–0xFEFFF`, Prototype `0xFF000–0xFF3FF`, Developer `0xFF400–0xFF7FF`. The high nibble of the low 32 bits of the program ID relates to the hardware a title may run on; a value of `0x2` means the title runs only on a New 3DS.

Regions are not stored in the title ID. Region ordering appears sequential (JPN, USA, EUR, then KOR, CHN, and TWN is 3DS-only).

**Common title ID prefixes:** `00040010` system applications, `0004001B` system data archives, `00040030` system applets, `0004009B` shared data archives, `000400DB` system data archives, `00040130` system modules, `00040138` system firmware; `00040001` Download Play titles; `0004800*` TWL titles.

### Version numbers

A 16-bit version: 6-bit major (matches the NCCH remaster version), 6-bit minor, 4-bit micro. Example: v2069 = `0b000010 000001 0101` = v2.1.5.

### Storage layout

| Item | NAND | SD |
| ---- | ---- | -- |
| Title data | `nand/title` | `sdmc/Nintendo 3DS/<ID0>/<ID1>/title` |
| Save data | `nand/data/<ID0>/sysdata` | per-title `data` directory |

Per-title structure: `<Title ID High>/<Title ID Low>/{00000000.ctx, content/{<ContentID>.app, <ContentID>.tmd, cmd/<ContentID>.cmd}}`; SD titles add `data/00000001.sav`. DLC titles (`0004008C`) use a different layout with an index separator. Title data on the SD card is encrypted with a console-unique key; NAND title data is cleartext. The initial counter for each file is derived from its path and never changes. When a newer version installs, the old files remain until the install is finalised.

### Title metadata (TMD)

All big-endian. The **TMD** lists a title's contents and their hashes.

| Offset | Size | Description |
| ------ | ---- | ----------- |
| `0x0` | Y | Signature data (type, signature, padding to `0x40`) |
| Y | `0xC4` | Header |
| `0xC4+Y` | `0x24 × 64` | Content info records |
| `0x9C4+Y` | `0x30 × ContentCount` | Content chunk records |

Signature types: `0x010003` RSA-4096/SHA-256, `0x010004` RSA-2048/SHA-256, `0x010005` ECDSA/SHA-256 (SHA-1 types exist but are unused on the 3DS). The signature covers the header.

Header fields: issuer (`0x40`), version, CRL versions, system version (`0x44`, 8 bytes), title ID (`0x4C`), title type (`0x54`), group ID, save data size (`0x5A`, little-endian), SRL private save size, SRL flag, access rights (`0x98`), title version (`0x9C`), content count (`0x9E`), boot content (`0xA0`), SHA-256 of the content info records (`0xA4`).

Each content chunk record: content ID (4), content index (2), content type (2), size (8), SHA-256 (32). Content type flags: `1` encrypted, `2` disc, `4` CFM, `0x4000` optional, `0x8000` shared. Index `0000` = main content (`.CXI` / `.CFA` / `.SRL`), `0001` = Home Menu manual, `0002` = DLP child container.

Certificate chain (when fetched from Nintendo's CDN): TMD certificate `CP0000000b` (retail; `CP0000000a` debug), RSA-2048; CA `CA00000003` (retail; `CA00000004` debug), RSA-4096, issued by Root whose public key is stored in `NATIVE_FIRM`.

### Tickets

Tickets are big-endian: signature data followed by a `0x210`-byte body. The body includes issuer, an ECC public key, version (always 1), the 16-byte **titlekey** (normal key encrypted by a common key), ticket ID, console ID, title ID, ticket title version, license type, an index (`0xB1`) selecting one of six **common keyY** values stored in Process9, an eShop account ID, audit, limits (demos: first `u32` = 4, second = max play count) and a content index. The titlekey decrypts content downloaded from the CDN with AES-128-CBC; the IV is the content index as a big-endian `u16` padded with zeroes. Certificates: ticket `XS0000000c` (retail; `XS00000009` debug), RSA-2048; CA as above.

## Homebrew notes

Installing unsigned or modified titles requires signature checks to be disabled ([custom firmware](../homebrew/custom-firmware.md)); installers such as FBI or the dev menu handle CIA files ([file formats](file-formats.md)). Ticket forging and tools around `movable.sed` are covered under [exploits](../homebrew/exploits.md). The title list on 3dbrew documents every system title and the changes in each system update.

## Open questions

- The title-ID structure is "not documented fully yet" (3dbrew).
- Whether the unique-ID ranges are enforced on retail units is unknown.

## Sources

- [3dbrew: Titles](https://www.3dbrew.org/wiki/Titles) — title ID, versions, directory structure.
- [3dbrew: Title metadata](https://www.3dbrew.org/wiki/Title_metadata) — TMD layout.
- [3dbrew: Ticket](https://www.3dbrew.org/wiki/Ticket) — ticket layout and keys.
- [3dbrew: Title list](https://www.3dbrew.org/wiki/Title_list) — title ID prefixes and system titles.
- [3dbrew: Update Data](https://www.3dbrew.org/wiki/Update_Data) — per-update title changes.
