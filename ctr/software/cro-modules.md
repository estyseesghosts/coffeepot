# CTR dynamic modules (CRO, CRS, CRR)

Most 3DS games are one big executable, but a game can also load extra code at run time, much like a Windows DLL or a Linux shared library. These add-ons are **[CRO](../glossary.md#cro)** files ("dynamic modules"). A small system service, **RO** (`ldr:ro`), loads them, links them to the main program and to each other, and checks them against a list of approved files.

This page is a working description of that system; the Citra emulator team reverse-engineered it, and its authors flag that parts are still guesses (marked below).

## Behavior

### Vocabulary

| Term | Meaning |
| ---- | ------- |
| **Static module** | The main application ([ExeFS](file-formats.md) code). Loaded at boot; its symbol information is loaded at run time |
| **Dynamic module** | Code that can be loaded and unloaded while the game runs |
| **CRO** | `.cro` file: code, data and symbol information for a dynamic module |
| **CRS** | `.crs` file, same format as CRO: symbol information for the static module |
| **CRR** | `.crr` file: verification data for all modules; RO checks every dynamic module against it when loading |
| **Symbol** | A function or variable location. *Named* (found by name, usually a mangled C++ name), *indexed* (found by index) or *anonymous* (neither; the importer keeps its own list) |
| **Importing / exporting** | A module declares which symbols it uses / makes available |
| **Resolving / linking** | Looking up imports in the modules that export them and patching the importing code |
| **Auto-link module** | A loaded module that is linked automatically with newly loaded modules. The static module is always one |
| **Manual-link module** | Any other module |
| **Rebasing** | Converting every offset in the file into a real virtual address on load (and back on unload) |

### Life of a module

1. The application reads the CRS and CRO files from its [RomFS](file-formats.md) into memory buffers.
2. It calls the RO service (`Initialize` a.k.a. "LoadCRS", `LoadCRO`, `LoadCRO_New`) with a target address. RO maps the buffer to that address and always reads and writes through that mapping.
3. RO **rebases** the module, **registers** it in one of two doubly linked lists (auto-link or manual-link; the static module's `previous`/`next` fields head those lists) and **links** it by applying patches.
4. When unloaded, RO unlinks it, resets the patched places to point at the module's "OnUnresolved" function and unrebases it.

### Fixing

An application may "fix" a loaded module to give memory back. Higher levels crop more data and lose more features. Only level 1 is known to be used by games.

| Level | Crops | Consequence |
| ----- | ----- | ----------- |
| 0 | nothing | Can be unloaded and reloaded without re-reading RomFS |
| 1 | static anonymous symbol table, internal patch table, static anonymous patch table (and very likely `.data`) | Cannot be reloaded after unloading |
| 2 | everything level 1 does, plus imported-module, external-patch and all imported-symbol tables and strings | Cannot resolve symbols from modules loaded after it |
| 3 | everything level 2 does, plus exported-symbol tables, strings and name tree | Cannot export to modules loaded after it |

Fixed modules change their header magic from `CRO0` to `FIXD`.

## Technical details

### Header

| Offset | Size | Field |
| ------ | ---- | ----- |
| `0x00` | `0x20` × 4 | SHA-256 over: `0x80` to code offset; code; module name to data offset; data to end of file |
| `0x80` | 4 | Magic `CRO0` |
| `0x84` | 4 | Name offset |
| `0x88`, `0x8C` | 4 + 4 | Next / previous module (zero in the file; RO writes them) |
| `0x90` | 4 | File size |
| `0x94` | 4 | `.bss` size (RO doesn't touch it) |
| `0x98` | 4 | Fixed size (zero in file; RO writes it) |
| `0xA0`–`0xAC` | 4 each | Segment tags for the `nnroControlObject_` function, `OnLoad`, `OnExit` (`0xFFFFFFFF` if absent) and `OnUnresolved` |
| `0xB0`, `0xB4` | 4 + 4 | Code offset, size |
| `0xB8`, `0xBC` | 4 + 4 | Data offset, size |
| `0xC0`, `0xC4` | 4 + 4 | Module name offset, size |
| `0xC8`, `0xCC` | 4 + 4 | Segment table offset, count |
| `0xD0`–`0xEC` | | Exported named symbols, exported indexed symbols, exported strings, exported name tree (each offset + count/size) |
| `0xF0`–`0x11C` | | Imported modules, external patches, imported named / indexed / anonymous symbols, imported strings |
| `0x120`–`0x134` | | Static anonymous symbols, internal patches, static anonymous patches |

All offsets are file-relative until the module is rebased. Typical layout order: header, `.text`, `.rodata`, tables, `.data`.

### Segments and segment tags

Segment types: 0 `.text`, 1 `.rodata`, 2 `.data`, 3 `.bss`. A **segment tag** is a 32-bit value: low 4 bits segment index, high 28 bits offset in the segment. All symbol locations and patch targets use segment tags. For the static module, the CRS segment entries point directly at userland addresses (for example `.text` at `0x00100000`).

### Patches (relocations)

Linking applies **patches**: a module that exports symbols stores their locations as segment tags; a module that imports them keeps patches saying where and how to write each address. All patches for one imported symbol form a *batch*. Types are defined by `PatchType`. There are also **internal patches** (a module exporting to itself; needed because segment addresses change on rebase, not organised in batches) and **static anonymous patches** (symbols exported from dynamic modules to the static module, applied on rebase and never reset; no game is known to use them).

### Name lookup

RO finds an exported named symbol through a trie-like **name tree** rather than scanning the table: a `bit_trie<symbol_name, symbol_index, string_tester>` whose nodes do not store keys and use absolute offsets in branches.

## Homebrew notes

Homebrew `.3dsx` files do not use CRO; the format matters for emulators, for reverse-engineering retail games that ship CROs, and for modding. The Citra authors' CRO code (`HeaderField`, `Register`, `Fix`, `ApplyPatch`, `FindExportNamedSymbol`) is the reference implementation of this description. The RO service is `ldr:ro` ([services](services.md)); `loader` (a different module) loads whole processes.

## Open questions

- The Citra document says "CRO is still not completely understood" and marks many details with "(?)": which header fields RO ignores, behavior of fix levels 2–3, and the use of anonymous symbols.
- CRR verification (signature/hash format) is not described in the source.
- Whether the fix levels 2 and 3 behave as listed is untested because no known game uses them.

## Sources

- Citra project wiki, "3DS CRO Information" (reverse engineering by wwylele) — all of the above. The `citra-emu/citra` repository that hosted it has since been removed from GitHub, so no live link could be verified.
- [3dbrew: Services API](https://www.3dbrew.org/wiki/Services_API) — `ldr:ro` service.
