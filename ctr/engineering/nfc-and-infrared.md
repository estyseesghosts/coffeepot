# NFC and infrared (amiibo, the NFC accessory and the IR transceiver)

Two short-range radios sit outside normal Wi-Fi: **NFC** (the contactless reader that reads amiibo figures and cards) and **infrared** (a transceiver on the console that talks to accessories and, with a program's own protocol, to other infrared devices). Both are reached through services (`nfc:u`/`nfc:m` and `ir:u`) wrapped by libctru's `nfc` and `ir` modules. This page shows how to open and close each correctly, the tag-state machine that NFC follows, and the places where behaviour differs between an Old 3DS and a New 3DS. The infrared part is short because libctru documents the calls but not the protocol.

NFC is **built into a New 3DS**; an **Old 3DS** has no NFC reader of its own and needs the separate NFC Reader/Writer accessory (libctru contains an "Old3DS NFC adapter" path). The infrared transceiver exists on every model. The C-Stick/ZL/ZR service `ir:rst` is a different service and is on [input and sensors](input-and-sensors.md).

Background: [I/O and peripherals](../hardware/io-and-peripherals.md), [CTR services](../software/services.md).

## Before you start

- Checked against libctru 2.7.1 source (`services/nfc.h`, `nfc.c`, `ir.h`, `ir.c`) and the 3ds-examples `nfc` program ([source versions](source-versions.md)). The two examples compile with devkitARM r68 and `-Wall -Wextra`; they have not been run on any console.
- `nfc.h` states the NFC service "can only be used with system-version >= 9.3.0-X" [documented: `nfc.h`]. The NFC examples need an amiibo or NFC tag and, on an Old 3DS, the accessory; none was available for this page.
- Writing to an amiibo changes the figure's own memory. Test writes on a spare tag.

## NFC: the sequence

1. **Is NFC there?** `CFGU_IsNFCSupported(&ok)` (via `cfguInit`) reports support ("fangate" in the header). Whether it returns true for an Old 3DS with the accessory attached is not documented (> Unknown).
2. `nfcInit(NFC_OpType_NFCTag)` opens `nfc:m`, falling back to `nfc:u`, and calls the service's initialise command; **if that returns `0xD0A17480` libctru sleeps 0.5 ms and tries again with no limit** [documented: `nfc.c`], so a stuck NFC module blocks `nfcInit` forever. Run it from a thread you can abandon if that matters. `NFC_OpType_RawNFC` (for raw tag commands) needs system version 10.0.0-X or later.
3. `nfcStartScanning(NFC_STARTSCAN_DEFAULTINPUT)` starts looking for tags.
   - **Old 3DS:** it first starts communication with the NFC adapter accessory and **polls its status every 100 ms while the status is "initialising", with no timeout** [documented: `nfc.c`, `NFC_StartCommsAdapter`]. Any other non-success status ends the wait with the module's error code. What status an Old 3DS reports with no accessory attached is not documented (> Unknown), so assume the call can stall the calling thread and make it from a worker thread in a game.
   - **New 3DS:** the adapter step is skipped (`APT_CheckNew3DS` is true) and scanning starts at once.
4. Poll `nfcGetTagState(&state)`. The states change on their own, without any call from you [documented: `nfc.h`]:

| State | Meaning |
| ----- | ------- |
| `NFC_TagState_Uninitialized` (0) | `nfcInit` not used |
| `NFC_TagState_ScanningStopped` (1) | Not scanning (after `nfcInit` or `nfcStopScanning`) |
| `NFC_TagState_Scanning` (2) | Scanning (after `nfcStartScanning`) |
| `NFC_TagState_InRange` (3) | A tag is in range; set automatically from state 2 |
| `NFC_TagState_OutOfRange` (4) | The tag left range; set automatically from state 3 |
| `NFC_TagState_DataReady` (5) | Amiibo data loaded into memory by `nfcLoadAmiiboData` |

5. In range: `nfcGetTagInfo(&info)` gives the UID (`info.id[0..6]` when `id_offset_size` is 7). For an amiibo, `nfcLoadAmiiboData()` loads and verifies it; then `nfcGetAmiiboSettings` (owner Mii, nickname, setup date; only valid if the figure was set up in the amiibo Settings applet, otherwise it returns `NFC_ERR_AMIIBO_NOTSETUP`) and `nfcGetAmiiboConfig` (write counter, last write date, character and series IDs).
6. A tag that went out of range is not found again until you stop and restart scanning [documented: 3ds-examples `nfc` prompts]. `nfcResetTagScanState()` returns a data-ready state to in-range so you can process the same tag again.
7. `nfcStopScanning()` (on an Old 3DS this also stops adapter communication) and `nfcExit()`, which shuts the module down.

### Application data on an amiibo

An amiibo has a small application-data area, 0xD8 bytes, keyed by a 32-bit **AppID** your game chooses. The calls:

- `nfcOpenAppData(appId)` fails with `NFC_ERR_APPDATA_UNINITIALIZED` if no app data was ever initialised, or `NFC_ERR_APPID_MISMATCH` if a different AppID owns it.
- `nfcInitializeWriteAppData(appId, buf, size)` initialises it and, per the header, writes straight to the tag's storage. It cannot be used again once the `NFC_amiiboFlag_AppDataSetup` flag is set; the user must first run "Delete amiibo Game Data" in the system amiibo Settings applet.
- `nfcReadAppData(buf, size)` (buffer at least 0xD8 bytes; areas the application never wrote are uninitialised) and `nfcWriteAppData(buf, size, &taginfo)` (size at most 0xD8; only after a successful `nfcOpenAppData`).
- `nfcUpdateStoredAmiiboData()` "writes the amiibo data stored in memory to the actual amiibo data storage" and can only be used after `nfcLoadAmiiboData()`. Whether `nfcWriteAppData` already commits to the tag or only changes the in-memory copy until this call is not stated in the header (> Unknown); call it after a write and check the results.

Data-corruption results are `NFC_ERR_DATACORRUPTION0` (`0xC8C1760C`) and `NFC_ERR_DATACORRUPTION1` (`0xC8A17618`), HMAC mismatches. Removing the figure during a write is a plausible way to corrupt it `[hypothesis]`; keep the figure in range until your last write call has returned.

### Example: read the UID

Prints whether CFGU reports NFC, starts scanning, and shows the 7-byte UID when a tag is in range. A restarts scanning; START exits.

```c
#include <3ds.h>
#include <stdio.h>

// Read the 7-byte UID of an NFC tag/amiibo. START exits, A restarts scanning.
int main(void)
{
	gfxInitDefault();
	consoleInit(GFX_TOP, NULL);

	bool supported = false;
	if (R_SUCCEEDED(cfguInit())) {
		if (R_FAILED(CFGU_IsNFCSupported(&supported))) supported = false;
		cfguExit();
	}
	// Informational only: whether an Old 3DS with the NFC accessory reports "yes" is not documented.
	printf("CFGU_IsNFCSupported: %s\n", supported ? "yes" : "no");

	Result rc = nfcInit(NFC_OpType_NFCTag);            // opens nfc:m or nfc:u
	bool ready = R_SUCCEEDED(rc);
	if (!ready) printf("nfcInit failed: %08lX\n", (unsigned long)rc);
	bool scanning = false;

	while (aptMainLoop()) {
		hidScanInput();
		u32 down = hidKeysDown();
		if (down & KEY_START) break;

		if (ready && (!scanning || (down & KEY_A))) {
			if (scanning) nfcStopScanning();
			// Old 3DS: this call talks to the NFC Reader/Writer accessory first and can
			// take a while (nfc.c); a real program would run it off the main thread.
			rc = nfcStartScanning(NFC_STARTSCAN_DEFAULTINPUT);
			scanning = R_SUCCEEDED(rc);
			printf("\x1b[4;1HnfcStartScanning: %08lX      \n", (unsigned long)rc);
		}

		if (scanning) {
			NFC_TagState st;
			if (R_SUCCEEDED(nfcGetTagState(&st)) && st == NFC_TagState_InRange) {
				NFC_TagInfo info;
				if (R_SUCCEEDED(nfcGetTagInfo(&info))) {
					printf("\x1b[6;1HUID:");
					for (int i = 0; i < 7; i++) printf(" %02X", info.id[i]);
					printf("   \n");
				}
			}
		}
		gfxFlushBuffers();
		gfxSwapBuffers();
		gspWaitForVBlank();
	}

	if (scanning) nfcStopScanning();
	if (ready) nfcExit();
	gfxExit();
	return 0;
}
```

Expected: on a New 3DS, scanning starts immediately and the UID appears when a tag is held to the reader. On an Old 3DS without the accessory, `nfcInit` or `nfcStartScanning` prints an error. These are what the source implies; none of it has been run.

## Infrared: the transceiver

`iruInit(sharedmem, size)` opens `ir:u`, runs `IRU_Initialize`, and creates a shared-memory block over **your page-aligned buffer, which must already be mapped**; `iruExit()` runs `IRU_Shutdown` and closes both handles. Reference counted. Then:

- `iruSendData(buf, size, wait)` / `iruRecvData(buf, size, flag, &count, wait)` start a transfer and optionally wait for completion (`IRU_WaitSendTransfer`, `IRU_WaitRecvTransfer`); `wait = true` blocks until it finishes, with no timeout parameter, so use `wait = false` plus the events from `IRU_GetSendFinishedEvent` / `IRU_GetRecvFinishedEvent` and wait on those with a timeout.
- `IRU_SetBitRate(value)` / `IRU_GetBitRate(&value)` and `IRU_SetIRLEDState(value)` / `IRU_GetIRLEDRecvState(&value)` configure the link. The meaning of the numeric values, the `flag` of `iruRecvData` and the shared-memory contents are not documented in libctru (> Unknown: see the 3dbrew IR services page).

```c
#include <3ds.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>

// Open the IR transceiver session, read the current bit rate, close it again.
int main(void)
{
	gfxInitDefault();
	consoleInit(GFX_TOP, NULL);

	u32 *shared = memalign(0x1000, 0x1000);        // page aligned, already mapped (ir.h)
	Result rc = shared ? iruInit(shared, 0x1000) : -1;
	if (R_SUCCEEDED(rc)) {
		u8 bitRate = 0;
		rc = IRU_GetBitRate(&bitRate);
		printf("IRU_GetBitRate: rc=%08lX value=%u\n", (unsigned long)rc, bitRate);
		iruExit();                                 // IRU_Shutdown + close both handles
	} else {
		printf("iruInit: %08lX\n", (unsigned long)rc);
	}
	free(shared);                                  // only after iruExit

	printf("START exits.\n");
	while (aptMainLoop()) {
		hidScanInput();
		if (hidKeysDown() & KEY_START) break;
		gfxFlushBuffers();
		gfxSwapBuffers();
		gspWaitForVBlank();
	}
	gfxExit();
	return 0;
}
```

This only opens the session, reads the bit rate and closes it. It checks the lifecycle and the ownership rule (free the buffer after `iruExit`); real infrared use needs a partner device and protocol that this wiki has not tested.

**New 3DS:** the head-tracking infrared LED is a different thing, controlled by QTM ([New 3DS: QTM and MVD](new-3ds-qtm-and-mvd.md)), not by `ir:u`.

## Technical details

| Item | Value | Source |
| ---- | ----- | ------ |
| NFC services | `nfc:m`, then `nfc:u` | `nfc.c` |
| NFC init retry | on result `0xD0A17480`: `svcSleepThread(500000)` (0.5 ms), repeat, unbounded | `nfc.c` |
| Old 3DS adapter step | `NFC_StartCommunication`, then `NFC_CommunicationGetStatus` every 100 ms (`1` = initialising, `2` = done); other status reads `NFC_CommunicationGetResult` and returns that error | `nfc.c` |
| App data size | 0xD8 bytes (`NFC_AmiiboConfig.appdata_size`) | `nfc.h` |
| Error codes | `NFC_ERR_INVALID_STATE 0xC8A17600`, `APPDATA_UNINITIALIZED 0xC8A17620`, `AMIIBO_NOTSETUP 0xC8A17628`, `APPID_MISMATCH 0xC8A17638`, `DATACORRUPTION0 0xC8C1760C`, `DATACORRUPTION1 0xC8A17618` | `nfc.h` |
| `NFC_TagInfo` | `id_offset_size` (7 when the field is the UID size), `id[0x28]` | `nfc.h` |
| IR service | `ir:u` | `ir.c` |
| IR shared memory | `svcCreateMemoryBlock(addr, size, 1, 3)` over the caller's buffer | `ir.c` |

## Pitfalls

- **Calling `nfcStartScanning` on the main thread of a game on an Old 3DS.** Symptom: the frame loop stalls for as long as the adapter handshake takes. Fix: call it from a worker thread ([threading](threading.md)).
- **Expecting a tag to be re-detected after it left range.** Symptom: no further `InRange`. Fix: `nfcStopScanning` then `nfcStartScanning` again.
- **Using the amiibo app-data calls out of order.** Symptom: `NFC_ERR_INVALID_STATE`. Fix: load the data (state `DataReady`) first; open app data before read or write; initialise before the first open.
- **Freeing the IR shared buffer before `iruExit`.** Symptom: the service writes into freed memory. Fix: free after `iruExit`.
- **A busy-wait on `nfcGetTagState` at full speed.** Wasteful; poll once per frame.

## Open questions

- Whether `CFGU_IsNFCSupported` is true with the Old 3DS accessory, and exactly which accessory states produce which error from `nfcStartScanning` (experiment: Old 3DS with and without the accessory).
- Reader latency from tag in range to `InRange`, and the behaviour across sleep and HOME ([application lifecycle](application-lifecycle.md)).
- Meaning of the IR bit-rate, LED-state and receive-flag values; whether `ir:u` and `ir:rst` (used by libctru's HID on a New 3DS) can be open at once.
- Whether `nfcInit` can really hang forever in practice, and under which conditions.

## Verification status

| Item | Status |
| ---- | ------ |
| Both examples | Build with devkitARM r68-1 (GCC 16.1.0), libctru 2.7.1-1, `-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft -O2 -Wall -Wextra`, no warnings; linked with `3dsx.specs` |
| Old 3DS | not run |
| New 3DS | not run |
| Emulator | not run |

## Sources

- [libctru `nfc.h`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/include/3ds/services/nfc.h) and [`nfc.c`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/source/services/nfc.c) — states, errors, init loop, Old 3DS adapter path.
- [libctru `ir.h`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/include/3ds/services/ir.h) and [`ir.c`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/source/services/ir.c) — IR session and shared memory.
- [3ds-examples `nfc/`](https://github.com/devkitPro/3ds-examples/tree/be2001fee08cf8ec7d3366e095e83f6f75da8942/nfc) — scan, load, read/write app data flow.
- [3dbrew: NFC Services](https://www.3dbrew.org/wiki/NFC_Services), [Amiibo](https://www.3dbrew.org/wiki/Amiibo) and [IR Services](https://www.3dbrew.org/wiki/IR_Services) — referenced by the headers for the protocol details; not re-read for this page.
