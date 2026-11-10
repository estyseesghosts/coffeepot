# CTR models and variants

Six console models make up the 3DS family. They fall into two generations: the **Old 3DS** group (2011–2013) and the **New 3DS** group (2014–2017). Games run on both, but the New models have more processor cores, more memory, an optional faster mode, a better 3D screen and extra controls. This page lists the models and the parts they use; for what the extra hardware does, follow the links to the hardware pages.

## Where it fits

Every other hardware page assumes the Old 3DS as the baseline and calls out New 3DS differences (see [CPU](cpu.md), [memory](memory.md), [I/O](io-and-peripherals.md)). The two groups are written **[Old 3DS and New 3DS](../glossary.md#old-new-3ds)** throughout this wiki.

## How it works

| Model | Year | Group | Notes |
| ----- | ---- | ----- | ----- |
| Nintendo 3DS (CTR-001) | 2011 | Old | The original; 3D top screen |
| Nintendo 3DS XL (SPR-001) | 2012 | Old | Bigger screens; same architecture |
| Nintendo 2DS (FTR-001) | 2013 | Old | Cheaper; slate shape; no 3D screen (top and bottom cropped from one panel) |
| New Nintendo 3DS (KTR-001) | 2014 | New | Quad-core, more RAM, faster clock option, face-tracked 3D, NFC, extra buttons and C-Stick |
| New Nintendo 3DS XL (RED-001) | 2015 | New | New 3DS with large screens |
| New Nintendo 2DS XL (JAN-001) | 2017 | New | New hardware without the 3D screen |

The boot ROMs are identical across all models, which matters for [security flaws](../homebrew/boot9strap.md).

## Technical details

### Component table

Values from the 3dbrew teardown table where each model's cell is unambiguous. `?` means the source has no value.

| Component | 3DS | 3DS XL | 2DS | New 3DS | New 3DS XL | New 2DS XL |
| --------- | --- | ------ | --- | ------- | ---------- | ---------- |
| SoC family | CTR | CTR | CTR | LGR | LGR | LGR |
| FCRAM | 2×64 MB Fujitsu MB82M8080-07L (one package, two dies) | Fujitsu MB82DBS16641 | Fujitsu MB82DBS1664 | ? | Fujitsu MB82MK9A9A | Fujitsu MB82MK9A9A |
| Top screen | 3.53 in, 3D | 4.88 in, 3D | 3.53 in, cropped from a single panel | 3.88 in, 3D | 4.88 in, 3D | 4.88 in (?) |
| Bottom screen | 3.00 in | 4.18 in | 3.00 in, cropped | 3.33 in | 4.18 in | 4.18 in (?) |

Other parts the source documents (the table does not line every cell up with a model, so they are listed without a model mapping): SoC revisions `CPU CTR`, `CPU CTR A`, `CPU CTR B` and `CPU LGR A`; internal flash from Toshiba (THGBM2G3P1FBAI8 1 GB; THGBMBG4P1KBAIT 2 GB MLC) and Samsung (KLM4G1YEQC 4 GB, usable in SLC mode as ~1.3 GiB; KLM4G1FEPD 4 GB), changing with production date; speaker/microphone/circle-pad/touch codec from the TI AIC3010 family; Wi-Fi Atheros AR6014 (Foxconn J27H023.01 in the 2DS); infrared ICs from NXP (S750); MCU Renesas "UC CTR"/"UC KTR"; PMIC TI 93045A4; charging IC TI BQ24072; gyroscope Invensense ITG-3270; accelerometer ST LIS331DH.

Both groups also differ in whether the SoC includes the New-only parts described elsewhere: four cores and L2 cache ([CPU](cpu.md)), extra memory ([memory](memory.md)), MVD decoder and NFC/head-tracking ([I/O](io-and-peripherals.md)).

### Capacity notes

Internal flash partition sizes are fixed by the firmware regardless of the chip: **943 MiB** used on Old 3DS and **1240 MiB** on New 3DS, so larger chips leave space unused (see [NAND filesystem](../software/nand-filesystem.md)). SD cards: Nintendo recommends up to 32 GB; the SD controller accepts much larger cards (2 TB has been tested) at the cost of boot time. FAT32 with a 64 KB cluster is the most compatible choice.

Boot times measured on firmware 11.15/11.16: about 7–8 s with no SD, 10–11 s with 16 GB, 12 s with 64 GB, 19–20 s with 256 GB, 60 s with 2 TB, measured until the Home Menu appears.

### Model identifiers

3dbrew also lists model codes: retail unit `CTR-001 (region)`, debug test unit `CTR-001(-01)`, kiosk unit `CTR-001(-04)`, "Louvre audio guide" special debug units `CTR-001(-06)`, retail game card `CTR-005`, developer flash card `CTR-005(-01)`, battery pack `CTR-003`, stylus `CTR-004`.

## Homebrew notes

Behavior differences matter for compatibility and tooling: New 3DS has the extra [CPU](cpu.md) cores and `NATIVE_FIRM` differences, and some exploits are restricted to one group (see [exploits](../homebrew/exploits.md)). Development hardware for the New models (Intelligent Systems/Partner "SNAKE" kits) is covered in [dev units](../software/dev-unit-software.md).

## Open questions

- Many cells in the part table are unknown, and the source table's column alignment is unclear for flash, Wi-Fi and MCU parts; a photographed teardown per model would settle it.
- The Old 3DS XL internal flash vendor depends on production date; the exact split is not documented.

## Sources

- [3dbrew: Hardware](https://www.3dbrew.org/wiki/Hardware) — part numbers, SD capacity and boot-time table, MCU, NAND pinouts.
- [3dbrew: Peripherals](https://www.3dbrew.org/wiki/Peripherals) — model and accessory codes.
- [Rodrigo Copetti, Nintendo 3DS Architecture: Models and variants](https://www.copetti.org/writings/consoles/nintendo-3ds/) — model grouping, release years.
- [Copetti, Nintendo 3DS Architecture: Operating System](https://www.copetti.org/writings/consoles/nintendo-3ds/) — NAND partition size normalisation.
