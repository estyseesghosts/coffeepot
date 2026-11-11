# CTR system title list

Every piece of built-in 3DS software is a [title](titles.md) with its own ID: the Home Menu, System Settings, the eShop, each background service module, the firmware images, even tables of country names and bad-word lists. This page lists those system titles by category, so that a title ID seen in a file path, a log or an update list can be recognised.

3dbrew generates its title lists automatically from Nintendo's system-update SOAP service, so the version lists there are very long and change; this page keeps the identification data (IDs, names, what each is) and leaves the version numbers to 3dbrew. Per-update changes for the early firmware versions are on the [update data](update-data.md) page.

## Behavior

System titles are installed on the [NAND](nand-filesystem.md) and updated by system updates ([firmware](firmware.md)). Most exist in several regional variants whose low ID differs; most also come in a normal (`..02`) and a `SAFE_MODE` (`..03`) copy so that the updater environment can run without the normal system ([titles](titles.md)).

Several titles are present on a New 3DS only (their program-ID low word starts with `2`). **New 3DS:** these have no Old 3DS counterpart or are the New 3DS variant of an Old 3DS title; an Old 3DS skips program IDs with those bits set when handling a title's dependency list (see Modules below).

## Technical details

### Category prefixes (title ID high)

Each system category is a bit mask combination ([ID layout](titles.md#title-id-layout)):

| High word | Title type | Internal description | Mask bits |
| --------- | ---------- | -------------------- | --------- |
| `00040010` | Application | `SYSTEM_APPLICATION` | Normal, System |
| `0004001B` | System data archives | `SYSTEM_CONTENT` | Contents, CannotExecution, System |
| `0004009B` | Shared data archives | `SHARED_CONTENT` | Contents, CannotExecution, System, NotRequireRightForMount |
| `000400DB` | System data archives | `AUTO_UPDATE_CONTENT` | Contents, CannotExecution, System, NotRequireUserApproval, NotRequireRightForMount |
| `00040030` | Applet | `APPLET` | Normal, System, RequireBatchUpdate |
| `00040130` | Module | `BASE` | Normal, System, RequireBatchUpdate, CanSkipConvertJumpId |
| `00040138` | Firmware | `FIRMWARE` | Normal, CannotExecution, System, RequireBatchUpdate, CanSkipConvertJumpId |

Application titles (non-system):

| Category low half | Meaning | Mask bits |
| ----------------- | ------- | --------- |
| `0x0000` (`00040000`) | Application / eShop | Normal |
| `0x0001` (`00040001`) | Download Play child | DlpChild |
| `0x0002` (`00040002`) | Demo | Demo |
| `0x000E` (`0004000E`) | Patch | CannotExecution, Patch |
| `0x008C` (`0004008C`) | Downloadable content | NotRequireRightForMount, CannotExecution, AddOnContents |

Download Play titles (`00040001`): only one is installed on NAND at a time, up to 255 children per unique ID are indexed by the variation byte (legal range `0x0`-`0xFF`).

### Region columns

Regional titles use one low ID per region. For system applications the region shows in the fourth hex digit of the low word: for example System Settings is `00020000` (JPN), `00021000` (USA), `00022000` (EUR), `00026000` (CHN), `00027000` (KOR), `00028000` (TWN). Other categories use different spacings (Home Menu is `00008202` JPN, `00008F02` USA, `00009802` EUR, `0000A102` CHN, `0000A902` KOR, `0000B102` TWN), so look IDs up rather than computing them. The first 3DS update with CHN and KOR titles is 4.0.0-7 and TWN is 4.1.0-8 ([update data](update-data.md)).

### System applications (`00040010`)

Low IDs are the JPN value; the other regions follow the stepping above. Product codes in 3dbrew are marked uncertain (`?`). `N/A` regions are as 3dbrew states.

| JPN low | Name (short name) | Notes |
| ------- | ----------------- | ----- |
| `00020000` | System Settings (`mset`) | `CTR-N-HAS`. A CHN-only update touched only this title. The `ExeFS:/.code` is identical across USA/EUR/JPN regardless of version; 4.0.0-7 (4.1.0-8 for TWN) is identical for all regions; 5.0.0-11 identical except CHN; 7.0.0-13 differs for CHN, KOR, TWN. |
| `00020100` | Download Play (`dlplay`) | `CTR-N-HDL` |
| `00020200` | Activity Log (`PLOG`) | `CTR-N-HMK` |
| `00020300` | Health and Safety Information (`safe`) | New 3DS variant `ssafe` has high `2` (`20020300`) |
| `00020400` | Nintendo 3DS Camera (`CtrApp`) | `CTR-N-HEP`. Its SD private folder holds `phtcache.bin` ([SD filesystem](sd-filesystem.md)) |
| `00020500` | Nintendo 3DS Sound (`CtrApp`) | `CTR-N-HES`. SD private folder holds `voice/XX/*.m4a` |
| `00020700` | Mii Maker (`EDIT`) | `CTR-N-HED` |
| `00020800` | StreetPass Mii Plaza (`MEET`) | `CTR-N-HME` |
| `00020900` | eShop (`tiger`) | `CTR-N-HGR`; not in CHN |
| `00020A00` | System Transfer (`CARDBOARD`) | `CTR-N-HCB`; not in CHN |
| `00020B00` | Nintendo Zone viewer (`MARS`) | JPN/USA/EUR only |
| `00020D00` | Face Raiders | New 3DS variant `FACE` (`20020D00`) |
| `00020E00` | AR Games (`AR_ACT`) | `CTR-N-HAR` |
| `00020F00` | SAFE_MODE System Updater (`mset`) | `CTR-N-HSH`; one of the few system applications also carried on game-card system updates |
| `00023000` | Promotional video | Stubbed |
| `0002BF00` | Nintendo Network ID Settings (`act`) | `CTR-N-HAF`; JPN/USA/EUR only |
| `20023100` | microSD Management (`mcopy`) | **New 3DS** only |
| `2002C800` ... | **New 3DS** HOME menu, Friends list, Notifications and Game Notes digital manuals | Stubbed; one title per manual and region |

A limited set of system applications is shipped with game-card system updates: System Settings, Download Play, SAFE_MODE System Updater and Nintendo 3DS Sound (the last only in firmwares 3.0.0 to 6.2.0). The New 3DS game-card system-update partition carries only a `menu` stub and a `friend` stub.

### System applets (`00040030`)

All applets use the `SYSTEM` memory region ([applet notes in the OS page](os.md)). The Home Menu `ExeFS` is identical for USA/EUR/JPN.

| JPN low | Name (short name) | Notes |
| ------- | ----------------- | ----- |
| `00008102` | Test Menu (Demo1) | Dev units; the dev menu pair is on [dev unit software](dev-unit-software.md) |
| `00008202` | Home Menu (`menu`) | `CTR-P-HMM` |
| `00008402` | Camera applet (`CtrApp`) | Used by Home Menu |
| `00008602` | Instruction Manual applet | |
| `00008702` | Game Notes (`Cherry`) | |
| `00008802` | Internet Browser (`spider`) | **New 3DS:** `SKATER` browser at `20008802` |
| `00008A02` / `00008A03` | Fatal error viewer (`ErrDisp`) / SAFE_MODE ErrDisp | New 3DS: `20008A03` |
| `00008D02` | Friend List (`friend`) | |
| `00008E02` | Notifications (`newslist`) | |
| `0000C002` / `0000C003` | Software Keyboard (`swkbd`) / SAFE_MODE | New 3DS SAFE_MODE: `2000C003` |
| `0000C102` | Mii picker (`appletEd`) | |
| `0000C302` | Picture picker (`PNOTE_AP`) | |
| `0000C402` | Voice memo picker (`SNOTE_AP`) | |
| `0000C502` / `0000C503` | Non-critical error display (`error`) / SAFE_MODE | New 3DS SAFE_MODE: `2000C503` |
| `0000CD02` | Circle Pad Pro test/calibration applet (`extrapad`) | |
| `0000C602` | eShop applet (`mint`) | Lets applications reach the eShop (DLC and so on); the eShop application uses it too |
| `0000BC02` | Miiverse (`olv`) | |
| `0000F602` | Miiverse system library (`memolib`) (likely) | |
| `00008302` | In-app Miiverse posting applet (`solv3`) | |
| `00009502` | Cabinet (amiibo Settings) | |

One applet (`00008502`) is "not available on CDN".

### System data archives (`0004001B`)

| Low ID | Content |
| ------ | ------- |
| `00010002` | `ClCertA` (certificates) |
| `00010702` | `NS CFA` |
| `00010802` | A CFA holding a 1-byte `dummy.txt` containing `0` |
| `00018002` | Same as `00010802` at first; from 7.1.0-15 the dummy is gone and the RomFS holds web-browser data for NNID/networking |
| `00018102` | Local web-browser data (HTML/JS, graphics) for Miiverse offline mode |
| `00018202` | WebKit/OSS CROs used by the Miiverse applet and `act` ([CRO modules](cro-modules.md)) |
| `00019002` | `Fangate_updater` |

### Shared data archives (`0004009B`)

| Low ID (all regions unless noted) | Content |
| --------------------------------- | ------- |
| `00010202` | Probably Mii related; RomFS holds `CFL_Res.dat` |
| `00010402` | **Region manifest**, mounted as `area:` (the [country and region archive](country-region-data.md)) |
| `00010602` | Non-Nintendo TLS root CA certificates (`CACERT_PUBLIC_CA_<5..8>.der`) |
| `00011002`-`00011E02` | Per-language software-keyboard dictionaries (e.g. CHN/CN `00011002`, TWN/TN `00011102`, NL `00011202`, EN/GB `00011302`, EN/US `00011402`, FR/FR `00011502`, FR/CA `00011602`, DE `00011702`, IT `00011802`, JA `00011902`, KO `00011A02`, PT `00011B02`, RU `00011C02`, ES `00011D02`, PT/BR `00011E02`) |
| `00012x02`/`00013x02` | Error-string lists (regional IDs, 3dbrew marks the purpose with `?`); and the `eula:` archive |
| `00014002`-`00014302` | System fonts mounted as `font:`: `00014002` JPN/EUR/USA, `00014102` CHN, `00014202` KOR, `00014302` TWN |
| `00015x02` | The `rate:` archive (age ratings) |

### System data archives (`000400DB`)

| Low ID | Content |
| ------ | ------- |
| `00010302` | NGWord bad-word list |
| `00010502` | Nintendo Zone hotspot list |
| `00016x02` | **NVer** (`00016202` JPN, `00016302` USA, `00016102` EUR, `00016402` CHN, `00016502` KOR, `00016602` TWN; **New 3DS:** `20016202` JPN, `20016302` USA, `20016102` EUR, `20016502` KOR; no CHN or TWN) |
| `00017x02` | **CVer** (`00017202` JPN, `00017302` USA, `00017102` EUR, `00017402` CHN, `00017502` KOR, `00017602` TWN) |

The NVer title IDs can be found at offset `0x320` in every CCI. CVer holds the firmware version: see the note in [titles](titles.md#version-numbers) about its RomFS build field always being zero.

### System modules (`00040130`)

After Home Menu has loaded, all these modules are running except `MP`, `RO` and `act`, which are loaded on demand. When `PM` terminates a process it checks whether the processes listed as its dependencies are listed as dependencies of other processes, and terminates those that no longer appear in any list. All modules use the `BASE` memory region except `NS` (`SYSTEM`).

| Low ID (`02` = normal, `03` = SAFE_MODE) | Module | Notes |
| ---------------------------------------- | ------ | ----- |
| `00001002`, `00001102`, `00001202`, `00001302`, `00001402` | `sm`, `fs`, `pm`, `loader`, `pxi` | Stored inside `NATIVE_FIRM` (SAFE_MODE copies in `SAFE_MODE_FIRM`) |
| `00001502` | AM (application manager) | |
| `00001602` | Camera | New 3DS variant `20001602` |
| `00001702` | Config (`cfg`) | |
| `00001802` | Codec | |
| `00001A02` | DSP | |
| `00001B02` | GPIO | |
| `00001C02` | GSP | New 3DS variant `20001C02` |
| `00001D02` | HID | |
| `00001E02` | `i2c` | New 3DS variant `20001E02` |
| `00001F02` | MCU | New 3DS variant `20001F02` |
| `00002002` | MIC | |
| `00002102` | PDN | |
| `00002202` | PTM (play time, pedometer, battery) | New 3DS variant `20002202` |
| `00002302` | `spi` | New 3DS variant `20002302` |
| `00002402` | AC (network manager) | |
| `00002602` | `cecd` (StreetPass) | See [StreetPass](streetpass-cec.md) |
| `00002702` | CSND | |
| `00002802` | DLP | |
| `00002902` | HTTP | |
| `00002A02` | MP | |
| `00002B02`, `00002C02` | NDM, NIM | |
| `00002D02` | NWM | |
| `00002E02`, `00002F02` | Sockets, SSL | |
| `00003000` | Process9 | In SAFE_MODE and normal `NATIVE_FIRM` |
| `00003102` | PS (process manager) | |
| `00003202` | `friends` | |
| `00003302` | IR | |
| `00003402`, `00003502`, `00003702`, `00003802` | BOSS (SpotPass), News, RO, `act` | |
| `00004002` / `20004002` | NFC (Old 3DS / **New 3DS**) | |
| `20004102` | **New 3DS** `mvd` (video decode) | |
| `20004202` | **New 3DS** `qtm` (head tracking) | The only New 3DS-specific module loaded automatically at boot is `qtm` |
| `00008002` | NS | Memory region `SYSTEM` |

Behaviour differences (3dbrew): starting with 8.0.0-18 the Old 3DS `PM` skips handling dependency-list titles that have any bit of program-ID-low mask `0xF0000000` set (hard-coded), because the New 3DS has system modules that do not exist on an Old 3DS. **New 3DS:** when `PM` launches any title except `NS` it first tries the title with program-ID-low `0x20000000` set and only if that fails launches the title with it clear.

### Firmware (`00040138`)

`NATIVE_FIRM` and `SAFE_MODE_FIRM` for the initial versions are identical apart from core-version fields. On retail the `SAFE_MODE_FIRM` appears to be used only to run the System Updater application. When a GBA Virtual Console title is launched `AGB_FIRM` handles it; GBA VC saves under SD `/title/<TID>/data` use a custom format handled by `AGB_FIRM`.

| Low ID | Firmware | Notes |
| ------ | -------- | ----- |
| `00000001` | Unknown, very similar to `SAFE_MODE_FIRM` | Dev units only; seems used only by system updaters |
| `00000002` | `NATIVE_FIRM` | CHN, KOR, TWN versions match USA/EUR/JPN from 4.0.0-7 on |
| `20000002` | **New 3DS** `NATIVE_FIRM` | |
| `00000003` / `20000003` | `SAFE_MODE_FIRM` / **New 3DS** | CHN etc. from 3.0.0-5 |
| `00000102` / `20000102` | `TWL_FIRM` (DSi mode) / **New 3DS** | |
| `00000202` / `20000202` | `AGB_FIRM` (GBA mode) / **New 3DS** | |

See [firmware](firmware.md) for what the four images do.

### Factory and repair titles

Hard-coded title IDs referenced in retail code but not on the CDN:

| Title ID | Use |
| -------- | --- |
| `000400000F802A00` | Unknown; appears to be a game-card title; first non-system title in the play log from a repair |
| `000400000F802100`, `000400000F802200` | Second and third non-system titles listed from a repair |
| `000400000FFFFD00`, `...FFFC00`, `...FFFB00`, `...FFF900`, `...FFF800`, `...FFF700`, `...FFF600`, `...FFF500` | Used by retail `NS` for app IDs `0xF11`-`0xF18` |
| `0004003000008900` | Used by retail `NS` for app ID `0xF10` |
| `0004013000001902` | `dmnt`, a debugger sysmodule using dev-unit-only HIO; installed at the factory, deleted at the factory on retail units |
| `0004013000003602` | "debugger", dev units only; `NS` launches it depending on certain configuration-memory values |
| `000401300FEF0000` | Referenced in eShop metadata (`NS_UID 50010000000584`) but not on the retail CDN |

### TWL (DSi mode) titles

TWL category bits differ from CTR: bit `0x1` is a system title (decides whether a System Update touches it; sufficient but not necessary to hide it from Home Menu), `0x2` may mean no-execute, `0x4` internal storage, `0x10` developer tools.

| High word | Category | Bit masks |
| --------- | -------- | --------- |
| `00048004` | DSiWare port (application, `0x8004`) | TWL, `0x4`. Content is identical to DSi SRLs; a separate title-ID high word is used |
| `00048005` | System application (`0x8005`) | TWL, `0x1`, `0x4` |
| `0004800F` | System archive (`0x800F`) | TWL, `0x1`, `0x2`, `0x4`, `0x8` |
| `00048015` | Developer tool (`0x8015`) | TWL, `0x1`, `0x4`, `0x10` |

TWL system applications: `42383841` (`B88A`) DS Internet, `484E4441` (`HNDA`) DS Download Play client (`HNDC` CHN, `HNDK` KOR). System data archives: `484E4841` (`HNHA`) Nintendo DS cartridge whitelist, `484E4C41` (`HNLA`) version data. New system updates block DS flash cards only when the whitelist or `TWL_FIRM` is updated; the whitelist is the data `TWL_FIRM` uses to detect flash cards.

### Other Nintendo software named on 3dbrew

3dbrew's Nintendo Software page also lists, beyond the system titles above: notable downloadable software (3D Video, 3DS Virtual Console, Save Data Transfer Tool, Swapnote, Nintendo Video, Eurosport, Louvre Guide, YouTube, and titles that contain homebrew exploits), the Sample 3D Video (officially available on 1.1.0-1 only), Quick Camera (`ctrapp`), and software that is not publicly available (the [development unit software](dev-unit-software.md), Nintendo Service Center tools and software used in manufacturing). The page is a name list without IDs or details.

## Homebrew notes

This is mostly an index for decoding what a title ID in a CIA, a `title.db` or a log refers to. `NVer`/`CVer` are what tools read to report the installed firmware version ([titles](titles.md#version-numbers)). Custom firmware patches several of the system modules listed here ([Luma3DS](../homebrew/luma3ds.md)).

## Open questions

- The purposes of several error-string and `rate:` archives and of a few applet/title IDs are marked `?` by 3dbrew.
- Region IDs for some applets are blank on 3dbrew (CHN/TWN for many Miiverse titles).
- The version lists were deliberately not copied; 3dbrew regenerates them automatically.

## Sources

- [3dbrew: Title list](https://www.3dbrew.org/wiki/Title_list) — all categories, IDs, names and notes (page last edited 18 August 2025).
- [3dbrew: Nintendo Software](https://www.3dbrew.org/wiki/Nintendo_Software) — names of system titles and notable downloadable software.
- [3dbrew: Titles](https://www.3dbrew.org/wiki/Titles) — category masks and variations.
