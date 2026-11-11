# System information, battery and the MCU (CFG, PTM, MCU, os.h)

A program often needs to know what it is running on (which model, which language, which system version), how much memory it got, and whether the battery is low or the lid is closed. libctru answers these through a handful of small services: **CFG** (`cfg:u`, `cfg:s`, `cfg:i`: the console's configuration), **PTM** (`ptm:u`: battery, lid, pedometer, play history), **APT** (the applet service, which can tell you whether you are on a New 3DS), and the **MCU** (the console's power and peripheral microcontroller, reached through the `mcu::*` services). It also reads facts the kernel publishes in shared pages (`os.h`) without any service call. This page shows which call to use for each question, the init and cleanup rules, and which parts are not meant for normal applications.

Everything here exists on every model, except where **New 3DS:** says otherwise.

Background: [CTR services](../software/services.md), [models](../hardware/models.md), [I/O and peripherals](../hardware/io-and-peripherals.md). Writing a client by hand: [IPC and services](ipc-and-services.md) (its worked example is `CFGU_GetSystemModel`).

## Before you start

- Checked against libctru 2.7.1 source (`cfgu.h/.c`, `ptmu.h/.c`, `ptmsysm.h`, `ptmplays.h`, `mcu*.h`, `os.h`, `os-versionbin.c`) ([source versions](source-versions.md)). The example compiles with devkitARM r68, `-Wall -Wextra`; it has not been run on any console.
- Which services a given launcher lets a program open depends on its service-access list ([IPC and services](ipc-and-services.md#getting-a-session-srv-getservicehandle)). The `cfg:u` and `ptm:u` calls below are the ones intended for applications; `ptm:sysm`, `ptm:sets`, `mcu::*` and the `cfgi` calls are system-level, and whether a `.3dsx` can open them was not tested here.

## Which call answers which question

| Question | Call | Needs a service | Notes |
| -------- | ---- | --------------- | ----- |
| Which console model? | `CFGU_GetSystemModel(&m)` -> `CFG_MODEL_3DS` (Old 3DS), `3DSXL` (Old 3DS XL), `N3DS` (New 3DS), `2DS` (Old 2DS), `N3DSXL` (New 3DS XL), `N2DSXL` (New 2DS XL) | `cfg:*` | Distinguishes all six |
| Is it a New 3DS (any size)? | `APT_CheckNew3DS(&b)` | `APT` (libctru starts it) | Simplest test; use it to gate New 3DS features |
| Language / region | `CFGU_GetSystemLanguage`, `CFGU_SecureInfoGetRegion` (`CFG_Language`, `CFG_Region` enums) | `cfg:*` | `CFG_LANGUAGE_DEFAULT` (-1) means "system language" in `errorInit` |
| Country | `CFGU_GetCountryCodeString`, `CFGU_GetCountryCodeID` | `cfg:*` | |
| Does it have NFC? | `CFGU_IsNFCSupported` | `cfg:*` | See [NFC and infrared](nfc-and-infrared.md) |
| A per-console stable id | `CFGU_GenHashConsoleUnique(appSalt, &hash)` | `cfg:*` | Hash of console-unique data with your salt; it identifies a console, so treat it as personal data |
| System version string | `osGetSystemVersionDataString` | file read of the NVer/CVer data | Same format as System Settings; the result can be a positive errno |
| Kernel / FIRM version | `osGetKernelVersion()`, `osGetFirmVersion()` compared with `SYSTEM_VERSION(a,b,c)` | none (shared page) | |
| Memory layout and free memory | `osGetApplicationMemType()` (0-5 Old 3DS, 6-8 New 3DS), `osGetMemRegionSize/Used/Free(MEMREGION_*)` | none (shared page / `svcGetSystemInfo`) | See [memory and cache](memory-and-cache.md#how-much-memory-you-get) |
| Battery level, charging, adapter, lid | `PTMU_GetBatteryLevel` (0-5), `PTMU_GetBatteryChargeState`, `PTMU_GetAdapterState`, `PTMU_GetShellState` (0 closed, 1 open) | `ptm:u` | Poll at a low rate (seconds), not every frame |
| Step count / history | `PTMU_GetTotalStepCount`, `PTMU_GetStepHistory(hours, buf)`, `PTMU_GetPedometerState` | `ptm:u` | |
| Play history (what was launched when) | `PTMPLAYS_GetPlayHistory(...)` | `ptm:plays` | A ring buffer of up to 0x11D28 events; a system-level view |
| Wi-Fi bars, 3D slider, headset | `osGetWifiStrength()` (0-3), `osGet3DSliderState()`, `osIsHeadsetConnected()` | none (shared page) | |
| Time | `osGetTime()` (milliseconds), `time()` | none | The wall clock is set by the user and can jump |

`CFGU_GetModelNintendo2DS` exists but its header documents the result as "0 = yes, 1 = no", the opposite of what a name suggests; prefer `CFGU_GetSystemModel`, which tells all models apart [documented: `cfgu.h`].

## Init and cleanup

Every service module follows the same pattern: `xxxInit()` opens the session (reference counted, so nested init is fine), `xxxExit()` closes it when the count reaches zero [documented: `cfgu.c`, `ptmu.c`, `mcuhwc.c`].

- `cfguInit()` tries `cfg:i`, then `cfg:s`, then `cfg:u` and keeps the first that opens, because `cfg:i` has the most commands and `cfg:u` the fewest [documented: `cfgu.c`]. A program that calls an `cfg:i`-only command gets an error from the session it actually has. Call `cfguExit()` when done.
- `ptmuInit()` opens `ptm:u`; `ptmuExit()` closes it. If the open fails the count is rolled back.
- Do not call a getter without a successful init; the session handle is 0 and the call fails with a kernel error.
- `HID`, `APT`, `FS` and `GSP` are initialised for you before `main` ([application lifecycle](application-lifecycle.md#technical-details)); the others on this page are not.

## Complete example

Prints model, language, region, New 3DS status, memory regions, system version and live battery status until START.

```c
#include <3ds.h>
#include <stdio.h>

static const char *model_name(u8 m)
{
	switch (m) {
	case CFG_MODEL_3DS:    return "Old 3DS";
	case CFG_MODEL_3DSXL:  return "Old 3DS XL";
	case CFG_MODEL_N3DS:   return "New 3DS";
	case CFG_MODEL_2DS:    return "Old 2DS";
	case CFG_MODEL_N3DSXL: return "New 3DS XL";
	case CFG_MODEL_N2DSXL: return "New 2DS XL";
	default:               return "unknown";
	}
}

int main(void)
{
	gfxInitDefault();
	consoleInit(GFX_TOP, NULL);

	u8 model = 0xFF, lang = 0xFF, region = 0xFF;
	if (R_SUCCEEDED(cfguInit())) {              // tries cfg:i, then cfg:s, then cfg:u
		if (R_FAILED(CFGU_GetSystemModel(&model))) model = 0xFF;
		if (R_FAILED(CFGU_GetSystemLanguage(&lang))) lang = 0xFF;
		if (R_FAILED(CFGU_SecureInfoGetRegion(&region))) region = 0xFF;
		cfguExit();
	}
	printf("model %s, language %u, region %u\n", model_name(model), lang, region);

	bool isNew = false;
	if (R_FAILED(APT_CheckNew3DS(&isNew))) isNew = false;
	printf("APT_CheckNew3DS: %s\n", isNew ? "New 3DS" : "Old 3DS");

	// Memory the kernel gave this process (see memory-and-cache.md). Mem type 0-5: Old 3DS, 6-8: New 3DS.
	printf("mem type %lu, application region %lu MiB, free %lu MiB\n",
	       (unsigned long)osGetApplicationMemType(),
	       (unsigned long)(osGetMemRegionSize(MEMREGION_APPLICATION) >> 20),
	       (unsigned long)(osGetMemRegionFree(MEMREGION_APPLICATION) >> 20));

	char verStr[32] = "?";
	if (R_FAILED(osGetSystemVersionDataString(NULL, NULL, verStr, sizeof verStr))) verStr[0] = '?', verStr[1] = 0;
	printf("system version %s\n", verStr);

	bool havePtm = R_SUCCEEDED(ptmuInit());

	while (aptMainLoop()) {
		hidScanInput();
		if (hidKeysDown() & KEY_START) break;

		if (havePtm) {
			u8 battery = 0, charging = 0, shell = 0;
			bool adapter = false;
			PTMU_GetBatteryLevel(&battery);          // 0-5
			PTMU_GetBatteryChargeState(&charging);   // 0 = no, 1 = yes
			PTMU_GetShellState(&shell);              // 0 = closed, 1 = open
			PTMU_GetAdapterState(&adapter);
			printf("\x1b[5;1Hbattery %u/5 charging %u adapter %d shell %u   \n",
			       battery, charging, adapter, shell);
		}
		gfxFlushBuffers();
		gfxSwapBuffers();
		gspWaitForVBlank();
	}
	if (havePtm) ptmuExit();
	gfxExit();
	return 0;
}
```

Expected on an Old 3DS: `model Old 3DS` (or `Old 3DS XL`, `Old 2DS`), `APT_CheckNew3DS: Old 3DS`, `mem type` between 0 and 5, and a battery line that follows the real battery and lid. On a New 3DS the model is `New 3DS`/`New 3DS XL`/`New 2DS XL`, `APT_CheckNew3DS: New 3DS` and mem type is 6-8. These follow from the headers; they were not observed.

## System-level services (reference only)

Present in libctru, not for normal applications; whether a given launcher exposes them to homebrew was not tested.

| Group | Service | Init | What it does |
| ----- | ------- | ---- | ------------ |
| PTM system | `ptm:sysm` | `ptmSysmInit` | Request/deny sleep, wake events and reason, step and play history clear/set, set user or RTC time, `PTMSYSM_ConfigureNew3DSCPU` (bit 0 enables the higher clock, bit 1 the L2 cache), shutdown/reboot with timeout |
| PTM time | `ptm:sets`, `ptm:gets` | `ptmSetsInit`, `ptmGetsInit` | Set or read the system time (milliseconds since 2000-01-01) |
| MCU hardware control | `mcu::HWC` | `mcuHwcInit` | Battery voltage and level, PCB temperature, volume slider level, power/Wi-Fi/camera/3D LEDs and the notification LED pattern, top/bottom LCD flicker, RTC time, MCU firmware version, raw register read/write |
| MCU others | `mcu::CAM` | `mcuCamInit` | Camera LED on/off |
| | `mcu::GPU` | `mcuGpuInit` | LCD power and backlight on/off, LCD flicker, 3D LED, MCU firmware version, interrupt event |
| | `mcu::HID` | `mcuHidInit` | MCU-side accelerometer enable, scale, filter, manual register access and data read; 3D slider position; volume slider level; interrupt event |
| | `mcu::NWM` | `mcuNwmInit` | Wi-Fi LED, Wi-Fi mode/enable, "wireless disabled" flag |
| | `mcu::PLS` | `mcuPlsInit` | Read-only RTC fields and a tick counter |
| | `mcu::SND` / `mcu::CDC` | `mcuSndInit` / `mcuCdcInit` | Volume slider level and one codec register each |
| | `mcu::RTC` | `mcuRtcInit` | The large one: set/read RTC and alarm, pedometer, battery/charger/lid state, LEDs, **power off, reboot, reset**, force-shutdown delay, interrupt mask, a storage area, and various system flags |
| Config internals | `cfg:s`/`cfg:i` through `cfguInit` | `cfguInit` | `CFG_GetConfigInfoBlk4/8`, `CFG_SetConfigInfoBlk4/8`, `CFG_UpdateConfigSavegame`, `CFGI_*` (restore or delete config data, format config, clear parental controls) |

The `CFGI_*` and `CFG_Set*` calls change or delete system data. Do not call them from an application.

**New 3DS:** the `PTMSYSM_ConfigureNew3DSCPU` call is the system-level way to select the 804 MHz clock and L2 cache; an application uses `osSetSpeedupEnable(true)` instead ([performance](performance.md#new-3ds-turn-on-the-fast-mode-yourself)).

## Technical details

| Item | Value | Source |
| ---- | ----- | ------ |
| CFG sessions tried | `cfg:i`, `cfg:s`, `cfg:u` in that order | `cfgu.c` |
| `CFG_SystemModel` values | 0 `3DS` (CTR), 1 `3DSXL` (SPR), 2 `N3DS` (KTR), 3 `2DS` (FTR), 4 `N3DSXL` (RED), 5 `N2DSXL` (JAN) | `cfgu.h` |
| `CFG_Language` | -1 default; 0 JP, 1 EN, 2 FR, 3 DE, 4 IT, 5 ES, 6 ZH (simplified), 7 KO, 8 NL, 9 PT, 10 RU, 11 TW (traditional) | `cfgu.h` |
| `CFG_Region` | 0 JPN, 1 USA, 2 EUR, 3 AUS, 4 CHN, 5 KOR, 6 TWN | `cfgu.h` |
| `ptm:u` outputs | shell 0/1; battery 0-5; charge 0/1; pedometer 0/1 | `ptmu.h` |
| Play history capacity | `PTM_MAX_PLAY_EVENTS` = 0x11D28; each event has a title ID, a type (0-12) and minutes since 2000-01-01 | `ptmplays.h` |
| Memory type | `osGetApplicationMemType()`: 0-5 Old 3DS, 6-8 New 3DS | `os.h` |
| Clock constants | `SYSCLOCK_ARM11` = 268 111 856 Hz; **New 3DS:** `SYSCLOCK_ARM11_NEW` = 3 x that = 804 335 568 Hz (see [CPU](../hardware/cpu.md)) | `os.h` |

## Pitfalls

- **Treating `APT_CheckNew3DS` failure as New 3DS.** Treat failure as Old 3DS; the baseline path always works.
- **Polling the battery every frame.** It is an IPC round trip per call ([IPC and services](ipc-and-services.md)); once per second is plenty.
- **Using `CFGU_GetModelNintendo2DS` as a boolean without reading its inverted doc.** Use the model enum.
- **Calling `cfguExit()` too early.** It is reference counted, but a `cfguInit()` you did not pair leaves the session open.
- **Treating `CFGU_GenHashConsoleUnique` as anonymous.** It is stable per console and salt; do not upload it casually.
- **Shipping a program that calls `CFGI_*` or `PTMSYSM_*`.** They are for system modules; failing is the good outcome.

## Open questions

- Which of `ptm:sysm`, `ptm:sets`, `mcu::*` a `.3dsx` can open under the default Luma3DS service list, on Old 3DS and New 3DS.
- The accuracy and update rate of `PTMU_GetBatteryLevel` versus `MCUHWC_GetBatteryLevel`.
- Whether `PTMU_GetShellState` reports promptly during the sleep transition.

## Verification status

| Item | Status |
| ---- | ------ |
| Example | Builds with devkitARM r68-1 (GCC 16.1.0), libctru 2.7.1-1, `-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft -O2 -Wall -Wextra`, no warnings; linked with `3dsx.specs` |
| Old 3DS | not run |
| New 3DS | not run |
| Emulator | not run |

## Sources

- [libctru `cfgu.h`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/include/3ds/services/cfgu.h) and [`cfgu.c`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/source/services/cfgu.c) — enums, session fallback.
- [libctru `ptmu.h`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/include/3ds/services/ptmu.h), [`ptmsysm.h`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/include/3ds/services/ptmsysm.h), [`ptmplays.h`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/include/3ds/services/ptmplays.h) — PTM calls and history events.
- [libctru `mcuhwc.h`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/include/3ds/services/mcuhwc.h) and the other `mcu*.h` headers — MCU service list.
- [libctru `os.h`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/include/3ds/os.h) and [`os-versionbin.c`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/source/os-versionbin.c) — shared-page accessors, system version.
