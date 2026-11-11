# CTR system flaws

The 3DS was designed so that only Nintendo-signed code runs and the secret keys stay in hardware ([security architecture](security-architecture.md)). Over the years people found many mistakes in that design. This page is a catalogue of the documented ones, grouped by the part of the system they sit in, with the first system version that fixed each. It exists so that a reader can tell what an exploit name refers to and which firmware it needs.

The names (sighax, gspwn, memchunkhax, ...) are the community's nicknames. How the flaws were chained into working exploits is on the [exploit catalogue](../homebrew/exploits.md) and the [history](../homebrew/history.md).

## Behavior

Most flaws are not usable alone. The usual pattern is: a bug in a game or the browser gives code in a normal application (return-oriented programming first, because the ARM11 enforces execute-never on memory and no official call lets an application make arbitrary memory executable except the `RO` module); a second bug then escalates to the ARM11 kernel, a third reaches the ARM9, and a boot ROM flaw finally allows permanent control. SD extdata and SD savegames can also be attacked on consoles whose `movable.sed` has been dumped, though running code on the console is easier [documented: 3dbrew System Flaws].

Fix columns below use the system version that fixed the flaw ("none" means no fix known). Unless a row says otherwise it applies to Old 3DS; **New 3DS:** differences are called out. The hardware and boot ROM rows apply to both models because the New 3DS has exactly the same boot ROM as the Old 3DS.

## Technical details

### Hardware and boot ROM

| Flaw | Effect | Fixed | Found by / when |
| ---- | ------ | ----- | --------------- |
| ARM9/ARM11 boot ROM exception vectors point at uninitialised RAM | A fault injected within about 100 CPU cycles after start-up (after the PLLs stabilise) falls into RAM, which is not cleared, so own code can dump the boot ROM and OTP | none (same boot ROM on all models) | derrek (boot ROM dumped summer 2015), WulfyStylez independently |
| AES engine does not clear keys on hard reset | Key slots survive a reboot | none | Mathieulh and others, 2014 |
| No RAM clearing on MCU-triggered reboot | FCRAM, ARM9 memory, AXI WRAM and VRAM keep contents | none | derrek, 2014 |
| Only 31-32 bits of TWL console-unique key data are console-unique | The 8 bytes at ARM9 `0x01FFB808` are XORed with fixed data; the high `u32` is always `0x0` on Old 3DS and always `0x2` on New 3DS, and the low `u32` always has its top bit set, so TWLNAND keys can be brute-forced from TWLNAND data (the DSi uses all 8 bytes) | none | Yellows8 |
| DSi / 3DS-TWL key generator | Overwriting parts of a generated normal-key and comparing outputs reveals the generator function; does not work for the 3DS generator because key slots 0-3 are TWL-only | none | Yellows8, 2011 |
| 3DS key generator is weak | Differential analysis from known normal-key / keyY pairs recovers the function | none | Yellows8, plutoo, 2015 |
| RSA key slots keep the exponent when the modulus is set | The write-only private exponent can be recovered with a smooth-order prime modulus and Pohlig-Hellman; of limited use (slot 0 only derives the 6.x save and 7.x NCCH keys, already known) | none | Myria, 2016 |
| `CFG11_GPUPROT` lets the GPU reach AXI WRAM and FCRAM | Any exploited sysmodule with access to the register page can turn the GPU into a memory-access tool (see gspwn) | none | Yellows8 |
| FIRM partitions known plaintext | FIRM is AES-CTR encrypted without a MAC; knowing a FIRM binary reveals its keystream, which allows minor FIRM downgrades (10.4 to 9.6, 9.5 to 9.4, not 9.6 to 9.5) and, with sighax, installing arbitrary FIRM (usually boot9strap) | none | everyone |
| Boot9 AES key-init issues | Two bugs in the key-init function | none | Yellows8, 2015 |
| **sighax**: Boot9 mis-validates FIRM RSA padding | Accepts block type 02, does not require the padding to fill the block and does not bounds-check the DER lengths, so a fake signature that makes Boot9 compare a hash with itself is valid; about one in 2^43 random signatures works (a GTX 1080 Ti needs about a week) | none | derrek, 2015 |
| Boot9 FIRM loading does not blacklist memory-mapped I/O | With sighax a FIRM can overwrite the Boot9 data-abort or IRQ handler | none | derrek, Normmatt and SciresM |
| **superhax**: blacklist check only tests section start and end | A FIRM section enclosing a whole blacklisted region (for example the Boot9 vector table at `0x08000000`) overwrites it | none | plutoo, Yellows8, 2015 |

