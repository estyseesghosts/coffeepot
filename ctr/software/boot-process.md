# CTR boot process

When you press the power button, a 3DS runs a short, strict chain of programs. Two permanent programs inside the chip — **Boot9** for the ARM9 and **Boot11** for the ARM11 — wake up first, find the firmware on the internal flash, check its digital signature, copy it into memory and hand over control. The operating system then starts the system services and finally the Home Menu. 3dbrew describes a roughly seven-second baseline to the Home Menu; startup varies with the console and SD-card configuration, and large cards can take substantially longer [reverse-engineered: 3dbrew].

The two programs are the console's [boot ROM](../../glossary.md#boot-rom): they are identical on every 3DS model and cannot be updated, which is why mistakes in them are permanent.

## Behavior

1. Power on: ARM9 and ARM11 start. The ARM9's reset vector is `0xFFFF0000` (Boot9); the ARM11 is held in reset until Boot9 releases it. The ARM11 starts Boot11 at `0x00000000`.
2. Boot9 sets up its [memory protection](../../glossary.md#mpu), loads RSA/AES keys into the crypto engines (see [security hardware](../hardware/security-hardware.md)) and decrypts the one-time-programmable (OTP) data.
3. Boot9 looks for a firmware to run: normally the NAND (every FIRM partition in NCSD order), then the Wi-Fi SPI flash as a fallback. If the shell is closed and a special key combination is held, it tries a DS cartridge before NAND.
4. It verifies the NCSD header and FIRM header (SHA-256 + RSA-2048), copies the sections to their load addresses and hides part of itself so FCRAM becomes accessible.
5. Both processors jump to the firmware's entry points. In the ARM11, extra cores park themselves in Boot11 (anything above core 2 hangs there) until the OS releases them. **New 3DS:** Kernel11 uses a `CFG11_BOOTROM_OVERLAY_CNT` register to redirect cores 3 and 4 into Kernel11 code.
6. On the ARM9, Kernel9 starts, hides the OTP, and starts Process9. On the ARM11, Kernel11 starts and launches system processes, including `PM` (process manager), which starts `NS` (the shell). `NS` either auto-boots a title or launches the Home Menu.

3dbrew gives an approximate baseline timeline: boot ROM and firmware load in under ~1 s, LCDs initialise at ~4 s and the Home Menu loads at ~7 s. These are not universal timings: its hardware page reports longer startup times for different SD-card configurations [reverse-engineered: 3dbrew].

Only `NATIVE_FIRM` boots usefully from power-off: the boot ROM can launch `TWL_FIRM` or `AGB_FIRM` as well, but 3dbrew says that ends in a Japanese error screen or a shutdown. The other [firmware images](firmware.md) are started afterwards by the OS. If every source fails the console shows a blue **BOOTROM** error screen.

## Technical details

### Boot ROM facts

- Parts of both boot ROMs are protected through `CFG_SYSPROT9` and `CFG_SYSPROT11` after use.
- Boot9 hard-codes NAND as the boot device (AES key slot `0x06` for NAND FIRM). It has working SD(HC) code but the device IDs are fixed to NAND; an SD card can only replace the NAND if the NAND chip is physically disconnected and the card is wired onto the NAND's bus. Because the card's CID differs, the partitions would have to be re-encrypted for the new CID, and DS-mode (TWL) boot would not work (3dbrew).
- NAND FIRM selection: Boot9 does not assume two FIRM partitions; it parses all 8 NCSD partitions and tries every one that is really a FIRM, in the listed order, until one boots. Among the partitions not yet processed, the one with the highest `u32` at `firmhdr+4` is tried first; that value is normally `0`, so the order is normally the NCSD order.
- The boot ROMs are identical for all six models (3DS, 3DS XL, 2DS, New 3DS, New 3DS XL, New 2DS XL) (3dbrew).
- Booting `TWL_FIRM` or `AGB_FIRM` with the boot ROM is possible in principle, but leads to a Japanese error screen or a shutdown right after the FIRM launch (3dbrew).
- Boot9 start-up (`crt0` at `0xFFFF8000`): clears `CFG_RST11` bit 0, sets stacks (SVC `0xFFF04000`, IRQ `0xFFF03F00`, system `0xFFF03B00`), initialises the MPU (ITCM region: base 0, 128 MB; DTCM region: `0xFFF00000`, 16 KB) and clears DTCM before `main`.
- `main()` initialises PXI, RSA, the crypto engines, sets up AES slot `0x3F` with the OTP-decryption key (a different key for dev units), decrypts and verifies the OTP, then tries NAND, then the Wi-Fi flash. On success it also overwrites the RSA key slots with private keys (see [security hardware](../hardware/security-hardware.md)). On failure it clears `0x07FFB800+0x3C70`.
- Wi-Fi flash FIRM boot: base offset `0x400`, encrypted with AES-CBC using a normal key stored in the protected Boot9 data (separate retail and dev keys) instead of the NAND-FIRM encryption, and signed with separate RSA keys. The encryption covers only the FIRM sections; the counter words are `{FIRM section offset, load address, section size, section size}`. It is only attempted if NAND boot failed and the Wi-Fi module is accessible (flash status register bit 0 clear). Everything below flash offset `0x1F300` is write-protected by the flash itself, so it cannot be written from 3DS or DS mode (3dbrew).
- DS-cartridge boot (**NTR boot**): with the shell closed and Start + Select + X held, Boot9 tries an inserted NTR card before NAND. NCSD/FIRM backup is not used for non-NAND boots. On the Old 3DS and 2DS the console normally will not turn on at all with the shell closed (the sleep switch can be faked with a magnet); 3dbrew says that with the key combination held the power button starts it (the page text is cut off at that point, so the exact condition is not stated).
- `crt0` (at `0xFFFF8000`, reached from the reset vector `0xFFFF0000`) also calls `mpu_init()`: it clears bits `0x000F9005` in the CP15 control register, invalidates the instruction and data caches and drains the write buffer, programs the eight protection regions (ITCM region register `0x24`: base `0`, 128 MB; DTCM region register `0xFFF0000A`: base `0xFFF00000`, 16 KB), sets cacheable/bufferable bits and permissions, and finally ORs `0x0005707D` into the control register. It then installs exception handler addresses at `0x08000000`, clears `0x08000030` (`0x10` bytes) and the DTCM, copies the initial DTCM image from the Boot9 data, and clears `0xFFF00230`-`0xFFF01AC0` before jumping to `main`.

### Boot protection and environment registers

The ARM9 system-configuration block includes these registers. They are privileged boot/system interfaces, not ordinary homebrew APIs. The following is 3dbrew's reverse-engineered description; the article does not specify the console and firmware used for each observation.

| Register | Physical address, width | Boot-related behavior |
| -------- | ----------------------- | --------------------- |
| `CFG9_SYSPROT9` | `0x10000000`, 8-bit | Bit 0 hides the protected half of Boot9 and enables FCRAM access; bit 1 hides the OTP area. Once set, each bit cannot be cleared until a hard reset. |
| `CFG9_SYSPROT11` | `0x10000001`, 8-bit | Bit 0 hides the protected half of Boot11 and enables FCRAM access; it cannot be cleared until a hard reset. |
| `CFG9_RST11` | `0x10000002`, 8-bit | Bit 0 is presumed to release ARM11 reset; after clearing, it cannot be set again. The release interpretation is tentative. |
| `CFG9_BOOTENV` | `0x10010000`, 32-bit | Persists across an MCU reboot and starts at 0 on cold boot. NATIVE_FIRM writes 1 on shutdown/FIRM launch; legacy firmware writes 3 for TWL and 7 for AGB title launch. |
| `CFG9_UNITINFO` | `0x10010010`, 8-bit | 0 for retail, nonzero for development/debug units. |

On Old 3DS, NATIVE_FIRM checks whether the OTP protection bit is already set before initializing TWL console-unique keys; it sets the bit afterward. **New 3DS:** the Kernel9 loader performs that work and uses OTP data to derive the NATIVE_FIRM decryption key, preserving the final keyslot for a later FIRM launch before hard reset. [reverse-engineered: 3dbrew CONFIG9 Registers, revision 22213]

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

Also `0xFD` (−3): SD driver initialisation failed because Boot9 state was not initialised (3dbrew calls this unobtainable).

Examples from 3dbrew (the words are `ERRCODE`, the two partition-code words, then the two SD error words):

| Words | Cause |
| ----- | ----- |
| `00F800FE 00000000 00000000 00000200 00000000` | SD-card reader connected to the NAND bus at boot |
| `00F800FE 00000000 00000000 00000400 00000000` | NAND not found (?) |
| `00F800FE FFFFFFFF FFFFFFFF 00000080 00800000` | NAND error with DAT1 wired as DAT0 |
| `00F800FE FFFFFFFF FFFFFFFF 00000005 00800000` | NAND error with DAT2 wired as DAT0 |
| `00F800FE FFFFFFFF FFFFFFFF 00000005 00000000` | NAND error with DAT3 wired as DAT0 |
| `00F800FF F8F8FFFF FFFFFFFF 00000000 00000000` | `firm0` and `firm1` both corrupt (failed signature checks) |
| `00F800FF DEDEFFFF FFFFFFFF 00000000 00000000` | `firm0` and `firm1` both corrupt (possibly certain flags missing?) |
| `00F800FF CFCFFFFF FFFFFFFF 00000000 00000000` | `firm0` and `firm1` both corrupt |
| `00F800EE FFFFFFFF FFFFFFFF 00000000 00000000` | NCSD header in sector 0 corrupt (failed signature check) |

The error screen prints `BOOTROM 8046`, `ERRCODE: %08X` (the word at `0x1FFFE00C`), then the words at `0x1FFFE010`/`014` and `018`/`01C`. The first three words at `0x1FFFE000` are filled by Boot11 with the ARM11 MPCore cycle counter and its two event counters (CCNT, PMN0, PMN1; see [ARM11 MPCore internals](../hardware/arm11-mpcore.md#performance-monitor-per-core-cp15-c15)), and remain in memory when the processors jump to FIRM.

**SD driver error bits** (word at `0x1FFFE018`):

| Bit | Meaning |
| --- | ------- |
| `0x1` | `STATUS2`: received command field does not match what was sent |
| `0x2` | `STATUS2`: received CRC does not match the calculated CRC |
| `0x4` | `STATUS2`: framing error, stop bit not seen |
| `0x8` | `STATUS2`: data not received within the timeout |
| `0x10` | `STATUS2`: RX FIFO overflow |
| `0x20` | `STATUS2`: TX FIFO overflow |
| `0x40` | `STATUS2` bit 31: illegal access error (3dbrew: "???") |
| `0x80` | At least one error bit set in the device's command reply, or an unexpected state |
| `0x100` | Device reported an illegal command |
| `0x200` | Timer-based timeout waiting for device operations to finish |
| `0x400` | Timer-based timeout during the MMC initialisation sequence |
| `0x800` | Some sort of timeout (unknown) |
| `0x8000` | Timeout performing an AES operation on sector data |
| `0x80000` | AES operation attempted while another was in progress |

The word at `0x1FFFE01C` is the device's R1 reply status (see the SD Physical Layer specification for its bit list).

**Hardware failures.** When the console detects a hardware fault it shows an LED pattern. 3dbrew's only documented example: if a 3DS's screen is replaced with another model's screen, the console powers on with black screens, and after about a minute the Wi-Fi LED blinks four times, stays on for a second and the console powers off, with the blue LED staying on. Usually a hardware fault just crashes or powers off the console, so little more is known.

### Boot ROM memory images

| Boot11 address | Size | Content |
| -------------- | ---- | ------- |
| `0x0001817C` | `0x78` | Error-screen font bitmap data (30 big-endian dwords, raw size 32x30, 1 bit per pixel); starts right after the crt0 code, the protected Boot11 code follows at `0x181F4` |
| `0x00019400` | - | Start of Boot11's data area (first 8 bytes unknown) |
| `0x00019408` | `0x2090` | Blowfish key data, later copied to the ARM9's ITCM at `twlkeydata+0x3E0` |
| `0x0001C498` | `0x60`, `0x40`, `0x40`, `0x80`, `0x80`, `0x200` | Further DS-mode key data copied to `twlkeydata+0x380`, `+0x340`, `+0x300`, `+0x280`, `+0x0` and `+0x80` |
| `0x0001C878` | `0x800` | The 3DS RSA-2048 moduli, copied to `arm9_itcm+0x4900`: Boot9 copies the first four on retail units, the last four on dev units |
| `0x0001D078` | `0x120` | Initial `.data` for `0x1FFE8000`; the very end of the protected ARM11 boot ROM |

Boot11's `main()` initialises PXI, installs a function pointer at `0x1FFE8028` that `firmboot()` may call, then runs `firmboot()`, the ARM11 twin of Boot9's final function. Its sleep-switch check reads the MCU over I²C; if that read fails it falls back to GPIO register `0x10147000` bit 4. The result is `0x2` when the shell is closed (sleep) and `0` otherwise.

## Homebrew notes

The boot ROM is the root of the whole [security architecture](security-architecture.md). Flaws in it — the exception vectors pointing at RAM that is not cleared, the signature parser behind [sighax](../homebrew/boot9strap.md), the DS-cartridge boot path — are the basis of today's standard homebrew workflow ([boot9strap](../homebrew/boot9strap.md)). The Wi-Fi flash and NAND pinouts are used for hardware recovery.

## Open questions

- Many Boot9 functions are only known by address (`LT_ffff…`), their purposes are partly inferred.
- Why Boot9 can in principle boot `TWL_FIRM`/`AGB_FIRM` but ends in an error screen or shutdown (per 3dbrew) is not explained.
- The condition under which the Old 3DS/2DS powers on with the shell closed (3dbrew text is truncated) is not fully stated.

## Sources

- [3dbrew: Hardware](https://www.3dbrew.org/wiki/Hardware) — reported boot times by SD-card configuration.

- [3dbrew: CONFIG9 Registers](https://www.3dbrew.org/wiki/CONFIG9_Registers) — boot protection, reset and environment register fields; saved revision 22213.
- [3dbrew: Bootloader](https://www.3dbrew.org/wiki/Bootloader) — Boot9/Boot11 flow, memory images, error codes, NAND reads.
- [Rodrigo Copetti, Nintendo 3DS Architecture: Operating System](https://www.copetti.org/writings/consoles/nintendo-3ds/) — boot sequence, multi-core start-up, New 3DS overlay register.
- [3dbrew: Flash Filesystem](https://www.3dbrew.org/wiki/Flash_Filesystem) — NCSD and partition layout read by Boot9.
