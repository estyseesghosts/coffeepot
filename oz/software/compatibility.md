# Switch compatibility mode

Switch 2 can run many original Switch games using a compatibility environment. This makes existing games useful, but does not turn an original Switch executable into a native Switch 2 application or grant it native application permissions.

## Behavior

[Reverse-engineered] SwitchBrew describes compatibility modules named `nnCompatTrampoline`, `nnCompatThin` and `nnCompat`. They adapt selected interfaces and can apply title-specific behavior. Compatibility is therefore more than simply running the same CPU instructions on a newer processor.

## Technical details

The documented adaptations include timing, CPU scheduling assumptions, graphics, audio and save-related behavior. Timing hooks account for differences between the original system tick and the newer system; do not derive elapsed time by assuming a fixed raw tick frequency. Game-specific compatibility configuration means a behavior observed in one game need not apply to another. See the source for individual hooks and version details.

## Homebrew notes

Treat a homebrew program inside an original game as that game's process: its permissions, available libraries, lifecycle and input are constrained by the host. A program that works there has not established native OZ compatibility. Test native and compatibility paths separately, including suspend, resume, timing and save operations.

## Sources

- [SwitchBrew compatibility mode](https://switchbrew.org/wiki/Switch_2:_Compatibility_Mode) — modules and observed adaptations.
- [Nintendo compatibility information](https://www.nintendo.com/us/gaming-systems/switch-2/transfer-guide/compatible-games/) — consumer compatibility context.
