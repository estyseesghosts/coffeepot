# History of 3DS hacking and homebrew

Over thirteen years the 3DS went from a locked-down console to one that anyone can run their own software on. The story has three acts: flashcarts and early kernel exploits (2011–2014), the open-source homebrew and custom-firmware era (2014–2016), and the boot-ROM break that made the system fully open (2016–2017), followed by a long tail of one-click installers racing Nintendo's patches until the shop and online services closed.

This page is the timeline. How each exploit works is on the [exploit catalogue](exploits.md); the design it attacked is on the [security architecture page](../software/security-architecture.md).

## Overview

| Era | What mattered |
| --- | ------------- |
| 2011–2013 | First ROM dumps, DS flashcards on 3DS, Gateway3DS card (the first real 3DS flashcart) |
| 2014 | Homebrew toolchain (`ctrulib`), the Ninjhax chain, first kernel exploits, emuNAND/RedNAND |
| 2015 | Custom firmware flourishes (rxTools, PastaCFW, ReiNand); New 3DS support; `arm9loaderhax` |
| 2016–2017 | Boot ROM dumped; sighax; `boot9strap` and ntrboot; bug bounty begins |
| 2018–2023 | Seedminer, *miner exploits, browser exploits, Safe Mode route, MSET9 |
| 2023–2024 | eShop closes (March 2023); online play ends (April 2024) |

## Timeline

### 2011–2013: the locked console

- **March 2011:** the 3DS releases in the West and the wiki **3dbrew** is created. **June 2011:** the first 3DS ROMs are dumped.
- Before anything 3DS-specific, DS flashcards that re-programmed their headers to impersonate whitelisted games worked on the 3DS — but only in DS mode with no access to 3DS hardware.
- **2012:** hardware RAM dumps by Neimod are believed to have led to the first userland and ARM9 exploits. A DS profile "message" field could be overflowed (the Settings app, `MSET`, crashes on a stack overflow), which became the entry point to Gateway's chain. In **March 2012** the first homebrew app (a "Hello World" in `.cxi` format) is written.
- **August 2013:** the **Gateway-3DS** flashcart launches: a DS card (to install the exploit), a loader file for the SD card and a 3DS card with an FPGA and microSD for one game at a time. The exploit chain went through the Settings app and a kernel exploit; it also contained a custom firmware.
- **October–December 2013:** Smealum demonstrates **redNAND** (booting a NATIVE_FIRM copy from the SD card); Gateway adds **emuNAND** and its payload is reverse-engineered and open-sourced as the "3DS Toolkit".

### 2014: the open-source start

- **January:** the "brickgate" scandal — Gateway firmware that intentionally bricked clone-card users also bricked innocent users.
- **February:** yellows8 discovers **memchunkhax**, the first ARM11 kernel exploit: Kernel11 tracks free memory pages in a linked list of "memchunk headers" stored in FCRAM, which an earlier exploit can overwrite to grant access to AXI WRAM and eventually control the kernel's page table. Patched in December 2014; derrek's **memchunkhax2** (a race condition) followed.
- **Early 2014:** `ctrulib` (now `libctru`) is released by Smealum with yellows8, ichfly, WinterMute, fincs, mtheall and plutoo.
- **March:** the first Citra emulator commit.
- **Mid/late 2014:** the **Ninjhax** package — a crafted QR code for *Cubic Ninja*, a GPU-DMA userland exploit ("GSPWN") and the **Homebrew Launcher** loading `.3dsx` files — gives clean, flashcart-free homebrew (limited to userland: 64 MB of RAM, no DSP). yellows8's **oot3dhax** follows; **Palantine**, a closed-source CFW, leaks in November. Sky3DS cards appear (piracy-only, high ban risk).

### 2015: custom firmware and the New 3DS

