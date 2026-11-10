# CTR file formats

The 3DS wraps its code and data in a small family of container formats that nest inside each other. The outermost containers hold *partitions* (**NCSD**: cartridge images and the internal flash), the inner ones hold an executable or a data archive (**NCCH**), and a third kind packs everything for installation (**CIA**). Knowing which container you are looking at is half of any reverse-engineering task on this console.

This page is a map; the byte-level details of titles, tickets and metadata are on the [titles page](titles.md), and encrypted save containers are on the [savegame page](savegames.md).

## Behavior

| Container | File extension | What it holds | Where it appears |
| --------- | -------------- | ------------- | ---------------- |
| **NCSD** | `.cci` (cartridge image), `.csu` (system update) | Up to 8 partitions, each an NCCH (or a firmware/other partition) | Game cards, the internal [NAND](nand-filesystem.md) |
| **NCCH** | `.cxi` (executable), `.cfa` (data archive), `.app` (the per-console encrypted installed form), `.caa` | Code, exheader, icon, banner, logo, RomFS | Inside CCI and CIA; installed on NAND/SD |
| **CIA** | `.cia` | A title: [TMD](titles.md#title-metadata-tmd), [ticket](titles.md#tickets), certificates, and up to 65,535 NCCH contents | Downloaded/installable software |
| **DISA** | `.sav` | A save-data container holding a FAT filesystem | [Savegames](savegames.md) |

An **NCCH** consists of: the **code** (ExeFS `.code`), the **exheader** (permissions and kernel capability descriptors), the **plain region** (lists SDK library add-ons), an **icon**, a **banner** (a model and a sound shown on the Home Menu), a **logo** (splash screen) and a **RomFS** (read-only filesystem for resources). Typical uses: an executable image with all of those, an e-Manual archive, a DLP child archive, an update-data archive, DLC index/data archives.

A **CCI** is limited to 8 NCCH files (executable image at index 0, manual at 1, DLP child at 2, update data at 7) and only for applications. A **CIA** can hold any title type.

## Technical details

### Layers of trust

Each layer is independently signed: the NCSD header (RSA-2048/SHA-256) by Nintendo, the NCCH (RSA-2048/SHA-256) and its encryption (AES-128-CTR), and the title (TMD + ticket, RSA-2048/4096 or ECDSA). See [security architecture](security-architecture.md).

### Other formats listed by 3dbrew

| Group | Formats |
| ----- | ------- |
| Descriptors | `.BSF` (banner/icon description), `.RSF` (CCI/CXI build options), `.DESC` |
| ExeFS | `.AXF` (ARM ELF before linking), `.BNR` (banner: `.CBMD` + `.BCWAV`), `.ICN` (icon, names, flags) |
| RomFS | `.ARC` (LZ-10 compressed archive), `.BCMA` (manual), `.SARC` (uncompressed archive), `.BCFNT`/`.BFFNT` (fonts v3/v4) |
| Title installation | `.CIA`, `.TMD`, `.CMD` (content index of installed `.app` files, encrypted), `.CTX` (transfer state when downloading) |
| Homebrew | `.3dsx` (see [development workflow](../homebrew/development-workflow.md)), `.smdh` |

### Makerom

`makerom` (by 3dsguy, maintained by profi200; version 0.15 documented) builds NCCH, CCI and CIA files from ELF code and RSF/descriptor files. Required NCCH sets for CIA: applications (executable image required; manual and DLP child optional), system applets/modules (executable image), system data archives (data archive), DLC (index archive plus data archives).

## Homebrew notes

`makerom`, `bannertool` and related tools are listed in the [development workflow](../homebrew/development-workflow.md). NCCH content encryption was reshuffled by Nintendo over time (e.g. system version 7.0.0 added RSA-based key derivation), which complicates decrypting retail content — see [history](../homebrew/history.md).

## Open questions

- The byte layout of NCSD/NCCH headers, ExeFS and RomFS is not reproduced here; it is documented on 3dbrew's NCSD/NCCH/ExeFS/RomFS pages, which are outside this source set.

## Sources

- [3dbrew: File formats category](https://www.3dbrew.org/wiki/Category:File_formats) — format list.
- [3dbrew: Makerom](https://www.3dbrew.org/wiki/Makerom) — NCCH/CCI/CIA overview and configuration.
- [3dbrew: Title metadata](https://www.3dbrew.org/wiki/Title_metadata) and [Ticket](https://www.3dbrew.org/wiki/Ticket) — CIA contents.
- [Rodrigo Copetti, Nintendo 3DS Architecture: Anti-Piracy and Homebrew](https://www.copetti.org/writings/consoles/nintendo-3ds/) — NCSD/NCCH chain of trust.