### arm9loader (New 3DS FIRM loader; runs on Old 3DS too)

| Flaw | Effect | Fixed |
| ---- | ------ | ----- |
| Keysector keys derivable from Boot9 + ITCM | Boot9 puts the first `0x90` bytes of the plaintext OTP in ITCM, arm9loader hashes the raw encrypted OTP; knowing the OTP keydata and IV lets anyone compute the keys, even on Old 3DS. A fix would need more OTP data and a NAND keysector update (dangerous). Public January 2017 (Yellows8). | none |
| NAND keystore is AES-ECB encrypted | Blocks can be rearranged; with 10.0 FIRM this can execute ARM9 memory (arm9loaderhax given existing ARM9 execution). | 11.3.0-X last checked |
| Uncleared OTP hash keydata (`SHA_HASH`) | Kernel9Loader leaves the hash of the OTP data in the register; with the NAND keystore it yields the full `0x200`-byte New 3DS keystore (all keys identical when decrypted), so all New 3DS keys can be derived on any system version, even Old 3DS. Reported by WulfyStylez, Dazzozo, shinyquagsire23 (13 January 2016). | none; K9L cannot be reused as is |
| enhanced-arm9loaderhax | Combination of the above and the Boot9 trick; works on all New 3DS with the FIRM known-plaintext bug and brute-forcing the keystore's second entry | cannot be fully fixed |
| arm9loaderhax: no verification block for the 9.6 keys | A wrong key in NAND makes arm9loader decrypt the kernel to garbage and jump to it; repeated with NOP sleds, this gives very early ARM9 code execution (plutoo, March 2015). Recovers the 6.x save and 7.x NCCH keys. Works on Old 3DS only with the plaintext New 3DS key from sector `0x96` and an OTP dump. | none |
| arm9loader runs on Old 3DS | New 3DS FIRM in the FIRM partitions of an Old 3DS runs until Kernel9, so arm9loader bugs also affect Old 3DS | none |
| Uncleared New 3DS key slot `0x11` | The loader originally cleared slot `0x11` only at firmlaunch; found one day after 9.5.0-X | mostly 9.5.0-X, fully 9.6.0-X (new keys) |

### Process9 and Kernel9

