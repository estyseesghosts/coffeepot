# boot9strap, sighax and ntrboot

**boot9strap** is the standard way today to run custom code on a 3DS before the operating system starts. It works on every 3DS-family model and every firmware version, because it attacks the unchangeable [boot ROM](../../glossary.md#boot-rom) rather than any system software: once installed, the console's startup chain accepts a firmware that boot9strap's author signed with a forged signature, and that firmware loads the custom firmware of your choice (normally [Luma3DS](custom-firmware.md)).

The flaw underneath it is called **[sighax](../glossary.md#sighax)**; **ntrboot** is a related trick that boots such a firmware from a DS flash cartridge, which also doubles as the best unbrick tool. All three descend from the 2016 33C3 and 2017 33.5C3 talks (derrek; SciresM and Myria).

## Console setup

For installation, troubleshooting, recovery and backup procedures, follow the [3DS Hacks Guide](https://3ds.hacks.guide/). Once the console is ready, use the [development workflow](development-workflow.md) to build and run homebrew. This page describes the boot mechanism and its history.

## How it works

The 3DS boot ROM checks every firmware with RSA-2048 signatures ([boot process](../software/boot-process.md)). Derrek found that Boot9's signature parser is missing safety checks (including bounds checking): RSA signatures in the PKCS #1 v1.5 format contain padding and an ASN.1-encoded SHA-256 hash, and Boot9 would accept a specially crafted signature that always verifies for any data. With such a signature, **anyone can make a firmware that Boot9 trusts** — and the whole [chain of trust](../software/security-architecture.md) is nullified at its root.

Making the signature needed brute force. Derrek's announcement did not publish a usable signature or a copy of Boot9; SciresM and Myria found that firmware versions before 1.0.0 shared flaws similar to sighax and used it to begin brute-forcing, eventually matching a signature using many Nvidia GPUs.

The remaining question was how to run the forged firmware early enough. The pair found a route through the ARM9's exception handlers: the ARM9 cannot overwrite them, but its NDMA controller can (3dbrew describes the loader-side reason: Boot9's memory blacklist misses memory-mapped I/O and can be bypassed by a section that encloses the vector table; see Technical details). The team made NDMA fill the exception handlers with a jump to their code and had the ARM9 copy to address zero, causing an exception that runs their payload with unrestricted access. Packaged, this is **boot9strap**: a bootloader that either loads a payload from the SD card or continues to boot normally.

Because boot9strap runs before Boot9 hides its ROM, it can keep access to the boot ROMs and the decrypted OTP, which tools such as GodMode9 use to dump both.

### ntrboot

During boot, Boot9 asks whether a specific key combination is pressed and the shell is closed; if so it redirects to a DS card in the slot with full privileges ([boot process](../software/boot-process.md)). `ntrboot` uses a compatible DS flashcart to supply a sighax-signed firmware at boot. The mechanism can support recovery because it bypasses the normal NAND boot path; consult the [3DS Hacks Guide](https://3ds.hacks.guide/) for current procedures.

## Technical details

- Boot ROM signature check `[documented]` ([3dbrew: 3DS System Flaws](https://www.3dbrew.org/wiki/3DS_System_Flaws), "sighax"): FIRM partitions are signed with RSA-2048, SHA-256, PKCS #1 v1.5. Boot9 validates the padding wrongly in three ways: (1) it accepts block type 02 (meant for encryption) as well as 01, so the padding need not be a run of `FF` bytes; (2) it does not require the padding to fill the block; (3) it does not bounds-check the DER-encoded hash-algorithm and hash lengths, so the "hash" can point outside the block, at the hash Boot9 itself computed on the stack, and Boot9 then compares a value with itself. 3dbrew estimates about 1 in 2^43 random signatures would pass, about one week on one GTX 1080 Ti.
- The same page says the flaw is also in factory firmware and earlier (down to 0.11) and was fixed in 1.0.0-0, which is how the brute force could start without a Boot9 dump (jakcron's finding as credited there).
- Getting code to run from the forged FIRM needs a second flaw `[documented]` (3dbrew, same page): Boot9's FIRM loader blacklists some of its own memory but not memory-mapped I/O (derrek, 2015), and its section check only tests that a section's start and end are outside a blacklisted region, so a section that encloses the ARM9 exception vectors at `0x08000000` overwrites them ("superhax"; found independently by Normmatt and SciresM, January 2017). Overwriting the data-abort vector and adding a 4th section that NDMA-copies to NULL gives the abort that runs the payload.
- Because the flaw lives in boot ROM, Nintendo could not fix it with a system update. A hardware revision would be needed; none appeared (the New 2DS XL was found to have the same boot ROM in June 2017).
- Boot9's ARM9 exception vectors (`0x08000000`, six 8-byte entries) are the entry point exploited with NDMA; the first-stage chain executes before the boot ROM is hidden.
- Boot9 can load a FIRM from NAND, from an NTR card or from Wi-Fi flash; its status bytes at `0x1FFFE000+0xC` track each device (3dbrew: Bootloader).
- ntrboot first practical release: August 2017 (initially ak2i and R4 cards). Stargate, a 3-in-1 hybrid, was announced in September 2017 and abandoned within months.

## Limits and risks

- Installing it modifies the FIRM partitions: an incorrect installation can still brick a console. Per the community history, boot9strap itself carries near-zero brick risk; ntrboot is described as an extremely potent unbricking tool because it bypasses the normal boot chain.
- It is not removed by system updates in the sense that the vulnerable boot ROM cannot be changed, but the sources do not describe what happens to an installed copy during an update, so treat that as unverified.
- It gives privileged code access to sensitive console data (including OTP and boot ROMs). Follow the [3DS Hacks Guide](https://3ds.hacks.guide/) for backup and recovery guidance.
- Its predecessor `arm9loaderhax` could be unsafe to install and required a prior OTP dump.
- Online bans are moot since online services ended in April 2024 ([history](history.md)).

## Open questions

- The actual forged-signature bytes are not given here; 3dbrew explains the three padding/DER flaws but no usable signature is reproduced on this wiki.
- Sources differ in framing: Copetti says the ARM9 cannot write the vectors but NDMA can; 3dbrew attributes it to Boot9's incomplete blacklist. They are compatible, but the wiki has not checked which of the two (MMIO omission or the enclosing-range bug) boot9strap itself uses.
- The exact set of flashcarts supported by ntrboot is not listed here.

## Sources

- [Rodrigo Copetti, Nintendo 3DS Architecture: Anti-Piracy and Homebrew](https://www.copetti.org/writings/consoles/nintendo-3ds/) — sighax mechanism, boot9strap development, ntrboot.
- [Hacks Guide Wiki: History of 3DS Hacking](https://wiki.hacks.guide/wiki/3DS:History_of_3DS_Hacking) — dates of 33C3, 33.5C3, ntrboot release, New 2DS XL boot ROM finding.
- [3dbrew: Bootloader](https://www.3dbrew.org/wiki/Bootloader) — Boot9 behavior and the NTR-card boot condition.
- [3dbrew: 3DS System Flaws](https://www.3dbrew.org/wiki/3DS_System_Flaws) — boot ROM vectors / exception path and signature flaws.
