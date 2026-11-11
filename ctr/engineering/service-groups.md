# libctru service groups: what each one is, how to open and close it

libctru ships one header and one source file per system service it knows. This page is the index for all of them from a developer's point of view: what each group is for, the call that opens it, the call that closes it, which services it tries, who owns what you get back, and whether it exists on an Old 3DS or only on a New 3DS. Groups with their own engineering page are only linked; the rest are summarised here. It also contains a small probe program that tells you which services your own launcher lets you open.

If a service is not in your program's access list the open fails, and nothing in libctru can change that ([IPC and services](ipc-and-services.md#getting-a-session-srv-getservicehandle)). Nothing here was run on a console.

## Before you start

- Checked against libctru 2.7.1 headers and sources (`include/3ds/services/*.h`, `include/3ds/applets/*.h`, `source/services/*.c`) ([source versions](source-versions.md)). Model availability comes from the header text; where a header says nothing, the table says "not stated" and this page does not claim the service works on every model.
- Names of services (`cfg:u`, `ptm:u`, ...) are at most 8 characters; the open call is `srvGetServiceHandle` ([IPC and services](ipc-and-services.md)).

## The common pattern

Almost every group follows one pattern, visible in the sources [documented: e.g. `ac.c`, `cfgu.c`, `mcuhwc.c`, `ptmu.c`]:

```c
Result xxxInit(void) {
    if (AtomicPostIncrement(&refCount)) return 0;      // already open: just count
    Result rc = srvGetServiceHandle(&handle, "name");  // maybe with fallbacks
    if (R_FAILED(rc)) AtomicDecrement(&refCount);      // roll back on failure
    return rc;
}
void xxxExit(void) {
    if (AtomicDecrement(&refCount)) return;            // still used elsewhere
    svcCloseHandle(handle);
}
```

So: pair every successful `xxxInit` with one `xxxExit`; a failed init must not be paired; nested init is safe; the handle belongs to libctru, so never close it yourself. Groups that own buffers or shared memory (`mic`, `ir`, `httpc`, `soc`, `uds`, `mvd`, `nim`) also free or unmap it in `xxxExit`, so the buffer must outlive the session ([microphone](microphone.md), [NFC and infrared](nfc-and-infrared.md), [networking](networking.md), [local wireless](local-wireless.md)). Several groups try services in order of capability and keep the first that opens: `ac:i` then `ac:u`; `cfg:i`, `cfg:s`, `cfg:u`; `am:app` / `am:sys` / `am:net` / `am:u` (by function); `nfc:m` then `nfc:u`; `hid:USER` then `hid:SPVR`; `act:a` or `act:u` and `frd:a` or `frd:u` chosen by a `forceUser` argument.

## Groups with their own page

| Group | Service(s) | Open / close | Page |
| ----- | ---------- | ------------ | ---- |
| FS, archives, save data, RomFS | `fs:USER` | `fsInit` / `fsExit` (started by default) | [files and saves](filesystem-and-saves.md) |
| APT, applets, HOME | APT services | `aptInit` / `aptExit` (started by default) | [application lifecycle](application-lifecycle.md) |
| GSP (GPU) | `gsp::Gpu` | `gspInit` / `gspExit` (started by `gfxInit`) | [GPU rendering](gpu-rendering.md), [frame pacing](frame-pacing.md) |
| DSP, ndsp, CSND | `dsp::DSP`, `csnd:SND` | `dspInit`, `ndspInit`, `csndInit` | [audio](audio.md) |
| Sockets | `soc:U` | `socInit(buf, size)` / `socExit` | [networking](networking.md) |
| HTTP, TLS | `http:C`, `ssl:C` | `httpcInit(size)`, `sslcInit` | [networking](networking.md) |
| HID, IRRST | `hid:USER`, `ir:rst` | `hidInit` (default), `irrstInit` | [input and sensors](input-and-sensors.md) |
| Cameras, Y2R | `cam:u`, `y2r:u` | `camInit`, `y2rInit` | [cameras and Y2R](camera-and-y2r.md) |
| Microphone | `mic:u` | `micInit(buf, size)` | [microphone](microphone.md) |
| NFC, IR transceiver | `nfc:m`/`nfc:u`, `ir:u` | `nfcInit(type)`, `iruInit(buf, size)` | [NFC and infrared](nfc-and-infrared.md) |
| CFG, PTM, MCU, os.h | `cfg:*`, `ptm:*`, `mcu::*` | `cfguInit`, `ptmuInit`, `mcuHwcInit` ... | [system information](system-info-and-power.md) |
| Local wireless | `nwm::UDS` (+`ndm:u`) | `udsInit(size, name)` | [local wireless](local-wireless.md) |
| QTM, MVD | `qtm:*`, `mvd:STD` | `qtmInit`, `mvdstdInit` | [New 3DS extras](new-3ds-qtm-and-mvd.md) |
| Debugging channels | `3dslink`, GDB HIO, `errf` | `link3dsStdio`, `gdbHioDevInit`, `errfInit` | [debugging](debugging.md) |

