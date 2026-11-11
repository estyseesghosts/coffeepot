# Case study: Atmosphere CFW (reported + documented)

A custom **firmware** ([**firmware**](../../../glossary.md#firmware)) stack for the Nintendo **NX** ([**NX**](../../glossary.md#nx)) family that replaces the package1/package2 boot chain, owns the TrustZone monitor and reimplements kernel and sysmodules. What follows summarises the project's own tree and docs. Nothing was measured by Coffeepot. Tree facts are `[documented]`; ban and thermal claims are `[reported]` with conditions.

## When is this useful?

This page overlaps [Atmosphere and running](../../homebrew/atmosphere-and-running.md), which is canonical. Use it for the component-to-source-file map.

## Before you start

- Evidence: `Atmosphere/README.md`, `docs/components/`, `docs/features/` (`cheats.md`, `dns_mitm.md`, `configurations.md`), `docs/changelog.md`, nine SwitchBrew HTMLs (named in Sources), and `Homebrew-Guide/`. Source files named below are in Atmosphere commit `4e3f370` (see [source versions](../source-versions.md)). This page overlaps [Atmosphere and running](../../homebrew/atmosphere-and-running.md), which is canonical.
- Hardware: unpatched Erista (RCM entry) vs patched/Mariko (modchip only); Tegra X1, Cortex-A57 cores, 256-core Maxwell GPU. Companions: [rendering](../rendering.md), [performance and porting](../performance-and-porting.md), [NX GPU](../../hardware/gpu.md).
- Not in-tree: Hekate and sys-clk are absent (no matches; `utilities/` holds only scripts), so Hekate flow is cited from `Homebrew-Guide/usingcfw/hekate.rst` and OC rows stay `[reported]`. The RCM bug is cited without a CVE number. `faq.md` lacks ban/OC guidance (rows unpinned).
- Unknown: the `Memory layout` HTML documents 32/36-bit userspace VA (38-bit from 2.0.0+) but not 39-bit or any applet-pool size, so the pool size below is `Unknown`.

Jargon: a **custom firmware** ([**CFW**](../../../glossary.md#cfw)) patches system software at boot; **homebrew** ([**homebrew**](../../../glossary.md#homebrew)) is unofficial software; the **chain of trust** ([**chain of trust**](../../../glossary.md#chain-of-trust)) is each stage verifying the next; an **exploit** ([**exploit**](../../../glossary.md#exploit)) breaks that chain; **Atmosphere** ([**Atmosphere**](../../glossary.md#atmosphere)) is this CFW; **title override** ([**title override**](../../glossary.md#title-override)) gives homebrew a game's memory budget instead of the applet pool.

## What it is

| Layer `[documented]` | Role and grounding |
| ----- | ---- |
| Fusee | First-stage payload replacing Package1Loader/NX Bootloader: inits hardware, sets up crypto, mounts/emulates eMMC, patches package2, launches Exosphere. RCM vs modchip entry per `Homebrew-Guide`. |
| Exosphere | TrustZone monitor at EL3: key storage (`program/source/secmon_key_storage.cpp`) plus extensions `SmcIramCopy` (0xF0000201), `SmcWriteAddress` (0xF0000003), `SmcGetEmummcConfig` (0xF0000404) (`program/source/smc/secmon_smc_handler.cpp`). |
| Mesosphere | Reimplemented Horizon kernel, not a patch set: scheduler, threads, memory, processes, capabilities, IPC, and the full SVC table in `libraries/libmesosphere/source/` (`kern_k_scheduler.cpp`, `kern_k_process.cpp`, 37 `svc/kern_svc_*.cpp` files), instantiated per-core in `kern_kernel_instantiations.cpp`. |
| Thermosphere | EL2 hypervisor (`thermosphere/README.md`); emuNAND redirection rides on it (`docs/components/thermosphere.md`, `emummc.md`). |
| Stratosphere | Reimplemented sysmodules on `libstratosphere`: `loader`, `sm`, `pm`, `ro`, `ncm`, `spl`, `boot`/`boot2`, `creport`/`fatal`/`erpt`, `dmnt`, `ams_mitm`. IPS exefs and embedded loader patches sit at the edges. |
| Troposphere | "Application-level Horizon OS patches" per the README; the directory holds small applications (`daybreak`, `haze`, `reboot_to_payload`). `dns.mitm` and the cheat manager are Stratosphère components, not Troposphère. |

On top: LayeredFS (`fs_mitm/fsmitm_layered_romfs_storage.cpp`: `LayeredRomfsStorageImpl::Read/GetSize` merging SD over base ROMFS), `ns:am`/`ns:web` intercepts (`ns_mitm/nsmitm_module.cpp`: `AcknowledgeMitmSession`), `dns.mitm` telemetry redirect (`dns_mitm/` sources), and the `dmnt` cheat VM (`dmnt/source/cheat/impl/dmnt_cheat_vm.cpp`, `dmnt_cheat_api.cpp`: attach via `svc::DebugActiveProcess` plus a debug-events thread).

A dev edits five ini surfaces (`docs/features/configurations.md`): `stratosphere.ini` (nogc), `exosphere.ini` (debugmode, prodinfo blanking), `override_config.ini` (button bindings, parsed by `libstratosphere/source/cfg/cfg_override.board.nintendo_nx.inc`), `system_settings.ini` (`name!key` typed settings), and per-title `flags/` (`boot2`, `redirect_save`); `emummc.ini` is external (emuMMC project).

## How it interacts with A57 and Maxwell

Title override is the key CPU/memory interaction. `libstratosphere/source/cfg/cfg_override.board.nintendo_nx.inc` defaults hbl to 64-bit-39; `loader/source/ldr_meta.cpp` applies the override to the NPDM descriptor; `ldr_process_creation.cpp` emits `CreateProcessParameterFlag_AddressSpace64Bit39` (`AddressMap39Start/Size`) and picks the pool partition — homebrew gets game-class VA instead of the applet pool (size `Unknown`) `[documented]`. `ldr_launch_record.cpp` special-cases qlaunch (`Title list` HTML). Clocks stay in Nintendo's `pcv`/`apm` tables; OC tooling is not in-tree `[reported]`. `sm` mitm handoff runs through `sm/source/impl/sm_service_manager.cpp` (`AcknowledgeMitmSession`) and `sm_user_service.hpp` (`AtmosphereInstallMitm`), per the `Services API` HTML.

## What it gets right

| Technique | Effect as stated |
| --------- | ---------------- |
| Full kernel reimplementation | Scheduler, memory, SVC, and IPC redone in source instead of binary patches; 1.12.0 tracks 42-bit VA and kernel-RNG changes `[documented]`. |
| Title override in `loader` | Game-class address space instead of applet OOM (`ldr_meta.cpp`, `cfg_override.board.nintendo_nx.inc`) `[documented]`. |
| Reimplementation + edge patches | Sysmodules rebuilt on `libstratosphere`; IPS and embedded patches cover only per-build differences `[documented]`. |
| NAND backup plus emuMMC | sysNAND stays clean while tests run on a redirect copy (Hekate backup per Homebrew-Guide) `[documented]`. |
| `dns.mitm` defaults plus cheat gating | Telemetry filter survives forgotten custom lists; cheats bind to matching build IDs `[documented]`. |

## What it gets wrong (costs and traps)

| Cost / trap | Detail |
| ----------- | ------ |
| LayeredFS overhead | Layered opens/reads flow through `LayeredRomfsStorageImpl::Read/GetSize`; profiling with mods on measures the mitm `[documented]` mechanism, magnitude `[reported]`. |
| Cheat attach cost | Held `svc::DebugActiveProcess` session plus VM and debug-events thread adds jitter, worst at loading screens `[documented]`. |
| OC, heat, storage risk | Sustained OC raises temperature and SD error rates; exFAT plus hard shutdown corrupts emuMMC images — unpinned, `faq.md` silent `[reported]`. |
| Ban surface | Telemetry, `erpt`/`creport`/`fatal` reports, and online play from CFW raise ban risk; filters reduce but never remove it — unpinned `[reported]`. |
| Update churn, pinned | Firmware 23.0.0 broke `dns.mitm` (new c-ares commands) and layered storage (`IStorageForBatchRead`), fixed in 1.12.1; 1.12.0 tracks 23.0.0 kernel changes via browser-applet memory patches `[documented]`. |

## Lesson: what to copy first

| # | Symptom to fix |
| - | -------------- |
| 1 | Applet OOM — launch via title override for game-class VA (`ldr_meta.cpp`); pool size `Unknown`, do not quote one `[documented]`. |
| 2 | Modded profile slower than stock — benchmark with overlays and cheats off first, then enable one at a time (`fsmitm_layered_romfs_storage.cpp`) `[documented]` mechanism. |
| 3 | Dirty sysNAND — test in emuMMC; Hekate NAND backup before first boot, stored off-SD (Homebrew-Guide) `[documented]`. |
| 4 | Telemetry leak — keep `dns.mitm` defaults on, split `emummc.txt`/`sysmmc.txt`, never take a dirty NAND online; bans happen anyway — unpinned `[reported]`. |
| 5 | Cheats miss or stutter — confirm `<program_id>/cheats/<build_id>.txt` matches the build, check the enable key, detach for other debuggers (`dmnt_cheat_api.cpp`) `[documented]`. |
| 6 | Black screen after update — update Atmosphere, payload, and sigpatches as a set; 23.0.0-style breakage is the norm (1.12.x) `[documented]`. |

## Open questions

- Per-title LayeredFS cost has no controlled open/read benchmark, only user reports.
- `dmnt` plus debugger jitter and long-term emuMMC SD wear/corruption rates are anecdotal `[reported]` only.
- Which Horizon IPC or keyblob change forces the next rework is unknown until the next major firmware.

## Sources

Repositories at pinned commits: [Atmosphere-NX/Atmosphere](https://github.com/Atmosphere-NX/Atmosphere) `4e3f37038d9e82dde7054897213ab32b2ecf651f`, [noahc3/Homebrew-Guide](https://github.com/noahc3/Homebrew-Guide) `6d7b7bc` (archived). Files:

- `Atmosphere/README.md` — component split, credits, licensing.
- `Atmosphere/docs/components/fusee.md`, `exosphere.md` — boot duties; monitor design and custom SMCs (read against `secmon_smc_handler.cpp`, `secmon_key_storage.cpp`).
- `Atmosphere/docs/components/mesosphere.md`, `thermosphere.md`, `troposphere.md`, `libraries.md` — kernel, EL2, app-layer, and library roles (read against `libmesosphere/source/` listing, `kern_kernel_instantiations.cpp`, `thermosphere/README.md`).
- `Atmosphere/docs/components/stratosphere.md`, `modules/loader.md`, `sm.md`, `dmnt.md` — sysmodule list; loader exefs/IPS/homebrew semantics; `sm:` mitm commands; `dmnt:cht` service (read against `ldr_meta.cpp`, `ldr_process_creation.cpp`, `sm_service_manager.cpp`, `dmnt_cheat_api.cpp`, `dmnt_cheat_vm.cpp`).
- `Atmosphere/docs/features/cheats.md`, `dns_mitm.md`, `configurations.md` — cheat VM and attach rules; hosts format and defaults; ini surfaces plus content flags.
- `Atmosphere/docs/changelog.md` (1.12.0, 1.12.1) — pinned churn: 23.0.0 c-ares/BatchRead breakage, 42-bit kernel, browser-applet patches. `faq.md` — checked, no ban/OC guidance.
- `Homebrew-Guide/gettingstarted/`, `usingcfw/hekate.rst`, `emummc/` — RCM vs modchip, Hekate backup (not vendored in-tree), emuMMC setup.
- SwitchBrew HTMLs — `Secure Monitor` (keys in TrustZone), `Cryptosystem` (keyblob/SBK/SSK), `Services API` (`sm:`/`ldr:pm`/`ldr:dmnt`), `SVC` (`CreateProcess`), `Kernel` (`KProcessCapabilities`), `Memory layout` (32/36/38-bit documented; lacks 39-bit/pool size), `Tegra X1` (256-core Maxwell), `Error codes` (erpt/fatal/creport), `Title list` (qlaunch).
