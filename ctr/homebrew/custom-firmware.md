# CTR custom firmware

A **[custom firmware](../../glossary.md#cfw)** (CFW) on the 3DS is a patched version of the system software that is loaded in place of — or on top of — the normal one. It removes the signature checks that normally stop unofficial software from running, and adds features such as a layered file system for game mods, region-free play and an in-game menu. The dominant CFW today is **[Luma3DS](../glossary.md#luma3ds)**, usually started by [boot9strap](boot9strap.md).

Required console: any 3DS-family model. Required firmware: depends on the loader; with boot9strap, any version. Outcome: normal system with signature checks disabled and extras.

## What you need

- A way to get code running early at startup: [boot9strap](boot9strap.md) (current standard), or historically `arm9loaderhax`, a flashcart-based loader, or a kernel/ARM9 exploit chain ([exploits](exploits.md)).
- A CFW payload on the SD card.

## How it works

The CFW is loaded early with ARM9 privileges (see [boot9strap](boot9strap.md)) and patches the system software as it starts. Typical features, per Copetti and the community history:

- Remove signature and region checks, so unsigned [titles](../software/titles.md) and CIA installs work.
- Add a **layered file system** that redirects file reads to the SD card, which enables game modifications.
- Optionally boot from a copy of the system on the SD card (**[emuNAND](../glossary.md#emunand)**, introduced as "redNAND" by Smealum in 2013) so the real NAND is untouched and can stay on an exploitable firmware while the emulated one is updated.
- Provide an in-game overlay menu — in Luma3DS, the **Rosalina** menu, also capable of cleanly launching `.3dsx` homebrew as a native process.
- Support AGB_FIRM signature patches (Riku's patcher), loading arbitrary GBA ROMs.

### Generations

| Name | Notes |
| ---- | ----- |
| Gateway's `Launcher.dat` (2013) | First CFW; delivered through the Settings-app exploit; required the red flashcard to work |
| Palantine (leaked 2014) | Closed-source; running CIAs |
| rxTools (2015, roxas75) | Open-source; RXMode: no signature checks, emuNAND, removes TWL whitelist checks |
| CakesFW, PastaCFW (Brahma loader), ReiNand | Open-source alternatives; PastaCFW was the first open-source CFW with no emuNAND support at release; ReiNand was the first full CFW for the New 3DS |
| AuReiNand → **Luma3DS** (April 2016) | Rewritten from scratch; became the de facto standard; signature/region patches, layered FS, Rosalina |

Related tools commonly installed alongside: **GodMode9** (all-access file browser, dumps NAND/OTP/boot ROMs), **FBI** (open-source CIA installer, replacing the leaked dev-kit Dev Menu and BigBlueMenu), **Anemone3DS** (Home Menu themes), **nds-bootstrap** and **TWiLight Menu++** (DS game loader; using `TWL_FIRM`, not emulation), CtrBootManager/BootCtr (early boot managers).

## Technical details

- emuNAND works by redirecting the system's NAND reads to a copy on the SD card; it began as Smealum's private redNAND demonstration (October 2013) and became a Gateway selling point in December 2013.
- Rosalina, part of Luma3DS, replaces an installed title's ExeFS and ExHeader during load to run `.3dsx` applications as a native process with full ARM11 system permissions. 3dbrew states it is currently the only option for running `.3dsx` on 11.4+ Old 3DS.
- Patching internals (which functions are patched in which processes) are not covered by the sources used here.

## Limits and risks

- boot9strap can either load a payload from the SD card or continue to boot normally, so a missing payload does not by itself stop the console starting (Copetti).
- Online features: Nintendo banned consoles seen running piracy-related tools; online services have since ended ([history](history.md)).
- Don't mix emuNAND and system NAND data across consoles: console-unique keys differ.
- Feature lists depend on version; this page reflects the sources (2015–2023).

## Open questions

- The exact patch list of Luma3DS, and whether patches modify stored firmware or only memory, is not stated in the sources used here.
- Which loaders remain in use besides boot9strap is not stated.

## Sources

- [Rodrigo Copetti, Nintendo 3DS Architecture: Anti-Piracy and Homebrew](https://www.copetti.org/writings/consoles/nintendo-3ds/) — Gateway, rxTools, PastaCFW, ReiNand, Luma3DS features, GodMode9, FBI, Anemone3DS, nds-bootstrap.
- [Hacks Guide Wiki: History of 3DS Hacking](https://wiki.hacks.guide/wiki/3DS:History_of_3DS_Hacking) — dates and lineage of each CFW.
- [3dbrew: Homebrew Exploits](https://www.3dbrew.org/wiki/Homebrew_Exploits) — Rosalina and `.3dsx` on 11.4+.
- [3dbrew: Homebrew Applications](https://www.3dbrew.org/wiki/Homebrew_Applications) — launchers, title managers, theme managers.