| Flaw | Effect | Fixed |
| ---- | ------ | ----- |
| Leak of normal-key matching key-scrambler key | **New 3DS:** 8.1.0 to 9.2.0 used a hard-coded normal-key for amiibo data; 9.3.0 "fixed" this by hard-coding the matching keyY, which revealed the scrambler algorithm | 10.0.0-X (sort of) |
| Leak of normal-key matching key-generator key (common key index 1) | The devkit key generator could be deduced; dev common keys follow once the keyX is recovered (jakcron, January 2016) | before 1.0.0-X |
| Factory firmware vulnerable to sighax | Boot9 flaw also in factory firmware down to 0.11 (SciresM, Myria, 2017) | 1.0.0-X |
| safecerthax | Old 3DS and Old 2DS `SAFE_FIRM` still has the `PXIAM:ImportCertificates` flaw (fixed in 5.0.0-11) and SSLoth (fixed in 11.14.0-46); lets one spoof the update server and remotely reach ARM9 code (MrNbaYoh, December 2020) | none |
| twlhax / agbhax | Process9 does not verify the RSA signature of `launcher.srl` and `TwlBg` hands it a crafted SRL header; DSi memory is interleaved in FCRAM, so load addresses can overwrite Process9 stack (smea, 2018). `AGB_FIRM` shares the vulnerable code (December 2020) | 11.8.0-X (twlhax), 11.14.0-X (agbhax) |
| safefirmhax | `SAFE_MODE_FIRM` was not updated for the firmlaunch fix; a firmlaunch into it repeats the attack. The first fix (a global flag set when a non-sysmodule title launched) was bypassed by launching `NATIVE_FIRM` then `SAFE_MODE_FIRM` immediately | 9.5.0-X for NATIVE only; fix completed with CFG9_BOOTENV checks in 11.4.0-X |
| ntrcardhax | Process9 trusts a register shared with the ARM11 to learn when an NTR banner has been read; a custom flashcart banner overflows a buffer (plutoo, March 2015) | 10.4.0-X |
| Title downgrading via AM | Version check is skipped if the title is deleted first. 11.0.0 added a hard-coded minimum-version list for key system titles; legacy FIRMs were added only in 11.8.0 | 11.0.0-X / 11.8.0 |
| `TWL_FIRM` cmd 8/9 unchecked offset | In 1.0.0 `TWL_FIRM` an unchecked write offset gives ARM9 code execution with any NTR cart inserted (Riley, January 2018) | 2.0.0-X |
| FIRM launch does not check the target FIRM version | An old, exploitable FIRM can be booted from a newer one. Use after 9.6.0: boot 9.2.0 system software with the 9.6.0 Process9 so keyslots `0x15`/`0x16` are set, then firmlaunch-hax. Moot once the New 3DS keystore was dumped | none (2011 anti-downgrade does not affect installed titles) |
| FAT code null dereference | A corrupt file on SD can crash Process9 on `FSFile:Read`; useless null read | 9.6.0-X |
| `FSPXI:EnumerateExtSaveData` crash (**MSET9**) | An unchecked return value lets a file named with eight hex digits placed in the extdata directory crash Process9; in one case it jumps to the ID1 string, which can hold ARM instructions as the 3DS only checks that ID1 is 32 characters (zoogie, April 2022 / 7 August 2023) | 11.17.0-X |
| RSA signature padding checks | TWL RSA padding check issues (the main 3DS check has proper padding checks first) | 9.5.0-X |
| `AMPXI:ValidateDSiWareSectionMAC` keyslot reuse | An out-of-range section index leaves the previous key slot selected | 11.3.0-X last checked |
| `pxips9` keyslot reuse | An invalid key type selects slot `0x40`, which is silently ignored; needs `ps:ps` (e.g. via snshax on 10.1.0 or lower) | 11.3.0-X last checked |
| firmlaunch-hax | The FIRM header used for launch sat in FCRAM (`0x200` bytes, time-of-check to time-of-use); not exploitable from ARM11 userland. Address moved to ARM9-only `0x01FFFC00` (found 2012, 3 days after Yellows8 started reversing Process9) | 9.5.0-22 |
| Uninitialised data in PXI replies | Stubbed commands and error returns can leak ARM registers | 9.3.0-X last checked |
| `FSPXI OpenArchive` ignores the SD mount permission | Little practical use as most processes already have SD access | 9.3.0-X last checked |
| `AMPXI:ExportDSiWare` export path | Path not validated, so DSiWare can be exported to arbitrary Process9 paths such as `nand:/...`, but the data cannot be controlled | 9.5.0-22 |
| DSiWare CTCert verification | 3DS still verifies the APCert of DSiWare on SD with the CTCert inside the `.bin`, as the DSi did before 1.4.2; with seedminer this lets modified DSiWare containers be injected (sudokuhax, ugopwn) | 11.10.0-X |
| **seedminer**: `movable.sed` keyY brute force | Half of the 128 bits leak through the LFCS, and the LFCS leaks almost half of the rest (`keyY[3]` is about 1/5 of the LFCS); final uncertainty about 2^40 (zoogie, December 2017) | 11.8.0-X last checked |
| DSiWare SRL not matched to the TMD title ID | Older exploitable DSiWare can be embedded in an unrelated title; DSiWare has raw NAND read/write, giving ARM9 control via FIRM known plaintext and sighax | 11.10.0-X |
| DSiWare import/export accepts TWL system titles | `AM ImportTwlBackup/ExportTwlBackup` allow titles such as DS Download Play; usable with a dsihax primary that loads a payload from non-NAND (Flipnote Studio JPN v0) (zoogie, 2018) | 11.10.0-X |
| Gamecard PXI transfer-size index | An unchecked `u8` indexes a `u16` array; harmless out-of-bounds read | 5.0.0-X |
| PXI command buffer overrun | Command size not checked before copying; likely ARM9 code execution | 5.0.0-11 |
| `PXIAM:ImportCertificates` | A 0x2800-byte heap buffer is filled from four FCRAM buffers with no size check; ARM9 code execution (Yellows8, May 2013) | 5.0.0-X |
| `ps` RSA command buffer overflows | Unchecked copy in `VerifyRsaSha256` into `.bss`; signature buffer moved to the stack with a size check | 5.0.0-X |
| `pxi_id` bad check | Read as a signed char, used as a table index; negative values pass a `< 7` check | 3.0.0-5 |
| `CFG9_SYSPROT9` bit 1 not set by Kernel9 | The `0x10012000` region (holding OTP) is unlocked and can be dumped once ARM9 code runs. Fixed in Kernel9 from 3.0.0-X; **New 3DS:** arm9loader sets the bit instead, and downgrading to a vulnerable version was used to dump the New 3DS OTP (plutoo, Normmatt, February 2015) | 3.0.0-X |