- **January:** Gateway cracks firmware 9.2; yifanlu publishes the **memchunkhax/firmlaunchhax** combination; community re-implementations follow.
- **February–May:** **rxTools** (roxas75), then **PastaCFW** (first open-source CFW, built on patois's Brahma), and **ReiNand** (first full CFW for the New 3DS, August). A dispute over piracy-enabling signature patches leads roxas75 to release the rxTools source and quit.
- **July–September:** Ninjhax2x, Tubehax, Ironhax (the first secondary exploit), **Menuhax** (an untethered, boots-with-the-Home-Menu exploit) and the Browserhax family.
- **December:** at 32C3, Smealum, derrek and plutoo reveal `arm9loaderhax` (a permanent boot exploit using the New 3DS loader stage), `memchunkhax2`, `snshax` and `ntrcardhax`. `arm9loaderhax` exploits the 9.6.0 loader: it boots even if the decrypted ARM9 key is garbage, and when `firm0` fails it falls back to `firm1` while the garbage-decrypted remnants of `firm0` remain in ARM9 RAM, so crafted data at the end of `firm0` runs with Kernel9 privileges at boot. The first version of `arm9loader` also left an ARM9 key in the AES engine.

### 2016–2017: the golden age

- **Early 2016:** exploit chains downgrade 10.x to 9.2 until 10.4 patches them; `ctr-httpwn`, `memchunkhax2.1` (Aliaspider) and DSiWare-based downgrades follow. **AuReiNand** (a fork) becomes **Luma3DS** (April, rewritten from scratch).
- **April–June 2016:** `salt_sploit_installer` enables the secondary exploits `(v*)hax`, `supermysterychunkhax`, `humblehax` (required buying a Humble Bundle game) and `basehaxx`.
- **September:** CTRNand Transfer and OTPless installers make `arm9loaderhax` easier (OTPless was removed for a rare random brick chance).
- **December:** at 33C3 derrek's talk reveals **soundhax** (a free primary exploit using the Sound app), **fasthax** and **sighax**, and how the boot ROMs were dumped. Nintendo opens a bug-bounty program on HackerOne ($100–$20,000), after which exploit developers largely stop publishing.
- **January 2017:** `safehax` (ARM9 privilege escalation, up to 11.2) streamlines `arm9loaderhax` installs. **February:** 11.3 patches safehax and fasthax; a later update restores safehax for 11.3.
- **April:** `udsploit` (smealum) lands as 11.4 patches it.
- **May:** SciresM's unofficial 33.5C3 talk reveals **boot9strap** and the principles of **ntrboot**; **June:** the New 2DS XL ships with the same vulnerable boot ROM; **August:** the first practical ntrboot is released.

### 2018–2023: one-click chains and patch races

- **January 2018:** **Seedminer** brute-forces `movable.sed` from the LocalFriendCodeSeed, allowing injection of crafted DSiWare with just one console.
- **September 2018:** **Frogminer** (a free *miner exploit using Flipnote Studio injected through Download Play). **August 2018:** Smealum shows an ARM9 chain already patched in 11.8; it was never completed by others.
- **July 2019:** `BannerBomb3` (System Settings). **December 2019:** MrNbaYoh's StreetPass-based remote chain is shown at 36C3 after being reported and patched in 11.12.
- **2020:** `unSAFE_MODE` (11.13), the free browser exploits `new-browserhax` / `-xl` (zoogie), the **SSLoth** entry point with `safecerthax` (Safe Mode, Old 3DS), TuxSH's `universal-otherapp`, and `menuhax67`. Nintendo patches several; the HackerOne program closes **July 15, 2020**; retail production ends in **September 2020**; **Unity3DS and many dev items end in January 2021**.
- **2021–2023:** `nimhax`, `old-browserhax-xl`, `kartdlphax` (Mario Kart 7, needs a second modded console); 11.15 (July 2021) and 11.16 (August 2022) patch browser exploits, SSLoth and `smpwn`; `ENLBufferPwn` (online RCE in Mario Kart 7, disclosed Dec 2022) was already patched in game v1.2; `super-skaterhax` (March 2023, New 3DS browser); 11.17 (May 2023) patches `BannerBomb3`; `nimdsphax` (July) disables GPU protection and overwrites kernel code with the GPU; and **MSET9** (October 2023, zoogie) — a free, stable, extra-hardware-free route on both Old and New 3DS.
- **March 27, 2023:** the eShop closes, removing the free/paid games many exploits relied on from the main paths.

### 2024

**8 April 2024:** Nintendo discontinues online play and communication for the 3DS and Wii U.

## Technical details

A key progression is the **ladder** each generation climbed:

1. A primary userland exploit (QR code, browser, a game, a DSiWare/settings bug) →
2. kernel (ARM11) access (memchunkhax family, `khax`, `udsploit`, `fasthax`, `nimdsphax`) →
3. ARM9 access (`safehax`, `unSAFE_MODE`, `universal-otherapp`, `arm9loaderhax`) →
4. boot-ROM-level persistence (`boot9strap`, which uses the sighax flaw) — see [boot9strap](boot9strap.md).

## Homebrew notes

For present-day use, the shape of the journey is: get any entry point (see [exploits](exploits.md) for what still works), install `boot9strap` ([boot9strap](boot9strap.md)), then use a custom firmware like Luma3DS ([custom firmware](custom-firmware.md)). Development is covered in the [workflow page](development-workflow.md).

## Open questions

- Some dates (and attributions) differ between sources by a month or so; the Hacks Guide wiki page is used as the master timeline and Copetti for narrative detail.
- Gateway's exact ARM9 exploit is described as unknown by Copetti.
- Any post-2024 developments are outside the sources.

## Sources

- [Luma3DS README](https://github.com/LumaTeam/Luma3DS) — project start (2015), later maintainers and when each system module was added.
- [Hacks Guide Wiki: History of 3DS Hacking](https://wiki.hacks.guide/wiki/3DS:History_of_3DS_Hacking) — master timeline (itself adapted from zoogie's "A Pretty Brief History of the 3DS Hacking/Homebrew Scene" on GBAtemp).
- [Rodrigo Copetti, Nintendo 3DS Architecture: Anti-Piracy and Homebrew](https://www.copetti.org/writings/consoles/nintendo-3ds/) — narrative of Gateway3DS, Ninjhax, memchunkhax, arm9loaderhax, sighax, boot9strap, ntrboot.
- [3dbrew: Homebrew Exploits](https://www.3dbrew.org/wiki/Homebrew_Exploits) — exploit names and firmware ranges.
- [3dbrew: 3DS System Flaws](https://www.3dbrew.org/wiki/3DS_System_Flaws) — kernel and ARM9 flaw details.
