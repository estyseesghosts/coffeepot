# CTR services

Games on the 3DS cannot touch most hardware themselves. Instead they ask **services** — background system [processes](../../glossary.md#process) that sit waiting for requests — to do it for them: read a file, draw to the GPU, play audio, join Wi-Fi. A service is looked up by a short text name such as `fs:USER`, and a request is a small structured message.

The set of services forms the official API that retail developers see. A few, such as the GPU, are very thin wrappers; others are layered, where a game-facing service calls a more privileged one.

## Behavior

A program first connects to the **service manager** port `srv:` and asks for a service by name; it gets back a handle to a *session* with that service. It then sends commands with the [`SendSyncRequest`](kernel.md) system call and blocks until the reply arrives. Services are an abstraction over kernel *ports*: they behave the same way except that service handles are fetched through `srv:` instead of being retrieved directly.

Each command is identified by a 32-bit **command header** (3dbrew lists them such as `0x00010040` for `dsp::DSP` `RecvData`). When a request passes a buffer, the kernel maps that memory into the receiving process (see [kernel](kernel.md)). The exact message layout is described on 3dbrew's *IPC Command Structure* page, which is outside this wiki's source set.

Many services come in access levels, written as suffixes: `:USER`/`:u` for ordinary apps, `:S` or `:sys` for system modules, `:P` for privileged.

## Technical details

Services grouped by the process that provides them (all present on Old and New 3DS unless noted):

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
| Camera | `cam:u`, `y2r:u`, `cam:s`, `cam:c`, `cam:q` (New 3DS only) | |
| GPU/LCD | `gsp::Lcd`, `gsp::Gpu` | See [GPU](../hardware/gpu.md) and [displays](../hardware/display.md) |
| Misc | `boss:U/P/M` (SpotPass), `cecd:u/s/ndm` (StreetPass; see [StreetPass (CEC)](streetpass-cec.md)), `ir:u/USER/rst`, `dlp:CLNT/FKCL/SRVR` (Download Play), `ptm:*` (power/play history), `hid:USER/SPVR/NFC/QTM`, `i2c::MCU/CAM/LCD/DEB/HID/IR/EEP/NFC/QTM`, `gpio:*` | |

### Services and the ARM9

`pxi:*` and `Pxi*` services are the ARM11 side of the [PXI](../glossary.md#pxi) channel to the ARM9. File access to NAND and cartridges goes `fs:USER` → `fs` process → PXI → [Process9](os.md).

### Service wrappers in libctru

`libctru` has a header and source file per service group, which is a convenient index of what is publicly usable: `ac`, `act`, `am`, `ampxi`, `apt`, `boss`, `cam`, `cdcchk`, `cfgnor`, `cfgu`, `csnd`, `dsp`, `frd`, `frdn`, `fs`, `fspxi`, `fsreg`, `gspgpu`, `gsplcd`, `hid`, `httpc`, `ir`, `irrst`, `loader`, `mcu*` (camera, codec, GPU, HID, hardware controller, network, power-LED, RTC, sound), `mic`, `mvd`, `ndm`, `news`, `nfc`, `nim`, `ns`, `nwmext`, `pmapp`, `pmdbg`, `ps`, `ptmgets`/`ptmplays`/`ptmsets`/`ptmsysm`/`ptmu`, `pxidev`, `pxipm`, `qtm`/`qtmc`, `soc`, `srvpm`, `sslc`, `uds`, `y2r`. See [libctru](../homebrew/libctru.md).

### System titles that implement services

Luma3DS replaces three of the early service-related system modules with its own versions: `sm` (the service manager, to remove access-control restrictions), `pxi` and `pm` ([Luma3DS](../homebrew/luma3ds.md)).

The Home Menu, applets, system settings, browser and other bundled programs are ordinary [titles](titles.md). 3dbrew lists them (Home Menu "menu", eShop "tiger", Internet Browser "spider"/"skater", Mii Maker "edit", System Settings "mset", and so on).

## Homebrew notes

Homebrew calls the same services through `libctru` wrappers. Some [exploits](../homebrew/exploits.md) attack service modules themselves (`ctr-httpwn` is listed as a system-module exploit, `nimhax` takes over the `nim` module, `nimdsphax` also the `dsp` module).

## Open questions

- Several services are listed without notes (`?`), and `cdc:CHK`, `nwm::EXT/SAP/TST`, `ns:c` purposes are unknown.
- The IPC message format is documented on 3dbrew's *IPC Command Structure* page, which is not part of this wiki's source set.

## Sources

- [3dbrew: Services API](https://www.3dbrew.org/wiki/Services_API) — the full service list.
- [3dbrew: DSP Services](https://www.3dbrew.org/wiki/DSP_Services) — example command headers.
- [libctru services headers](https://github.com/devkitPro/libctru) — the list of service wrappers.
- Citra project wiki, "3DS CEC (StreetPass) Documentation" — `cecd:u`/`cecd:s` functions (no verified link; see [StreetPass (CEC)](streetpass-cec.md)).
- [3dbrew: Nintendo Software](https://www.3dbrew.org/wiki/Nintendo_Software) — system applications.
- [Rodrigo Copetti, Nintendo 3DS Architecture: Operating System](https://www.copetti.org/writings/consoles/nintendo-3ds/) — how services layer over the kernel.
