# CTR models and variants

Six console models make up the 3DS family. They fall into two generations, written **[Old 3DS and New 3DS](../glossary.md#old-new-3ds)** throughout this wiki: **Old 3DS** means the Nintendo 3DS, Nintendo 3DS XL and Nintendo 2DS (2011–2013); **New 3DS** means the New Nintendo 3DS, New Nintendo 3DS XL and New Nintendo 2DS XL (2014–2017). Games run on both, but the New 3DS has more processor cores, more memory, an optional faster mode, a better 3D screen and extra controls. This page lists the models and the parts they use; for what the extra hardware does, follow the links to the hardware pages.

**Default rule:** every statement in the `ctr/` pages describes the Old 3DS unless it says otherwise. Anything that applies only to the New 3DS, or differs on it, follows in a **New 3DS:** disclaimer. "All models" means both groups. The 2DS and New 2DS XL are named only where they differ in a way that matters (they have no stereoscopic 3D screen).

## Where it fits

Every other hardware page takes the Old 3DS as the baseline and calls out New 3DS differences (see [CPU](cpu.md), [memory](memory.md), [I/O](io-and-peripherals.md)).

## How it works

| Model | Year (first release; regional dates differ and were not checked) | Group | Notes |
| ----- | ---- | ----- | ----- |
| Nintendo 3DS (CTR-001) | 2011 | Old 3DS | First model; 3D top screen |
| Nintendo 3DS XL (SPR-001) | 2012 | Old 3DS | Bigger screens; same architecture |
| Nintendo 2DS (FTR-001) | 2013 | Old 3DS | Cheaper; slate shape; no 3D screen (top and bottom cropped from one panel) |
| New Nintendo 3DS (KTR-001) | 2014 | New 3DS | Quad-core, more RAM, faster clock option, face-tracked 3D, NFC, extra buttons and C-Stick |
| New Nintendo 3DS XL (RED-001) | 2015 | New 3DS | Larger screens than the New Nintendo 3DS |
| New Nintendo 2DS XL (JAN-001) | 2017 | New 3DS | New 3DS hardware without the 3D screen |

The boot ROMs are identical across all models, which matters for [security flaws](../homebrew/boot9strap.md).

## Technical details

### Component table

Values from the 3dbrew teardown table where each model's cell is unambiguous. `?` means the source has no value. In the XL column 3dbrew lists two SoC markings; the plain text of the page does not say which units carry which, so both are shown.

| Component | Nintendo 3DS (Old 3DS) | Nintendo 3DS XL (Old 3DS) | Nintendo 2DS (Old 3DS) | New Nintendo 3DS (New 3DS) | New Nintendo 3DS XL (New 3DS) | New Nintendo 2DS XL (New 3DS) |
| --------- | --- | ------ | --- | ------- | ---------- | ---------- |
| SoC revision (board marking) | `CPU CTR` | `CPU CTR A` / `CPU CTR` | `CPU CTR B` | `CPU LGR A` | `CPU LGR A` | `CPU LGR A` |
| FCRAM | 2×64 MB Fujitsu MB82M8080-07L (one package, two dies) | Fujitsu MB82DBS16641 | Fujitsu MB82DBS1664 | ? | Fujitsu MB82MK9A9A | Fujitsu MB82MK9A9A |
| Top screen | 3.53 in, 3D | 4.88 in, 3D | 3.53 in, cropped from a single panel | 3.88 in, 3D | 4.88 in, 3D | 4.88 in (?) |
| Bottom screen | 3.00 in | 4.18 in | 3.00 in, cropped | 3.33 in | 4.18 in | 4.18 in (?) |

Other parts the source documents (the table does not line every cell up with a model, so they are listed without a model mapping): internal flash from Toshiba (THGBM2G3P1FBAI8 1 GB; THGBMBG4P1KBAIT 2 GB MLC) and Samsung (KLM4G1YEQC 4 GB, usable in SLC mode as ~1.3 GiB; KLM4G1FEPD 4 GB), changing with production date; speaker/microphone/circle-pad/touch codec from the TI AIC3010 family; Wi-Fi Atheros AR6014 (Foxconn J27H023.01 in the 2DS); infrared ICs from NXP (S750); MCU Renesas "UC CTR"/"UC KTR"; PMIC TI 93045A4; charging IC TI BQ24072; gyroscope Invensense ITG-3270; accelerometer ST LIS331DH.

SoC family: Old 3DS parts are marked "CPU CTR" (revisions A and B exist), New 3DS parts "CPU LGR". The SoC (marked "Nintendo 1048 0H" on 3dbrew) contains the CPU, GPU, DSP and VRAM (6 MB); having so much in one chip lowers cost and power, simplifies the board and makes tampering harder (3dbrew). The Old 3DS SoC lacks the New 3DS-only parts described elsewhere. **New 3DS:** the SoC adds four cores and L2 cache ([CPU](cpu.md)), extra memory ([memory](memory.md)), MVD decoder and NFC/head-tracking ([I/O](io-and-peripherals.md)).

### Capacity notes

Internal flash partition sizes are fixed by the firmware regardless of the chip: **943 MiB** used on the Old 3DS. **New 3DS:** 1240 MiB. Larger chips leave space unused (see [NAND filesystem](../software/nand-filesystem.md)). SD cards: Nintendo recommends up to 32 GB; the SD controller accepts much larger cards (2 TB has been tested) at the cost of boot time. FAT32 with a 64 KB cluster is the most compatible choice.

Boot time until the Home Menu appears (icons still loading) after pressing the power button, per 3dbrew:

| SD card | New 2DS XL (**New 3DS**), firmware 11.16.0.49J | Old 3DS, firmware 11.15.0.47J |
| ------- | ----------- | -------- |
| none | 7 s | 8 s |
| 16 GB | 10 s | 11 s |
| 64 GB | 12 s | 11 s |
| 256 GB | 19 s | 20 s |
| 2 TB | 60 s | 60 s |

Capacities of 64 to 128 GB are described as a good compromise (about 0.5 to 1 s extra). The SD controller reportedly supports up to 2.199 TB (32-bit sector numbers); a 2 TB card (AGI TF138) has been tested and works, but boot time grows with capacity. Format as FAT32 with a 64 KB unit size. The Old 3DS column's exact model (3DS, XL or 2DS) is not stated, only "OLD3DS".

### Memory chips

The FCRAM (fast-cycle RAM) is one Fujitsu MB82M8080-07L package on the Old 3DS, which contains two dies branded MB81EDS516545 and MB82DBS08645. The MB81EDS516545 die is a CMOS FCRAM with an LPDDR SDRAM interface and 512 Mbit organised as 64 bits wide (3dbrew). 3dbrew adds that the datasheet it links is for a chip of the same series with less capacity (128 Mbit vs 512 Mbit), so the datasheet does not describe the 3DS chip exactly. Sizes per model are on [memory](memory.md).

### Model identifiers

3dbrew's *Peripherals* page lists model numbers for units and accessories. "(region)" stands for the region code (EUR, JPN, USA, KOR, CHN, TWN, or the subset listed).

| Code | Item | Model group |
| ---- | ---- | ----------- |
| `CTR-001` (region) | Retail Nintendo 3DS (EUR/JPN/USA/KOR/CHN/TWN) | Old 3DS |
| `CTR-001(-01)` (region) | Debug 3DS test unit (EUR/USA/JPN/AUS) | Old 3DS |
| `CTR-001(-04)` (region) | Kiosk 3DS unit (only EUR spotted) | Old 3DS |
| `CTR-001(-06)` | Louvre "Audio Guide" unit (a special debug unit) | Old 3DS |
| `CTR-002` | AR cards pack | accessory |
| `CTR-003` | 3DS / 2DS battery pack | accessory |
| `CTR-004` | 3DS stylus | accessory |
| `CTR-005` | Retail game card | media |
| `CTR-005(-01)` | Developer flash card (same shape as a retail card) | media |
| `CTR-006` | Game box (the EUR box is slightly thicker) | accessory |
| `CTR-007` | Charging cradle | accessory |
| `CTR-008` | Developer flash card (large; replaceable save flash) | media |
| `CTR-009` | Circle Pad Pro | accessory |
| `CTR-010` | 3DS stand (bundled with Kid Icarus: Uprising) | accessory |
| `CTR-012` | NFC adapter | accessory |
| `NVL-001` | amiibo NFC tag | accessory |
| `SPR-001` (region) | Retail Nintendo 3DS XL (EUR/JPN/USA/KOR/CHN/TWN) | Old 3DS |
| `SPR-001(-01)` | Debug 3DS XL test unit | Old 3DS |
| `SPR-003` | (New) 3DS XL battery pack | accessory |
| `SPR-004` | 3DS XL stylus | accessory |
| `SPR-009` | Circle Pad Pro LL/XL | accessory |
| `KTR-001` (region) | Retail New Nintendo 3DS (EUR/JPN/USA) | New 3DS |
| `KTR-003` | New Nintendo 3DS battery | accessory |
| `KTR-004` | New Nintendo 3DS stylus | accessory |
| `KTR-007` | New Nintendo 3DS charging cradle | accessory |
| `KTR-009`, `KTR-010` | New Nintendo 3DS front / back cover plate | accessory |
| `RED-001` (region) | Retail New Nintendo 3DS XL (EUR/JPN/USA) | New 3DS |
| `RED-004` | New Nintendo 3DS XL stylus | accessory |
| `RED-007` | New Nintendo 3DS XL charging cradle | accessory |
| `FTR-001` (region) | Retail Nintendo 2DS (EUR/JPN/USA/KOR/CHN/TWN) | Old 3DS |
| `FTR-004` | 2DS stylus | accessory |
| `JAN-001` (region) | Retail New Nintendo 2DS XL (EUR/JPN/USA) | New 3DS |
| `JAN-004` | New Nintendo 2DS XL stylus | accessory |
| `WAP-002` | (New) 3DS / 2DS (XL) AC adapter | accessory |

Copetti says most accessories for the Old 3DS relied on its single infrared transceiver, so only one can be connected at a time ([I/O](io-and-peripherals.md)).

## Homebrew notes

Behavior differences matter for compatibility and tooling: some exploits are restricted to one group (see [exploits](../homebrew/exploits.md)). **New 3DS:** it has the extra [CPU](cpu.md) cores and `NATIVE_FIRM` differences, and its development hardware (Intelligent Systems/Partner "SNAKE" kits) is covered in [dev units](../software/dev-unit-software.md).

## Open questions

- Many cells in the part table are unknown, and the source table's column alignment is unclear for flash, Wi-Fi and MCU parts; a photographed teardown per model would settle it.
- The Old 3DS XL internal flash vendor depends on production date; the exact split is not documented.

## Sources

- [3dbrew: Hardware](https://www.3dbrew.org/wiki/Hardware) — part numbers, SD capacity and boot-time table, MCU, NAND pinouts.
- [3dbrew: Peripherals](https://www.3dbrew.org/wiki/Peripherals) — model and accessory codes (full list reproduced above).
- [Rodrigo Copetti, Nintendo 3DS Architecture: Models and variants](https://www.copetti.org/writings/consoles/nintendo-3ds/) — model grouping, release years.
- [Copetti, Nintendo 3DS Architecture: Operating System](https://www.copetti.org/writings/consoles/nintendo-3ds/) — NAND partition size normalisation.
