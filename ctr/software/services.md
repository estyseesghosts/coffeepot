# CTR services

Games on the 3DS cannot touch most hardware themselves. Instead they ask **services** — background system [processes](../../glossary.md#process) that sit waiting for requests — to do it for them: read a file, draw to the GPU, play audio, join Wi-Fi. A service is looked up by a short text name such as `fs:USER`, and a request is a small structured message.

The set of services forms the official API that retail developers see. A few, such as the GPU, are very thin wrappers; others are layered, where a game-facing service calls a more privileged one.

## Behavior

A program first connects to the **service manager** port `srv:` and asks for a service by name; it gets back a handle to a *session* with that service. It then sends commands with the [`SendSyncRequest`](kernel.md) system call and blocks until the reply arrives. Services are an abstraction over kernel *ports*: they behave the same way except that service handles are fetched through `srv:` instead of being retrieved directly.

Each command is identified by a 32-bit **command header** (3dbrew lists them such as `0x00010040` for `dsp::DSP` `RecvData`). When a request passes a buffer, the kernel maps that memory into the receiving process (see [kernel](kernel.md)). The exact message layout (header word, normal and translate parameters, buffer descriptors) is explained with a worked example on [the IPC engineering page](../engineering/ipc-and-services.md).

For a hands-on guide to writing a correct client (command buffer, translate descriptors, result codes, session lifetime) see [Writing a correct service client](../engineering/ipc-and-services.md); this page is the reference catalogue. The session handle is per process, and the service name is at most 8 characters [documented: 3dbrew Services API].

Many services come in access levels, written as suffixes: `:USER`/`:u` for ordinary apps, `:S` or `:sys` for system modules, `:P` for privileged.

## Technical details

Services grouped by the process that provides them, following 3dbrew's *Services API* list. 3dbrew marks every group below as present on Old 3DS except `mvd` and `qtm`; **New 3DS:** `mvd`, `qtm` and `cam:q` exist only there.

| Process / area | Service names | Notes |
| -------------- | ------------- | ----- |
| File system (`fs`) | `fs:USER`, `fs:LDR`, `fs:REG` | `USER` for apps and modules, `LDR` loader, `REG` registration |
| Process (`ps`) | `ps:ps` | |
| PXI (ARM9 bridge) | `PxiFS0`, `PxiFS1`, `PxiFSB`, `PxiFSR`, `PxiPM`, `pxi:am9`, `pxi:dev`, `pxi:mc`, `pxi:ps9` | Talk to [Process9](os.md) |
| Application manager (`am`) | `am:app`, `am:net`, `am:u`, `am:sys`, `am:pipe` | Title installation; `pipe` is an internal FSFile-like interface |
| Process manager (`pm`) | `pm:app`, `pm:dbg` | Launching titles (debug variant with debugging enabled) |
| NIM | `nim:aoc`, `nim:ndm`, `nim:s`, `nim:u` | DLC, eShop, updater |
| Config | `cfg:u`, `cfg:s`, `cfg:i`, `cfg:nor` | User/system/initialisation config; `nor` is the Wi-Fi SPI flash |
| NS and APT | `ns:s`, `ns:p`, `ns:c`, `APT:A`, `APT:S`, `APT:U` | Shell launching, power, applet management |
| RO | `ldr:ro` | Dynamic library loading ([CRO modules](cro-modules.md)) |
| Network | `ndm:u`, `nwm::CEC/EXT/INF/SAP/SOC/TST/UDS`, `http:C`, `ssl:C`, `soc:P`, `soc:U` | StreetPass, infrastructure, local wireless, HTTP, TLS, sockets |
| Sound | `csnd:SND`, `dsp::DSP`, `cdc:HID/MIC/CSN/DSP/LGY/CHK` | See [audio](../hardware/audio.md) |
| Camera | `cam:u`, `y2r:u`, `cam:s`, `cam:c`, `cam:q` (**New 3DS:** only) | |
| GPU/LCD | `gsp::Lcd`, `gsp::Gpu` | See [GPU](../hardware/gpu.md) and [displays](../hardware/display.md) |
| Other groups from 3dbrew | `ac:i/u` (Wi-Fi connection), `act:a/u` (Nintendo Network accounts), `frd:a/n/u` (friends), `news:s/u` (notifications), `pdn:s/d/i/g/c`, `SPI::NOR/CD2/CS2/CS3/DEF`, `Loader`, `mcu::CAM/GPU/HID/RTC/SND/NWM/HWC/PLS/CDC`, `mic:u`, `mp:u`, `nfc:dev/m/p/r/s/u`, `mvd:STD` and `qtm:*` (New 3DS only) | `nfc:m` is the amiibo-capable path used by libctru's `nfc`; `qtm` is head tracking |
| Misc | `boss:U/P/M` (SpotPass), `cecd:u/s/ndm` (StreetPass; see [StreetPass (CEC)](streetpass-cec.md)), `ir:u/USER/rst`, `dlp:CLNT/FKCL/SRVR` (Download Play), `ptm:*` (power/play history), `hid:USER/SPVR/NFC/QTM`, `i2c::MCU/CAM/LCD/DEB/HID/IR/EEP/NFC/QTM`, `gpio:*` | |

Suffix meanings in 3dbrew's notes: `am` — `app` for applications, `net` network installation (`nim`), `u` local installation, `sys` system titles; `nim` — `aoc` DLC, `ndm`, `s` eShop, `u` updater; `ns:s` system, `ns:p` power (shutdown/reboot), `APT:A/S/U` application/system/user; `cdc:` — HID, MIC, CSN, DSP, LGY (legacy, some backward-compatibility role), CHK; `dlp:` — CLNT client, FKCL fake client, SRVR server; `boss:` U user, P privileged, M for `ndm`; `ptm:` — gets (get system time), play (play history), s, sets (set system time), sysm (Home Menu/Test Menu), u; `nwm::` — CEC (StreetPass), INF infrastructure, SOC socket, UDS local WLAN; `act:`/`frd:` — a admin; `hid:` — NFC, QTM, SPVR (system privileged?). The PXI service groups are filesystem, process, application manager, process manager, development, gamecard and the legacy FIRM (`TWL_FIRM`/`AGB_FIRM`) interfaces. The only named port is `ErrDisp`.

### Services and the ARM9

`pxi:*` and `Pxi*` services are the ARM11 side of the [PXI](../glossary.md#pxi) channel to the ARM9. File access to NAND and cartridges goes `fs:USER` → `fs` process → PXI → [Process9](os.md).

### Practical catalogue: which service for which job

Which services a normal (non-system) homebrew program should reach for, with the libctru init call that opens them. All names and init functions were read in libctru at the pinned commit (`include/3ds/services/*.h`, `source/**`) [documented]; whether each works under a given CFW or title permission set is not listed here (access control is set by the process's `exheader` service list; see [Luma3DS](../homebrew/luma3ds.md)).

| Job | Service | libctru init | Notes |
| --- | ------- | ------------ | ----- |
| Files on SD, save data, ROMFS | `fs:USER` | `fsInit` | See [SD](sd-filesystem.md), [savegames](savegames.md), [NAND](nand-filesystem.md) |
| Draw to screen, GPU command lists | `gsp::Gpu` | `gspInit` / `gfxInit` | See [GPU](../hardware/gpu.md); LCD backlight via `gsp::Lcd` (`gspLcdInit`) |
| Buttons, touch, accelerometer, gyro | `hid:USER` (`hid:SPVR` fallback) | `hidInit` | |
| Applet and home-button handling, suspend | `APT:A/S/U` | `aptInit` | Required for a well-behaved app; see [application lifecycle](../engineering/application-lifecycle.md) |
| System settings (language, model, username) | `cfg:u` (fall back to `cfg:i`, `cfg:s`) | `cfguInit` | Worked example on the IPC page |
| Audio (DSP-based) | `dsp::DSP` | `dspInit`, `ndspInit` | See [audio](../hardware/audio.md); legacy `csnd:SND` via `csndInit` |
| Microphone | `mic:u` | `micInit` | |
| Cameras and colour conversion | `cam:u`, `y2r:u` | `camInit`, `y2rInit` | |
| Sockets (BSD-like) | `soc:U` | `socInit` | Needs a shared-memory buffer; see [networking](../engineering/networking.md) |
| HTTP and TLS | `http:C`, `ssl:C` | `httpcInit`, `sslcInit` | |
| Local wireless (UDS) | `nwm::UDS` | `udsInit` | |
| Wi-Fi status | `ac:u` | `acInit` | |
| Friends list | `frd:u` | `frdInit` | |
| SpotPass, notifications | `boss:U`, `news:u` | `bossInit`, `newsInit` | |
| Infrared, circle pad pro | `ir:u`, `ir:rst` | `iruInit`, `irrstInit` | |
| NFC (amiibo) | `nfc:u` | `nfcInit` | |
| Battery, play time, pedometer | `ptm:u` | `ptmuInit` | |
| Title management | `am:*`, `nim:s`, `pm:app` | `amInit`, `nimsInit`, `pmAppInit` | System-level; `am:*` needs elevated service access |
| Hardware video decode (New 3DS) | `mvd:STD` | `mvdstdInit` | New 3DS only |
| Head tracking (New 3DS) | `qtm:*` | `qtmInit` | New 3DS only |

Failure modes: if the process's service-access list does not include a name, `srvGetServiceHandle` returns an error instead of a session; if too many sessions are open for a service it can block or fail (see the IPC page, [hypothesis] on exact limits); `srvInit` itself must come first. A service that exists only on a newer system version simply fails to open on older firmware [hypothesis: not measured here].

### Service wrappers in libctru

`libctru` has a header and source file per service group, which is a convenient index of what is publicly usable: `ac`, `act`, `am`, `ampxi`, `apt`, `boss`, `cam`, `cdcchk`, `cfgnor`, `cfgu`, `csnd`, `dsp`, `frd`, `frdn`, `fs`, `fspxi`, `fsreg`, `gspgpu`, `gsplcd`, `hid`, `httpc`, `ir`, `irrst`, `loader`, `mcu*` (camera, codec, GPU, HID, hardware controller, network, power-LED, RTC, sound), `mic`, `mvd`, `ndm`, `news`, `nfc`, `nim`, `ns`, `nwmext`, `pmapp`, `pmdbg`, `ps`, `ptmgets`/`ptmplays`/`ptmsets`/`ptmsysm`/`ptmu`, `pxidev`, `pxipm`, `qtm`/`qtmc`, `soc`, `srvpm`, `sslc`, `uds`, `y2r`. See [libctru](../homebrew/libctru.md).

### System titles that implement services

`mvd` and `qtm` have libctru wrappers but exist only on New 3DS. Luma3DS replaces three of the early service-related system modules with its own versions: `sm` (the service manager, to remove access-control restrictions), `pxi` and `pm` ([Luma3DS](../homebrew/luma3ds.md)).

The Home Menu, applets, system settings, browser and other bundled programs are ordinary [titles](titles.md). 3dbrew lists them (Home Menu "menu", eShop "tiger", Internet Browser "spider"/"skater", Mii Maker "edit", System Settings "mset", and so on).

## Homebrew notes

Homebrew calls the same services through `libctru` wrappers. Some [exploits](../homebrew/exploits.md) attack service modules themselves (`ctr-httpwn` is listed as a system-module exploit, `nimhax` takes over the `nim` module, `nimdsphax` also the `dsp` module).

## Open questions

- 3dbrew's API page says light-weight critical sections exist but gives no API for the light semaphore and light event ([kernel](kernel.md)); error codes are on a separate 3dbrew page.
- Several services are listed without notes (`?`), and `cdc:CHK`, `nwm::EXT/SAP/TST`, `ns:c` purposes are unknown.
- Exact access-control (service-list) differences between retail titles, system modules and Luma3DS's patched `sm` are not catalogued here.
- Exact open-session limits per service are not recorded; see the IPC page's open questions.

## Sources

- [3dbrew: Services API](https://www.3dbrew.org/wiki/Services_API) — the full service list.
- [Writing a correct service client](../engineering/ipc-and-services.md) — IPC format and client examples (this wiki).
- [3dbrew: DSP Services](https://www.3dbrew.org/wiki/DSP_Services) — example command headers.
- [libctru services headers](https://github.com/devkitPro/libctru) — the list of service wrappers.
- Citra project wiki, "3DS CEC (StreetPass) Documentation" — `cecd:u`/`cecd:s` functions (no verified link; see [StreetPass (CEC)](streetpass-cec.md)).
- [3dbrew: Nintendo Software](https://www.3dbrew.org/wiki/Nintendo_Software) — system applications.
- [Rodrigo Copetti, Nintendo 3DS Architecture: Operating System](https://www.copetti.org/writings/consoles/nintendo-3ds/) — how services layer over the kernel.
