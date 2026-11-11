# Files and saves in 3DS homebrew

Where can a 3DS program read assets and keep its progress, and how do you write a save file that survives power loss, a full SD card and a bad write? This page covers the four storage areas libctru exposes (SD card, bundled read-only files, save data, extended data), the file APIs for each, the commit rule for save data, how failures show up, and a complete two-copy save manager with a version header and checksums.

The short answer: homebrew `.3dsx` programs use the SD card with ordinary `fopen`; installed `.cia` titles can also have real save data, which needs an explicit commit. Never overwrite your only good save in place; write the other copy, check every return value, and let a checksum decide which copy is valid at load time.

## The whole thing in 10 lines

1. A `.3dsx` program should keep its data on the SD card under `sdmc:/3ds/<yourapp>/` with plain `fopen`.
2. Bundled read-only assets go in RomFS (`romfsInit()`, then `romfs:/...`).
3. Real save data and extdata belong to installed titles (`.cia`); save data needs `archiveCommitSaveData()`.
4. Extdata cannot be written through `fopen` (`ENOTSUP`); use `FSUSER_*`/`FSFILE_*`.
5. Never overwrite your only good save: keep two slots, write the older one.
6. Put a magic number, a format version, a generation counter, the size and CRC-32s in a header.
7. Check `fopen`, `fwrite`, `fflush`, `fsync` and `fclose`; any failure means "this save failed".
8. At load, read both slots, drop invalid ones, keep the highest generation.
9. Do not rely on `rename` being crash-safe; nothing in libctru promises it.
10. Save from the main thread, before the loop ends; never in an exit hook ([lifecycle](application-lifecycle.md#exit)).

## Before you start

- Checked against libctru 2.7.1 at the commit pinned on [source versions](source-versions.md) (`62b7c42c88b1ee837f513358409d2e5c8f9001a9`), devkitARM r68. Source links are at that commit.
- Nothing here was run on hardware or an emulator; see [Verification status](#verification-status).
- Background: the on-card layout and encryption are on [SD filesystem](../software/sd-filesystem.md), the save container format on [savegames](../software/savegames.md), the file service on [services](../software/services.md), the library on [libctru](../homebrew/libctru.md). Lifecycle interaction (when to save) is on [application lifecycle](application-lifecycle.md).
- Terms: an **[archive](../glossary.md#fs-archive)** is one mounted storage area opened through the file service; **[RomFS](../glossary.md#romfs)** is the read-only file tree inside a program; **[save data](../glossary.md#save-data)** is the per-title private storage; **[extdata](../glossary.md#extdata)** is the larger shared storage a title can create on the SD card.

## Picking a storage area

| Area | libctru device | Writable from `fopen`? | Needs a title (`.cia`)? | Use for |
| ---- | -------------- | ---------------------- | ----------------------- | ------- |
| SD card | `sdmc:` (mounted automatically at startup) | Yes | No | Homebrew saves, configs, logs, user-supplied files |
| RomFS | `romfs:` after `romfsInit()` | No (read-only) | No: a `.3dsx` can embed one | Bundled assets |
| Save data | any name you give `archiveMount`, e.g. `save:` | Yes, plus a commit | Yes: a title with a save, not a `.3dsx` `[hypothesis]` | Progress of an installed title |
| Extdata | a name you mount | **No through stdio**; use `FSUSER_*` / `FSFILE_*` | A title creates it | Large shared data, DLC-like content |

Facts behind the table [documented: `libctru/source/system/appInit.c` (SD mount), `libctru/include/3ds/romfs.h`, `libctru/source/archive_dev.c`]:

- `__appInit` calls `fsInit()` then `archiveMountSdmc()`, so `sdmc:/...` works from `main` with no setup. `__appExit` calls `archiveUnmountAll()` and `fsExit()` before `aptExit()`.
- `romfsMountSelf(name)` mounts your own RomFS: for a `.3dsx` it is the RomFS section embedded in the file; for a title it is the title's RomFS. `romfsInit()` is the wrapper using the name `romfs`.
- `archiveMount(archiveID, archivePath, deviceName)` opens any archive and registers a stdio device. When the archive id is `ARCHIVE_EXTDATA`, `ARCHIVE_SHARED_EXTDATA`, `ARCHIVE_BOSS_EXTDATA` or `ARCHIVE_EXTDATA_AND_BOSS_EXTDATA` the device is flagged `is_extdata` and **opening for write, creating, renaming and the other mutating calls return `ENOTSUP`** (the source comment calls extdata "weird"). Write extdata through `FSUSER_OpenFile`/`FSFILE_Write`.
- A `.3dsx` has no title of its own, so whether `ARCHIVE_SAVEDATA` opens for it is not established by the files read `[hypothesis]`: test before relying on it. The SD card is the portable choice.

A homebrew-friendly SD layout is `sdmc:/3ds/<yourapp>/`. Create the folders with `mkdir` (it fails with `EEXIST` if they exist; that is fine).

## Reading and writing with stdio

`fopen`, `fread`, `fwrite`, `fclose`, `fsync`, `rename`, `mkdir`, `stat`, `opendir` all work on mounted devices through libctru's archive device layer [documented: `archive_dev.c`]. What each call does underneath:

| Call | Underlying FS call | Notes |
| ---- | ------------------ | ----- |
| `fopen(p, "wb")` | open with write+create, then `FSFILE_SetSize(fd, 0)` for `O_TRUNC` | The old contents are gone as soon as the open returns; a crash between truncation and the end of your write leaves a short or empty file |
| `fwrite` | `FSFILE_Write(handle, &n, offset, buf, len, flags)` | `flags` is `FS_WRITE_FLUSH \| FS_WRITE_UPDATE_TIME` only for files opened `O_SYNC`; otherwise `0`. The returned count is the bytes the service reported written |
| `fsync(fileno(f))` | `FSFILE_Flush` | Header text: "Flushes a file's contents" (`fs.h`) |
| `fclose` | `FSFILE_Close` | Can fail; check it |
| `rename(a, b)` | `FSUSER_RenameFile`; if the target exists it deletes the target and renames again; if that fails it tries directory rename | The delete-then-rename fallback means **two operations with a gap between them** |
| `mkdir`, `unlink`, `stat` | `FSUSER_CreateDirectory`, `FSUSER_DeleteFile`, … | |

Note that stdio's own buffer sits in front of this: call `fflush` before `fsync`, as in the example.

### Checking errors

FS calls return a libctru `Result`; stdio converts it to `errno`. The table in `archive_dev.c` maps only a few codes (`0x082044BE`→`EEXIST`, `0x086044D2`→`ENOSPC`, `0xC8804478` and `0xC92044FA`→`ENOENT`, `0xE0E046BE`→`EINVAL`, `0xE0E046BF`→`ENAMETOOLONG`) [documented: `archive_dev.c` `error_table`]. Anything else becomes a generic errno, so for diagnostics log the raw `Result` when you call `FSUSER_*` directly. Disk full (`ENOSPC`) shows up on a write or on `fclose`/`fflush`; treat any failed step as a failed save, not only a short `fwrite`.

### Short writes

`fwrite` returning fewer items than requested means the write failed part-way. The example below treats `put != len` as failure and also checks `fflush`, `fsync` and `fclose`. Do not retry a short write blindly at the same offset without re-seeking; simplest is to abandon this slot and report failure, because the other slot is still intact.

## Committing save data

Save-data archives buffer changes until you commit them. libctru's `archiveCommitSaveData(deviceName)` runs `FSUSER_ControlArchive(archive, ARCHIVE_ACTION_COMMIT_SAVE_DATA, NULL, 0, NULL, 0)`, and the header states: "Not done automatically at unmount" [documented: `archive.h`, `archive_dev.c`, `fs.h` (`ARCHIVE_ACTION_COMMIT_SAVE_DATA = 0`, "Commits save data changes")]. The header documents that unmounting does not commit; libctru's `fclose` path only calls `FSFILE_Close`, which does not mention a commit either [documented: `archive.h`, `archive_dev.c`]. So:

```c
// title with save data (not tested; shown for shape)
archiveMount(ARCHIVE_SAVEDATA, fsMakePath(PATH_EMPTY, ""), "save");
FILE *f = fopen("save:/slot.bin", "wb");
/* ... write, fflush, fsync, fclose ... */
archiveCommitSaveData("save");   // without this, the changes may be lost
```

What "lost" means exactly when you forget the commit (rolled back at next mount, versus partially visible) is not stated in the files read; assume the whole uncommitted batch can vanish `[hypothesis]`. Commit after each logical save, not after each file, and check the `Result`. Do not commit on the SD card: `sdmc:` has no commit action in libctru, and the example passes `NULL` as the device for that case.

### Lower-level calls

stdio is a convenience layer over the file service. When you need extdata, a path type the device layer does not build, or a raw `Result`, call the `FSUSER_OpenArchive(&archive, id, path)`, `FSUSER_OpenFile`, `FSFILE_Read`, `FSFILE_Write`, `FSFILE_Close`, `FSUSER_ControlArchive` and `FSUSER_CloseArchive` functions declared in `fs.h` yourself; the stdio layer is exactly these calls plus path conversion [documented: `fs.h`, `archive_dev.c`]. This page's example uses stdio only.

## Do not assume rename is crash-safe

A common recipe is "write `save.tmp`, then `rename` over `save`". Nothing in libctru's headers or source promises that `FSUSER_RenameFile` is atomic across power loss, and the stdio wrapper explicitly falls back to **delete then rename** when the target exists [documented: `archive_dev.c` `archive_rename`], which is not atomic by construction. Whether the FAT layer on the SD card, or the save-data layer, makes a single rename atomic is not documented in the sources used `[hypothesis]`. This page therefore does not rely on rename. It uses two fixed slots and a checksum, so recovery needs only reads.

## A two-generation save manager

Design:

- Two files, `save_a.bin` and `save_b.bin`. Each holds a header and a payload.
- The header has a magic number, a **format version** (so future builds can detect and migrate old payloads), the header size it was written with, a **generation** counter, the payload size, a CRC-32 of the payload, and a CRC-32 of the header itself.
- `save_store` reads both slots, finds the newest valid one, and writes generation + 1 into the *other* slot. The newest valid copy is never touched while the new one is being written.
- `save_load` validates both slots and returns the newest valid one. If one slot is torn, truncated or corrupt, the other still loads. Generation comparison uses signed subtraction so it survives wraparound.
- Version policy: a slot with a version newer than the build knows is treated as invalid (we cannot interpret it), and the loader falls back to the older slot. A real game would add a migration step for older versions at the marked point.

A **CRC-32** here is only a corruption detector (it catches torn writes and bit rot, not tampering).

```c
#include <3ds.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define SAVE_MAGIC     0x56415343u  /* "CSAV" little-endian */
#define SAVE_VERSION   1u
#define SAVE_MAX_BYTES (1u << 20)

typedef struct {
	uint32_t magic;
	uint16_t version;      /* format version of the payload */
	uint16_t header_size;  /* sizeof(SaveHeader) when written */
	uint32_t generation;   /* increases by 1 per successful store */
	uint32_t payload_size;
	uint32_t payload_crc;  /* CRC-32 of the payload */
	uint32_t header_crc;   /* CRC-32 of all preceding header bytes */
} SaveHeader;
_Static_assert(sizeof(SaveHeader) == 24, "header layout");

static uint32_t crc32(const void *data, size_t n)
{
	const uint8_t *p = data;
	uint32_t c = 0xFFFFFFFFu;
	while (n--) {
		c ^= *p++;
		for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
	}
	return ~c;
}

#ifdef SAVE_FAULT_INJECTION
/* >= 0: the next slot write stops after this many bytes and fails (a torn write). */
long g_fault_after_bytes = -1;
#endif

/* Write all of buf to path; true only if every step succeeded. */
static bool write_file(const char *path, const void *buf, size_t len)
{
	FILE *f = fopen(path, "wb");
	if (!f) return false;
	size_t want = len;
#ifdef SAVE_FAULT_INJECTION
	if (g_fault_after_bytes >= 0 && (size_t)g_fault_after_bytes < len) want = (size_t)g_fault_after_bytes;
#endif
	size_t put = fwrite(buf, 1, want, f);
	bool ok = (put == len);           /* a short count is a failure, not a partial success */
	if (fflush(f) != 0) ok = false;
	if (fsync(fileno(f)) != 0) ok = false;
	if (fclose(f) != 0) ok = false;
	return ok;
}

/* Read and validate one slot. On success *out is malloc'd (caller frees). */
static bool read_slot(const char *path, SaveHeader *hdr, void **out)
{
	FILE *f = fopen(path, "rb");
	if (!f) return false;
	void *buf = NULL;
	bool ok = false;
	if (fread(hdr, sizeof *hdr, 1, f) != 1) goto done;
	if (hdr->magic != SAVE_MAGIC || hdr->header_size != sizeof *hdr) goto done;
	if (hdr->header_crc != crc32(hdr, offsetof(SaveHeader, header_crc))) goto done;
	if (hdr->version == 0 || hdr->version > SAVE_VERSION) goto done; /* newer than we know */
	if (hdr->payload_size > SAVE_MAX_BYTES) goto done;
	buf = malloc(hdr->payload_size ? hdr->payload_size : 1);
	if (!buf) goto done;
	if (hdr->payload_size && fread(buf, 1, hdr->payload_size, f) != hdr->payload_size) goto done;
	if (crc32(buf, hdr->payload_size) != hdr->payload_crc) goto done;
	ok = true;
done:
	fclose(f);
	if (ok) *out = buf; else free(buf);
	return ok;
}

typedef struct {
	char dir[96];     /* e.g. "sdmc:/3ds/savedemo" */
	const char *dev;  /* save-data device name to commit ("save"), or NULL for sdmc */
} SaveStore;

static void slot_path(const SaveStore *s, int slot, char *out, size_t n)
{
	snprintf(out, n, "%s/save_%c.bin", s->dir, slot ? 'b' : 'a');
}

/* Newest valid generation wins; -1 if neither slot is valid. Comparison is wrap-safe. */
int save_load(const SaveStore *s, void **payload, uint32_t *size, uint32_t *generation)
{
	int best = -1;
	SaveHeader bh = {0};
	void *bp = NULL;
	for (int i = 0; i < 2; i++) {
		char path[128];
		SaveHeader h;
		void *p;
		slot_path(s, i, path, sizeof path);
		if (!read_slot(path, &h, &p)) continue;
		if (best < 0 || (int32_t)(h.generation - bh.generation) > 0) {
			free(bp);
			bp = p; bh = h; best = i;
		} else {
			free(p);
		}
	}
	if (best >= 0) { *payload = bp; *size = bh.payload_size; *generation = bh.generation; }
	return best;
}

/* Write the next generation into the slot that does NOT hold the newest valid copy. */
bool save_store(const SaveStore *s, const void *payload, uint32_t size)
{
	if (size > SAVE_MAX_BYTES) return false;
	void *old = NULL;
	uint32_t osz = 0, gen = 0;
	int cur = save_load(s, &old, &osz, &gen);
	free(old);
	int target = (cur == 0) ? 1 : 0;
	uint32_t next = (cur < 0) ? 1 : gen + 1;

	uint8_t *img = malloc(sizeof(SaveHeader) + size);
	if (!img) return false;
	SaveHeader h = { SAVE_MAGIC, SAVE_VERSION, sizeof h, next, size, crc32(payload, size), 0 };
	h.header_crc = crc32(&h, offsetof(SaveHeader, header_crc));
	memcpy(img, &h, sizeof h);
	memcpy(img + sizeof h, payload, size);

	char path[128];
	slot_path(s, target, path, sizeof path);
	bool ok = write_file(path, img, sizeof h + size);
	free(img);
	if (ok && s->dev) ok = R_SUCCEEDED(archiveCommitSaveData(s->dev)); /* save archives only */
	return ok;
}

int main(void)
{
	gfxInitDefault();
	consoleInit(GFX_TOP, NULL);

	SaveStore st = { "sdmc:/3ds/savedemo", NULL };
	mkdir("sdmc:/3ds", 0777);
	mkdir(st.dir, 0777);

	void *p = NULL;
	uint32_t sz = 0, gen = 0;
	int slot = save_load(&st, &p, &sz, &gen);
	printf("load: slot=%d gen=%u size=%u\n", slot, (unsigned)gen, (unsigned)sz);
	free(p);

	char msg[] = "hello save";
	printf("store: %s\n", save_store(&st, msg, sizeof msg) ? "ok" : "FAILED");

	while (aptMainLoop()) {
		hidScanInput();
		if (hidKeysDown() & KEY_START) break;
		gfxFlushBuffers();
		gfxSwapBuffers();
		gspWaitForVBlank();
	}
	gfxExit();
	return 0;
}
```

What to expect (derived from reading, not observed): on first run `load: slot=-1`, then `store: ok` and `save_a.bin` appears under `sdmc:/3ds/savedemo/`; the next run reports `slot=0 gen=1` and writes `save_b.bin` with generation 2.

Known limits of this design:

- It protects against a torn write of *one* slot. It does not protect against a card that silently returns bad data and a bad CRC collision, against both slots being damaged, or against power loss that leaves the *SD card's FAT metadata* inconsistent (a file-system level problem this layer cannot see).
- `fopen("wb")` truncates immediately, so there is a window where the target slot is empty; that is fine only because the other slot is valid. If the very first save is interrupted there is nothing to fall back to.
- Whether `fsync`/`FSFILE_Flush` forces data to the card before it returns is not documented in the files read (the header says only "Flushes a file's contents"). The scheme tolerates this: ordering is guaranteed only by the CRC check, not by the flush `[hypothesis]`.
- The one-megabyte cap and the single payload blob are simplifications; a real game may want several records.

### Verification status

Compiled and linked (not run), with and without `-DSAVE_FAULT_INJECTION`, using devkitARM r68-1 (GCC 16.1.0) and libctru 2.7.1-1, flags `-Wall -Wextra -O2 -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft -specs=3dsx.specs`, no warnings. Built directly with `arm-none-eabi-gcc` to an ELF, not through the template `Makefile`, and not converted to `.3dsx`. The save-data commit snippet in [Committing save data](#committing-save-data) is a fragment that was **not compiled on its own**; only the `archiveCommitSaveData(s->dev)` call inside `save_store` was compiled and linked, never executed. Emulator: not run. Old 3DS: not run. New 3DS: not run.

## Testing failure handling

The example has a fault knob: compile with `-DSAVE_FAULT_INJECTION`, set `g_fault_after_bytes` to a byte count, and the next `write_file` writes only that many bytes and reports failure, imitating a torn write. A test plan for hardware (none of these has been run):

| # | Setup | Action | Pass condition |
| - | ----- | ------ | -------------- |
| 1 | Empty directory | Load | Returns -1, no crash |
| 2 | Empty directory | Store A, store B, store C; load after each | Generations 1, 2, 3; slots alternate; payload equals what was stored |
| 3 | After #2 | Set fault to 0, 1, 23, 24, 25 and `payload_size-1` bytes; store | Store returns false; load still returns the previous generation with identical payload |
| 4 | After #3 | Clear fault; store | Succeeds with generation +1; the damaged slot is repaired |
| 5 | Two valid slots | Flip one byte in the newest slot's payload with a PC tool, then in its header, then in the magic | Load returns the older slot each time |
| 6 | Two valid slots | Truncate the newest slot to 0 and to 10 bytes | Older slot loads |
| 7 | Damage both slots | Load | Returns -1; the game starts with defaults and does not crash |
| 8 | Header version set to 2 (edit and recompute CRC) | Load | Treated as invalid; older slot used |
| 9 | Fill the card (or use a tiny test volume) | Store | Returns false with `errno == ENOSPC` `[hypothesis]` for the exact errno; the previous generation still loads |
| 10 | Real power loss | Loop storing every second with increasing payload; pull the battery/power mid-run (repeat at least 20 times) | After each reboot, load succeeds and returns a generation no more than 1 behind the last one printed over 3dslink; the payload CRC always matches |
| 11 | Lid close / HOME during a store loop | See [application lifecycle](application-lifecycle.md) | Same as #10 |

Record the console model, firmware, card brand and size, and number of repetitions with each result; per [AGENTS.md](../../AGENTS.md), a pass becomes `[measured]` only when those are written down.

For an imported save format, test checksum implementations against known external vectors and saves produced by the original system, then test both directions of the round-trip. A self-consistent writer and reader can share the same wrong algorithm. In HGSS, a reflected CRC table was paired with a non-reflected update; the port accepted its own saves while rejecting retail saves [reported: project S17, November 2026; host investigation, hardware round-trip pending]. Any legacy migration should recognize only the known legacy format and still reject data matching neither checksum. [Case study](case-studies/case-study-hgss-port.md#audio-and-save-compatibility-failures).

## Technical details

| Item | Value | Source |
| ---- | ----- | ------ |
| Archive IDs | `ARCHIVE_ROMFS`=3, `ARCHIVE_SAVEDATA`=4, `ARCHIVE_EXTDATA`=6, `ARCHIVE_SHARED_EXTDATA`=7, `ARCHIVE_SYSTEM_SAVEDATA`=8, `ARCHIVE_SDMC`=9, `ARCHIVE_SDMC_WRITE_ONLY`=0xA, `ARCHIVE_USER_SAVEDATA`=0x567890B2, `ARCHIVE_GAMECARD_SAVEDATA`=0x567890B1, others in the header | `fs.h` |
| Open flags | `FS_OPEN_READ`=BIT(0), `FS_OPEN_WRITE`=BIT(1), `FS_OPEN_CREATE`=BIT(2) | `fs.h` |
| Write flags | `FS_WRITE_FLUSH`=BIT(0), `FS_WRITE_UPDATE_TIME`=BIT(8) | `fs.h` |
| Archive control | `ARCHIVE_ACTION_COMMIT_SAVE_DATA`=0 (no input/output), `ARCHIVE_ACTION_GET_TIMESTAMP`=1 | `fs.h` |
| Commit wrapper | `archiveCommitSaveData(name)` returns -1 if the device name is not mounted; not called automatically at unmount | `archive.h`, `archive_dev.c` |
| Path types | `PATH_EMPTY`=1, `PATH_BINARY`=2 ("meaning is per-archive"), built with `fsMakePath(type, data)` | `fs.h` |
| Extdata identity | `FS_ExtSaveDataInfo`: media type (8 bits), 64-bit save id | `fs.h` |
| Rename | `FSUSER_RenameFile`, then delete target + retry on `RD_ALREADY_EXISTS`, then `FSUSER_RenameDirectory`; refused with `ENOTSUP` across archives and on extdata | `archive_dev.c` |
| Append | `O_APPEND` re-reads the file size before each write | `archive_dev.c` |
| Sync | `O_SYNC` → `FS_WRITE_FLUSH \| FS_WRITE_UPDATE_TIME` on each `FSFILE_Write`; `fsync` → `FSFILE_Flush` | `archive_dev.c` |
| Create save data | `FSUSER_FormatSaveData(...)`, `FSUSER_CreateExtSaveData(info, directories, files, sizeLimit, smdhSize, smdh)`, `FSUSER_DeleteExtSaveData(info)` exist; their permission requirements were not examined | `fs.h` |
| Free space | `FSUSER_GetFreeBytes(&bytes, archive)` | `fs.h` |

All files are under `https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/` (`include/3ds/services/fs.h`, `include/3ds/archive.h`, `include/3ds/romfs.h`, `source/archive_dev.c`, `source/system/appInit.c`, `source/system/appExit.c`).

## Troubleshooting

| Symptom | Likely cause | Check |
| ------- | ------------ | ----- |
| `fopen` for write returns NULL on `sdmc:/...` | Parent directory missing (`mkdir` first), or a path longer than the limit | `errno`: `ENOENT`, `ENAMETOOLONG` |
| `fopen` for write on an extdata device fails with `ENOTSUP` | stdio cannot write extdata | Use `FSUSER_*`/`FSFILE_*` |
| Save-data changes gone after restart | No `archiveCommitSaveData` | Commit after each logical save, check the `Result` |
| Save always loads defaults | Both slots rejected: wrong magic, header size changed, version newer than the build, payload over the cap | Print which check fails in `read_slot` |
| Old save lost after a failed write | Overwrote the only copy, or one-slot design | Use two slots |
| Write fails only on some cards | Card full or failing | `errno == ENOSPC`, `FSUSER_GetFreeBytes` |
| Works from `.cia`, fails from `.3dsx` (or the reverse) | Save-data archives exist only for titles | Use the SD card for `.3dsx` |
| Edited save rejected | PC tool wrote big-endian fields or forgot the CRC | Fields are little-endian; recompute both CRCs |

## Not covered yet

Interrupt-during-write results on real hardware, SD versus title-save comparisons, `FSFILE_Flush` durability and what an uncommitted save archive loses are all open (see [Open questions](#open-questions)). The `.3dsx` versus `.cia` permission differences are only partly established. Input devices and sensors, and local wireless, are outside this page; see [networking](networking.md#not-covered-yet).

## Pitfalls

- **Overwriting the only copy.** Symptom: a crash or a full card destroys the save. Fix: two slots as above.
- **Trusting `fwrite`'s count alone.** Symptom: success reported, file short or missing. Fix: also check `fflush`, `fsync`, `fclose`.
- **Forgetting `archiveCommitSaveData`.** Symptom: changes to a save archive vanish after restart. Fix: commit after each logical save and check the `Result`.
- **Writing extdata with `fopen`.** Symptom: `ENOTSUP`. Fix: use `FSUSER_*`/`FSFILE_*` directly.
- **Saving in an exit hook.** Symptom: failure, since the FS service is already closed by then. Fix: save before leaving the main loop ([application lifecycle](application-lifecycle.md#exit)).
- **Relying on rename.** See the section above.
- **Assuming a save exists.** First-run and corrupt-save paths must produce usable defaults.
- **Long writes with HOME enabled.** The user can suspend mid-write; the main thread stops inside `aptMainLoop`, not inside your save, so this is only a problem if you save from a worker thread. Either save from the main thread or guard with `aptSetHomeAllowed(false)` for the duration.
- **Endianness and padding in headers.** The header above uses fixed-width fields and a size assertion; the 3DS runs little-endian here (the example's `"CSAV"` magic is defined as a little-endian constant), so a PC tool that edits these files must read and write little-endian fields. The endianness of the ARM11 as configured by the 3DS kernel is not sourced on this wiki `[hypothesis]`.

## Open questions

- Is the SD card's single `rename` atomic across power loss? Experiment: the rename stress test in the plan row 10, with a write-then-rename variant, run 100+ times and verify no state other than old or new.
- Does `FSFILE_Flush` guarantee durability before returning? Experiment: write a counter with and without flush, cut power at random times, compare the highest counter seen on reboot to the last one printed over 3dslink.
- What exactly is lost when `archiveCommitSaveData` is skipped on a title save? Experiment: a `.cia` that writes without committing, then a clean exit, then a restart.
- Can a `.3dsx` open `ARCHIVE_SAVEDATA` under Luma3DS? Experiment: call `archiveMount(ARCHIVE_SAVEDATA, ...)` from a `.3dsx` and log the `Result`.
- What errno does a full SD card produce for `fwrite`, `fflush` and `fclose` on current firmware?
- The transition effects of HOME or sleep on in-flight file requests are not documented in the files read.

## Sources

- [libctru `fs.h` at the pinned commit](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/include/3ds/services/fs.h) — archive ids, open/write flags, `FSUSER_*` and `FSFILE_*` declarations, commit action.
- [libctru `archive.h`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/include/3ds/archive.h) — `archiveMount`, `archiveCommitSaveData`, unmount behaviour.
- [libctru `archive_dev.c`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/source/archive_dev.c) — stdio mapping, extdata restrictions, rename, sync, error table.
- [libctru `romfs.h`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/include/3ds/romfs.h) — RomFS mounting.
- [libctru `appInit.c`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/source/system/appInit.c) and [`appExit.c`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/source/system/appExit.c) — automatic SD mount and unmount order.
- Wiki pages: [SD filesystem](../software/sd-filesystem.md), [savegames](../software/savegames.md), [application lifecycle](application-lifecycle.md), [testing on hardware](testing-on-hardware.md), [source versions](source-versions.md).