### ARM11 kernel (Kernel11)

| Flaw | Effect | Fixed |
| ---- | ------ | ----- |
| `svcUnbindInterrupt` double free (irqId 15) | Use after free; kernel code execution (TuxSH, 2019) | 11.14.0-X (irq 15 cannot be bound on retail) |
| `svcKernelSetState` op=3 maps the NULL page | Maps the firmlaunch parameters page at any VA, including 0 (TuxSH, 2019) | 11.14.0-X |
| `svcMapProcessMemory` can map the NULL page | Destination VA unchecked (TuxSH, 2020) | none known |
| Resource-limit use after free | The refcount is not incremented when assigned to a process; asking `pm` to terminate itself with TID `0004000100001000` then triggers it; very hard to exploit | none (near impossible on 11.14.0-X) |
| `svcSetProcessIdealProcessor` refcount overflow | Passing idealprocessor 3 returns early without decrementing the refcount (st4rk, November 2017) | 11.6.0-X |
| `svcGetThreadList` reference leak | Refcount overflow, probably not exploitable (TuxSH, April 2017) | 11.3.0-X |
| kernelhax via gspwn | Kernel did not initialise `CFG11_GPUPROT`, so the GPU could access all FCRAM and AXI WRAM (plutoo, Yellows8, 2017) | 3.0.0-X |
| fasthax | A pulse-mode `KTimer` can be closed from another core while the scheduler is locked, giving a use-after-free vtable call (nedwill, May 2016) | 11.3.0-X |
| ipctakeover | Command replies do not check that source address and size match the destination (Yellows8, November 2016); used by ctr-httpwn (bosshaxx) and NIM takeover | 11.3.0-X last checked |
| IPC input buffers used as output buffers | Reply descriptor type is not checked against the request | 11.3.0-X last checked |
| SVC table too small | Table covers up to `0x7D` but SVCs `0x7E`/`0x7F` are allowed; needs kernel patching to exploit | 11.3.0-X last checked |
| `svcBackdoor` (`0x7B`) | Runs SVC-mode code at a user-chosen address; used by Process9 | 11.0.0-X (deleted) |
| veryslowpidhax | The kernel PID counter is not checked for wraparound to 0; PIDs below the module count get all services; would take weeks | none |
| slowhax / waithax | `svcWaitSynchronizationN` does not decrement references on an invalid handle, so a refcount can be overflowed to zero (nedwill, derrek, 2016) | 11.2.0-X |
| Kernel virtual memory `0xEFF00000`/`0xDFF00000` | Maps all DSP memory and AXI WRAM RW- and is never unmapped | none |
| memchunkhax2 / 2.1 | Next pointer of a free-memory chunk header is read after being mapped to userland (race condition, derrek); the GPU variant (memchunkhax2.1, derrek and aliaspider) survived the 10.4 fix | 10.4.0-X / 11.0.0-X (memchunk header MAC) |
| heaphax | Free chunk sizes in FCRAM changed by DMA allow allocating over the `NS` module (smea, April 2015) | 11.0.0-X |
| snshax | Forces creation of Safe `NS` into gspwn-able memory | 10.1.0-X |
| AffinityMask / processor ID validation | `svcCreateThread` off-by-one: `processorid == total_cores` writes out of bounds (Old 3DS: crash; **New 3DS:** uncontrolled write into a kernel L1 MMU table) (Yellows8, May-September 2015) | 10.0.0-X |
| memchunkhax | Free-memory chunk headers were not validated; with gspwn raw access they allow overwriting AXI WRAM code via the `0xEFF00000` mapping (Yellows8, February 2014) | 9.3.0-21 |
| `KLinkedListNode` use after free | Fields read after freeing on the slab heap (derrek, April 2015) | 8.0.0-18 |
| PXI buffer permissions | Input/output buffers must be RW from 6.0.0 or kernel panic | 6.0.0-11 |
| `svcStartInterProcessDma` | Kernel read `DmaConfig` without access checks; integer overflows in `srcaddr+size` and `dstaddr+size` and addresses above `0x20000000` | 6.0.0-11 |
| `svcControlMemory` checks | Range checks keyed on the full operation value `0x10003`, and unchecked `addr0+size` overflows | 5.0.0-11 |
| Command buffer overflow | Kernel did not check command header word counts; now panics above `0x40` words | 5.0.0-11 |
| SVC stack allocation overflows | Variable-length stack arrays in `svcSetResourceLimitValues`, `svcGetThreadList`, `svcGetProcessList`, `svcReplyAndReceive`, `svcWaitSynchronizationN` | 5.0.0-11 |
| `svcControlMemory` MAP permissions | `.text` could be mapped elsewhere as RW- (Yellows8, 2012) | 4.1.0-8 |
| Command buffer permissions | I/O buffers' permissions unchecked, so `FSUSER` file read could write `.text` | 4.0.0-7 |
| `svcRead/WriteProcessMemory` | Only the first page checked; no known retail process has access | 4.0.0-7 |

