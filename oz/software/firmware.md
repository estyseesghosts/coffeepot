# Firmware and versions

Switch 2's firmware is versioned separately from the original Switch's. The version number matters more than usual here, because the first public exploit chain worked on one version and stopped working on the next. This page records the version history and what changed.

## Where it fits

Firmware starts the boot chain and then Horizon; the chain itself is described on the [security page](security.md) and the operating system on the [system page](system.md). What the versions mean for homebrew is on the [homebrew page](../homebrew/state-and-workflow.md).

## How it works

The installed system version and the SDK version used to build a title are different things. Both can affect behavior: the system supplies services and enforcement, while the title contains code built against a particular SDK.

## Technical details

### Release history

[Reverse-engineered] SwitchBrew's system version table:

| System version | Release date | SDK version | Notes |
| -------------- | ------------ | ----------- | ----- |
| 19.0.0 | Before console release | 19.3.0 | |
| 19.1.0 | On release day cartridges | | |
| 20.1.0 | On SMP and Jamboree TV cartridges | | |
| 20.1.1 | June 4, 2025 (UTC) | 20.5.4 | |
| 20.1.5 | June 19, 2025 (UTC) | | |
| 20.2.0 | July 15, 2025 (UTC) | | |
| 20.3.0 | July 29, 2025 (UTC) | | |
| 20.4.0 | September 2, 2025 (UTC) | | |
| 20.5.0 | September 30, 2025 (UTC) | | |
| 21.0.0 | November 11, 2025 (UTC) | | Applets switch to vtable and function pointer PAC |
| 21.0.1 | November 25, 2025 (UTC) | | |
| 21.1.0 | December 9, 2025 (UTC) | | |
| 21.2.0 | January 13, 2026 (UTC) | | |
| 22.0.0 | March 17, 2026 (UTC) | | |
| 22.1.0 | April 7, 2026 (UTC) | | |
| 22.5.0 | June 16, 2026 (UTC) | | |
| 23.0.0 | September 10, 2026 (UTC) | | |
| 23.0.1 | October 1, 2026 (UTC) | | The version the first public exploit chain was demonstrated on |
| 23.5.0 | November 10, 2026 (UTC) | | The version that patched the system-level exploit |

The source table also carries a build-date column, which is empty for every version it lists.

### What changed in 23.5.0

[Reverse-engineered] The change is documented. 23.5.0 fixes the heap buffer overflow in `nn::fs::PathNormalizer::Normalize` in the filesystem service: the temporary buffer used when path normalization needs to replace a parent directory path is now allocated with the size of the source path, up to a maximum of `0x600` bytes, instead of a fixed `0x301` bytes. Before the fix, a path arriving in a HipcPointer buffer of `0x800` bytes could overrun that buffer by up to `0x4FF` bytes, giving arbitrary read and write inside the `fs` system module.

This is the flaw the first public Switch 2 exploit chain used to escalate out of a game and into a privileged system process, so 23.5.0 removes that escalation path.

[Reported] Nintendo did not publish a patch note describing the change. Community reverse engineering and reporting in November 2026 reached the same conclusion independently, relayed by Nintendo Life, GameGaz and IT之家, and attributed to the Alula and OatmealDome communities. Reporting dates the update to November 9; SwitchBrew's version table dates it to November 10, 2026 (UTC).

The game-level vulnerabilities themselves survive. The distinction that matters in practice:

- On 23.0.1, the published chain starting in a game escalated to a privileged system process.
- On 23.5.0, the documented `fs` escalation is patched. A game entry alone still runs with its host process permissions; other chains and later versions require separate verification.

> Unknown: Nintendo's own account of what 23.5.0 changed has not been published, so the intent behind the fix is inferred from the change itself rather than documented.

### Version-dependent behaviour

[Reverse-engineered] Several protections change with the firmware version, so a test performed on one version may not carry to another:

- Execute-only memory for applets changed at 21.0.0, and `offlineWebGen0` only gained it at 23.0.0.
- Applets gained pointer authentication on vtable and function pointers at 21.0.0.
- Execute-only memory was controlled through the [NPDM](../glossary.md#npdm) on 19.0.0 and became a per-module header field at 20.0.0.

The full list is on the [security page](security.md).

## Homebrew notes

Record the firmware version with every result, along with the console's SDK-facing behaviour. A measurement or a failure report without a version is not reproducible, and the 23.5.0 patch shows how a change in one service can invalidate a chain that depends on it.

Do not carry an offset, a service command number or a memory layout from one firmware version to another without re-verifying it. The exploit that was patched relied on a heap layout that also changed under an unrelated microSD Express support update, so "same firmware line" does not guarantee "same layout".

## Sources

- [SwitchBrew system versions](https://switchbrew.org/wiki/Switch_2:_System_Versions) — the release table and dates above.
- [SwitchBrew system flaws](https://switchbrew.org/wiki/Switch_System_Flaws) — the `fs` path-normalization overflow and its 23.5.0 fix.
- [SwitchBrew security mitigations](https://switchbrew.org/wiki/Security_Mitigations) — the version-dependent protections.
- [GameGaz firmware analysis](https://gamegaz.com/2026111046509/) — the community confirmation, in Japanese, that the system exploit was patched.
