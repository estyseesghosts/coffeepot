# CTR title metadata and tickets

Two small signed files describe every installable 3DS [title](titles.md). The **title metadata** (TMD) says what the title consists of and gives a checksum for each piece. The **ticket** says who may use it and carries the key that unlocks the content, itself locked with a console-wide key. Both are signed by Nintendo, and both travel inside a `.cia` installer ([file formats](file-formats.md)) and are fetched from Nintendo's update servers.

This page holds the byte-level layouts; the plain-language story of titles, IDs and installed files is on the [titles page](titles.md). The terms are in the [glossary](../glossary.md#tmd-ticket).

## Behavior

When software is installed, the system checks the TMD signature, then hashes each content file against the TMD. The ticket is checked as the right to run it. System titles (not sold in the eShop) use a **common ticket** (`cetk`) that is identical on every console. Tickets for eShop purchases are generated before download and are unique to the console they were made for.

After installation the important TMD data is copied into the title databases and `.cmd` files, so the stored TMD is a reference copy only ([titles](titles.md)).

## Technical details

All values are big-endian unless a field says otherwise. Offsets below are relative to the section they list. These layouts are the same on Old 3DS and New 3DS; no model difference is shown in the sources.

### Signature data (TMD and ticket)

Both files start with a signature block of the same shape: a 4-byte type, the signature, then zero padding so the block ends on a `0x40` boundary. Call the whole block size `Y`.

| Type value | Method | Signature size | Padding | Note |
| ---------- | ------ | -------------- | ------- | ---- |
| `0x010000` | RSA-4096, SHA-1 | `0x200` | `0x3C` | Unused on the 3DS |
| `0x010001` | RSA-2048, SHA-1 | `0x100` | `0x3C` | Unused on the 3DS |
| `0x010002` | ECDSA, SHA-1 | `0x3C` | `0x40` | Unused on the 3DS |
| `0x010003` | RSA-4096, SHA-256 | `0x200` | `0x3C` | |
| `0x010004` | RSA-2048, SHA-256 | `0x100` | `0x3C` | |
| `0x010005` | ECDSA, SHA-256 | `0x3C` | `0x40` | |

The signature hash is calculated over the TMD header (for a TMD) or over the ticket data (for a ticket).

### TMD layout

| Offset | Size | Description |
| ------ | ---- | ----------- |
| `0x0` | `Y` | Signature data |
| `Y` | `0xC4` | Header |
| `0xC4 + Y` | `0x24 * 64` | Content info records (64 slots; usually only the first is used) |
| `0x9C4 + Y` | `0x30 * ContentCount` | Content chunk records |

**Header** (offsets from the start of the header):

| Offset | Size | Field |
| ------ | ---- | ----- |
| `0x00` | `0x40` | Signature issuer |
| `0x40` | `0x1` | Version |
| `0x41` | `0x1` | `ca_crl_version` |
| `0x42` | `0x1` | `signer_crl_version` |
| `0x43` | `0x1` | Reserved |
| `0x44` | `0x8` | System version |
| `0x4C` | `0x8` | Title ID |
| `0x54` | `0x4` | Title type |
| `0x58` | `0x2` | Group ID |
| `0x5A` | `0x4` | Save data size in bytes (**little-endian**; also the SRL public save size) |
| `0x5E` | `0x4` | SRL private save data size in bytes (little-endian) |
| `0x62` | `0x4` | Reserved |
| `0x66` | `0x1` | SRL flag |
| `0x67` | `0x31` | Reserved |
| `0x98` | `0x4` | Access rights |
| `0x9C` | `0x2` | Title version (see [versions](titles.md#version-numbers)) |
| `0x9E` | `0x2` | Content count |
| `0xA0` | `0x2` | Boot content |
| `0xA2` | `0x2` | Padding |
| `0xA4` | `0x20` | SHA-256 of the content info records |

**Content info record** (`0x24` bytes): content index offset (2), content command count `k` (2), then a SHA-256 (`0x20`) over the next `k` content chunk records that have not yet been hashed.

**Content chunk record** (`0x30` bytes, one per content, count from the header):

| Offset | Size | Field |
| ------ | ---- | ----- |
| `0x00` | 4 | Content ID |
| `0x04` | 2 | Content index |
| `0x06` | 2 | Content type (flags below) |
| `0x08` | 8 | Content size |
| `0x10` | `0x20` | SHA-256 of the content |

| Content type flag | Meaning |
| ----------------- | ------- |
| `0x0001` | Encrypted |
| `0x0002` | Disc |
| `0x0004` | CFM (3dbrew does not know what the abbreviation stands for) |
| `0x4000` | Optional |
| `0x8000` | Shared |

| Content index | Content (not valid for DLC) |
| ------------- | --------------------------- |
| `0000` | Main content: `.CXI` executable, `.CFA` data archive (system data archives) or `.SRL` (TWL content) |
| `0001` | Home Menu manual (`.CFA`) |
| `0002` | Download Play child container (`.CFA`) |

> Unknown: 3dbrew says the "System version" and "Title type" fields exist but does not decode them. 3dbrew also notes that an example struct once on the page was wrong and was removed; do not trust old third-party TMD structs without checking against these tables.

### Ticket layout

| Offset | Size | Description |
| ------ | ---- | ----------- |
| `0x000` | `Y` | Signature data |
| `Y` | `0x210` | Ticket data (below, plus a variable-size content index at the end) |

Ticket data. The v1 format is the 3DS update of the Wii/DSi (v0) format.

| Offset | Size | Field |
| ------ | ---- | ----- |
| `0x00` | `0x40` | Issuer |
| `0x40` | `0x3C` | ECC public key |
| `0x7C` | `0x1` | Version (always 1 on the 3DS) |
| `0x7D` | `0x1` | `CaCrlVersion` |
| `0x7E` | `0x1` | `SignerCrlVersion` |
| `0x7F` | `0x10` | **Titlekey**, encrypted with a common key (see below) |
| `0x8F` | `0x1` | Reserved |
| `0x90` | `0x8` | Ticket ID |
| `0x98` | `0x4` | Console ID |
| `0x9C` | `0x8` | Title ID |
| `0xA4` | `0x2` | Reserved |
| `0xA6` | `0x2` | Ticket title version (usually equal to the TMD version; it does not have to match to be valid) |
| `0xA8` | `0x8` | Reserved |
| `0xB0` | `0x1` | License type |
| `0xB1` | `0x1` | Index of the common keyY used for this ticket; usually `0x1` for retail system titles |
| `0xB2` | `0x2A` | Reserved |
| `0xDC` | `0x4` | eShop account ID (3dbrew marks this with a question mark) |
| `0xE0` | `0x1` | Reserved |
| `0xE1` | `0x1` | Audit |
| `0xE2` | `0x42` | Reserved |
| `0x124` | `0x40` | Limits |
| `0x164` | X | Content index (variable size) |

- **Limits:** in demo tickets the first `u32` is `0x4` and the second `u32` is the maximum play count.
- **Content index size:** the block describes its own size, at least 20 bytes; the second `u32` (big-endian) gives the full value of X.

### Decrypting the titlekey and the content

1. The common keyY is chosen by the index at ticket offset `0xB1` from a plaintext array of six common keyYs stored in the data section of Process9.
2. The titlekey is decrypted on the hardware AES engine with the ticket common-key keyslot, **AES-128-CBC**, using the **big-endian title ID** as the IV.
3. Content downloaded from the CDN is decrypted with the titlekey, AES-128-CBC, with the content index (big-endian `u16`, padded with trailing zeroes) as the IV.

On a retail unit common keyY index 0 is a real keyY run through the hardware key scrambler; on a development unit index 0 is the dev common key, which is a normal-key ([dev units](dev-unit-software.md)). Index 1 of the retail set is the one used for system titles.

### Certificate chain

A TMD or ticket fetched from Nintendo's CDN (or by SOAP) has two certificates appended. Remove them to get the plain file.

| Certificate | Signature type | Retail name | Debug name | Verifies |
| ----------- | -------------- | ----------- | ---------- | -------- |
| TMD | RSA-2048 | `CP0000000b` | `CP0000000a` | the TMD signature |
| Ticket | RSA-2048 | `XS0000000c` | `XS00000009` | the ticket signature |
| CA | RSA-4096 | `CA00000003` | `CA00000004` | the TMD or ticket certificate |

The CA certificate is issued by `Root`, whose public key is stored in `NATIVE_FIRM` ([security architecture](security-architecture.md)).

### Where tickets come from

- **Common tickets (`cetk`)** exist for titles that are not sold in the eShop, such as system titles. They can be fetched over HTTP from the update server by replacing `tmd` with `cetk` in the title's TMD URL.
- The **NIM** system module fetches system tickets by the SOAP request `GetSystemCommonETicket` rather than downloading `cetk` files over HTTPS. The `cetk` files are also reachable over plain HTTP although the console never uses that route [documented: 3dbrew Ticket].
- Installed tickets are kept in `ticket.db`; the `nand/ticket` directory is empty ([NAND](nand-filesystem.md)).

## Homebrew notes

Installing titles with modified or forged TMDs and tickets needs signature checks to be bypassed on the console ([custom firmware](../homebrew/custom-firmware.md)); `makerom` writes the TMD and ticket for homebrew CIAs ([file formats](file-formats.md)). A 3DS also using the eShop or system updates should not mix in custom CIAs without care, because conflicts are likely (3dbrew's Makerom page). Older tools such as "trap15's TMD parser" exist but their structs should be checked against the tables here.

## Open questions

- Meaning of the TMD "System version", "Title type", "Group ID", "Access rights" and of the ticket "License type", "Audit" and `0xDC` fields is not given by the sources.
- Whether the unused SHA-1 signature types are rejected by Process9 or merely never seen is not stated.

## Sources

- [3dbrew: Title metadata](https://www.3dbrew.org/wiki/Title_metadata) — TMD layout, signature types, content flags, certificates (page last edited 28 July 2020).
- [3dbrew: Ticket](https://www.3dbrew.org/wiki/Ticket) — ticket layout, titlekey decryption, cetk (page last edited 14 October 2019).
- [3dbrew: Titles](https://www.3dbrew.org/wiki/Titles) — content indexes and `.cmd`.
- [3dbrew: Makerom](https://www.3dbrew.org/wiki/Makerom) — CIA contents and the warning about mixing CIAs with eShop updates.
