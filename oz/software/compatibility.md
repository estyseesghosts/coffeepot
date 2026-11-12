# Switch compatibility mode

Switch 2 can run many original Switch games using a compatibility environment. This makes existing games useful, but does not turn an original Switch executable into a native Switch 2 application or grant it native application permissions.

## Behavior

[Reverse-engineered] SwitchBrew describes compatibility modules named `nnCompatTrampoline`, `nnCompatThin` and `nnCompat`. They adapt selected interfaces and can apply title-specific behavior. Compatibility is therefore more than simply running the same CPU instructions on a newer processor.

Nintendo does not generally replace the original game binary. Instead it inserts compatibility libraries into the game process and selectively overrides functions through dynamic linking. Most SDK functions remain compatible enough that they need no replacement; the overrides are mostly in graphics, where the calls have to be translated to the new GPU, but there are also overrides for timing, saves and audio.

## Technical details

### Program metadata changes

[Reverse-engineered] Two changes are made to the game process's [NPDM](../glossary.md#npdm), and both widen the game's permissions without touching the game's own file:

| Permission | Original Switch NPDM | Under compatibility |
| ---------- | -------------------- | ------------------ |
| System calls | — | The game gains access to SVC `0x54` and SVC `0x80`, even though neither is in its NPDM |
| Core mask | `0x7` (cores 0–2) | `0x3F` (cores 0–5) |

[Reverse-engineered] There are no known differences in service access control or filesystem permissions.

Note what the core mask change does and does not mean. The process is allowed six cores, but the game does not get to use six cores: the `nnCompatThin` wrapper intercepts `nn::os::GetThreadAvailableCoreMask()` and ANDs the resolved mask with `7` before returning it. The game therefore still believes it has three cores, and it schedules onto three, even though the system would allow six.

### Module structure

[Reverse-engineered] The three compatibility binaries are compiled with pointer authentication enabled.

- The dynamic loader (`rtld`) is identical to the original Switch's, except for an extra page at the end of `.bss` that holds the compatibility parameter.
- The compatibility modules are mapped between the main module and the `(sub)sdk*` modules.
- The existing modules are not modified directly.

### The compatibility parameter

[Reverse-engineered] The system places an additional information block in the `ProgramArgument` parameter of the process. This block carries per-game overrides that customize the compatibility hooks.

To retrieve it, `nnCompatThin` intercepts `nnosInitialize` and takes the pointer from there. An ordinary game never reads the program parameter, so it does not notice that it was passed one.

### Per-title configuration

[Reverse-engineered] The system's `ApplicationCompatibilityInfo` data contains an `ApplicationCompatibilityInfo.json` file. Its `records` array holds one object per title, each with:

- `application_id`, the title's application identifier in lowercase hexadecimal.
- `target_max_version`, normally the maximum 32-bit value.
- `application_layer_info`, an object of numeric fields where only the fields the entry needs are present, such as `graphics_config` and `misc_config`.

The data from this file is what is passed to the compatibility parameter, so it is how a title's overrides are selected.

### Game-specific patches

[Reverse-engineered] `nnCompatTrampoline` contains game-specific patches for at least five games. They follow the pattern `nnCompatPatchFor*****`, where the last five characters are the game's code, for example `nnCompatPatchForAyl8a` for Metroid: Dread.

This is the clearest evidence that compatibility is not entirely generic: a title can need behaviour that no general rule would produce.

### Other adaptations

[Reverse-engineered] The documented adaptations include:

| Adaptation | Why it exists |
| ---------- | ------------- |
| Presenting three cores to older games | Keeps the scheduling behaviour a Switch 1 title was written for |
| Adjusting tick and timestamp frequency handling | Keeps timer and timing assumptions from breaking |
| Translating graphics functions | Bridges Maxwell-era calls to the newer hardware |
| Special-casing save access | Accounts for synchronization differences |
| Changing selected audio behaviour | Adapts to different devices and runtime rules |

[Reverse-engineered] One detail is worth stating precisely. The original Switch's GPU timestamp clock runs at 19.2 MHz and the Switch 2's at 31.25 MHz, and the compatibility hooks account for the difference. A program that derives elapsed time by counting raw ticks will get the wrong answer, and the ratio between the two rates is large enough to matter.

## Homebrew notes

Treat a homebrew program inside an original game as that game's process: its permissions, available libraries, lifecycle and input are constrained by the host. A program that works there has not established native OZ compatibility.

Test native and compatibility paths separately, including suspend, resume, timing and save operations. The timing difference above is the one most likely to produce a bug that looks like a logic error.

Do not rely on the widened process permissions. SVC `0x54` and `0x80` are available to a compatibility-mode game, but that is a property of the compatibility environment, not a statement about what a native program may do.

## Sources

- [SwitchBrew compatibility mode](https://switchbrew.org/wiki/Switch_2:_Compatibility_Mode) — modules, NPDM adjustments, the compatibility parameter and the per-title configuration.
- [ns2jb compatibility affinity reference](https://gitlab.com/kurumuz/ns2jb) — the `nnCompatThin` core-mask wrapper, captured from application modules.
- [NVIDIA Switch 2 announcement](https://blogs.nvidia.com/blog/nintendo-switch-2-leveled-up-with-nvidia-ai-powered-dlss-and-4k-gaming/) — the platform's graphics features, which the compatibility layer translates to.
