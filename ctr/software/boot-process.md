# CTR boot process

When you press the power button, a 3DS runs a short, strict chain of programs. Two permanent programs inside the chip — **Boot9** for the ARM9 and **Boot11** for the ARM11 — wake up first, find the firmware on the internal flash, check its digital signature, copy it into memory and hand over control. The operating system then starts the system services and finally the Home Menu. The whole sequence takes about seven seconds.

The two programs are the console's [boot ROM](../../glossary.md#boot-rom): they are identical on every 3DS model and cannot be updated, which is why mistakes in them are permanent.

## Behavior

1. Power on: ARM9 and ARM11 start. The ARM9's reset vector is `0xFFFF0000` (Boot9); the ARM11 is held in reset until Boot9 releases it. The ARM11 starts Boot11 at `0x00000000`.
2. Boot9 sets up its [memory protection](../../glossary.md#mpu), loads RSA/AES keys into the crypto engines (see [security hardware](../hardware/security-hardware.md)) and decrypts the one-time-programmable (OTP) data.
3. Boot9 looks for a firmware to run: normally the NAND (every FIRM partition in NCSD order), then the Wi-Fi SPI flash as a fallback. If the shell is closed and a special key combination is held, it tries a DS cartridge before NAND.
4. It verifies the NCSD header and FIRM header (SHA-256 + RSA-2048), copies the sections to their load addresses and hides part of itself so FCRAM becomes accessible.
5. Both processors jump to the firmware's entry points. In the ARM11, extra cores park themselves in Boot11 (anything above core 2 hangs there) until the OS releases them; on the New 3DS Kernel11 uses a `CFG11_BOOTROM_OVERLAY_CNT` register to redirect cores 3 and 4 into Kernel11 code.
6. On the ARM9, Kernel9 starts, hides the OTP, and starts Process9. On the ARM11, Kernel11 starts and launches system processes, including `PM` (process manager), which starts `NS` (the shell). `NS` either auto-boots a title or launches the Home Menu.

Approximate timeline: LCDs initialised at ~4 s; Home Menu fully loaded at ~7 s; boot ROM and firmware load in under ~1 s.

Only `NATIVE_FIRM` can be booted from power-off. The other [firmware images](firmware.md) are started afterwards by the OS. If every source fails the console shows a blue **BOOTROM** error screen.

## Technical details

### Boot ROM facts

- Parts of both boot ROMs are protected through `CFG_SYSPROT9` and `CFG_SYSPROT11` after use.
- Boot9 hard-codes NAND as the boot device (AES key slot `0x06` for NAND FIRM). It has working SD(HC) code but the device IDs are fixed to NAND; an SD card can only replace the NAND if the NAND chip is physically disconnected.
- Boot9 start-up (`crt0` at `0xFFFF8000`): clears `CFG_RST11` bit 0, sets stacks (SVC `0xFFF04000`, IRQ `0xFFF03F00`, system `0xFFF03B00`), initialises the MPU (ITCM region: base 0, 128 MB; DTCM region: `0xFFF00000`, 16 KB) and clears DTCM before `main`.
- `main()` initialises PXI, RSA, the crypto engines, sets up AES slot `0x3F` with the OTP-decryption key (a different key for dev units), decrypts and verifies the OTP, then tries NAND, then the Wi-Fi flash. On success it also overwrites the RSA key slots with private keys (see [security hardware](../hardware/security-hardware.md)). On failure it clears `0x07FFB800+0x3C70`.
- Wi-Fi flash FIRM boot: base offset `0x400`, encrypted with AES-CBC using a key kept in the protected ROM data, and signed with separate RSA keys. It is only attempted if NAND boot failed and the Wi-Fi module is accessible.
- DS-cartridge boot (**NTR boot**): with the shell closed and Start + Select + X held, Boot9 tries an inserted NTR card before NAND. NCSD/FIRM backup is not used for non-NAND boots.

### NAND reads during a successful boot (firmware 6.x)

Sector order: `00000000` (NCSD partition table), `0B130000` and `0B530000` (FIRM headers), then the FIRM header, section 1 (`0B130200`), section 2 (`0B163E00`), section 3 (`0B193E00`), followed by reads at `00013000…00017000` and `09011A00…` (the system booting).

### Shared memory and error reporting

Boot9 reports status to the ARM11 error screen through memory at `0x1FFFE000`:

| Offset | Meaning |
| ------ | ------- |
| `+0x0/+4/+8` | ARM11 MPCore cycle counter and performance counters |
| `+0xC` | Status bytes: NAND, NTR card, Wi-Fi flash, sleep-switch state |
| `+0x10` | Per-partition status (8 bytes, one for each NCSD partition) |
| `+0x18` | SD driver internal error bits |
| `+0x1C` | R1 status bits from the SD/eMMC device (masked with `0xFDFF0080` for eMMC, `0xFDF90008` for SD) |

The error screen reads `BOOTROM 8046` / `ERRCODE: ffWWGGNN`, with `ff` the sleep-switch state, `WW` the Wi-Fi-flash code, `GG` the NTR-boot code, `NN` the NAND header (NCSD) code, followed by the 8 partition codes and the SD software/hardware error words.

| Status | Meaning |
| ------ | ------- |
| `0x00` | Not tried (can also mean success) |
| `0xFF` (−1) | Partition skipped: not a FIRM partition |
| `0xFE` (−2) | Device initialisation failed (missing or malfunctioning) |
| `0xF8` (−8) | FIRM header magic mismatch |
| `0xF7` (−9) | FIRM skipped: already found an equal or higher priority FIRM |
| `0xEF` (−17) | NCSD header load failed |
| `0xEE` (−18) | NCSD header magic wrong or RSA verification failed |
| `0xDF` (−33) | FIRM header read failed |
| `0xDE` (−34) | FIRM header magic wrong or RSA verification failed |
| `0xCF` (−49) | FIRM section load failed (address blacklist, read error, or hash mismatch) |

Examples from 3dbrew: `ERRCODE: 00F800FF` with partition codes `DEDEFFFF FFFFFFFF` is what appears when the RSA signatures of both FIRM partitions are corrupted; `00F800FE …` patterns appear when the NAND chip is not found or is wired wrongly.

### Boot ROM memory images

The Boot11 image contains the boot error font (`0x1817C`), Blowfish key data for DS mode, the RSA-2048 moduli for 3DS (`0x1C878`), and initial data. Full details are in 3dbrew's *Bootloader* page.

## Homebrew notes

The boot ROM is the root of the whole [security architecture](security-architecture.md). Flaws in it — the exception vectors pointing at RAM that is not cleared, the signature parser behind [sighax](../homebrew/boot9strap.md), the DS-cartridge boot path — are the basis of today's standard homebrew workflow ([boot9strap](../homebrew/boot9strap.md)). The Wi-Fi flash and NAND pinouts are used for hardware recovery.

## Open questions

- Many Boot9 functions are only known by address (`LT_ffff…`), their purposes are partly inferred.
- Why Boot9 can in principle boot `TWL_FIRM`/`AGB_FIRM` but ends in an error screen or shutdown (per 3dbrew) is not explained.

## Sources

- [3dbrew: Bootloader](https://www.3dbrew.org/wiki/Bootloader) — Boot9/Boot11 flow, memory images, error codes, NAND reads.
- [Rodrigo Copetti, Nintendo 3DS Architecture: Operating System](https://www.copetti.org/writings/consoles/nintendo-3ds/) — boot sequence, multi-core start-up, New 3DS overlay register.
- [3dbrew: Flash Filesystem](https://www.3dbrew.org/wiki/Flash_Filesystem) — NCSD and partition layout read by Boot9.
