# CTR system update data

Each 3DS system update is a list of [titles](titles.md) that changed: new versions of System Settings, the Home Menu, individual background modules, data archives and, in many updates, the firmware itself. This page records what 3dbrew's *Update Data* page covers and what can be learnt from it, without copying its hundreds of table rows.

The page is useful for two jobs: knowing which title versions belong to a given system version (for example when deciding which `NATIVE_FIRM` a title was built for), and spotting which regions an update touched. How updates are delivered and applied is on the [firmware page](firmware.md).

## Behavior

A system update does not replace the whole system: it installs new versions of only the titles in its list, in `SAFE_MODE_FIRM`'s System Updater ([firmware](firmware.md)). Because titles are versioned independently, one system version name such as `4.1.0-8` stands for a particular set of title versions and each region gets its own regional titles.

## Technical details

### Scope of the source

3dbrew's page lists updates only from **2.1.0-3 to 5.0.0-11**; the later updates are not on that page. Its tables are generated from the update server and give, per update, rows of (title ID, region, new version). The source page says it was mostly generated from Nintendo's own update listing and that the current listings are kept elsewhere (the [title list](title-list.md) page on 3dbrew regenerates automatically).

> Unknown: this wiki has no per-update data for system versions after 5.0.0-11. The title version histories on the [title list](title-list.md) pages cover later versions only as long version lists on 3dbrew.

### Updates documented and what they introduced

| Update | Regions in list | Firmware titles in the list | Notes |
| ------ | --------------- | --------------------------- | ----- |
| 2.1.0-3 | USA, JPN, EUR | `NATIVE_FIRM` v3553 | eShop (USA v1026), Home Menu, MCU, AC, HTTP and SSL modules, `NVer`/`CVer` archives |
| 2.1.0-4 | USA, JPN, EUR | none | Applications (Download Play, Camera, Sound, StreetPass Mii Plaza, Face Raiders, AR Games), Internet Browser, `NVer`, DS Internet (`B88A`) |
| 2.2.0-X | all regions together | `NATIVE_FIRM` v4595 | ErrDisp v2048, Home Menu v3075, Friend List v2051, about a dozen modules (including `NS` v2048), DS cartridge whitelist |
| 3.0.0-5 | USA, JPN, EUR | `NATIVE_FIRM` v5647, `SAFE_MODE_FIRM` v5632, `TWL_FIRM` v2565, `AGB_FIRM` v519 | A large update: many applications, applets and modules, `0004009B` shared archives (region manifest v2050, error strings, eula) and all four firmware images |
| 3.0.0-6 | USA, JPN, EUR | none | Only a few applications and `NVer` |
| 4.0.0-7 | adds CHN and KOR | `NATIVE_FIRM` v6677, `TWL_FIRM` v4625, `AGB_FIRM` v2576 | Chinese and Korean titles first appear (system data archive `0004001B00010002`, fonts, dictionary archives); region manifest v3072 |
| 4.1.0-8 | adds TWN | `NATIVE_FIRM` v7712 | Taiwanese titles; most modules re-listed for TWN |
| 4.2.0-9 | all six regions | none | Mostly per-region applets, shared data archives and `NVer`/`CVer` |
| 4.3.0-10 | all six regions | none | NGWord and Nintendo Zone hotspot data archives (v4096), DS whitelist |
| 4.4.0-10 | all six regions | `TWL_FIRM` v5681 | Hotspot data, DS whitelist, CHN/KOR/TWN StreetPass Mii Plaza v4096 |
| China-only, 17 October 2012 | CHN | none | One system-application title (System Settings, v1024) |
| 4.5.0-10 | all six regions | `TWL_FIRM` v6704 | Hotspot data, whitelist, one CHN applet |
| 5.0.0-11 | all six regions | `NATIVE_FIRM` v8758 | A large update: many applets, modules and archives (hotspot v7169, NGWord v5120) |

Title versions in the update lists use the version encoding described in [titles](titles.md#version-numbers) (for example `NATIVE_FIRM` v8758 is system version 5.0.0-11).

**New 3DS:** the Update Data page does not list any New 3DS titles (the New 3DS arrived later than 5.0.0-11); New 3DS titles such as `20000002` appear only on the [title list](title-list.md).

> Unknown: the "Firmware titles" column above is derived from the rows 3dbrew shows for firmware title IDs (`00040138...`) and is not a statement that no firmware changed in updates where none is listed; some updates may reuse the previous firmware.

## Homebrew notes

Update lists matter to downgrade work: a title can only be installed at a version that exists, and early firmware flaws were fixed in specific updates ([system flaws](system-flaws.md)). Game-card system updates carry only system titles ([title list](title-list.md)).

## Open questions

- The page ends at 5.0.0-11; versions from 6.0.0 on are not covered by this source.
- Three consecutive updates (4.3.0-10, 4.4.0-10, 4.5.0-10) share the trailing `-10` in the source's names; the source does not explain this.

## Sources

- [3dbrew: Update Data](https://www.3dbrew.org/wiki/Update_Data) — per-update title version tables, 2.1.0-3 to 5.0.0-11.
- [3dbrew: Title list](https://www.3dbrew.org/wiki/Title_list) — firmware title version lists used to cross-check the firmware versions above.
