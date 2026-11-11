# CTR development units and their software

Nintendo ships special non-retail 3DS hardware to game developers. These **development units** ("dev units", also "panda" units for original-3DS-based kits) look and act like consoles but run a different system software with debugging tools, more memory options, and extra applications for testing features such as StreetPass, Download Play and the eShop. Retail consoles do not contain this software.

This page covers the *software* on dev units. The hardware kits are described below only briefly.

## Behavior

### Hardware kits

Studios could rent a "CTR-BOX" (a metal box with the 3DS electronics and a dummy handheld case acting as a controller and display), cheaper developer test units, or official flash cards to give prototypes to external testers; the New 3DS generation uses the "SNAKE" kits from Intelligent Systems and Partner (see Copetti). Developer flash cards (model `CTR-005(-01)`) have the same shape as retail cards.

### System updaters

Two **system updaters** exist: the CTR System Updater (CSU) for Old 3DS-based units; **New 3DS:** the SNAKE System Updater is used for New 3DS-based units. They install the whole system software (all system titles plus Dev Menu and Config) from a RomFS.

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

A text-mode NAND application (`00040000-0ff40102`) with settings that retail units lack: choose between Home Menu and Test Menu; set the **system mode** (memory allowance): `Prod` 64 MB (default; Home Menu normally runs only in this mode), `Dev1` 96 MB (no browser or Miiverse), `Dev2` 80 MB, `Dev3` 72 MB, `Dev4` 32 MB (**New 3DS:** `snake Prod` 124 MB, `snake dev1` 178 MB (no browser/Miiverse), `snake dev2` 124 MB); exception-handler and break-stop behavior; debug mode (always disabled on retail); Download Play debug/force-import; filesystem latency emulation (0–2550 ms in 10 ms steps); country/region/user-name settings; touch-screen and circle-pad calibration; screen flicker bias, backlight and brightness; and hardware RTC setting (which can change the clock without a game detecting the change via `GetUserTimeOffset()`).

Which units may change the mode differs: all CTR debuggers and prototype units can, while on Developer Test Units the ability depends on the unit (most retail-looking units cannot leave `Prod`). Outside `Prod`, memory-hungry software such as the web browser cannot be launched from the Home Menu. Config also has an EULA "Agree Version" setting (two bytes, `00.00` means not accepted; commonly set to `ff.ff` to skip the agreement) [documented: 3dbrew Dev Unit Software].

