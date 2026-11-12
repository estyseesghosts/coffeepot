# Homebrew project inventory

Switch 2 homebrew is a set of independent projects at different maturity levels, built for different execution environments. This page lists what actually exists, what each one gives you, and how much of the advertised capability has been demonstrated. The environments themselves are on the [homebrew environments page](state-and-workflow.md).

## Why this is a separate page

A repository containing a launcher, an emulator or a GPU example is not evidence that every advertised feature works on hardware. Several projects below publish hardware logs; others publish a README and an example. The distinction matters when you are deciding what to base work on.

## Tooling

| Project | What it provides | Assessment |
| ------- | ---------------- | ---------- |
| [mac2jb](https://github.com/cpypwn/mac2jb) | A Mac-based front end for several exploit and homebrew workflows | Useful tooling, but it carries notable host-security requirements |
| [ns2-savekit](https://github.com/dougchansan/ns2-savekit) | Save enumeration, backup, modification and verification | The most thorough experimental evidence of the set; its `RESULTS.md` records actual Switch 2 measurements, and `ARTEMIS-LAUNCHER.md` records firmware, title versions, upstream commits, hashes, command results and observed failures |
| [OunceCake](https://github.com/misshexa66/OunceCake) | Exports decrypted [RomFS](../glossary.md#romfs) and [ExeFS](../glossary.md#exefs) from games, updates and DLC | A practical reverse-engineering tool, with an important limit: it asks the console's own filesystem services to load content normally and then exports what they produced. It supplies no decryption keys and does not show that raw Switch 2 packages can be loaded freely |

## Native execution

| Project | What it provides | Assessment |
| ------- | ---------------- | ---------- |
| [ns2jb](https://gitlab.com/pepsipu/ns2jb) and [kurumuz/ns2jb](https://gitlab.com/kurumuz/ns2jb) | The privileged native execution chain, its native probes and its debug logs | The strongest evidence for native execution on 23.0.1; the maintained fork documents reliability improvements and firmware-specific memory layouts. Patched as a system escape in 23.5.0 |
| [HBLauncher](https://github.com/ravensclawx/HBLauncher) | A native AArch64 launcher, an application ABI, examples and Vulkan-backed rendering | An experimental native development environment. It operates inside a privileged, externally established session rather than providing an unrestricted loader, and public issue reports document SDK symbol-resolution failures |
| [IZ2S Platform](https://github.com/byizmysh/IZ2S-Platform) | A PC-managed launcher and modular applications | Experimental, with varying hardware verification |
| [libnx-ounce](https://github.com/alula/libnx-ounce) | A fork of the original Switch's `libnx` userland library, ported | A concrete starting point for native API portability, but not a complete or generally supported SDK |

## Game-sandbox homebrew

| Project | What it provides | Assessment |
| ------- | ---------------- | ---------- |
| [CivSwitch](https://github.com/Gezine/CivSwitch) | Civilization VII chat input injection and JavaScript-engine exploitation, plus a save-based launcher | The entry point that does not depend on keeping a delisted eShop application installed. It uses the original Switch physical cartridge of Civilization VII, version 1.0.0 |
| [CivHomebrew](https://github.com/XITRIX/CivHomebrew) | A save-installed launcher, FTP and `.civapp` packages | A useful game-sandbox application framework, for its supported game and firmware versions. Ordinary `.nro` files are explicitly unsupported |
| [Moonlight-CivSwitch](https://github.com/XITRIX/Moonlight-CivSwitch) | PC streaming, controller input and hardware video decoding | A working example of substantial functionality inside the sandbox. [Reported] Its author reports Switch 2 testing at 1080p and 60 fps; the result depends on the stated conditions and has not been reproduced by Coffeepot |
| [Balatro RCE](https://github.com/Luminger/balatro-switch-rce) | Code execution through the game's LÖVE runtime, with a Lua console, native function calls within the game, and FTP | The LÖVE runtime mounts saved data ahead of the original game resources, so a modified save replaces the Lua startup files. The November 6 release adds a native-function-call bridge using the existing game's code. It stays inside the game's process |
| [Artemis launcher](https://github.com/alula/switch-artemis-launcher) | Lua execution and HTTP-based tools through crafted visual-novel save data | Persists at the game level rather than system-wide. Supported examples include Seedsow Lullaby, Hamidashi Creative and Nora, Princess and Crying Cat 2 |

## Modding and content

| Project | What it provides | Assessment |
| ------- | ---------------- | ---------- |
| [ns2jb-modloader](https://github.com/SebaaMG/ns2jb-modloader) | [RomFS](../glossary.md#romfs) and [ExeFS](../glossary.md#exefs) overlays and patches | Uses the familiar `contents/<title-id>/romfs` and `exefs` directory layout. Its November releases document tests involving Mario Kart World, Super Smash Bros. Ultimate and Mario Party Superstars, and the developer explicitly warns that plugins, runtime cheats and overlays are not supported |

## Custom firmware research

| Project | What it provides | Assessment |
| ------- | ---------------- | ---------- |
| [Ozone](https://github.com/alula/Ozone) | Porting selected Atmosphère components | Early research. Its README explicitly says it is not full Switch 2 custom firmware and very likely never will be. It is still useful for identifying which engineering blockers stand between a port and a working environment |

## Related projects

Two forks extend an existing project rather than starting a new one, and are worth knowing about because they change what the base project can do:

- [CivSwitchLDN](https://github.com/ItsNerisma/CivSwitchLDN) delivers the CivSwitch payload through local wireless instead of simulated keyboard input, with a [rooted-Android implementation](https://github.com/ItsNerisma/CivSwitchAndroid) alongside it.
- [IZCIVSwitch](https://github.com/byizmysh/IZCIVSwitch), released November 11, 2026, combines CivSwitch with the older `fs` exploit, with work originating in `civswitch-kit`. Its documented target remains firmware 23.0.1, not 23.5.0.

## What this list does not include

[Reported] A purported UnlockSwitch development board was covered by GameGaz on November 11, 2026. It is not a released hardware modification, and nothing about it has been demonstrated on a retail console.

> Unknown: Japanese scene reporting says Gezine has claimed to know additional suitable game entry points that are being withheld. That is a researcher's claim, not evidence of another released exploit.

## Sources

- [ns2jb original research](https://pepsipu.com/blog/2026-10-switch2-jailbreak-ns2jb/) — the chain and its demonstrated scope.
- [kurumuz/ns2jb](https://gitlab.com/kurumuz/ns2jb) — reliability, memory layouts and native probes.
- [ns2-savekit results](https://github.com/dougchansan/ns2-savekit/blob/main/docs/RESULTS.md) — recorded hardware measurements.
- [CivHomebrew](https://github.com/XITRIX/CivHomebrew) — game-hosted launcher and application format.
- [GameGaz research roundup](https://gamegaz.com/2026110946503/) — the Japanese scene's view of the same projects.
