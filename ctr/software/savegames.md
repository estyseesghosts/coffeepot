# CTR savegames

A 3DS game stores progress in one of three places: a small flash chip inside the game card, the SD card, or the internal flash. Whichever it is, the data is wrapped in a container called **DISA** that holds a miniature FAT filesystem — and on game cards a further layer of encryption and wear-levelling sits underneath.

This page covers the format and protection of those saves. Where the files sit on SD and NAND is on the [SD](sd-filesystem.md) and [NAND](nand-filesystem.md) pages.

## Behavior

- **Game card saves** live in a serial flash chip next to the ROM (128 KB or 512 KB; some cards are all-flash). They are encrypted so one card's save cannot be copied to another, and the console wears the chip evenly using a block map and journal.
- **SD saves** (downloaded games) are stored with the title; downloaded software may request up to 1 MB of SD storage, unless the retail version of the game needs more, in which case it gets as much as the cartridge provides. See [sd-filesystem](sd-filesystem.md).
- **System titles** keep saves in NAND under `nand/data/<ID0>/sysdata`.
- When a flash save is all `0xFF` the game assumes it is uninitialised and creates default data without asking the player.

## Technical details

### Container

Saves use the **DISA** container; inside it is a FAT filesystem. The same AES-CMAC key Y signs the DISA container and encrypts the gamecard layer.

### Game card encryption

AES-CTR is applied over the DISA container, below the wear-levelling layer (if any). Several revisions exist:

| Introduced in | Old 3DS | AES key slots (encryption / CMAC) | Key Y method | Repeating counter? |
| ------------- | ------- | --------------------------------- | ------------ | ------------------ |
| Initial version | yes | `0x37` / `0x33` | v1 | yes |
| 2.0.0-2 | yes | `0x37` / `0x33` | v2 | yes |
| 2.2.0-4 | yes | `0x37` / `0x33` | v2 | no |
| 6.0.0-11 | yes | `0x37` / `0x33` | v3 | no |
| 9.6.0-X | no (New 3DS) | `0x1A` / `0x19` | v2? | no |

**Repeating-counter weakness:** in the earliest scheme the keystream repeated every 512 bytes. Split a save into 512-byte chunks, discard the all-`0xFF` chunks, and the most common remaining chunk is the keystream, which lets the whole save be decrypted without the key.

**Key Y generation:**

- **v1:** first 8 bytes of the plaintext CXI access-descriptor signature, then the card's ID values `CardID0` and `CardID1` (from card commands `0x90` and `0xA0`).
- **v2:** first `0x10` bytes of SHA-256 over the first 8 signature bytes and a `0x40`-byte value read from the card (the same data returned by `GetRomId`, the card's unique ID).
- **v3** (6.0.0-11 and later; enabled by NCSD partition flags; all retail games finalised after that release use it): SHA-256 over the 8 signature bytes, the `GetRomId` data, the CXI program ID and the ExeFS `.code` hash, then an AES-CMAC of that hash with a key in slot `0x2F`. The slot-`0x2F` key Y is generated through the RSA engine during `NATIVE_FIRM` start-up.

SD and NAND saves are plain DISA containers once the common SD/NAND encryption layer is removed; the AES-CTR key-Y layer and wear levelling described here apply only to game-card saves [documented: 3dbrew Savegames].

### Wear levelling

Only used on "CARD1" game cards: a block map at offset 0 of the flash chip, immediately followed by a journal. The initial state is the block map and the journal is applied on top. Two versions are known: V1 for 128 KB and 512 KB chips, V2 for 1 MB chips (uncommon; Pokemon Sun/Moon is the example 3dbrew gives).

| Part | V1 | V2 |
| ---- | -- | -- |
| Header | two 32-bit integers of unknown purpose (they grow as the save is rewritten) | same |
| Block map entry | 10 bytes: `phys_sec` (bit 7 = initialised), `alloc_cnt`, 8 checksum bytes | 2 bytes: `alloc_cnt`, `phys_sec` (bit 7 on the first byte = initialised), no checksums |
| Entry count | `flash_size / 0x1000 - 1`, one per 0x1000 sector counting from physical sector 1 (sector 0 holds map and journal) | same |
| CRC16 after the map | directly after the last entry | at `0x3FE`, preceded by zero padding |

The CRC16 covers all bytes before it (CRC-16/MODBUS style: polynomial `0xA001`, initial `0xFFFF`). Journal entries are a pair of identical `journal_entry_half` records (virtual/physical sector, previous mappings, realloc counts, checksums) plus a 32-bit field that is `0xFFFFFFFF` on newer systems; the rest of the first 0x1000 bytes is `0xFF`. Each checksum byte is the CRC16 of one encrypted 0x200-byte block with its two bytes XORed together [documented: 3dbrew Savegames].

## Homebrew notes

Save tools exist for extracting and editing DISA saves, extdata and title databases (`save3ds`, `3ds-save-tool`, and older FUSE-based tools; the older ones mishandle the format). Game-specific save exploits (a save file crafted to overflow a parser) form a whole class of [secondary userland exploits](../homebrew/exploits.md), and the 3DS cards' save chips can be read and written with a "save dongle" per 3dbrew.

## Open questions

- Details of the DISA, DIFI and DPFS (journaling tree) layers are not in the sources used here.
- The purpose of the two unknown header integers and of the duplicated journal half is not known (3dbrew says so).

## Sources

- [3dbrew: Savegames](https://www.3dbrew.org/wiki/Savegames) — encryption versions, key Y methods, wear levelling, tools.
- [3dbrew: Gamecards](https://www.3dbrew.org/wiki/Gamecards) — save chip.
- [Rodrigo Copetti, Nintendo 3DS Architecture: Games](https://www.copetti.org/writings/consoles/nintendo-3ds/) — SD save space rule.
- [3dbrew: Homebrew Exploits](https://www.3dbrew.org/wiki/Homebrew_Exploits) — save-based exploits (e.g. oot3dhax installs through save images).
