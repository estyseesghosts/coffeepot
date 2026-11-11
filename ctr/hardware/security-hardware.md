# CTR security hardware

Besides general-purpose processors, the 3DS SoC contains a small set of dedicated security blocks that only the [ARM9](cpu.md) can use: an encryption engine, a signature-checking engine, a hashing engine, a random-number generator and a block of one-time-programmable secrets. Together with the unchangeable [boot ROM](../../glossary.md#boot-rom), they make up the foundation that the rest of the 3DS's [security architecture](../software/security-architecture.md) builds on.

Nearly every public attack on the console started by finding a flaw in how these blocks are used — not in the cryptography itself.

## Where it fits

All of these blocks are memory-mapped I/O on the ARM9 side (see the [I/O map](io-and-peripherals.md)): they are invisible to the ARM11 and to games. The ARM9 boot ROM (**[Boot9](../glossary.md#boot9-boot11)**) loads their keys; `Process9` (see [OS](../software/os.md)) then uses them for [NAND](../software/nand-filesystem.md), SD, [title](../software/titles.md) and [game card](gamecard.md) encryption and verification.

## How it works

- **AES engine.** Inherited from the DSi but with up to 64 key slots and several block-cipher modes (CTR, CCM, CBC, ECB). Each slot can combine two values (key X and key Y) through a key scrambler; the resulting key can be used but never read back.
- **RSA engine.** Four key slots, write-only, no key scrambler. Used to check RSA-2048 signatures (and RSA-4096 for certificates).
- **SHA engine.** Fingerprinting of firmware, content and metadata. (A second HASH block is accessible to the ARM11 filesystem services.)
- **PRNG.** Registers that return a different value on each read, used as an entropy source.
- **OTP memory.** Written once at the factory: console-unique keys and information, plus `CTCert`, an ECDSA private key used to authenticate to Nintendo's servers. It is encrypted by a key stored in Boot9 and includes a flag that hides it once not needed. Before system version 3.0.0 it was never hidden.
- **eMMC CID.** The internal flash's card-identification register is mixed into the key scrambler so the storage cannot be decrypted without its own chip.
- **Execute protection.** The ARM11 cores implement the execute-never (XN) page flag and the ARM9 has a memory protection unit, so neither will run code from just any memory location (Copetti); details of XN and page attributes are on [ARM11 MPCore internals](arm11-mpcore.md#address-translation-mmu).
- **Boot ROMs.** Boot9 and Boot11 are stored unencrypted but are unreadable once the system is running, because they are sealed inside the SoC (Copetti).
- **No ECDSA hardware.** Elliptic-curve signatures (used on some titles) are verified in software, as there is no accelerator.

## Technical details

### Boot ROM key storage (Boot9 data)

The ARM9 boot ROM carries RSA public-key data and AES key material. Separate data exists for retail and development hardware (`devunit`).

| RSA slot | Use |
| -------- | --- |
| 0 | Not set at start |
| 1 | NAND FIRM signature |
| 2 | Non-NAND (SPI flash) FIRM signature |
| 3 | NAND NCSD-header FIRM signature |

After a firmware is successfully loaded, Boot9 overwrites all four RSA key slots with modulus and private-exponent pairs from its data section and copies related blobs to the ARM9's instruction TCM (boot-ROM view) at `0x07FFB800+0x500` (moduli, `0x400` bytes) and `+0x900` (modulus plus private exponent, `0x800` bytes) per 3dbrew. The ROM also stores the key data that decrypts the OTP (via AES slot `0x3F`), a block of AES key material (`0x400` bytes each for retail and dev units), and the initial DTCM image.

Boot9 data layout (retail starts at `0xFFFFB088`; addresses from 3dbrew):

| Address | Size | Content |
| ------- | ---- | ------- |
| `0xFFFFB088` | `0x38` | Memory-range blacklist used while loading FIRM sections |
| `0xFFFFB0C0` | `0x20` | Unknown |
| `0xFFFFB0E0` | `0x2F80` | All keys in the image (next rows) |
| `0xFFFFB0E0` | `0x2600` | RSA key data: first `0x1300` bytes retail, then `0x1300` from `0xFFFFC3E0` for dev units |
| `0xFFFFD6E0` | `0x40` | OTP decryption key data for AES slot `0x3F` (retail first `0x20`, dev at `0xFFFFD700`); within each, +0 is the normal key and +0x10 the AES IV |
| `0xFFFFD760` | `0x100` | First `0x80` bytes retail, the rest (at `0xFFFFD7E0`) dev; `0x80` bytes are copied to `0x07FFCD00` (in two `0x40` chunks) |
| `0xFFFFD860` | `0x400` | Retail AES key-init data (the "bootrom_dataptr" passed to the key-init function) |
| `0xFFFFDC60` | `0x400` | Dev unit version of the above |
| `0xFFFFE060` | `0x230` | Initial DTCM image for `0xFFF00000` |
| `0xFFFFE290` | rest | All zero |

RSA key data (offsets within each `0x1300` retail/dev half): `+0x000`/`+0x100`/`+0x200` moduli for RSA slots 3, 1 and 2; then four `0x200` blocks (modulus then private exponent) for slots 0 to 3 starting at `+0x300`, `+0x500`, `+0x700`, `+0x900` (copied to `0x07FFBD00` area, the private exponent is not copied there), and four more `0x200` blocks at `+0xB00`, `+0xD00`, `+0xF00`, `+0x1100` copied to the `0x07FFC100` area. Boot9 itself does not use that second set except to copy it. The Boot9 DTCM image holds mostly pointers; one unknown `0x10`-byte block sits at DTCM `+0x124`.

Boot9 uses **AES slot `0x06`** for NAND FIRM crypto; **slot `0x3F`** decrypts the OTP and is also used for the AES-CBC layer on FIRM images booted from non-NAND (SPI flash / cartridge); that slot is overwritten afterwards only if non-NAND boot fails (3dbrew Bootloader).

### Known weaknesses in these blocks

See the full list in [exploits](../homebrew/exploits.md#hardware-and-boot-rom). The ones that affect this hardware directly:

- AES key slots are not cleared by a hard reset/reboot.
- RAM, including FCRAM, ARM9 memory, AXI WRAM and VRAM, keeps its contents across an MCU-triggered reboot.
- The key-generator algorithm for normal-keys is cryptographically weak and susceptible to differential cryptanalysis when a normal-key/KeyY pair is known.
- RSA slots do not clear the private exponent when a new modulus is written (an attacker can recover it via a discrete logarithm of outputs).
- The TWL (DSi-mode) console-unique key data has only 32 bits of true entropy on the Old 3DS, making it brute-forceable from NAND data alone.
- Boot ROM exception vectors point at uninitialised RAM, allowing a precisely timed glitch to run attacker code and dump the ROM (done in 2015–16).
- The boot ROM's RSA signature parser accepts forged signatures ("sighax"); see [boot9strap](../homebrew/boot9strap.md).

## Homebrew notes

Access to these blocks requires [ARM9 code execution](../homebrew/exploits.md). Once there, tools can dump the OTP and the boot ROMs (Boot9/Boot11) and use the AES engine to decrypt console-specific data; see [boot9strap](../homebrew/boot9strap.md).

## Open questions

- The exact way key X is derived per slot is not covered in the sources used here.
- AES, RSA and SHA register layouts are documented on 3dbrew but not summarised here.

## Sources

- [Rodrigo Copetti, Nintendo 3DS Architecture: Anti-Piracy and Homebrew](https://www.copetti.org/writings/consoles/nintendo-3ds/) — dedicated hardware overview, key-slot counts, OTP contents.
- [3dbrew: Bootloader](https://www.3dbrew.org/wiki/Bootloader) — Boot9 RSA key-slot use, data layout, AES slot `0x06`/`0x3F`.
- [3dbrew: 3DS System Flaws](https://www.3dbrew.org/wiki/3DS_System_Flaws) — hardware weaknesses listed above.
- [3dbrew: IO Registers](https://www.3dbrew.org/wiki/IO_Registers) — AES/SHA/RSA/PRNG/OTP register blocks.