### ARM11 system modules

| Flaw | Effect | Fixed |
| ---- | ------ | ----- |
| `srv:pm` registration | Originally any process could re-register itself and bypass the exheader service-access list; since 7.0.0-13 it is a service for PIDs below 6 with one session only | 7.0.0-13 |
| `FSDir` null dereference, SM off-by-one write, PXI module command overrun, SM `RegisterPort` overflow (**smpwn**, TuxSH, July 2017) | Null reads, an unused overwritten field, ROP under PXI after Process9 compromise (used by safecerthax), and SM takeover | `smpwn` 11.16.0-48 |
| CSND `PlaySoundDirectly` out-of-bounds channel | Channel above 3 crashes the module (PabloMK7, January 2021) | none |
| **SSLoth** | The SSL module did not verify certificate signatures; allows spoofing official servers (MrNbaYoh, shutterbug2000, December 2020) | 11.14.0-46 |
| CECD flaws | `SetNZoneMacFilter` stack smash (MrNbaYoh, July 2020, fixed 11.13.0-45); any process can write to any title's message box; packet type `0x32`/`0x34` stack smash and `TMP_XXX` file parser overflows (fixed 11.12.0-44) | as listed |
| `CFG:CreateConfigInfoBlk` integer underflow | Write past the buffer, only a crash in practice (MrNbaYoh, November 2018) | 11.8.0-41 |
| MP `SendDataFrame` / cmd1 | Unvalidated index into state and a signed index for handle reads (Yellows8, January 2017) | 8.0.0-18 |
| AM `ReadTwlBackupInfo` stack infoleak | Unchecked size (Yellows8, 2016; not tested on hardware) | 10.0.0-27 |
| APcert infoleak via `.ctx` | The download state file is all `0xFF` after the header just after purchase, so an xorpad can be made and the later APcert read; the APcert contains the device ID, related to the LFCS and helpful for `movable.sed` (zoogie, 2022 / March 2023) | 11.16.0-49 |
| MVD `SetupOutputBuffers` stack overflow | Unchecked `total_entries`, only crashes | none known |
| NWM / HTTP use the CTRSDK heap with user-provided shared memory | After gspwn the shared memory can be overwritten and the allocator's chunk-header writes give ROP; exploited by ctr-httpwn | 11.13.0-X (HTTP) |
| DLP spectator frame bugs | Out-of-bounds checksum read and out-of-bounds write on a title list, only remote crashes (Yellows8, April 2016) | 10.0.0-X |
| IR stack buffer overflow | Unvalidated I2C FIFO levels, exploitable with custom hardware (Yellows8, February 2016) | 10.6.0-31 |
| NIM old-version download | NIM does not validate the requested title version; with AM downgrading this allows installing any title version still on the CDN (Yellows8, October 2015) | 10.0.0-X |
| `spi` out-of-bounds write | `cmd1` overwrites `.data` statics; gives `CFG11_GPUPROT` and kernel code execution (plutoo, March 2015) | 11.14.0-46 |
| NFC command buffer overflows | `nfc:dev` `0x000C` and `nfc:s` `0x0037` overflow the stack; these commands are stubbed in the Old 3DS module, so **New 3DS:** only | 9.5.0-22 (New 3DS) |
| NEWS `news:s` notification ID | Out-of-bounds array index (Yellows8, December 2014) | 9.7.0-X |
| `NWMUDS:DecryptBeaconData` heap overflow | Remote denial of service by a crafted beacon; breaks all Wi-Fi communication until reboot | 9.0.0-20 |
| HID shared-memory index | Unvalidated index; HID data written to arbitrary addresses, very unlikely to be exploitable | 9.3.0-21 |
| **gspwn** | GSP does not validate addresses given to the GPU, so an application can read and write physical FCRAM by GPU DMA (smea, Yellows8, early 2014); the GPU can reach FCRAM up to `0x26800000` (Old 3DS) | 9.6.0-X |
| GSP client management; unbound DMA | Shared memory of GSP clients is on one page, `TriggerCmdReqQueue` does not check render rights, and `AcquireRight` accepts any process handle, giving arbitrary read/write into other processes (kynex7510, May-June 2025) | 11.17.0-50 |
| **rohax** and RO flaws | A CRO0/CRR0 overwritten by gspwn after its signature check gives code execution in `ro` (fixed after ninjhax with pointer checks); `ro` keeps CRR pointers without tracking the owning process and a custom CRS can patch relocation targets, including executable pages (kynex7510, June-July 2025) | 9.3.0-21 (rohax) / 11.17.0-50 |
| Region free | Only Home Menu checks a game card's region, so launching the card through `ns:s` (for example during a FIRM launch) skips it and the card-system-update installation (Yellows8, June 2014) | none (last tested 10.1.0-X) |
| NWM state null dereference | Crash only | 9.0.0-20 |

