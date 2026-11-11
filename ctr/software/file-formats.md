# CTR file formats

The 3DS wraps its code and data in a small family of container formats that nest inside each other. The outermost containers hold *partitions* (**NCSD**: cartridge images and the internal flash), the inner ones hold an executable or a data archive (**NCCH**), and a third kind packs everything for installation (**CIA**). Knowing which container you are looking at is half of any reverse-engineering task on this console.

This page is a map; the byte-level details of titles, tickets and metadata are on the [titles page](titles.md), and encrypted save containers are on the [savegame page](savegames.md).

## Behavior

| Container | File extension | What it holds | Where it appears |
| --------- | -------------- | ------------- | ---------------- |
| **NCSD** | `.cci` (cartridge image), `.csu` (system update) | Up to 8 partitions, each an NCCH (or a firmware/other partition) | Game cards, the internal [NAND](nand-filesystem.md) |
| **NCCH** | `.cxi` (executable), `.cfa` (data archive), `.app` (the per-console encrypted installed form), `.caa` | Code, exheader, icon, banner, logo, RomFS | Inside CCI and CIA; installed on NAND/SD |
| **CIA** | `.cia` | A title: [TMD](title-metadata-and-tickets.md), [ticket](title-metadata-and-tickets.md#ticket-layout), certificates, and up to 65,535 NCCH contents | Downloaded/installable software |
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

Required NCCH contents by title type: CCI (index 0 executable, 1 manual, 2 DLP child, 7 update data; at most 8 NCCHs; applications only); CIA for applications (index 0 executable required, 1 manual, 2 DLP child), system applet/module (index 0 executable), system data archive (index 0 data archive), DLC (DLC index archive at 0 plus a varying number of DLC data archives). NCCH variants: executable image (code, exheader, plain region, icon, banner, logo, RomFS), e-Manual, DLP child, update data, standalone data, DLC index (icon plus RomFS) and DLC data archives. A CIA holds up to 65,535 NCCHs. Installing custom CIAs on a console that also uses eShop and system updates is unwise because conflicts are likely.

Main `makerom` options (command form: `makerom [general] [rsf] [crypto] [ncch 0 build] [cci] [cia]`):

| Option | Meaning |
| ------ | ------- |
| `-f ncch/cxi/cfa/cci/cia`, `-o path`, `-v` | Output format (ncch/cxi/cfa have no effect), output path, verbose |
| `-rsf path`, `-D<NAME>=<VALUE>` | Rom specification file (YAML); substitute `$(NAME)` in it |
| `-target t/d/p` | Key chain: t test (homebrew), d devkit (incomplete), p retail ("unusable"); affects encryption, signing and `-desc` templates |
| `-ckeyid 0-255`, `-showkeys` | Override the CIA common key; dump the key chain |
| `-elf`, `-icon`, `-banner` | Code, icon and banner inputs |
| `-desc app/ecapp/demo/dlpchild:<fw>` | Hard-coded exheader/accessdesc template (fw = kernel minor version, 1-7); not for titles needing special permissions |
| `-exefslogo` | Put the logo in ExeFS; required for systems below 5.0 |
| `-code`, `-exheader`, `-logo`, `-plainrgn`, `-romfs` | Rebuild an NCCH from extracted parts |
| `-content path:index[:id]` (`-i`) | Add a built NCCH to a CCI (index 0-7) or CIA (index up to `0xFFFF`, content ID random if omitted) |
| `-devcci`, `-nomodtid`, `-alignwr` | CCI: debug CCI; keep included NCCH title IDs; align Card2 writable region |
| `-major`, `-minor`, `-micro`, `-dver`, `-dlc`, `-rand` | CIA: version parts (0-63, 0-63, 0-15), data-title version (0-4095), DLC mode, random titlekey |

Example: `makerom -f cia -o sample.cia -rsf sample.rsf -target t -elf sample.elf -icon sample.icn -banner sample.bnr -desc app:4 -i sample.cfa:1:1` builds a CXI and puts it as index 0 of a CIA together with a CFA. ELF input is produced by the latest devkitARM with ctrulib or by the official SDK. Not run by this wiki (see [development workflow](../homebrew/development-workflow.md)). Other formats from 3dbrew's category list: audio `(B)CWAV`, `(B)CSTM`, `(B)CSAR`; graphics `CBMD`, `CTPK`, `CMDL`, `CTEX`, `CCAD`, `CSKA`/`CSKLA`, `CMATA`, `CMDLA`, `CLGT`, `CPTL`, `CLTS`, `CENV`, `CCAM`, `CMAT`; other `NSA`, `CRO`/`CRS`/`CRR` ([CRO modules](cro-modules.md)), `MPO`, `CGRP`. The 3DS filesystem needs absolute paths and supports 8.3 and long (255 character) names. Of the category's 30 pages, the formats such as `DARC`, `CGFX`, `SHBIN` are outside this source set.

## Homebrew notes

`makerom`, `bannertool` and related tools are listed in the [development workflow](../homebrew/development-workflow.md). NCCH content encryption was reshuffled by Nintendo over time (e.g. system version 7.0.0 added RSA-based key derivation), which complicates decrypting retail content — see [history](../homebrew/history.md).

## Open questions

- The byte layout of NCSD/NCCH headers, ExeFS and RomFS is not reproduced here; it is documented on 3dbrew's NCSD/NCCH/ExeFS/RomFS pages, which are outside this source set.

## Sources

- [3dbrew: File formats category](https://www.3dbrew.org/wiki/Category:File_formats) — format list.
- [3dbrew: Makerom](https://www.3dbrew.org/wiki/Makerom) — NCCH/CCI/CIA overview and configuration.
- [3dbrew: Title metadata](https://www.3dbrew.org/wiki/Title_metadata) and [Ticket](https://www.3dbrew.org/wiki/Ticket) — CIA contents.
- [Rodrigo Copetti, Nintendo 3DS Architecture: Anti-Piracy and Homebrew](https://www.copetti.org/writings/consoles/nintendo-3ds/) — NCSD/NCCH chain of trust.
