# Homebrew environments and workflow

As of 12 November 2026, Switch 2 homebrew spans several different execution environments. A game sandbox can run code with the host game's privileges; the published ns2jb chain demonstrated native code in system processes on firmware 23.0.1. Neither fact alone establishes a general, persistent custom firmware or a drop-in NX `.nro` environment.

## What you need

Choose a project and read its current prerequisites before changing firmware or buying a game. Record the exact console firmware, game and game version, entry point, host computer, launcher revision and application revision. Availability and version support change; the project itself is the authority for its setup.

## How it works

| Environment | What runs | Practical consequence |
| ----------- | --------- | --------------------- |
| Game sandbox | Code or scripts inside a particular game | Host permissions, APIs and lifecycle apply. [CivHomebrew](https://github.com/XITRIX/CivHomebrew) targets Civilization VII and its own application format; ordinary NX NRO compatibility must not be assumed. |
| Native system-process session | Native code via a compatible exploit chain | [ns2jb](https://pepsipu.com/blog/2026-10-switch2-jailbreak-ns2jb/) describes a tethered chain demonstrated on 23.0.1; it does not establish kernel control. |
| Switch compatibility | Original Switch software in the compatibility environment | See [compatibility mode](../software/compatibility.md); this is not native OZ execution. |

[Reported] The ns2jb author describes an initial game exploit, a filesystem-service vulnerability and a loader trust-boundary attack, ending in code running with system-process privileges. The original write-up's 23.0.1 claim is dated 21 October 2026; do not infer support for a later firmware from it. [HBLauncher](https://github.com/ravensclawx/HBLauncher) offers an experimental native application interface and examples; use its current build and setup instructions, not NX instructions by analogy.

## Technical details

Start with the project's smallest example. Build it with the versions specified by that project, preserve the build log, then verify startup, input, clean exit and a repeat launch before adding graphics or networking. Record whether each result was built, emulator-tested or tested on physical hardware. Capture the first error and symbol name when a launcher cannot resolve an import; confirm that the launcher, application ABI and firmware combination match before changing application code.

For an NX port, isolate platform-facing input, rendering, audio, storage and lifecycle code behind interfaces. Select implementations after establishing the target environment. The [comparison](../engineering/nx-comparison.md) gives the decision points.

## Limits and risks

A process exploit does not automatically provide unrestricted service access, kernel control, persistence or a stable graphics API. Exploit attempts and firmware changes can lose access or data; keep independent backups and follow the original project's instructions. This wiki has not verified these projects on hardware.

## Sources

- [ns2jb original research](https://pepsipu.com/blog/2026-10-switch2-jailbreak-ns2jb/) — chain and demonstrated 23.0.1 scope.
- [HBLauncher](https://github.com/ravensclawx/HBLauncher) — native launcher and example SDK.
- [CivHomebrew](https://github.com/XITRIX/CivHomebrew) — game-hosted launcher and application format.
- [CivSwitch](https://github.com/Gezine/CivSwitch) — game entry point and prerequisites.
