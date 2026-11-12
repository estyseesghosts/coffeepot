# Homebrew environments and workflow

As of 12 November 2026, Switch 2 homebrew spans several different execution environments. A game sandbox can run code with the host game's privileges; a privileged native session was demonstrated on firmware 23.0.1 and was patched in 23.5.0. Neither fact alone establishes a general, persistent custom firmware or a drop-in NX `.nro` environment.

## What you need

Choose a project and read its current prerequisites before changing firmware or buying a game. Record the exact console firmware, game and game version, entry point, host computer, launcher revision and application revision. Availability and version support change; the project itself is the authority for its setup.

The specific projects that exist, and how well each has been tested, are catalogued on the [project inventory](project-inventory.md).

## How it works

| Environment | What runs | Practical consequence |
| ----------- | --------- | --------------------- |
| Game sandbox | Code or scripts inside a particular game | Host permissions, APIs and lifecycle apply. [CivHomebrew](https://github.com/XITRIX/CivHomebrew) targets Civilization VII and its own application format; ordinary NX NRO compatibility must not be assumed. |
| Native system-process session | Native code via a compatible exploit chain | [ns2jb](https://pepsipu.com/blog/2026-10-switch2-jailbreak-ns2jb/) describes a tethered chain demonstrated on 23.0.1; it does not establish kernel control, and 23.5.0 patched the system-level escape. |
| Switch compatibility | Original Switch software in the compatibility environment | See [compatibility mode](../software/compatibility.md); this is not native OZ execution. |

### The first public chain

[Reverse-engineered] The ns2jb chain published on October 21, 2026 runs in four stages:

1. **A Switch game.** SmileBASIC 4 has a 32-bit integer overflow in its interpreter's array-remove operation that yields arbitrary memory reads and writes, and with them code execution inside the game.
2. **The Switch 2 filesystem service.** A heap buffer overflow in `nn::fs::PathNormalizer::Normalize` turns the game's access into privileged memory reads and writes, and with them filesystem access.
3. **The Switch 2 program loader.** The exploit replaces the filesystem object that serves executable code, so `ldr` launches a custom native program. Nintendo's loader still validates signed metadata; `fs` supplies the executable content, and after compromising `fs` the exploit substitutes the content while preserving valid signed metadata.
4. **Native system-module execution.** AArch64 code runs with `ro` permissions, and `fs` is patched and inspected.

#### How the interpreter overflow works

[Reverse-engineered] The interpreter's `REMOVE` command checks `position + count <= array.length` before shifting the remaining elements left, but the sum is a signed 32-bit value and can wrap to a negative number, which still passes the check.

On a string array `R` of length four, `REMOVE R,&H60000000,&H3FFFFFFF` makes `position + count` wrap to `0x9fffffff`. As a negative signed 32-bit integer that is less than `array.length`, so the check passes, and the multiplications by the element size wrap too. The interpreter ends up performing `memmove(backing, backing - 8, 32)`, pulling the eight bytes that precede the array into it.

That is the useful part: the eight bytes preceding any SmileBASIC array hold a pointer to the array itself. After the `REMOVE`, treating `R[0]` as a string therefore treats the array's backing store as string data, which is how a memory read and write primitive is obtained.

The command does report an out-of-memory error, but that does not matter, because the copy has already happened.

#### How the filesystem overflow works

[Reverse-engineered] A path containing a parent-directory component with a backslash, such as `/..\`, reaches a helper that allocates a temporary heap buffer of `0x301` bytes and copies the path into it without checking the length. SwitchBrew's system flaws page describes the same defect from the service side: when path normalization needs to replace a parent directory path, the temporary buffer is fixed at `0x301` bytes while the source path can be up to `PointerBufferSize`, which in `fs` is `0x800` bytes. That is a heap buffer overflow of up to `0x4FF` bytes.

Two conditions gate the copy. The underlying filesystem must allow backslashes in paths (`PathFlags_AllowBackslash`), and in retail only the save-data filesystem does. And exploiting the bug needs a suitable victim allocation and specific heap grooming, which is what makes the chain as fragile as it is; the published research used the `nn::fssystem::BlockCacheBufferedStorage` cache entries as the victim object.

[Reverse-engineered] Once the overflow lands, the process has "FullPermission" and can perform privileged filesystem actions, including dumping internal storage and the ordinary system-module code.

[Reverse-engineered] This is not a signature-forging or a kernel exploit. It is a trust-boundary failure between two privileged system services.

[Reported] The chain also explains why original Switch research mattered: the filesystem API, executable loader, signed metadata and service model were similar enough that Switch 1 reverse engineering made the Switch 2 chain much faster to construct.

[Reported] The original write-up describes the research method as well as the result. The author built a harness around a Switch 1 and a Switch 2 so that the analysis could run unattended; the hardware included JetKVM devices that send controller inputs over USB and return video over HDMI, and a button press was needed to restart the console when it crashed. A large language model was given the goal of jailbreaking the console, and its route through the WebKit browser engine was abandoned in favour of the interpreter overflow described above.

### Why the chain is fragile

[Reverse-engineered] The maintained kurumuz fork of ns2jb documents that the filesystem exploit is extremely timing-sensitive, with success rates as low as roughly 20%. It also documents two observed memory layouts, including a change that followed a microSD Express support update. A layout change can invalidate a working chain even when the console's features have not changed.

[Reverse-engineered] The published demo requires SmileBASIC 4.4.9, system software 23.0.1, Node.js 22 or newer on macOS or Linux, and over 20 MiB free in the SmileBASIC save. The host computer and console must be on the same reachable network; the connection uses port 39380.

The demo runs two small native functions inside `fs`, checks their results and restores the original instructions. The console-side steps are:

```basic
LOAD "TXT:NS2"
RUN
REMOVE NSR,&H60000000,&H3FFFFFFF
```

That `REMOVE` is the interpreter overflow described above, applied to a different array: the wrapped `position + count` is what produces the read and write primitive, and the "Out of memory" message it prints is expected rather than a failure. It is entered only once per application process. The program then runs the `V=START(...)` line the host prints. Restarting the console restores the original `ro` process, so the demo is not persistent and does not install custom firmware.

[Reported] The demo was physically tested on October 21, 2026: both instruction challenges, the restoration and the cleanup passed. Acquisition is timing-sensitive; the run described passed on the third fresh boot, and the first two attempts stopped at validation checks and cleaned up after themselves.

[Reverse-engineered] The demo's native helper is written in Zig 0.15.2 and built with the engine's own script.

### Other entry points

[Reverse-engineered] There is more than one way to execute code, and the routes give different levels of access:

| Entry point | Mechanism | Capabilities |
| ----------- | --------- | ------------ |
| [CivSwitch](https://github.com/Gezine/CivSwitch) | Civilization VII chat XSS and JavaScript-engine exploitation | Game-level code, a network loader, a save-based launcher |
| [Balatro RCE](https://github.com/Luminger/balatro-switch-rce) | The LÖVE runtime loads save files ahead of the game's original Lua files | A Lua console, native function calls within the game, FTP |
| [Artemis launcher](https://github.com/alula/switch-artemis-launcher) | Crafted visual-novel save data | Lua execution and HTTP-based tools |
| [ns2jb](https://gitlab.com/pepsipu/ns2jb) | Integer overflow followed by the system filesystem exploit | Privileged native execution on 23.0.1 |

[Reverse-engineered] CivSwitch is significant because it does not depend on keeping an already-installed, delisted eShop application. It uses the original Switch physical cartridge of Civilization VII, version 1.0.0, and its first stage uses JavaScript running inside the game's embedded interface. Originally a Raspberry Pi Pico acted as a USB keyboard to type the payload into multiplayer chat; [CivSwitchLDN](https://github.com/ItsNerisma/CivSwitchLDN) delivers it over local wireless instead.

[Reverse-engineered] CivSwitch installs a crafted save that starts the loader when the game loads it. This persists across restarts as a game save, not as system-wide custom firmware.

[Reverse-engineered] Nintendo has delisted SmileBASIC 4, which affects the availability of the first chain's entry point.

### The 23.5.0 patch

[Reverse-engineered] Firmware 23.5.0 fixes the path-normalization overflow described above. The temporary buffer is now allocated with the size of the source path, up to a maximum of `0x600` bytes, so the copy can no longer overrun it. SwitchBrew's system flaws page records the flaw as fixed in 23.5.0, with the result described as an `fs`-sysmodule heap buffer overflow allowing arbitrary read and write.

Because the flaw is in `fs` itself, the fix is not a per-title patch. The game-level entry points that reach it survive, because the games' own vulnerabilities are unaffected; what 23.5.0 removes is the system's ability to be escaped from.

[Reported] The same conclusion was reached independently by the community through reverse engineering, relayed by Nintendo Life, GameGaz and IT之家, and attributed to the Alula and OatmealDome communities. This is a community conclusion rather than a vendor statement, although in this case the technical fix is independently documented.

The practical result is a split:

- On 23.0.1, game-contained execution could escalate to a privileged system process.
- On 23.5.0 and later, the known entry points are confined to the game process they started in.

Game-sandbox homebrew remains usable, and the [project inventory](project-inventory.md) records which projects still work on current firmware. The [firmware page](../software/firmware.md) has the dates and the reasoning.

### What native execution does and does not give you

[Reverse-engineered] The ns2jb chain runs AArch64 code with `ro` permissions inside a system process. That is more than a game sandbox and less than a custom firmware. The levels are separated on the [security page](../software/security.md).

[Reported] The ns2jb author describes an initial game exploit, a filesystem-service vulnerability and a loader trust-boundary attack, ending in code running with system-process privileges. The original write-up's 23.0.1 claim is dated 21 October 2026; do not infer support for a later firmware from it.

[Reported] [HBLauncher](https://github.com/ravensclawx/HBLauncher) offers an experimental native application interface and examples; use its current build and setup instructions, not NX instructions by analogy. Its public issue reports include a hardware failure on firmware 23.0.1 where the launcher failed to resolve `GetNpadState` despite a successful bootstrap and host communication, which is the kind of failure a development guide should anticipate.

## Technical details

Start with the project's smallest example. Build it with the versions specified by that project, preserve the build log, then verify startup, input, clean exit and a repeat launch before adding graphics or networking. Record whether each result was built, emulator-tested or tested on physical hardware.

Capture the first error and symbol name when a launcher cannot resolve an import; confirm that the launcher, application ABI and firmware combination match before changing application code.

For an NX port, isolate platform-facing input, rendering, audio, storage and lifecycle code behind interfaces. Select implementations after establishing the target environment. The [comparison](../engineering/nx-comparison.md) gives the decision points.

For a game-sandbox program, remember that it is the host game's process: its permissions, available libraries, lifecycle and input are constrained by the host, and a program that works there has not established native compatibility. See [compatibility mode](../software/compatibility.md) for the same reasoning applied to compatibility-mode titles.

## Limits and risks

A process exploit does not automatically provide unrestricted service access, kernel control, persistence or a stable graphics API. Exploit attempts and firmware changes can lose access or data; keep independent backups and follow the original project's instructions.

There is no publicly released Switch 2 equivalent of the original Switch's fusee-gelee orhekate boot compromises, and no unrestricted `.nro` loader. A port that assumes either has no target.

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
