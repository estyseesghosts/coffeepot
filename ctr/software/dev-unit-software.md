# CTR development units and their software

Nintendo ships special non-retail 3DS hardware to game developers. These **development units** ("dev units", also "panda" units for original-3DS-based kits) look and act like consoles but run a different system software with debugging tools, more memory options, and extra applications for testing features such as StreetPass, Download Play and the eShop. Retail consoles do not contain this software.

This page covers the *software* on dev units. The hardware kits are described below only briefly.

## Behavior

### Hardware kits

Studios could rent a "CTR-BOX" (a metal box with the 3DS electronics and a dummy handheld case acting as a controller and display), cheaper developer test units, or official flash cards to give prototypes to external testers; the New 3DS generation uses the "SNAKE" kits from Intelligent Systems and Partner (see Copetti). Developer flash cards (model `CTR-005(-01)`) have the same shape as retail cards.

### System updaters

Two **system updaters** exist: the CTR System Updater (CSU) for original-3DS-based units and the SNAKE System Updater for New 3DS-based units. They install the whole system software (all system titles plus Dev Menu and Config) from a RomFS.

### Dev Home Menu

The dev Home Menu adds a debug overlay (X+Y: banner region, build date, revision, system mode, and bars for maximum level, CPU load and GPU load), themes loaded from `SD:/theme/`, and a shortcut (hold X+Y at boot) to clear the icon database and cache.

### Dev Menu

A NAND application (title `00040000-0ff40002`) that shows firmware/revision, free space on the DSi NAND partition and SD, and has tabs: **Program** (run or uninstall applications from NAND, SD or card), **Import**/**SDMC** (install CIA files from the SD card; pressing X also deletes them after installation), **HIO** (install CIAs from the host PC over USB, debugger only) and **ExtData** (delete extra save data). It can also update the firmware from a CIA, allowing downgrades (Start + Y on the `NATIVE_FIRM` CIA). Later revisions open access to more kinds of titles (see table).

| Dev SDK version | Titles accessible from Dev Menu |
| --------------- | ------------------------------- |
| 0.9.2 – 0.13.2 | NAND applications, system applications, TWL titles |
| 0.14.0 – 1.1.0 | Dev NAND applications, system applications, TWL titles |
| 1.2.0 – 2.0.0 | Dev NAND applications, DSiWare |
| 2.1.0 – 2.4.0 | SD applications, Dev NAND applications, DSiWare |
| 3.0.0 – 3.1.0 | + demos |
| 3.2.0 | + web browser |
| 3.3.0 – ? | + TwlNandFiler |
| 11.4 (0.24.67) | + eShop, Miiverse, dummy CFA |

### Config

A text-mode NAND application (`00040000-0ff40102`) with settings that retail units lack: choose between Home Menu and Test Menu; set the **system mode** (memory allowance): `Prod` 64 MB (default; Home Menu normally runs only in this mode), `Dev1` 96 MB (no browser or Miiverse), `Dev2` 80 MB, `Dev3` 72 MB, `Dev4` 32 MB, and on New 3DS `snake Prod` 124 MB, `snake dev1` 178 MB (no browser/Miiverse), `snake dev2` 124 MB; exception-handler and break-stop behavior; debug mode (always disabled on retail); Download Play debug/force-import; filesystem latency emulation (0–2550 ms in 10 ms steps); country/region/user-name settings; touch-screen and circle-pad calibration; screen flicker bias, backlight and brightness; and hardware RTC setting (which can change the clock without a game detecting the change via `GetUserTimeOffset()`).

The memory modes match the retail [APPMEMTYPE table](../hardware/memory.md#fcram-regions-appmemtype).

### CTR-SDK tools and others

Test tools shipped on dev units include: Downloadplay Client, Network Settings, SaveDataFiler, CTR MenuSelector, CecBoxTool and CecLotCheckTool (StreetPass box/lot tools), BossLotCheckTool (SpotPass), Stereo Camera Calibrator, SdCardDebugUtility, FragDisk, Friend Add Tool and Friend Testing Partner, Game Coin Setter, PedometerHistoryChanger, IR Communication Checker, EcDevTool and Dummy Nintendo eShop, FangateFwUpdater, NFC tools (NoftWriter, NfpSdmcTool, NfpManager, NfpUtility), UdsLocalIdChecker (local wireless), and TWL development tools (NINTENDO DS WMtest, TWLNandFiler). Middleware viewers include NintendoWare for CTR (H3D Model Viewer, Layout Viewer, Effect Viewer) and the Mobiclip 3D movie player.

## Technical details

Dev units use distinct signing/certificate sets: development RSA key data exists beside the retail data in Boot9 ([security hardware](../hardware/security-hardware.md)), TMD and ticket certificates have "debug" names (`CP0000000a`, `XS00000009`, `CA00000004`; see [titles](titles.md)). Boot9 chooses retail or dev keys by checking `CFG_UNITINFO`. Dev Menu installs CTR applications to NAND with title variation `0x02`.

## Homebrew notes

Early homebrew used a leaked Nintendo-authored `DevMenu` (and later `BigBlueMenu`, also from the dev kit) as a CIA installer on retail consoles; `FBI` later replaced them with an open-source tool ([history](../homebrew/history.md)). Nintendo ended Unity3DS and many debugging/dev hardware items in January 2021. Sources used here do not discuss hardware debugging interfaces in detail.

## Open questions

- Several tool purposes are listed with "unknown" notes (e.g. "FangateFwUpdater", "Nup").
- Boot ROM and kernel behaviors on dev units that differ from retail are noted only where documented in [boot process](boot-process.md).

## Sources

- [3dbrew: 3DS Development Unit Software](https://www.3dbrew.org/wiki/3DS_Development_Unit_Software) — dev Home Menu, Dev Menu, Config, test tools.
- [3dbrew: Peripherals](https://www.3dbrew.org/wiki/Peripherals) — unit and card model codes.
- [Rodrigo Copetti, Nintendo 3DS Architecture: Games](https://www.copetti.org/writings/consoles/nintendo-3ds/) — development hardware kits and SDK.
- [Hacks Guide Wiki: History of 3DS Hacking](https://wiki.hacks.guide/wiki/3DS:History_of_3DS_Hacking) — end of dev hardware services.
- [3dbrew: Bootloader](https://www.3dbrew.org/wiki/Bootloader) — retail vs dev key selection.
