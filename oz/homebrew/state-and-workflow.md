# Homebrew environments and workflow

As of 12 November 2026, Switch 2 homebrew spans several different execution environments. A game sandbox can run code with the host game's privileges; a privileged native session was demonstrated on firmware 23.0.1 and was patched in 23.5.0. Neither fact alone establishes a general, persistent custom firmware or a drop-in NX `.nro` environment.

## What you need

Choose a project and read its current prerequisites before changing firmware or buying a game. Record the exact console firmware, game and game version, entry point, host computer, launcher revision and application revision. Availability and version support change; the project itself is the authority for its setup.

The [OZ entry paths guide](oz-entry-paths.md) walks through development and research in these environments. The specific projects that exist, and how well each has been tested, are catalogued on the [project inventory](project-inventory.md).

## How it works

| Environment | What runs | Practical consequence |
| ----------- | --------- | --------------------- |
| Game sandbox | Code or scripts inside a particular game | Host permissions, APIs and lifecycle apply. [CivHomebrew](https://github.com/XITRIX/CivHomebrew) targets Civilization VII and its own application format; ordinary NX NRO compatibility must not be assumed. |
| Native system-process session | Native code via a compatible exploit chain | [ns2jb](https://pepsipu.com/blog/2026-10-switch2-jailbreak-ns2jb/) describes a tethered chain demonstrated on 23.0.1; it does not establish kernel control, and 23.5.0 patched the system-level escape. |
| Switch compatibility | Original Switch software in the compatibility environment | See [compatibility mode](../software/compatibility.md); this is not native OZ execution. |

### The first public chain

[Reverse-engineered] ns2jb combined a SmileBASIC game entry, an `fs` filesystem-service flaw and a loader trust-boundary attack to demonstrate native system-process execution on 23.0.1. See [ns2jb](ns2jb.md) for the full engineering walkthrough, requirements, demonstrated uses and limits.

### Other entry points

[Reverse-engineered] There is more than one way to execute code, and the routes give different levels of access:

| Entry point | Mechanism | Capabilities |
| ----------- | --------- | ------------ |
| [CivSwitch](https://github.com/Gezine/CivSwitch) | Civilization VII chat XSS and JavaScript-engine exploitation | Game-level code, a network loader, a save-based launcher |
| [Balatro RCE](https://github.com/Luminger/balatro-switch-rce) | The LÖVE runtime loads save files ahead of the game's original Lua files | A Lua console, native function calls within the game, FTP |
| [Artemis launcher](https://github.com/alula/switch-artemis-launcher) | Crafted visual-novel save data | Lua execution and HTTP-based tools |
| [ns2jb](ns2jb.md) | Integer overflow followed by the system filesystem exploit | Privileged native execution on 23.0.1 |

[Reverse-engineered] CivSwitch is significant because it does not depend on keeping an already-installed, delisted eShop application. It uses the original Switch physical cartridge of Civilization VII, version 1.0.0, and its first stage uses JavaScript running inside the game's embedded interface. Originally a Raspberry Pi Pico acted as a USB keyboard to type the payload into multiplayer chat; [CivSwitchLDN](https://github.com/ItsNerisma/CivSwitchLDN) delivers it over local wireless instead.

[Reverse-engineered] CivSwitch installs a crafted save that starts the loader when the game loads it. This persists across restarts as a game save, not as system-wide custom firmware.

[Reverse-engineered] Nintendo has delisted SmileBASIC 4, which affects the availability of the first chain's entry point.

### Firmware and native execution

[Reverse-engineered] SwitchBrew records the filesystem flaw as fixed in 23.5.0. This blocks the documented escalation, not every game entry or potential future path; see [ns2jb](ns2jb.md) and [firmware](../software/firmware.md).

[Reported] [HBLauncher](https://github.com/ravensclawx/HBLauncher) offers an experimental native application interface and examples. Its prerequisites and application ABI are project-specific; a successful exploit bootstrap does not establish that every import or subsystem works.

## Technical details

Start with the project's smallest example. Build it with the versions specified by that project, preserve the build log, then verify startup, input, clean exit and a repeat launch before adding graphics or networking. Record whether each result was built, emulator-tested or tested on physical hardware.

Capture the first error and symbol name when a launcher cannot resolve an import; confirm that the launcher, application ABI and firmware combination match before changing application code.

For an NX port, isolate platform-facing input, rendering, audio, storage and lifecycle code behind interfaces. Select implementations after establishing the target environment. The [comparison](../engineering/nx-comparison.md) gives the decision points.

For a game-sandbox program, remember that it is the host game's process: its permissions, available libraries, lifecycle and input are constrained by the host, and a program that works there has not established native compatibility. See [compatibility mode](../software/compatibility.md) for the same reasoning applied to compatibility-mode titles.

## Limits and risks

A process exploit does not automatically provide unrestricted service access, kernel control, persistence or a stable graphics API. Exploit attempts and firmware changes can lose access or data; keep independent backups and follow the original project's instructions.

No publicly established Switch 2 equivalent of the original Switch's Fusée Gelée boot-ROM exploit is documented in the sources reviewed. Hekate is a bootloader, not itself the exploit. See [why NX entry paths differ](nx-entry-paths.md) for the hardware and software dependencies.

Exploit attempts and firmware changes can leave a console in a state the user cannot recover from. This wiki has not verified these projects on hardware.

## Sources

- [ns2jb original research](https://pepsipu.com/blog/2026-10-switch2-jailbreak-ns2jb/) — chain and demonstrated 23.0.1 scope.
- [SwitchBrew system flaws](https://switchbrew.org/wiki/Switch_System_Flaws) — the `fs` path-normalization overflow, its exploitation preconditions and its 23.5.0 fix.
- [kurumuz/ns2jb](https://gitlab.com/kurumuz/ns2jb) — reliability, memory layouts, native probes and debug logs.
- [ns2jb demo and README](https://gitlab.com/pepsipu/ns2jb) — the host requirements, console-side commands and the October 21, 2026 hardware test.
- [HBLauncher](https://github.com/ravensclawx/HBLauncher) — native launcher and example SDK.
- [CivSwitch](https://github.com/Gezine/CivSwitch) — game entry point and prerequisites.
- [Balatro RCE](https://github.com/Luminger/balatro-switch-rce) — save-data resource shadowing and Lua execution.
- [Artemis launcher](https://github.com/alula/switch-artemis-launcher) — Artemis-engine save manipulation.
