# CTR security architecture

Nintendo designed the 3DS so that every piece of code and every game must be digitally signed, and so that most of the secret keys never leave special hardware. The result is a **[chain of trust](../../glossary.md#chain-of-trust)**: each stage proves the next one is genuine, starting from an unchangeable [boot ROM](../../glossary.md#boot-rom). Almost every successful attack on the console found a gap in how this chain was built, not a break in the cryptography itself.

This page explains the design; the individual attacks are in the [exploit catalogue](../homebrew/exploits.md) and the [history](../homebrew/history.md).

## Behavior

### The chain

1. **Boot9** (in the SoC) holds the RSA public keys and AES keys. It decrypts the one-time-programmable (OTP) data that gives each console unique keys. See [security hardware](../hardware/security-hardware.md).
2. The internal flash ([NAND](nand-filesystem.md)) is encrypted with Boot9's keys mixed with the flash chip's CID. Its **NCSD** header (up to 8 partitions) is signed with RSA-2048/SHA-256; Boot9 verifies it, then verifies the FIRM header of the chosen firmware ([boot process](boot-process.md)). Game cards use the same NCSD format; the key used to check the signature depends on whether the NCSD came from NAND or a card.
3. Each partition is an **NCCH** with its own RSA-2048/SHA-256 signature and AES-128-CTR payload encryption, whether it came from NAND, a card, or the SD card ([file formats](file-formats.md)).
4. Installed software is catalogued as [titles](titles.md), signed with RSA-2048, RSA-4096 or ECDSA plus SHA-256. The public keys are stored in `NATIVE_FIRM`. eShop software also needs a **ticket**; tickets are tied to one console and account, or are global for system titles.
5. The payload (executable, library or asset) is then handed to the ARM11.

`TWL_FIRM` and `AGB_FIRM` have their own cryptography on top of this.

### Operating-system protections

- User programs reach hardware only through [system calls](kernel.md) authorised by Kernel11 and, for storage and cryptography, through the ARM9 via PXI; ARM11 software is unaware of the ARM9.
- Applications are sandboxed from one another.
- ARM11 cores honour the XN (execute-never) page flag; the ARM9 has an MPU.
- Online services require a legitimate game card and an up-to-date firmware, which discouraged staying on a vulnerable version.
- The browser (WebKit) and other bundled apps are a large attack surface that also constrained patching.

### Later changes

> Conflict/unverified: this paragraph follows Copetti. Only the "OTP readable on firmware before 3.0.0" point was confirmed in 3dbrew's System Flaws page (OTP dumping "required a boot of < 3.0.0-X"). The 7.0.0 RSA-engine and 11.0.0 anti-downgrade statements were not found there and should be read as [hypothesis] until checked against the flaw tables.

Nintendo reworked the chain over time: system version 7.0.0 added the RSA engine to the NCCH decryption path (keys cleared after Kernel9 loads, so earlier exploits could not read newer games); 3.0.0 hid the OTP after boot; **New 3DS:** a loader stage (**arm9loader**) with OTP-derived keys was added (fixed in 9.5.0/9.6.0 after flaws); title downgrade protection was added later (11.0.0 closed the anti-downgrade list gap).

## Technical details

### Design weaknesses (design level)

| Weakness | Consequence |
| -------- | ----------- |
| ARM11 kernel sets page tables in AXI WRAM with read/write/execute for the whole block | XN does not protect Kernel11 |
| OTP visible before 3.0.0 | Keys readable by any exploit on older firmware |
| Kernel9 gives Process9 a system call to run arbitrary code in kernel mode | No privilege separation on the ARM9 |
| No ASLR | Return-oriented programming works |
| No protection against firmware downgrade (early) | Old, vulnerable firmware can be reinstalled |
| WebKit browser | Frequent entry point |

### Catalogue of documented flaws

3dbrew's *3DS System Flaws* page lists hardware, boot-ROM, ARM9 and ARM11 flaws with discovery dates, fixes and "last checked" firmware versions. Highlights by area:

| Area | Flaws (examples) |
| ---- | ---------------- |
| Hardware / boot ROM | Exception vectors point at uninitialised RAM (glitch to dump ROM); no RAM clearing on reboot; AES key slots not cleared by hard reset; weak key generator; RSA exponent not cleared; 32-bit TWL key entropy on Old 3DS; boot-ROM RSA signature parsing (sighax) |
| arm9loader | Console-unique keysector keys derivable from Boot9 + ITCM data; NAND keystore encrypted with AES-ECB so blocks can be rearranged; uncleared New 3DS key slot `0x11` and OTP hash key data |
| Process9 | Leaks of normal-key / key-scrambler data; RSA padding checks; DSiWare import/export validation flaws; unchecked PXI parameters and buffer overruns; title downgrading via AM; `FIRM launch` not checking target FIRM version; SD mount permission flag ignored; gamecard transfer size unchecked |
| Kernel9 / Kernel11 | ARM9 and ARM11 privilege escalations (e.g. memchunk-based kernel exploits) |

Two further entries from the same page: `srv:pm` was a globally reachable port until 7.0.0-13, letting any process re-register itself and bypass the exheader service-access list (since then it is a service restricted to PIDs below 6, one session at a time) [documented: 3dbrew System Flaws]; and because FIRM partitions are AES-CTR encrypted with a fixed keystream, knowing a FIRM binary reveals keystream that enables minor FIRM downgrades and, with sighax, arbitrary FIRM installation. Downgrading titles via AM worked by deleting the installed title first so Process9 had nothing to compare against.

The full tables, with credit and fix versions, are on that page.

## Homebrew notes

See [exploits](../homebrew/exploits.md) for how these flaws were chained into userland → kernel → ARM9 → boot ROM, and [boot9strap](../homebrew/boot9strap.md) for the outcome.

## Open questions

- The catalogue above is a summary; fixed-in-version dates for each flaw should be read from 3dbrew.
- Whether later firmware changed the chain beyond the items listed is not covered in the sources used.

## Sources

- [Rodrigo Copetti, Nintendo 3DS Architecture: Anti-Piracy and Homebrew](https://www.copetti.org/writings/consoles/nintendo-3ds/) — the designed chain of trust, OS functions, design flaws.
- [3dbrew: 3DS System Flaws](https://www.3dbrew.org/wiki/3DS_System_Flaws) — detailed flaw list.
- [3dbrew: Bootloader](https://www.3dbrew.org/wiki/Bootloader) — Boot9 signature use.
- [3dbrew: Titles](https://www.3dbrew.org/wiki/Titles), [Ticket](https://www.3dbrew.org/wiki/Ticket), [Title metadata](https://www.3dbrew.org/wiki/Title_metadata) — signing and certificate chains.
