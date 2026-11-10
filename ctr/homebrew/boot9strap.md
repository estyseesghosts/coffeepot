# boot9strap, sighax and ntrboot

**boot9strap** is the standard way today to run custom code on a 3DS before the operating system starts. It works on every 3DS-family model and every firmware version, because it attacks the unchangeable [boot ROM](../../glossary.md#boot-rom) rather than any system software: once installed, the console's startup chain accepts a firmware that boot9strap's author signed with a forged signature, and that firmware loads the custom firmware of your choice (normally [Luma3DS](custom-firmware.md)).

The flaw underneath it is called **[sighax](../glossary.md#sighax)**; **ntrboot** is a related trick that boots such a firmware from a DS flash cartridge, which also doubles as the best unbrick tool. All three descend from the 2016 33C3 and 2017 33.5C3 talks (derrek; SciresM and Myria).

## What you need

- Any Nintendo 3DS-family console (3DS, 3DS XL, 2DS, New 3DS, New 3DS XL, New 2DS XL) on any firmware version — the boot ROM is identical on all of them.
- Some way to write boot9strap's signed firmware into the console's NAND, which needs code running with ARM9 or NAND access: any of the ARM9-level entries in the [exploit catalogue](exploits.md) (for example MSET9, or the Safe Mode route on Old 3DS), or a hardware route such as ntrboot with a compatible DS flashcart (a magnet is used to simulate the closed shell; the Start + Select + X key combination is held).
- **Back up your NAND and your OTP data first.**

## How it works

The 3DS boot ROM checks every firmware with RSA-2048 signatures ([boot process](../software/boot-process.md)). Derrek found that Boot9's signature parser is missing safety checks (including bounds checking): RSA signatures in the PKCS #1 v1.5 format contain padding and an ASN.1-encoded SHA-256 hash, and Boot9 would accept a specially crafted signature that always verifies for any data. With such a signature, **anyone can make a firmware that Boot9 trusts** — and the whole [chain of trust](../software/security-architecture.md) is nullified at its root.

Making the signature needed brute force. Derrek's announcement did not publish a usable signature or a copy of Boot9; SciresM and Myria found that firmware versions before 1.0.0 shared flaws similar to sighax and used it to begin brute-forcing, eventually matching a signature using many Nvidia GPUs.

The remaining question was how to run the forged firmware early enough. The pair found a route through the ARM9's exception handlers: the ARM9 cannot overwrite them, but its NDMA controller can. The team made NDMA fill the exception handlers with a jump to their code and had the ARM9 copy to address zero, causing an exception that runs their payload with unrestricted access. Packaged, this is **boot9strap**: a bootloader that either loads a payload from the SD card or continues to boot normally.

Because boot9strap runs before Boot9 hides its ROM, it can keep access to the boot ROMs and the decrypted OTP, which tools such as GodMode9 use to dump both.

### ntrboot

During boot, Boot9 asks whether a specific key combination is pressed and the shell is closed; if so it redirects to a DS card in the slot with full privileges ([boot process](../software/boot-process.md)). `ntrboot` therefore writes a sighax-signed firmware onto a compatible DS flashcart ("3DS mode" switch on some carts), closes the shell with a magnet and boots it. This gives instant installation and an extremely potent recovery tool, since it bypasses the normal boot chain entirely.

## Technical details

- Boot ROM signature parser: no bounds checking; a crafted PKCS #1 v1.5 padding/ASN.1 block can be constructed to verify against any hash. Used for all signatures Boot9 verifies (NCSD, FIRM, and the Wi-Fi flash FIRM path).
- Because the flaw lives in boot ROM, Nintendo could not fix it with a system update. A hardware revision would be needed; none appeared (the New 2DS XL was found to have the same boot ROM in June 2017).
- Boot9's ARM9 exception vectors are the entry point exploited with NDMA; the first-stage chain executes before the boot ROM is hidden.
- ntrboot first practical release: August 2017 (initially ak2i and R4 cards). Stargate, a 3-in-1 hybrid, was announced in September 2017 and abandoned within months.

## Limits and risks

- Installing it modifies the FIRM partitions: an incorrect installation can still brick a console. Per the community history, boot9strap itself carries near-zero brick risk; ntrboot is described as an extremely potent unbricking tool because it bypasses the normal boot chain.
- It is not removed by system updates in the sense that the vulnerable boot ROM cannot be changed, but the sources do not describe what happens to an installed copy during an update, so treat that as unverified.
- It gives total control of the console and access to every secret on it (OTP, boot ROMs). Dump and keep backups before doing anything else.
- Its predecessor `arm9loaderhax` could be unsafe to install and required a prior OTP dump.
- Online bans are moot since online services ended in April 2024 ([history](history.md)).

## Open questions

- Details of the actual forged-signature construction are not in these sources.
- The exact set of flashcarts supported by ntrboot is not listed here.

## Sources

- [Rodrigo Copetti, Nintendo 3DS Architecture: Anti-Piracy and Homebrew](https://www.copetti.org/writings/consoles/nintendo-3ds/) — sighax mechanism, boot9strap development, ntrboot.
- [Hacks Guide Wiki: History of 3DS Hacking](https://wiki.hacks.guide/wiki/3DS:History_of_3DS_Hacking) — dates of 33C3, 33.5C3, ntrboot release, New 2DS XL boot ROM finding.
- [3dbrew: Bootloader](https://www.3dbrew.org/wiki/Bootloader) — Boot9 behavior and the NTR-card boot condition.
- [3dbrew: 3DS System Flaws](https://www.3dbrew.org/wiki/3DS_System_Flaws) — boot ROM vectors / exception path and signature flaws.