The memory modes match the retail [APPMEMTYPE table](../hardware/memory.md#fcram-regions-appmemtype).

### CTR-SDK tools and others

Descriptions derive from SDK 2.3.4 unless noted. Title IDs are written as 3dbrew writes them (high-low).

| Tool | Title ID | What it does |
| ---- | -------- | ------------ |
| Downloadplay Client | `00040000-0ff40402` | Text-mode replacement for Download Play to test 3DS-mode child apps (no DS Download Play). A dev unit cannot receive software from a retail unit or the reverse (different keys). |
| Network Settings | `00040000-0ff40702` | Edit the three connection slots: security mode (open, WEP, WPA/WPA2 TKIP/AES), SSID (32 chars), key in plain text, auto-DNS; "Write Setting & Connect Test" overrides other settings such as proxy. Needs a reboot to exit. |
| SaveDataFiler | `00040000-0ff40a02` | Export/import saves and extdata to SD under `sdmc:/filer/UserSaveData/`; can corrupt saves on purpose; L+R+Start deletes all shared extdata. TWL apps ignored; GBA apps shown as broken. |
| CTR MenuSelector | `00040010-0ff40b02` | Switched Home Menu / Test Menu; replaced by Config's "Menu" in SDK 1.1.0. |
| CecBoxTool | `00040000-0ff41602` | StreetPass backup/restore, delete boxes, count inbox/outbox, reset MAC filter ([StreetPass](streetpass-cec.md)). |
| CecLotCheckTool | `00040000-0f800902` | Manage up to twelve StreetPass boxes, create dummy boxes, reset the 8-hour per-source rate limit (it checks the console is not retail first). |
| BossLotCheckTool | `00040000-0ff40e00` | Inspect SpotPass tasks per title (status, count, interval, URL, result, HTTP status, size, last modified) and run a task; shows the friend code. |
| Stereo Camera Calibrator | `00040000-0ff40600` | Adjust the outer cameras relative to each other, automatically with a chart or manually. |
| SdCardDebugUtility | `00040000-07fffa00` | Fill free SD space, create sample pictures and empty voice memos, launch pickers, create dummy SpotPass tasks. |
| FragDisk | `00040000-0ff41800` | Fragment SD free space by creating many small files and deleting every second one. |
| Friend Add Tool | `00040000-0fe04000` | Delete all friends or fill the list with dummy codes or 1-100 codes from `friend_code.txt`. |
| Friend Testing Partner | `00040000-0fe00c00` | Replace presence data (playing title, message, Mii, favourite title) with dummy values refreshed about every ten seconds. |
| Game Coin Setter | `00040000-0ff40d00` | Set Play Coins (0-300); L+R+X stops today's walking increment. |
| PedometerHistoryChanger | `00040000-0ff30300` | Export/import pedometer data as `PedometerExport.csv` / `PedometerImport.csv` (year, month, day, hour, steps). |
| IR Communication Checker | `00040000-0f804900` | Counts data on the infrared port; cannot be closed except by power cycle. |
| EcDevTool | `00040000-0ff41b00` | Add balance (not on production servers), delete title rights, delete eShop account, change country, tax address (USA/Canada). |
| Dummy Nintendo eShop | (shares the retail eShop's IDs) | Tests `JumpToEshop()`; region specific. |
| FangateFwUpdater | `00040000-0ff41f00` | Update or destroy the external NFC reader firmware; CTR devices only, not SNAKE. |
| NoftWriter / NfpSdmcTool / NfpManager / NfpUtility | `00040000-0f807f00` / `0ff42100` / `0ff41e00` / `0ff42000` | amiibo image flashing, tag data editing, connection and data management, bulk tag viewing. |
| UdsLocalIdChecker | `00040000-0ff3ff00` | Scan for local (UDS) networks. |
| NINTENDO DS - WMtest, TWLNandFiler | | DS-mode Wi-Fi scan, measure and noise tools; TWL NAND save browser with hex viewer. |
| NintendoWare for CTR (H3D Model Viewer, Layout Viewer, Effect Viewer) | | Need Host IO; full function only on a CTR-PARTNER debugger. |
| Mobiclip 3D Movie Player | `00040000-00036A00` | Plays `.moflex` from the SD root. |

### More Config settings

Beyond those above, Config offers: Language (with an "invalid" option); Fangate failure emulation and "Nup" (online update check; both unknown purpose); name and NG-name flag; country ID (also undefined 255) and region ID; Hid setting (touch and circle pad calibration); flicker top/bottom (screen bias voltage); ABL enable (power-save backlight; also on 2DS); luminance level (older versions allowed above 5 and crashed Home Menu quick settings); backlight dimming (**New 3DS:** camera-controlled); sound mode (mono/stereo/surround); Coppacs setting (parental consent for the Americas); OLV access key; test setting; stereo camera and gyro/accelerometer "for check" calibrations that appear to write incorrect data; "Ctr Info Setting" reading `ctrsetting.txt` from SD; and reset settings (re-runs initial setup without deleting software). If anything other than `Prod` or `Dev2` is chosen as system mode, the menu setting automatically switches to the Test Menu. Dev Home Menu themes need dev themes; retail themes do not work. In Dev Menu, L+R+Left+B+Start (after confirmation) bulk-uninstalls all SDK tools and then DevMenu itself crashes.

## Technical details

Dev units use distinct signing/certificate sets: development RSA key data exists beside the retail data in Boot9 ([security hardware](../hardware/security-hardware.md)), TMD and ticket certificates have "debug" names (`CP0000000a`, `XS00000009`, `CA00000004`; see [titles](titles.md)). Boot9 chooses retail or dev keys by checking `CFG_UNITINFO`: for example it sets AES key slot `0x3F` from a pointer at `0xFFFFD6E0` on retail and `0xFFFFD700` on a dev unit, and non-NAND FIRM boot uses separate retail/dev RSA public keys and AES-CBC normal keys [documented: 3dbrew Bootloader]. Dev Menu installs CTR applications to NAND with title variation `0x02`.

## Homebrew notes

Early homebrew used a leaked Nintendo-authored `DevMenu` (and later `BigBlueMenu`, also from the dev kit) as a CIA installer on retail consoles; `FBI` later replaced them with an open-source tool ([history](../homebrew/history.md)). Nintendo ended Unity3DS and many debugging/dev hardware items in January 2021. Sources used here do not discuss hardware debugging interfaces in detail.

## Open questions

- Several tool purposes are listed with "unknown" notes (e.g. "FangateFwUpdater", "Nup").
- Boot ROM and kernel behaviors on dev units that differ from retail are noted only where documented in [boot process](boot-process.md).

## Sources

- [3dbrew: 3DS Development Unit Software](https://www.3dbrew.org/wiki/3DS_Development_Unit_Software) — dev Home Menu, Dev Menu, Config, test tools.
- [3dbrew: Nintendo Software](https://www.3dbrew.org/wiki/Nintendo_Software) — dev units listed as not publicly available.
- [3dbrew: Peripherals](https://www.3dbrew.org/wiki/Peripherals) — unit and card model codes.
- [Rodrigo Copetti, Nintendo 3DS Architecture: Games](https://www.copetti.org/writings/consoles/nintendo-3ds/) — development hardware kits and SDK.
- [Hacks Guide Wiki: History of 3DS Hacking](https://wiki.hacks.guide/wiki/3DS:History_of_3DS_Hacking) — end of dev hardware services.
- [3dbrew: Bootloader](https://www.3dbrew.org/wiki/Bootloader) — retail vs dev key selection.