## Groups summarised here

| Group | Header | Service | Open / close | Purpose and cautions | Model |
| ----- | ------ | ------- | ------------ | -------------------- | ----- |
| Wi-Fi connection (AC) | `ac.h` | `ac:i`, else `ac:u` | `acInit` / `acExit` | Connection status (`ACU_GetStatus`: 1 not connected, 3 connected), security mode, SSID, proxy settings. `acWaitInternetConnection` spins on `ACU_GetWifiStatus` with no sleep and no timeout (see [networking](networking.md#is-wi-fi-connected)) | not stated (all) |
| Nintendo Network accounts (ACT) | `act.h` | `act:a` or `act:u` | `actInit(forceUser)` / `actExit` | Account info, Mii images, tokens. Needs a shared-memory handle for `ACT_Initialize`; system-oriented | not stated |
| Friends (FRD, FRDN) | `frd.h`, `frdn.h` | `frd:a` or `frd:u`; `frd:n` | `frdInit(forceUser)`, `frdnInit` | Login state, own profile and presence, friend list. Privacy-sensitive; do not log friend keys | not stated |
| Application manager (AM) | `am.h`, `ampxi.h` | `am:app`, `am:sys`, `am:net`, `am:u`; `pxi:am9` | `amInit`, `amAppInit`, `amExit`; `ampxiInit(handle)` | Title list, install and delete, ticket and TMD queries, title database. Installing needs elevated service access; `ampxi.h` says its service is "normally not accessible to userland apps" | not stated |
| SpotPass (BOSS) | `boss.h` | `boss:U` or `boss:P` | `bossInit(programId, forceUser)` / `bossExit` | Register and run background download tasks, read downloaded data. Needs a storage (extdata) set-up with `bossSetStorageInfo` | not stated |
| News (NEWS) | `news.h` | `news:u`, `news:s` | `newsInit` / `newsExit` | Add and read HOME-menu notifications | not stated |
| Network install (NIM) | `nim.h` | `nim:s` | `nimsInit(buf, size)` / `nimsExit` | Register and start title downloads; needs a buffer that must outlive the session | not stated |
| Process control (NS, PM, SRVPM, Loader, PXI) | `ns.h`, `pmapp.h`, `pmdbg.h`, `srvpm.h`, `loader.h`, `pxipm.h`, `pxidev.h`, `fspxi.h` | `ns:s`, `pm:app`, `pm:dbg`, `srv:pm`, `Loader`, `PxiPM`, `pxi:dev` | `nsInit`, `pmAppInit`, `pmDbgInit`, `srvPmInit`, `loaderInit`, `pxiPmInit`, `pxiDevInit` | Launch, terminate and register titles and processes. System-level; `NS_RebootToTitle` and `NS_RebootSystem` reboot the console. Several headers say the service is normally unreachable to applications | not stated |
| PS (crypto helper) | `ps.h` | `ps:ps` | `psInit` / `psExit` | RSA-SHA256 sign/verify, AES, AES-CCM, random bytes, console id. `PS_GenerateRandomBytes` returns random bytes from the system | not stated |
| LCD (GSPLCD) | `gsplcd.h` | `gsp::Lcd` | `gspLcdInit` / `gspLcdExit` | Backlight power and brightness per screen; the wiki's display page explains the hardware | not stated |
| Codec check, hardware check | `cdcchk.h`, `qtmc.h` | `cdc:CHK`, `qtm:c` | `cdcChkInit`, `qtmcInit` | Factory/hardware-test interfaces (`qtm:c` manipulates the parallax barrier pattern directly) | `qtm:c` New 3DS |
| Config NOR | `cfgnor.h` | `cfg:nor` | `cfgnorInit(value)` / `cfgnorExit` | Raw access to the Wi-Fi configuration NOR flash. Do not write it | not stated |
| Wi-Fi extras | `nwmext.h` | `nwm::EXT` | `nwmExtInit` / `nwmExtExit` | Turn wireless on or off (`NWMEXT_ControlWirelessEnabled`) | not stated |
| Network daemon (NDM) | `ndm.h` | `ndm:u` | `ndmuInit` / `ndmuExit` | Exclusive states and suspend/resume of background daemons (StreetPass, SpotPass, updates, friends). Used by `udsInit`; leave what you enter | not stated |
| Mii selector, Error, Software keyboard | `miiselector.h`, `error.h`, `swkbd.h` | library applets | `miiSelectorInit/Launch`, `errorInit/errorDisp`, `swkbdInit/swkbdInputText` | Call the system applets and return when they close ([application lifecycle](application-lifecycle.md#software-keyboard-and-other-library-applets)) | all |
| Shared font | `font.h` | none (shared memory) | `fontEnsureMapped` | Glyph lookup in the system font, no service | all |

Several of these groups reach into the system (installing titles, rebooting, writing configuration). A homebrew program normally uses only the "own page" groups plus `ac`, `ps`, `ndm` (through UDS), the applets and the font.

## Utilities that are not services

These ship in libctru too and need no `srvGetServiceHandle`, but you will meet them next to the services above.

| Header | What it gives | Notes |
| ------ | ------------- | ----- |
| `util/utf.h` | `decode_utf8/16`, `encode_utf8/16`, `utf8_to_utf16`, `utf8_to_utf32`, `utf16_to_utf8`, `utf16_to_utf32`, `utf32_to_utf8`, `utf32_to_utf16` | System strings (Mii and user names, UDS user names, notification text) are UTF-16; most C code is UTF-8. The bulk converters return the number of units the input *would* produce (more than `len` means truncated), `-1` on invalid input, and do **not** NUL-terminate [documented: `utf.h`] |
| `util/decompress.h` | LZSS/LZ11/Huffman/RLE decompression | See [asset pipeline](asset-pipeline.md#data-compression) |
| `util/rbtree.h` | An intrusive red-black tree (`rbtree_init/insert/find/remove/clear`, `rbtree_item` to get the containing struct) | Plain data structure; no allocation |
| `console.h` | The text console: `consoleInit(screen, NULL)`, `consoleSelect`, `consoleSetWindow`, `consoleClear`, `consoleDebugInit(device)` (route `stderr` to a debug device) | For on-screen text and quick logging; see [debugging](debugging.md) |
| `env.h` | Facts about how the program was started: `envIsHomebrew()`, `envGetHandle(name)` (service handles passed by the launcher), `envGetHeapSize()`, `envGetLinearHeapSize()`, `envGetSystemArgList()`, `envGetSystemRunFlags()`, `envGetAptAppId()` | `envGetHandle` is why some `srvGetServiceHandle` calls succeed without asking `srv:` |
| `font.h` | `fontEnsureMapped()` maps the system shared font; glyph lookup and positioning helpers | Needs the shared font to be installed; the font is the system's, not yours |
| `romfs.h` | `romfsMountSelf`, `romfsMountFromFile`, `romfsInit()`, `romfsExit()` and the `romfs:` device | See [files and saves](filesystem-and-saves.md) |

```c
#include <3ds.h>
#include <stdio.h>

// UTF-8 -> NUL-terminated UTF-16 for system strings (Mii names, UDS user names, ...).
// utf8_to_utf16 returns the number of units the input WOULD produce (> len means truncated)
// or -1 on invalid input, and does not NUL-terminate (utf.h).
static bool to_utf16(u16 *dst, size_t dstUnits, const char *utf8)
{
	if (dstUnits == 0) return false;
	ssize_t n = utf8_to_utf16(dst, (const uint8_t *)utf8, dstUnits - 1);
	if (n < 0 || (size_t)n > dstUnits - 1) return false;   // invalid input or would not fit
	dst[n] = 0;
	return true;
}

int main(void)
{
	gfxInitDefault();
	consoleInit(GFX_TOP, NULL);

	u16 name[11];                                          // 10 characters + NUL
	bool ok = to_utf16(name, sizeof name / sizeof name[0], "Caf\xC3\xA9");
	printf("ok=%d first=%04X fourth=%04X\n", ok, ok ? name[0] : 0, ok ? name[3] : 0);

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

Expected: `ok=1 first=0043 fourth=00E9` ("C" and "é"). The conversion cannot overflow `dst` and always terminates it. Compiled only; not run.

## Probe: which services can your build open?

Run this from the launcher you ship with, on each console model, and keep the output. It tries each service name once and closes the session again. The result codes depend on the program's service-access list, which the launcher and the firmware determine, so an Old 3DS and a New 3DS, and a `.3dsx` and an installed CIA, may differ.

```c
#include <3ds.h>
#include <stdio.h>

// Which services can this program open? Tries each name once and closes the session again.
// Run it from the launcher you ship with, on each console model: the answer depends on both.
static const char *const names[] = {
	"hid:USER", "ir:rst", "ir:u", "cam:u", "y2r:u", "mic:u", "nfc:m", "nfc:u",
	"cfg:u", "cfg:s", "cfg:i", "ptm:u", "ptm:sysm", "mcu::HWC", "qtm:u", "mvd:STD",
	"nwm::UDS", "ndm:u", "ac:u", "ac:i", "http:C", "ssl:C", "soc:U",
	"frd:u", "news:u", "boss:U", "am:net", "ps:ps", "nim:s", "ns:s", "pm:app", "Loader",
};

int main(void)
{
	gfxInitDefault();
	consoleInit(GFX_TOP, NULL);
	printf("Service probe (START exits)\n");

	for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
		Handle h = 0;
		Result rc = srvGetServiceHandle(&h, names[i]);
		if (R_SUCCEEDED(rc)) {
			svcCloseHandle(h);                  // release the session straight away
			printf("%-10s ok\n", names[i]);
		} else {
			printf("%-10s %08lX\n", names[i], (unsigned long)rc);
		}
	}

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

Expected: `ok` for the services your launcher grants and a non-zero error code for the rest. On an Old 3DS `qtm:u` and `mvd:STD` should fail because the services do not exist there. If a service name appears in the environment handed over by the loader, `srvGetServiceHandle` returns a duplicate of that handle without asking `srv:` ([IPC and services](ipc-and-services.md#getting-a-session-srv-getservicehandle)), so `ok` does not by itself prove the system service was reachable by name. Nothing about the exact error values is recorded here.

## Technical details

| Item | Value | Source |
| ---- | ----- | ------ |
| Reference counting | Each `xxxInit` is `AtomicPostIncrement(&refCount)`; the first one opens the session | `ac.c`, `cfgu.c`, `ptmu.c`, `mcuhwc.c`, `gsplcd.c`, `qtm.c` |
| Session fallbacks | `ac:i` > `ac:u`; `cfg:i` > `cfg:s` > `cfg:u`; `nfc:m` > `nfc:u`; `hid:USER` > `hid:SPVR`; `am:*` by function | the matching `.c` files |
| `frd`/`act` selection | `forceUser` true selects `frd:u` / `act:u`, false tries `frd:a` / `act:a` | `frd.c`, `act.c` |
| `acWaitInternetConnection` | `while (ACU_GetWifiStatus succeeds && status == 0);` (no sleep, no timeout) | `ac.c` |
| `ACU_GetStatus` | 1 = not connected, 3 = connected | `ac.h` |
| `nimsInit` | takes a caller buffer and length | `nim.h` |
| `bossInit` | takes a program id and a `force_user` flag | `boss.h` |

## Pitfalls

- **Pairing `xxxExit` with a failed `xxxInit`.** The count was rolled back; an extra exit closes someone else's session. Pair only successes.
- **Closing a handle libctru owns.** Use `xxxGetSessionHandle()` only for IPC calls libctru does not wrap.
- **Calling system-level groups from a game.** Symptom: access errors at best, a reboot or data loss at worst (`nsInit`/`NS_RebootSystem`, `CFGI_*`, `MCURTC_Poweroff`).
- **Assuming a service is present on every model.** Check the model and the open result (`qtm`, `mvd`, NFC).
- **Holding a session you do not use.** Some services have a small session limit (`qtm` has 3 combined); open late, close early.

## Open questions

- A table of which services a `.3dsx` run under Luma3DS can open on an Old 3DS and on a New 3DS (run the probe and record the output with firmware and Luma3DS versions).
- Whether `ac`, `frd`, `act`, `news`, `boss` behave differently on a New 3DS (libctru does not say).
- Exact error values for each failure of the probe above.

## Verification status

| Item | Status |
| ---- | ------ |
| Probe and UTF-16 example | Build with devkitARM r68-1 (GCC 16.1.0), libctru 2.7.1-1, `-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft -O2 -Wall -Wextra`, no warnings; linked with `3dsx.specs` |
| Old 3DS | not run |
| New 3DS | not run |
| Emulator | not run |

## Sources

- libctru [`include/3ds/services/`](https://github.com/devkitPro/libctru/tree/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/include/3ds/services) and [`source/services/`](https://github.com/devkitPro/libctru/tree/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/source/services) — every header and init function summarised above (`ac`, `act`, `am`, `ampxi`, `boss`, `cdcchk`, `cfgnor`, `frd`, `frdn`, `gsplcd`, `loader`, `ndm`, `news`, `nim`, `ns`, `nwmext`, `pmapp`, `pmdbg`, `ps`, `pxidev`, `pxipm`, `qtmc`, `srvpm`).
- libctru [`util/utf.h`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/include/3ds/util/utf.h), [`env.h`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/include/3ds/env.h), [`console.h`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/include/3ds/console.h) and [`rbtree.h`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/include/3ds/util/rbtree.h) — utilities table.
- libctru [`include/3ds/applets/`](https://github.com/devkitPro/libctru/tree/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/include/3ds/applets) and [`font.h`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/include/3ds/font.h) — applets and shared font.
- [CTR services](../software/services.md) — wiki catalogue of service names.
