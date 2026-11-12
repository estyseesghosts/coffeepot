# ns2jb: Switch 2 software jailbreak

ns2jb is a tethered software exploit chain for Nintendo Switch 2. Researcher Sammy Hajhamid (pepsipu) demonstrated it on system software 23.0.1 using the original Switch game SmileBASIC 4 as an entry point. It progresses from control inside the game to filesystem-service access and then to native code running with system-process permissions. It does not establish kernel control or install persistent custom firmware; the filesystem flaw used by the published chain was fixed in 23.5.0. [Reverse-engineered: pepsipu; SwitchBrew]

This page explains the chain and what a developer can do after obtaining a session. For other execution environments, see [homebrew environments](state-and-workflow.md); for the underlying process and security model, see [system software](../software/system.md) and [security](../software/security.md).

## What you need

The original public demonstration and the researcher's explanation target **Switch 2 firmware 23.0.1**. The saved original demonstration README specifies SmileBASIC 4.4.9, Node.js 22 or later on macOS or Linux, more than 20 MiB free in the game's save, and a computer and console that can reach each other over the network. The console runs the Switch 1 game through [compatibility mode](../software/compatibility.md). [Documented: original ns2jb README]

Implementations and entry points differ. The [kurumuz fork](https://gitlab.com/kurumuz/ns2jb/-/blob/main/README.md) describes support for two observed memory layouts and a demo-version option; consult that project's exact README for its prerequisites, host networking, cleanup and revision. A separate [CivSwitch-based project](https://github.com/byizmysh/IZCIVSwitch/blob/main/DISCLAIMER.md) describes reusing the known filesystem stage from a different game entry. That does not make the patched filesystem flaw work on 23.5.0. Do not infer support for untested firmware or a game version from a different fork.

Use a console and content you control, keep an independent backup of the game save, and follow the selected project's instructions. The chain changes live process memory and can fail. No ns2jb run in this wiki has been independently reproduced by Coffeepot.

## What it does

| Stage reached | What the researcher demonstrated | What it does not establish |
| ------------- | -------------------------------- | -------------------------- |
| SmileBASIC process | Read and write memory in the game process, then construct native execution using its existing code. | Permission to use every system service or modify another process. |
| `fs` filesystem service | Read and write memory inside `fs` and obtain a more privileged filesystem context. | Arbitrary kernel execution or automatic native code execution in `fs`. |
| Ordinary system process | Substitute an executable while preserving valid program metadata; run native code with the selected program's permissions. | New permissions beyond the selected process or forged Nintendo signatures. |
| `fs` native code | Use a specially launched process and a handle to map and modify `fs` code. | A persistent boot modification or a compromise of all built-in modules. |

These are stages of the researcher's chain, not interchangeable names for the same access. The author reports code execution as ordinary system modules and inside `fs`; a particular released demo or downstream tool may implement only part of that capability. [Reverse-engineered: pepsipu]

## How it works

The first bug gives the attacker control *within a game*. The second crosses into a privileged filesystem service. Once `fs` is compromised, the researcher exploits the trust relationship between that service and the program loader to start a native helper with an existing signed process policy. Finally, the helper uses its own allowed process-memory operations to work with `fs`. A computer coordinates the demonstrated chain; restarting removes its live process modifications.

```text
SmileBASIC on Switch 2
  game-process memory access and existing-code execution
        ↓ filesystem request
fs: memory corruption → fs-process memory access
        ↓ compromised fs supplies loader's executable filesystem
ldr: valid metadata + substituted executable
        ↓
custom native code with selected ordinary system-module policy
        ↓ process handle and mapping, subject to that policy
fs code inspection and modification
```

### 1. From a game command to game-process memory access

[Reverse-engineered: pepsipu] SmileBASIC's interpreter removes array elements by checking a position plus a count against the array length and then moving the remaining data. The check uses 32-bit arithmetic. With suitable values, the sum wraps into a negative signed value and passes the check; subsequent element-size arithmetic wraps as well. In the researcher's example the move copies eight bytes from immediately before an array into its contents, even though the command later reports an out-of-memory error.

Those preceding bytes contain a pointer to the array. When the copied value is interpreted through a string-array element, a pointer occupies a field used as a string length. The resulting oversized view permits reads and writes beyond the intended string inside the **game's process**. The researcher then used existing instructions in that process to assemble native operations; see the [original write-up](https://pepsipu.com/blog/2026-10-switch2-jailbreak-ns2jb/) for the worked arithmetic. This stage alone does not bypass filesystem or kernel permissions.

### 2. From the game into the filesystem service

[Reverse-engineered: pepsipu; SwitchBrew] A filesystem request containing a parent-directory component and a backslash reaches a path-normalization helper in `fs`. On affected software, the helper allocates a `0x301`-byte temporary buffer but can receive a longer path. A copy can therefore overwrite adjacent heap data. The filesystem and path flags must allow the relevant backslash behavior; the published analysis identifies save data as the retail filesystem satisfying that condition. A callable request, the right path conditions and a useful neighboring allocation are all required.

The researcher used a cache entry associated with `BlockCacheBufferedStorage` as the neighboring object. Its metadata includes a pointer and size for cached data. Corrupting a suitable entry makes subsequent file operations follow an unintended pointer, yielding reads and writes in the `fs` process. The published account describes using those reads and writes to alter the filesystem connection's stored identity so that filesystem operations receive a more privileged policy. This is **filesystem-service access**, not a change to the game's kernel identity or proof that all Horizon services become accessible.

The overflow and heap arrangement are fragile. The researcher describes using one corrupted cache entry to adjust another, reducing the need to retrigger the overflow for every pointer change. The [fork](https://gitlab.com/kurumuz/ns2jb/-/blob/main/README.md) describes timing sensitivity and two observed layouts; these are implementation and environment observations, not a guarantee of success on every console.

### 3. From `fs` memory access to a native helper

Normally the [program loader](../glossary.md#ldr), `ldr`, obtains an executable filesystem and associated verification information through `fs`. A Nintendo-signed program descriptor (**[NPDM](../glossary.md#npdm)**) constrains the program's privileges. In the researcher's description, `ldr` verifies the provided content-header hash and signature and checks the signed metadata. [Reverse-engineered: pepsipu]

The important boundary is **which component supplies the information and executable objects**. After gaining read/write access in `fs`, the chain substitutes the executable filesystem object served to the loader while preserving valid header verification information and signed `main.npdm`. The substitute contains the valid metadata and a replacement `main` executable. The researcher describes swapping object-table entries to preserve reference counts, and controlling when the loader's pending requests are serviced so the substitution occurs at the required point.

The signature checks are not broken mathematically. The attack controls a service that the loader relies on, so a successful metadata check can be paired with different executable content. The resulting code runs under the permissions of the selected signed program descriptor; it does not receive unlimited privileges merely because its instructions are custom. The original write-up describes launching code using an ordinary system module's metadata, including `ro`.

### 4. From the helper into `fs`

[Reverse-engineered: pepsipu] The `ro` policy permits particular process-memory operations. Those operations require a handle to the target process. The researcher used existing `fs` memory access and an event-notifier interface to obtain a handle referring to `fs`, then passed it to the custom `ro`-policy process. That process could map a writable view of `fs` code and use it for inspection and modification.

This last step is why **memory read/write inside `fs`**, **native execution as a `ro`-policy process**, and **native code execution inside `fs`** are distinct claims. A helper running under one process's permissions does not automatically inherit those of the other. The specific handle and mapping mechanism is described in the [author's analysis](https://pepsipu.com/blog/2026-10-switch2-jailbreak-ns2jb/); it is not a generic application API.

## What you can do with a session

The following are capabilities demonstrated or described by their authors. Each tool has its own entry, firmware, application and connection requirements; a successful ns2jb demo is not a promise that every tool attaches to every fork.

| Goal | Example and limits |
| ---- | ------------------ |
| Investigate filesystem and system behavior | The author describes privileged filesystem operations and obtaining system-module code for analysis. A compatible fork's probe checks its own native execution and restores modified instructions. |
| Run native experiments | [HBLauncher](https://github.com/ravensclawx/HBLauncher) describes a launcher, application interface and graphics examples. Check its exact session and firmware prerequisites; it is a separate project, not part of every ns2jb demo. |
| Back up or work with saves | [ns2-savekit](https://github.com/dougchansan/ns2-savekit) describes save access through a compatible session. Verify the intended title and backup before any write. |
| Inspect game content | [OunceCake](https://github.com/misshexa66/OunceCake) describes exporting decrypted executable and resource filesystems via the console's services. It does not by itself establish unrestricted offline decryption of arbitrary packages. |
| Test game modifications | [ns2jb-modloader](https://github.com/SebaaMG/ns2jb-modloader) describes resource and executable overlays through a compatible filesystem session; its supported titles and features are project-specific. |

Consult the [project inventory](project-inventory.md) for further context. Some tools act on a held session while the original game and host program remain active; closing either can invalidate their connection. A save backup or exported file may persist after the session ends even though the exploit's live process modifications do not.

## Technical details

### Why the demo is not the whole research result

The saved original demo README says its standard run executes two small functions inside `fs`, checks their results, restores the modified instructions and cleans up. It does not install custom firmware or persistence. The original research describes the broader chain and outcomes; downstream launchers and tools add their own interfaces. Read the README for the exact implementation being used instead of assuming every package exposes a general launcher or a permanent filesystem endpoint. [Documented: original ns2jb README]

The saved source tree's `engine/switch2/pipeline.cjs` organizes its demo into console qualification, filesystem memory acquisition, native-helper startup, and an `fs` instruction check and cleanup. The local research copy has uncommitted changes, so this observation describes that saved copy, not a pinned upstream release. [Documented: saved `pipeline.cjs`]

### Firmware and patch scope

[Reverse-engineered: SwitchBrew] Before the fix, the filesystem path-normalization issue used a `0x301`-byte temporary buffer with a larger possible input; SwitchBrew describes an `fs` pointer-buffer size of `0x800`. On 23.5.0, it reports a changed allocation based on the source path, up to `0x600`, and lists the flaw as fixed. The author demonstrated the chain on 23.0.1; the 23.5.0 fix blocks this particular escalation, not all possible game entries or all future vulnerabilities. See [firmware](../software/firmware.md).

### Diagnosing a run

| Symptom | First distinction to make |
| ------- | ------------------------- |
| Host and console do not connect | Check the selected project's network prerequisites, reachable host address and whether the host is listening. This is not evidence that the memory corruption succeeded or failed. |
| The game entry reports an expected error | The researcher reports an out-of-memory message *after* the interpreter's invalid copy. Check the implementation's instructions; an error message alone is not proof of a successful later stage. |
| `fs` acquisition fails or the console hangs | The fork reports timing and layout sensitivity. Preserve logs, follow its restart and cleanup instructions, and distinguish acquisition from loader or application failure. |
| Helper starts but a tool fails | Check whether the implementation exposes the required held session, app format, imports and permissions. A completed helper stage is not proof of a usable graphics or save API. |
| A request times out | Treat its outcome as unknown. Check the project's cleanup guidance before restarting or reusing resources; do not automatically replay a write. |

## Limits and risks

- **Tethered and temporary.** The demonstrated workflow coordinates with a host computer and modifies running processes. A console restart restores the original process state; the demo is not a cold-boot or persistent custom-firmware installation.
- **No kernel or boot claim.** The published chain does not establish a kernel exploit, secure-monitor control or a Boot ROM exploit. A system-process policy can be powerful without being unrestricted.
- **Firmware-specific.** The known `fs` path was fixed in 23.5.0. A game entry may remain usable within its own process, but needs a separate compatible escalation to reproduce the privileged outcome.
- **Live writes and data.** The chain can crash the game or console; failed cleanup, save modification and filesystem writes can lose data. Back up independently and follow the selected project's recovery instructions. Avoid repeating operations whose outcome is uncertain.
- **Do not publish private data.** Logs, memory captures, saves and exports can contain personal or console-specific material. Inspect and sanitize them before sharing.

## Sources

- [Pepsipu: jailbreaking the Switch 2](https://pepsipu.com/blog/2026-10-switch2-jailbreak-ns2jb/) — original 21 October 2026 technical account, stages, trust boundaries and demonstrated capabilities.
- [Original ns2jb repository](https://gitlab.com/pepsipu/ns2jb) — released demonstration and its instructions; the saved README and modified research tree were also inspected locally.
- [kurumuz ns2jb README](https://gitlab.com/kurumuz/ns2jb/-/blob/main/README.md) — fork-specific requirements, observed memory layouts, debugging and cleanup.
- [SwitchBrew system flaws](https://switchbrew.org/wiki/Switch_System_Flaws) — filesystem normalization flaw, preconditions and reported 23.5.0 fix.
- [HBLauncher](https://github.com/ravensclawx/HBLauncher), [ns2-savekit](https://github.com/dougchansan/ns2-savekit), [OunceCake](https://github.com/misshexa66/OunceCake), [ns2jb-modloader](https://github.com/SebaaMG/ns2jb-modloader) — separate projects illustrating possible session uses and their requirements.
