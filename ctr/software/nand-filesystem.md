# CTR NAND filesystem

The 3DS keeps its system software, settings and built-in apps on a soldered flash chip (**[eMMC](../../glossary.md#emmc)** [NAND](../../glossary.md#nand)). Everything on it is encrypted with keys tied to that console. Inside it is a small partition table and a handful of [FAT](../../glossary.md#fat) filesystems — some for 3DS mode, some for the Nintendo DSi mode the console also supports.

Users never see this chip directly; the operating system exposes pieces of it to programs through filesystem services, and only a few directories are open to normal apps.

## Behavior

The NAND holds: the [firmware partitions](firmware.md) (`firm0` and `firm1`), the 3DS system filesystem (called **CTR-NAND**, volume label `CTR`), a DSi-mode filesystem (`twln`) plus a photo partition (`twlp`), and an optional Game Boy Advance save area. System titles and system save data live in CTR-NAND, in clear text — only the NAND as a whole is encrypted.

Normal ARM11 programs cannot reach any of this except the `twlp` photo partition and a small set of directories (for example `nand:/rw/`). Access to CTR-NAND or TWL-NAND from the ARM11 needs special exheader access flags that retail processes never get. Everything else goes through [Process9](os.md) on the ARM9.

## Technical details

### Sizes and encryption

The chip varies by vendor (see [models](../hardware/models.md)), but the firmware always uses the same partition table sizes: `0x3AF00000` bytes (**943 MiB**) on Old 3DS and `0x4D800000` bytes (**1240 MiB**) on New 3DS. A bigger chip leaves the rest unused.

| Property | Value |
| -------- | ----- |
| Cipher | AES-CTR |
| TWL regions | TWL NAND keyslot; counter nonce is the byte-reversed SHA-1 of the NAND CID |
| CTR regions | CTR NAND keyslots (separate keys for Old and New 3DS); nonce is the SHA-256 of the NAND CID |
| Never-written sectors | plaintext `0x00` or `0xFF` |

A NAND image cannot simply be re-encrypted and used on another console: some files in the `nand` partition are console-specific.

### Layout (Old 3DS | New 3DS)

| Offset | Size | Name | Description |
| ------ | ---- | ---- | ----------- |
| `0x0` | `0x200` | | NCSD header with the CTR partition offsets/sizes; also holds the TWL MBR partition table |
| `0x00000000` | `0x0B100000` | | TWL NAND region (NCSD index 0, key slot `0x03`) |
| `0x00012C00` | `0x200` | | New 3DS only: console-unique encrypted key store |
| `0x00012E00` | `0x08FB5200` | `twln` | DSi FAT16 filesystem |
| `0x09011A00` | `0x020B6600` | `twlp` | DSi photo FAT12 filesystem |
| `0x0B100000` | `0x30000` | | AGB_FIRM GBA savegame (empty if `AGB_FIRM` was never run), key slot `0x07` |
| `0x0B130000` | `0x400000` | `firm0` | Firmware (key slot `0x06`) |
| `0x0B530000` | `0x400000` | `firm1` | Backup firmware |
| `0x0B930000` | `0x2F5D0000` (Old) / `0x41ED0000` (New) | | CTR-NAND partition; key slot `0x04` (Old) or `0x05` (New) |
| `0x0B95CA00` (Old) / `0x0B95AE00` (New) | `0x2F3E3600` / `0x41D2D200` | `nand` | CTR FAT16 filesystem |

Some Old 3DS XL units also have a plaintext FAT16 boot record at `0x4000`. The New 3DS key store at `0x12C00` (0x200 bytes, AES-ECB) contains normal-keys used to derive the New 3DS key slots (`0x15`–`0x1F`) and is identical on all retail New 3DS (and all dev) units.

### CTR-NAND directory tree

```text
nand
├── __journal.nn_
├── data/<ID0>/{extdata, sysdata}   # cleartext; ID0 = SHA-256 hash of movable.sed keyY
├── dbs/                             # title database
├── fixdata/sysdata
├── private/movable.sed
├── ro/   (nandro archive)
│   └── sys/{HWCAL0.dat, HWCAL1.dat}
├── rw/   (nandrw archive)
│   └── sys/{lgy.log, LocalFriendCodeSeed_B, native.log, rand_seed, SecureInfo_A, updater.log}
├── ticket/                          # empty: tickets are in ticket.db
├── title/                           # same structure as the SD card title directory
└── tmp/
```

System titles' save data is stored under `nand/data/<ID0>/sysdata` (not beside the title as on SD). The `movable.sed` key Y is used only for AES MACs here. `native.log` is written by the error display; `lgy.log` by `TWL_FIRM`.

### TWL partition

Mostly the same as a DSi's, except tickets are stored in the CTR FAT filesystem; the `twln/title` structure mirrors the CTR layout but the `.cmd` file is cleartext (probably a dummy), and DSi directory names use the DSi's title-ID high words.

## Homebrew notes

Reading the chip needs its pinout (documented on 3dbrew) and keys from [OTP](../hardware/security-hardware.md); NAND dumping and "redirection to SD card" were key steps in the history of [custom firmware](../homebrew/custom-firmware.md) (the emuNAND concept). Tools like GodMode9 browse NAND once a console has [ARM9 access](../homebrew/boot9strap.md).

## Open questions

- Several offsets refer to "unknown" content (kernel-local files in `nand`).
- Region-changing procedure and NAND redirection are documented on 3dbrew pages outside this source set.

## Sources

- [3dbrew: Flash Filesystem](https://www.3dbrew.org/wiki/Flash_Filesystem) — all offsets, key slots, directory tree.
- [Rodrigo Copetti, Nintendo 3DS Architecture: Operating System](https://www.copetti.org/writings/consoles/nintendo-3ds/) — eMMC sizes and normalised partition sizes.
- [3dbrew: Titles](https://www.3dbrew.org/wiki/Titles) — title directory layout shared with SD.