### SDK library flaws

| Flaw | Effect |
| ---- | ------ |
| `cec` StreetPass message exheader stack smash | More than 16 exheader entries overflow the stack in any application parsing StreetPass messages (MrNbaYoh, 2019); fixed for system software in 11.12.0-44 |
| UDS beacon additional data overflow | At most `0x37` bytes past the buffer; difficult (Yellows8, 2014); fix version unknown |
| CTPK buffer overflow | Entry `wordsize` not validated when copying to a stack buffer; fixed in CTR_SDK 11.4.0 (Yellows8, November 2016) |
| Pia library | Flaws found in Pia 5.x for Switch are also in 3DS Pia 3.x/4.x; encryption was not used before the Switch. "pialease nerf": a UDS packet with command 5 (`UpdateMigrationNodeInfoMessage`) from the network host gives stack overflow in every client (Riley, June 2023); fixed in Pia 4.x, not on 3DS/Wii U titles built earlier |

> Conflict: the [security architecture](security-architecture.md) page, following Rodrigo Copetti, attributes some changes to 7.0.0 and 11.0.0 (RSA engine in NCCH decryption, anti-downgrade). 3dbrew's flaw tables confirm the 11.0.0 anti-downgrade list for key system titles, with legacy FIRMs added only in 11.8.0; this page follows 3dbrew.

## Homebrew notes

Each nickname above is an exploit component in some chain: sighax and arm9loaderhax relate to [boot9strap](../homebrew/boot9strap.md), gspwn and the memchunkhax family to userland-to-kernel steps, seedminer and the DSiWare flaws to the `movable.sed` path through [SD](sd-filesystem.md), and the CECD/SSL/HTTP bugs to StreetPass and network-facing entry points ([StreetPass](streetpass-cec.md)).

## Open questions

- 3dbrew's "fixed in" columns often give the last version checked, not the actual fixing version.
- The page notes a rejected early project (Neimod's RAM dump rig) as unreleased; nothing further is known.

## Sources

- [3dbrew: 3DS System Flaws](https://www.3dbrew.org/wiki/3DS_System_Flaws) — all flaw tables (hardware, boot ROM, ARM9, ARM11, sysmodules, SDK).
- [3dbrew: Titles](https://www.3dbrew.org/wiki/Titles) — `.ctx` files referred to by the APcert infoleak.
