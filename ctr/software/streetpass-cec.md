# CTR StreetPass (CEC) mailboxes

**StreetPass** lets two 3DS consoles that pass each other swap small pieces of game data without the player doing anything. The system module behind it is called **[CEC](../glossary.md#cec)**. Games never talk to other consoles directly: they drop messages into a per-game mailbox, and a background communicator carries them over the wireless link and files whatever arrives from other people.

This page describes how that mailbox system is built and the on-disk format of the mailbox data.

## Behavior

CEC has two cooperating parts, both working on one system save file (`00010026`) that stores every mailbox and message:

```text
Application <=> Mailbox Manager <=> System Save (Mailbox) <=> StreetPass communicator <=> NWM/network <=> another 3DS's communicator <=> ...
```

- The **mailbox manager** is a thick layer over file I/O: it initialises and checks data and exposes most of the `cecd:u` and `cecd:s` [service](services.md) functions.
- The **StreetPass communicator** runs in the background, reads outgoing messages from the system save, sends and receives them, and stores new messages. It has only a few control interfaces, some in `cecd:u`/`cecd:s` and others in `cecd:ndm` (reached through the `ndm:u` service).

## Technical details

### Mailbox manager functions (`cecd:u`, `cecd:s`)

Notation: `[command ID] (replies) Name(params)`; each parameter or reply takes one word except `PID` (process ID descriptor, two words) and `…Buffer` (mapped buffer descriptor, two words).

| ID | Function |
| -- | -------- |
| `0x0001` | `Open(TitleID, CecDataPathType, FileOption, PID)` → result, data size |
| `0x0002` | `Read(DataSize, DataBuffer)` |
| `0x0003` | `ReadMessage(TitleID, BoxType, MessageIDSize, DataSize, MessageIDBuffer, DataBuffer)` |
| `0x0004` | `ReadMessageWithHmac(…, HmacKeyBuffer, …)` |
| `0x0005` | `Write(DataSize, DataBuffer)` |
| `0x0006` | `WriteMessage(…)` |
| `0x0007` | `WriteMessageWithHmac(…)` |
| `0x0008` | `Delete(TitleID, CecDataPathType, BoxType, MessageIDSize, MessageIDBuffer)` |
| `0x0011` | `OpenAndWriteFile(…)` |
| `0x0012` | `OpenAndReadFile(…)` |

`TitleID` here is a 32-bit mailbox identifier, usually the game's title ID. `CecDataPathType` selects a file or folder in the system save (below). `FileOption` is a bit field: bit 1 read, bit 2 write, bit 3 create folder, bit 4 "skip some checks?", bit 30 unknown. `MessageIDSize` is always 8.

### System save layout

```text
[Root]
├── eventlog.dat
└── CEC                                  type 10
    ├── MacFilter___
    ├── MBoxList____                     type 1
    ├── <8-digit ID>                     type 11
    │   ├── MBoxInfo____                 type 2
    │   ├── MBoxData.001                 type 101   (icon)
    │   ├── MBoxData.010                 type 110   (game title, UTF-16)
    │   ├── MBoxData.050                 type 150   (8-byte title ID)
    │   ├── MBoxData.<nnn>               type 100 + nnn
    │   ├── InBox___                     type 12
    │   │   ├── BoxInfo_____             type 3
    │   │   └── _<12-char ID>            type 6     (messages)
    │   └── OutBox__                     type 13
    │       ├── BoxInfo_____             type 4
    │       ├── OBIndex_____             type 5
    │       └── _<12-char ID>            type 7
    └── …
```

### `MBoxList____`

| Offset | Size | Field |
| ------ | ---- | ----- |
| `0x00` | 2 | Magic `0x6868` |
| `0x04` | 4 | Version (always 1) |
| `0x08` | 4 | Number of boxes |
| `0x0C` | 16 × 24 (?) | Box names (16 characters each; usually 8-digit IDs padded with nulls) |

### `MBoxInfo____`

Magic `0x6363`; title ID (matches the box directory) at `0x04`; "private ID" at `0x08` (an application-chosen value; `0x00000000`, `0x00000001`, `0xFFFFFFFF`, `0xAABBCCDD` and random values were observed); a 32-byte HMAC key at `0x10`; 12-byte timestamps for last access (`0x34`) and last receive (`0x44`); several unknown flag bytes.

### `BoxInfo_____`

A `0x20`-byte header (magic `0x6262`, file size, maximum and current box size, maximum and current message count, maximum batch size, maximum message size) followed by an array of `0x70`-byte entries, each a copy of a message header.

### Message files

A message is a `0x70`-byte header, extra headers, the body and a `0x20`-byte HMAC(?). The file name is the message ID in base-64. Header fields:

| Offset | Size | Field |
| ------ | ---- | ----- |
| `0x00` | 2 | Magic `0x6060` |
| `0x04` | 4 | Message size |
| `0x08` | 4 | Header + extra headers size |
| `0x0C` | 4 | Body size |
| `0x10`, `0x14` | 4 + 4 | Title ID, Title ID 2? |
| `0x18` | 4 | Batch ID |
| `0x20` | 8 | Message ID |
| `0x34`–`0x37` | 1 each | Flags, send method, is-unopened, is-new |
| `0x38`, `0x40` | 8 + 8 | Sender ID, Sender ID 2? |
| `0x48`, `0x54`, `0x60` | 12 each | Timestamps: sent, received, created |
| `0x6C`, `0x6D` | 1 + 1 | Send count, forward count |
| `0x6E` | 2 | User data |

Each extra header is a type (4), a data size (4) and the data. Types: 2 icon, 3 game name, 4 info text, 1 and 5 (region?) unknown.

## Homebrew notes

StreetPass stores game-controlled data that other consoles can inject, and its system module has been a research target (the 2019 zero-interaction chain presented at 36C3 used StreetPass tags; see [history](../homebrew/history.md)). The services involved are `cecd:*` and `nwm::CEC` ([services](services.md)).

## Open questions

- The `MBoxList____` row is given as "16 × 24" in the note while the text says 16 characters per name; whether that means 24 names of 16 bytes or 16 names of 24 bytes is not stated.
- The 2019 StreetPass exploit claim above comes from the Hacks Guide history (MrNbaYoh, 36C3), not from the CEC note, which does not discuss security; the CEC format here is a Citra-wiki reverse-engineering and has no 3dbrew counterpart in the local mirror, so it is `[reverse-engineered]` throughout.
- The meaning of `CecDataPathType` values beyond those shown is marked TODO in the source.
- Many fields are labelled "?" (flags, second IDs, HMAC coverage).

## Sources

- Citra project wiki, "3DS CEC (StreetPass) Documentation" — all format and function descriptions above. The `citra-emu/citra` repository that hosted it has since been removed from GitHub, so no live link could be verified.
- [3dbrew: Services API](https://www.3dbrew.org/wiki/Services_API) — `cecd:*` and `nwm::CEC` service names.
- [Hacks Guide Wiki: History of 3DS Hacking](https://wiki.hacks.guide/wiki/3DS:History_of_3DS_Hacking) — StreetPass-based exploit chain.
