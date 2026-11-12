# Storage and saves on Switch homebrew

This page is for a developer who ships files with a program, reads assets from the SD card, or saves progress. The short answer: RomFS, the SD card and system save data are three different things with different rules; for a homebrew save on the SD card, use an explicit, versioned, checksummed format with two generation slots so a failed or interrupted write can never destroy the last good save, and test every failure point on the host before trusting it on a console.

## Before you start

- Toolchain: devkitA64 r30-1, `libnx` 4.12.0-1 ([source versions](source-versions.md)). Header facts are `[documented]` from `libnx` at `b156b4774fb2df7b93402169509266da7547fc4c`.
- Verification: the save format and slot logic are in `nx/engineering/examples/common/save_format.c`; **1,184 host-side checks pass** (`nx/tests/run.sh`: round trip, every single-bit flip rejected, every truncation length of an interrupted second save, silent torn writes, both slots damaged, the coin-persistence regression). The first game's self-test ran the real SD path in **Eden** (save, reload, second save into the other slot all pass). **Not run on a console**, and no power-loss test has been run anywhere; the durability of the SD card and of `fsync` is unverified.
- Where each kind of save lives is on [firmware](../software/firmware.md).

## Three storages

| Storage | Use | Property |
| ------- | --- | -------- |
| Embedded RomFS (`romfs:/`) | Shaders, immutable assets, defaults | Packed into the NRO or title; read-only; `romfsInit()` / `romfsExit()` |
| `sdmc:/…` | Portable saves, logs, mods, external assets | Ordinary SD filesystem behaviour and its failure risks |
| Title save data | A title's own system save storage | Uses filesystem services and an explicit commit |

For system save data, `fsdevMountSaveData()` mounts it and `fsdevCommitDevice()` **must be called after any save-data write**, ideally after each file close; unmounting does not commit `[documented]` (`fs_dev.h`; `switch-examples` `fs/save`). An arbitrary homebrew NRO is not guaranteed permission to mount or modify a particular game's save data; that and portable SD saves are separate topics. The rest of this page is about SD saves.

## A save that cannot lose the last good state

The first-game program originally saved position and a coin total but not **which coins were collected**, so after a restart every coin respawned and could be collected again. Its replacement:

| Field | Purpose |
| ----- | ------- |
| Magic `CPSV` | Identify the file |
| Version | Reject incompatible layouts |
| Payload length | Reject truncated or oversized data |
| Generation counter | Pick the newest valid save |
| Position (fixed point, 1/256 pixel) | Restore movement |
| Collected-coin bitset (880 map cells = 110 bytes) | Keep collected coins collected |
| Score | Cross-checked against the bitset |
| CRC-32 | Detect accidental corruption |

All fields are little-endian at fixed offsets (138 bytes total); nothing depends on compiler struct padding. The loader checks size, magic, version, length, CRC, then the meaning (position inside the map, bits only on coin cells, score equal to the bit count) and returns the valid slot with the higher generation.

Saving writes to the slot that does **not** hold the newest valid save, so the previous save is never touched. The write path checks `fwrite`, `fflush`, `fsync` and `fclose`, and reports failure if any step fails. A torn write damages only the inactive slot, which then fails its CRC at the next load. There are no renames, so the earlier question whether `rename` over an existing file works on the SD card through `libnx` no longer matters for this example.

```c
// slotWrite: create/truncate, write, flush, sync, close; non-zero if ANY step failed
FILE* f = fopen(path, "wb");
if (!f) return -1;
int rc = 0;
if (fwrite(buf, 1, len, f) != len) rc = -1;
if (fflush(f) != 0) rc = -1;
if (fsync(fileno(f)) != 0) rc = -1;
if (fclose(f) != 0) rc = -1;
return rc;
```

What a checksum proves: the bytes are the ones written, against accidental corruption. It does not prove the newest state reached the card (durability), and it does not authenticate the file against deliberate edits.

### What remains unverified

- Whether `fsync` through `libnx` forces data to the SD card, and whether directory entries survive power loss. The sequence is defensible either way: a half-written inactive slot is rejected and the older generation loads.
- Whether `fflush` or `fclose` can succeed while the data is lost; no source says.

## Tests

| Layer | What | Where |
| ----- | ---- | ----- |
| Host | Serialization, validation, generation choice, injected failure at every byte count and at every write/flush/close outcome, silent torn writes | `nx/tests/test_logic.c` |
| Emulator | The real file path: save, reload, collected coin stays collected, second save goes to the other slot | `first-game` self-test in Eden |
| Console | Same, plus an isolated test directory and cutting power mid-write (a separate durability test, not software fault injection) | Not done |

## Asset streaming from the SD card

Read in bounded jobs off the game thread, with an event for completion; keep file opens off the frame ([memory](memory-management.md#asset-loading)). SD cards differ in speed: report load-time distributions on a fast and a slow card, and small files against large sequential reads.

## Pitfalls

- **Saving only part of the state.** The coin bug: a restored game rebuilt coins from the map.
- **Ignoring return values.** Check every write, flush, sync and close.
- **Deleting the old save before the new one is committed.** Write elsewhere, then select.
- **Struct dumps to disk.** Padding and size change with the compiler.
- **Forgetting the commit for system save data.** Changes are lost.

## Open questions

- Does `fsync` reach the SD card on hardware, and what survives a power cut during a slot write? Needs a console test with power removal.
- How do cheap SD cards behave under many small writes? Needs a timing sweep.

## Sources

- [libnx](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/include/switch/runtime/devices/fs_dev.h) at `b156b4774fb2df7b93402169509266da7547fc4c` — `runtime/devices/fs_dev.h` (`fsdevMountSaveData`, `fsdevCommitDevice`, `fsdevGetLastResult`), `runtime/devices/romfs_dev.h`.
- [switch-examples](https://github.com/switchbrew/switch-examples) at `669786898205b7beb25ff1731e72982e6d0397d3` — `fs/save`.
- SwitchBrew [Flash Filesystem](https://switchbrew.org/wiki/Flash_Filesystem) (oldid 13575), [SD Filesystem](https://switchbrew.org/wiki/SD_Filesystem) (oldid 13379) — where saves live.
- `nx/engineering/examples/common/save_format.c`, `nx/tests/test_logic.c`, `nx/engineering/examples/first-game` — the format, its tests and its use.
