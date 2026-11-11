# Writing maintainable C for Nintendo homebrew

A code style and engineering guide for C programs targeting the Nintendo 3DS and Nintendo Switch.

Good C makes **ownership, bounds, state, errors, and hardware assumptions visible**. It is not necessarily short C. The goal is code that another developer can change without reconstructing its hidden rules, and that still behaves correctly when the compiler optimizes it.

This is a **recommended project style**, not a claim about what devkitPro, Nintendo, or the C standard requires. When an external API has different conventions, follow that API at its boundary and translate into the project's conventions internally.

## Before you start

- **Targets:** Old 3DS (ARM11) by default, plus New 3DS where applicable; Switch (AArch64). The language rules apply equally to both.
- **Toolchains:** devkitARM with libctru for 3DS; devkitA64 with libnx for Switch. Their official example Makefiles are the starting point for platform build flags, rather than flags copied from an unrelated host project. [documented]
- **Language baseline:** Prefer **C11** for new, portable project code *if the selected compiler and libraries support the features used*. C99 is a reasonable compatibility baseline. Do not require C17/C23, `<threads.h>`, or working lock-free atomics without checking the installed target toolchain. Set the standard explicitly (`-std=c11` or `-std=gnu11` where an extension is deliberate); don't silently depend on compiler defaults.
- **Verification:** The snippets below express design patterns; they are not complete console applications. They have **not** been compiled with a named devkitARM/devkitA64 release or tested on Old 3DS, New 3DS, or Switch hardware. Build and hardware-test adapted examples before treating them as platform-verified.

## 1. Rules and priorities

**MUST** means the rule should not be broken without a documented technical reason. **SHOULD** means the default choice; exceptions require a clearer alternative. **AVOID** identifies patterns that commonly damage maintainability. These are local style terms, not normative language from ISO C.

In order of priority:

1. **Correctness before cleverness.** No undefined behavior, invalid lifetime, unchecked bounds, unhandled partial initialization, or guessed concurrency semantics.
2. **Make contracts inspectable.** A caller should know which inputs are valid, who owns memory, whether it can be modified, and what failure looks like.
3. **Keep changes local.** Split by responsibility and isolate platform-specific work. Do not create abstractions without a second concrete use or a real boundary.
4. **Make performance measurable.** Do not trade away clarity for an unmeasured optimization. On Old 3DS, a measured improvement can justify low-level code, but the benchmark and assumptions belong beside it.
5. **Be consistent.** Formatter output is not a matter for code review. Explicit exceptions matter more than personal taste.

### Default decisions

| Question | Default |
| --- | --- |
| Naming | `snake_case` functions/variables; `PascalCase` **not** used for project-defined C types |
| Project types | `snake_case` with `_t` **avoided** for POSIX interoperability; e.g. `sprite_batch`, `texture_handle` |
| Constants | `SCREAMING_SNAKE_CASE` for macros and enum constants |
| Local helpers | `static` by default; export only intentional API |
| Source files | One coherent responsibility; no arbitrary line limit |
| Functions | One action or decision; short enough that invariants are clear |
| Indentation | 4 spaces, no tabs; braces on the same line |
| Conditions and loops | Always use braces, including one-line bodies |
| Line length | Aim for 100 characters; split when it helps reading |
| Headers | Include what they directly depend on; no hidden transitive includes |
| Allocation | Explicit owner and destruction path; avoid per-frame heap churn |
| Errors | Check every fallible operation; preserve the original failure |
| Performance work | Profile first, compare on target hardware, retain a readable baseline |

The details below explain how to apply these defaults.

## 2. Formatting is mechanical

### Braces, whitespace, declarations

**Do:**

```c
if (sprite_count == 0) {
    return true;
}

for (size_t i = 0; i < sprite_count; ++i) {
    update_sprite(&sprites[i]);
}
```

**Don't:**

```c
if(sprite_count==0) return true;
for(size_t i=0;i<sprite_count;i++) update_sprite(&sprites[i]);
```

Put one statement on each line. Use spaces around binary operators. Keep declarations near their first use; initialize them whenever possible. Prefer `const` for a local that will not be reassigned. Use a blank line to separate phases of a function, not every line.

```c
const size_t byte_count = sprite_count * sizeof(*sprites);
```

Here the multiplication is safe **only if** `sprite_count` has already been bounded so it cannot overflow `size_t`; see [Integer arithmetic](#8-integers-and-bounds).

Use parentheses where they resolve a real precedence ambiguity, not around every subexpression. Do not nest ternary operators. Keep `switch` cases simple and explicit about fallthrough.

### A baseline `.clang-format`

```yaml
BasedOnStyle: LLVM
Language: Cpp
IndentWidth: 4
UseTab: Never
ColumnLimit: 100
BreakBeforeBraces: Attach
AllowShortIfStatementsOnASingleLine: Never
AllowShortLoopsOnASingleLine: false
AllowShortFunctionsOnASingleLine: None
PointerAlignment: Right
SortIncludes: Never
```

`clang-format` uses `Language: Cpp` for formatting C-family source; this does **not** mean compiling C as C++. Pin a formatter version in CI before running mass rewrites. Include sorting is disabled here because separate include groups may document a dependency boundary. [documented]

A formatter settles layout. It cannot decide whether a function is too complicated or an API owns its buffer.

## 3. Names should explain intent

Prefer names that tell the reader *what a value means*, not merely its type.

```c
uint32_t tile_index;
size_t vertex_count;
size_t vertex_capacity;
bool texture_is_dirty;
```

Avoid `data2`, `tmp_buffer`, `flag`, `do_stuff`, and cryptic abbreviations. `i` is fine for a tiny local loop; `x` and `y` are fine for coordinates. Use the vocabulary of the domain consistently (`tile`, `sprite`, `vertex`, `frame`, `transfer`, `handle`).

Functions should read as operations or questions:

```c
bool atlas_contains(const atlas *atlas, uint32_t sprite_id);
bool renderer_submit(renderer *renderer, const draw_command *command);
void atlas_reset(atlas *atlas);
```

Predicates return `bool`; status-bearing operations should have a documented result type. Avoid a `bool` return whose meaning changes from function to function (`true` = success in one function, `true` = error in the next).

**Do not use suffixes like `_fast`, `_optimized`, or `_new` indefinitely.** Name the actual algorithm or representation, and retire obsolete alternatives.

### One naming exception: platform APIs

Preserve the actual API spelling at the adapter boundary: `LightLock_Lock`, `armDCacheClean`, `R_FAILED`, and so on. Do not rename the vendor API in copied examples, and do not scatter those names through game logic.

## 4. Files and modules: responsibilities, not arbitrary size limits

A `.c` file is a module implementation. Its `.h` file is its public contract. Split modules when they have **different reasons to change**, not because a file passed an arbitrary number of lines.

**Poor layout:**

```text
source/
  game.c          # startup, input, world, renderer, audio, file loading
  utils.c         # everything that had nowhere else to go
```

**Better layout:**

```text
include/
  game/world.h
  game/renderer.h
  game/assets.h
source/
  game/world.c
  game/renderer.c
  game/assets.c
  platform/ctr/video.c
  platform/nx/video.c
  platform/host/video.c
```

The host adapter is optional, but useful for tests and tools. Don't invent `manager`, `service`, `system`, or `context` layers just to make the tree look symmetrical.

**Rules:**

- Give each exported module a small public API and keep its implementation details private.
- Mark non-exported functions and objects `static`. C's linker's global namespace is not a module system.
- Prefer opaque state for complicated subsystems; do not expose internal fields just so callers can reach into them.
- Keep platform calls at a defined boundary. Avoid putting `#ifdef __3DS__` and `#ifdef __SWITCH__` throughout gameplay code.
- Let call direction flow **game → platform interface → platform implementation**; never let low-level code depend on gameplay state through hidden globals.
- A long data table or straightforward parser can remain in one file. A 200-line function handling six unrelated jobs should not.

### Public headers

A header MUST compile when included first in an otherwise empty translation unit, with its required standard headers. For example:

```c
#ifndef GAME_SPRITE_BATCH_H
#define GAME_SPRITE_BATCH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct sprite_batch sprite_batch;

/* Borrowed vertices remain valid only during this call. */
bool sprite_batch_submit(sprite_batch *batch,
                         const void *vertices,
                         size_t vertex_count);

#endif
```

This is an **interface illustration**, not a complete vertex-format API. In a real interface, replace `void *` with a concrete vertex type and document byte counts or element counts unambiguously.

Don't put function bodies, writable global definitions, or private dependency headers into public headers. Use `static inline` in headers only for genuinely small, header-owned operations. Macros do not substitute for a coherent header API.

## 5. Functions: make the sequence obvious

A useful function often reads as:

1. Validate inputs and preconditions.
2. Acquire or prepare resources.
3. Perform one operation.
4. Publish the result.
5. Release what this function owns.

Use **early returns** to reject invalid inputs and keep the main path left-aligned. Extract helpers when a block has a meaningful name or a different error/ownership contract.

**Don't:**

```c
bool load_level(game *g, const char *path) {
    if (g) {
        if (path) {
            if (open_and_parse_everything(g, path)) {
                if (upload_everything(g)) {
                    return true;
                }
            }
        }
    }
    return false;
}
```

**Do:**

```c
bool load_level(game *g, const char *path) {
    if (g == NULL || path == NULL) {
        return false;
    }

    if (!parse_level(g, path)) {
        return false;
    }

    return upload_level(g);
}
```

This is readable, but it is **not yet a complete design**: if `upload_level` fails, does `g` remain partially changed? Good code must also specify whether failure leaves the old level intact or resets the game to a documented empty state. A clean control flow cannot compensate for an unclear state transition.

### Parameters

- Pass small scalars by value.
- Pass larger immutable values by `const T *` where copying would be unwanted.
- Pass explicitly mutable objects by `T *`.
- Express the size of an array separately, in elements or bytes as named in the parameter.
- Avoid parameters that change the meaning of an API (`render(..., true, false, true)`). Use separate functions or a small options struct with named fields.
- Avoid output parameters when a plain return value is sufficient; use them when you must return both a status and a result.

## 6. Lifetime and ownership are part of the API

C cannot enforce who frees a pointer. Your contract must.

Distinguish:

- **Owned:** caller must eventually release with a named function.
- **Borrowed:** caller can use the pointer only for a stated lifetime and must not release it.
- **Transferred:** ownership moves to another subsystem, and the original owner must stop using it except as explicitly allowed.
- **Static:** storage lives until program termination; caller never frees it.

A clear pairing:

```c
bool texture_create(texture **out_texture, const texture_desc *desc);
void texture_destroy(texture *texture);
```

Document that `*out_texture` is set to `NULL` on failure, who owns it on success, and whether the destructor accepts `NULL`. On failure, don't return a non-NULL half-initialized object unless the API explicitly supports that state.

### One owner per resource

If a module owns a heap allocation, file, thread, kernel handle, GPU buffer, or texture, it must have one clear disposal path. Allocators and deallocators are **not interchangeable**: call the corresponding `free`, platform allocator's free operation, or service-specific destroy function. Never guess based on the pointer type.

Use a single cleanup block when several resources need release:

```c
bool load_asset(const char *path) {
    FILE *file = NULL;
    unsigned char *buffer = NULL;
    bool ok = false;

    file = fopen(path, "rb");
    if (file == NULL) {
        goto cleanup;
    }

    buffer = malloc(4096);
    if (buffer == NULL) {
        goto cleanup;
    }

    /* Validate length, read the asset, and process it here. */
    ok = true;

cleanup:
    free(buffer);
    if (file != NULL && fclose(file) != 0) {
        ok = false;
    }
    return ok;
}
```

This fragment **only illustrates the cleanup pattern**; it does not actually read or validate a file. `goto cleanup` is preferable to duplicating four error-path releases. It is not a license for uncontrolled jumps in normal game logic.

### Avoid ambiguous ownership

```c
uint8_t *get_texture_data(texture *texture);  /* Owned? Borrowed? Mutable? */
```

Prefer a documented view:

```c
typedef struct {
    const uint8_t *data;
    size_t size_bytes;
} byte_view;

byte_view texture_pixels(const texture *texture);
```

The API still needs a lifetime rule: for example, the view is invalid after mutation or destruction of `texture`.

## 7. Make failures and invariants explicit

### Define one internal error convention

For nontrivial code, prefer a project-defined error enum rather than a sea of unexplained `bool`s and magic negative values:

```c
typedef enum {
    GAME_OK = 0,
    GAME_ERROR_INVALID_ARGUMENT,
    GAME_ERROR_NO_MEMORY,
    GAME_ERROR_IO,
    GAME_ERROR_INVALID_DATA,
    GAME_ERROR_PLATFORM
} game_result;
```

At the platform boundary, translate the actual `Result` or return code into project errors. When diagnosing a failure, preserve or log the **original platform code** as well; mapping everything to `GAME_ERROR_PLATFORM` loses useful detail.

- Check each fallible call immediately unless intentionally deferred.
- Don't turn errors into success-shaped default values such as texture ID `0` or empty files without a documented fallback.
- An assertion checks an **internal invariant**, not an untrusted file, controller input, network packet, or allocation result.
- Don't hide production-side effects inside `assert(...)`: assertions may disappear under `NDEBUG`.
- Initialization should either fully succeed, or leave a destroyable, documented state.
- State whether an operation is transactional: on failure, has it changed the destination?

### Error messages have context

Prefer `asset_load: invalid palette index 17 in tileset 4` to `load failed`. Include resource identity, stage, platform result, and relevant lengths. Avoid logging once per sprite, pixel, or frame in normal operation.

## 8. Integers and bounds

C's integer promotion, conversion, and overflow rules are not optional details. They frequently cause failures in asset loaders, file offsets, texture dimensions, and vertex buffers.

### Pick types according to meaning

- `size_t`: object sizes, array lengths, allocation sizes, indices into in-memory containers.
- `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`: external formats, hardware fields, and values that require a specified width.
- `int` or a suitable signed type: values that genuinely can be negative.
- `bool`: predicates, not a substitute for an error code.
- `uintptr_t`: integer representation of a pointer when strictly necessary, not an invitation to assume an address is a device address.

**Do not assume** `int`, `long`, `size_t`, or pointer width matches across the 3DS and Switch. Never serialize a `size_t` or raw pointer into a file format. Explicitly defined formats are the contract.

### Validate before calculating

**Don't:**

```c
size_t byte_count = vertex_count * sizeof(vertex);
vertex *vertices = malloc(byte_count);
```

The multiplication can wrap before `malloc` sees it.

**Do:**

```c
if (vertex_count > SIZE_MAX / sizeof(vertex)) {
    return false;
}

size_t byte_count = vertex_count * sizeof(vertex);
vertex *vertices = malloc(byte_count);
if (vertices == NULL) {
    return false;
}
```

For an existing buffer, prefer `if (count > capacity - used)` **only after** establishing `used <= capacity`; otherwise the subtraction underflows. The same applies to `offset + length`: check `offset <= size && length <= size - offset`.

### Index by count, not by hope

```c
for (size_t i = 0; i < sprite_count; ++i) {
    /* sprites[i] valid if sprite_count <= allocated capacity. */
}
```

Avoid `i <= sprite_count` unless the last index is intentionally included and within bounds. Keep units visible: `size_bytes`, `tile_count`, `stride_bytes`, `width_pixels` are preferable to a generic `length` when confusion is plausible.

### Signedness and shifts

- Don't mix signed and unsigned operands casually. Validate negative values **before** converting to `size_t`.
- Use unsigned types for bit masks and bit shifts. Check shift counts are below the operand width.
- Signed overflow is undefined behavior; unsigned overflow wraps modulo the type range, but that does not make a wrapped size or index correct.
- Avoid implicit narrowing casts. Where a hardware API requires `uint32_t`, check that a `size_t` value fits before casting.

## 9. Memory layout, binary data, and alignment

The CPU's memory representation is not the asset format.

**Don't:**

```c
const file_header *header = (const file_header *)bytes;
```

This assumes alignment, host structure padding, compatible representation, and enough input bytes. Packed struct tricks do not solve all of those problems.

**Do:** decode external fields explicitly:

```c
static uint32_t read_le32(const uint8_t *p) {
    return (uint32_t)p[0]
        | ((uint32_t)p[1] << 8)
        | ((uint32_t)p[2] << 16)
        | ((uint32_t)p[3] << 24);
}
```

The **caller must first verify that at least four bytes remain**. A length-checked reader is better for a public parser. Use `memcpy` for same-representation object copying and safe bit reinterpretation, but do not treat `memcpy` as automatic endianness conversion.

Avoid type-punning with unrelated pointer types. Under optimizing GCC builds, violating aliasing rules can change program behavior. [documented]

Use `_Static_assert` when layout is genuinely contractual:

```c
_Static_assert(sizeof(uint32_t) == 4, "uint32_t must be 4 bytes");
```

For hardware or file formats, explicit encoding is generally safer than asserting a struct has the expected size. Don't use C bitfields to model serialized or GPU-visible register layouts without proving compiler, ABI, and access semantics.

## 10. `const`, `volatile`, `restrict`, and atomics

These qualifiers mean different things:

| Qualifier / mechanism | What it says | What it **does not** say |
| --- | --- | --- |
| `const` | This access path will not modify the object | Other aliases cannot modify it |
| `volatile` | Certain accesses are observable to the abstract machine | Access is atomic, synchronized, or coherent with GPU/DMA |
| `restrict` | Programmer promises restricted aliasing for accesses through that pointer | Compiler will validate the promise |
| C atomics / locks | Access follows defined synchronization rules, if supported/used correctly | GPU cache management is handled automatically |

**Never use `volatile` as a thread synchronization mechanism.** It does not establish the ordering and visibility guarantees of a lock or atomic operation. For actual threads, use the synchronization API provided by the target environment, and document which thread owns each mutable object. [documented]

`restrict` can help a measured inner loop, but using it when buffers overlap can trigger undefined behavior. Start without it; add it only when its non-aliasing contract is true and tested. [documented]

## 11. The preprocessor is not a second programming language

Prefer `enum`, `static const` (where a runtime object is acceptable), ordinary functions, and `static inline` to function-like macros.

**Don't:**

```c
#define SQUARE(x) ((x) * (x))
int y = SQUARE(i++);  /* i is modified twice: undefined behavior. */
```

**Do:**

```c
static inline int square_int(int value) {
    return value * value;
}
```

The function avoids repeated evaluation but **still requires** that the multiplication not overflow `int`.

Use macros for include guards, compile-time feature switches, and cases genuinely requiring token substitution. If a macro is unavoidable, fully parenthesize operands, avoid multiple evaluations, and document side effects. Use `do { ... } while (0)` for multi-statement macros used as a statement.

**Platform switches:** Prefer selecting one translation unit in the build system instead of wrapping every function in `#if defined(__3DS__)` / `__SWITCH__`. Only one place should decide how to build each platform.

## 12. State, initialization, and globals

Prefer explicit state objects to hidden mutable globals:

```c
typedef struct {
    uint32_t frame_number;
    bool paused;
} game_state;

void game_update(game_state *state, float delta_seconds);
```

This supports tests, multiple instances, and reasoning about call order. Avoid a universal `game_context *` passed to every function when each function needs only two fields.

- Define a valid zero/empty state where practical, but do not claim every API object can be initialized with `memset(0)`.
- Give initialization, reset, and shutdown different names and semantics.
- Make partial initialization safe to clean up, or explicitly forbid calling cleanup before success.
- Keep mutable state in the narrowest scope: local → object → module → global, in that order.
- Static immutable lookup tables are often appropriate; a single mutable global renderer that silently owns everything usually is not.

## 13. Data-oriented design without sacrificing clarity

A 3DS or Switch game may justify fixed capacities, compact representations, and allocation-free frame loops. None of these requires unreadable code.

### Capacity is part of the object invariant

```c
typedef struct {
    sprite *items;
    size_t count;
    size_t capacity;
} sprite_list;
```

Maintain `count <= capacity` after **every** operation. A function that appends must check capacity before writing, then increment count only after the write succeeds. Choose whether full capacity is an error, a flush point, or a documented truncation; never silently overwrite memory.

### Distinguish storage policy from game policy

A frame arena can be good for temporary CPU data; persistent textures have different lifetimes. Do not turn every allocation into a generic allocator framework. Document allocator alignment, lifetime, memory class, and cleanup, especially for buffers shared with graphics or system services.

### Preserve a readable reference implementation

When replacing a plain loop with SIMD, fixed-point arithmetic, lookup tables, or hand-written assembly, keep a clear statement of the original mathematical contract and tests that compare implementations. Hardware-specific fast paths may coexist with a portable, correct path.

## 14. Separate the game from the hardware

A platform module owns OS calls, graphics/device allocations, filesystem mounts, input polling, cache maintenance, and lifecycle integration. Game code should consume simple contracts.

```c
/* platform/video.h */
#include <stdbool.h>

typedef struct draw_list draw_list;
typedef struct platform_video platform_video;

bool platform_video_begin_frame(platform_video *video);
bool platform_video_submit(platform_video *video, const draw_list *list);
void platform_video_end_frame(platform_video *video);
```

The contract must state whether commands are **copied** or **borrowed**, when submitted memory may be reused, what happens on failed submission, and whether calls must occur on one thread. Merely giving the API three tidy functions does not make it safe.

### 3DS: Old 3DS is the baseline

[documented] libctru targets 3DS ARM11 user-mode code; the reference devkitARM Makefile uses ARMv6K/`mpcore` target flags. The example build system also offers platform graphics libraries and asset conversion steps; keep those in the platform/asset layer rather than in game logic.

- Budget and test against **Old 3DS first**. **New 3DS:** Additional resources may permit a separate fast path, but do not silently rely on them.
- Prefer simple bounded work, reusable scratch storage, and clear object ownership, especially in render and asset code. **Do not assert this is faster without a workload-specific measurement.**
- CPU-accessible memory, linear memory, VRAM, GPU-visible data, and service-owned buffers are not interchangeable. Use allocation and transfer APIs according to their stated requirements.
- A CPU write followed by a GPU or system transfer can require cache maintenance and explicit completion ordering. Document who writes a buffer, who reads it, and when it may be reused. Do not hide transfer synchronization behind a generic `volatile` pointer.
- Name ownership and lifetime of GPU command lists, staging buffers, tile/palette data, and persistent textures. Record buffer size and alignment alongside the allocation.
- For timing-sensitive code, test both gameplay and transitions on hardware; smoothness cannot be established from code appearance.

### Switch: AArch64 is not desktop x86-64

[documented] The libnx example build targets ARMv8-A and tunes for Cortex-A57. Pointers and object sizes in the Switch build differ from 32-bit 3DS assumptions; audit pointer/integer casts, file layouts, serialization, and allocation arithmetic.

- Use devkitA64/libnx APIs for system interfaces rather than expecting a Linux process, desktop signals, or unrestricted POSIX behavior.
- Device-facing buffers have their own alignment, cache, and lifetime requirements. In libnx, `armDCacheClean` and `armDCacheFlush` are different operations; the latter includes invalidation. Select the operation required by the **consumer and direction of ownership**, not by a rule to "flush everything." [documented]
- Separate CPU preparation, GPU submission, presentation, and completion. A successful submission is not necessarily permission to reuse the submitted data.
- Avoid allocations and blocking file I/O in a time-sensitive loop where feasible, but measure the actual impact on Switch hardware before introducing special cases.

### Shared rule for both consoles

**A buffer is not ready because its pointer exists.** Record these four facts at every CPU↔device boundary:

1. Who owns the storage and which allocator created it?
2. Who last wrote it, and in what representation?
3. What visibility/synchronization action is required before the next reader?
4. What event proves the previous reader is finished so it may be reused or freed?

Do not take the contents of an asynchronously changing buffer as a stable signature without a defined snapshot point. For example, hashing renderer-visible data **after** drawing it does not necessarily describe the data that was actually drawn.

## 15. Optimize with evidence, not folklore

- State a **baseline** and one target metric: frame time, CPU time per subsystem, number of allocations, memory usage, or bytes transferred.
- Reproduce on the same console model, build configuration, scene, and workload. Separate Old 3DS and New 3DS observations.
- Profile the whole frame before rewriting a small loop. Be suspicious of expensive allocations, repeated conversions, redundant work, and poor data access patterns.
- Prefer algorithmic savings and eliminating work over hand-written assembly.
- Do not guess that `inline`, `restrict`, bit tricks, smaller integers, `memcpy`, or branch-free code are inherently faster. Evaluate generated code and performance only after correctness tests pass.
- Never justify undefined behavior as a performance optimization; higher optimization levels can make the resulting failure harder to reproduce.
- Leave performance-focused deviations with a comment naming the measured workload, target, comparison, and reason the clearer implementation was insufficient.

**Example of a useful optimization comment:**

```c
/* Old 3DS, scene benchmark X, release flags Y:
 * contiguous tile walk reduced update time from A to B (measurement link).
 * Keep traversal order; the renderer assumes deterministic tile ordering.
 */
```

Replace placeholders with actual measurements before publishing this as a claim. Never invent numbers for documentation.

## 16. Comments explain constraints and decisions

Write comments for *why* the code does something non-obvious, especially when an apparent simplification would break a contract.

**Poor:**

```c
count++;  /* Increment count. */
```

**Useful:**

```c
/* Preserve source order: replay checksums depend on deterministic traversal. */
count++;
```

Comment on:

- Borrowed pointer lifetimes and thread ownership.
- Units, valid ranges, alignment, and endianness.
- Non-obvious hardware requirements, with a source or reproducible test.
- Error state and cleanup guarantees.
- Algorithmic invariants, not line-by-line implementation narration.

Avoid decorative banners, giant history comments, TODOs with no owner or issue, and claims that are contradicted by the code. Public functions should document parameters and failure behavior where the signature cannot express them.

## 17. Tests, warnings, and tooling

### Compile warnings

As a reasonable **starting set**, add to the project's existing devkitPro flags:

```make
CFLAGS += -std=c11 -Wall -Wextra -Wshadow -Wundef \
          -Wstrict-prototypes -Wmissing-prototypes \
          -Wpointer-arith -Wformat=2
```

Adapt to the project's C dialect and verify every flag with the **installed** target GCC. `-Wconversion` and `-Wsign-conversion` are valuable during focused audits but can be noisy in legacy code and third-party headers. Do not blindly paste host flags that override devkitPro architecture or linker settings.

- Keep warnings clean for first-party code. Consider `-Werror` in controlled CI builds after the warning set is stable; do not necessarily impose it on vendor libraries.
- Include each public header as the first include in its implementation to expose missing dependencies.
- Run `clang-format --dry-run --Werror` (or an equivalent formatting check) on first-party C files; do not run an unpinned formatter blindly over a repository.
- If available, build **portable modules** with host Clang/GCC under address/undefined-behavior sanitizers and unit tests. Sanitizer success on a host does **not** verify console cache semantics or device API use.
- Run actual devkitARM and devkitA64 builds. Review their warnings, artifacts, and link maps. Run representative workloads on hardware.

### What to test

| Area | Tests that find real bugs |
| --- | --- |
| Binary reader | Truncated input, missing fields, wrong endian tag, invalid offsets, overflowed lengths |
| Container | 0, 1, exactly capacity, capacity + 1; growth failure; repeated reset |
| Lifecycle | Init failure at every allocation, cleanup twice where promised safe, reload after error |
| Renderer | Empty draw list, max-size draw list, resize, context loss where applicable, delayed completion |
| Concurrency | Producer/consumer ordering, stop while waiting, asset replacement in flight |
| Gameplay | Deterministic updates under recorded input, pause/resume, save/load boundaries |
| Portability | Same format round-trips on host, 3DS and Switch; explicit 32/64-bit assumptions |

Prefer tests around invariants, not tests that merely mirror implementation lines. When fixing a bug, retain a small regression test that fails for the original reason.

## 18. Review checklist

Before accepting C code, a reviewer should be able to answer **yes** to these questions:

- [ ] Can I tell what every exported function promises and what happens on failure?
- [ ] Is each pointer's ownership and valid lifetime clear?
- [ ] Are every buffer length, element count, stride, and capacity expressed with unambiguous units?
- [ ] Are arithmetic, indexing, and narrowing casts safe for malformed or maximum inputs?
- [ ] Are partially acquired resources released on every failure path?
- [ ] Is each global or public symbol necessary?
- [ ] Is the work divided by responsibility without pointless one-function wrapper modules?
- [ ] Are comments about constraints and decisions, rather than restating the syntax?
- [ ] Are external file and network formats decoded without unproven layout assumptions?
- [ ] Are thread ownership, synchronization, cache visibility, and device completion explicit where needed?
- [ ] Are target-specific changes isolated and justified by a real platform requirement?
- [ ] Does the code build warning-free with the selected target toolchain, and are tests appropriate to the claimed support?
- [ ] If readability was traded for speed, is there a reproducible benchmark showing why?

**Code is not maintainable just because it is formatted.** Maintainability means the next engineer can determine what is valid, what can fail, and what can safely change.

## Technical details

### What this guide deliberately does not standardize

This is a guide for *game application C*, not a fork of the Linux kernel conventions, MISRA C certification process, or a single rigid source layout. These choices are intentionally left to each project:

- Whether exported types use `_t`, with the portability caveat above.
- Whether a subsystem returns its result by value, out parameter, or a richer status type.
- Whether an opaque object has a heap allocation or caller-supplied storage.
- Whether source files are grouped by feature or by platform inside each feature.
- Whether a proven hot path uses fixed-point math, NEON, assembly, or a software renderer.

Document the choice once per project. Do not combine two conventions within the same subsystem without a concrete reason.

### Verification and known limits

- The library names and target architecture examples were checked against the publicly available upstream libctru, libnx, and example Makefiles listed below on **2026-11-11**. Compiler and library package versions in an individual devkitPro installation may differ.
- No code fragment in this guide was verified on console hardware; examples of good practices are not performance measurements.
- This page does not certify adherence to ISO C, CERT C, or MISRA C. Where a formal requirement exists, consult that standard directly and add appropriate tooling.
- Whether C11 atomics are supported and appropriate for a particular cross-compiled runtime is an **environment-specific question**; a successful parse of `<stdatomic.h>` alone is not proof that a chosen operation is lock-free or appropriate for a GPU/OS boundary.

## Pitfalls

| Symptom | Likely design problem | Correction |
| --- | --- | --- |
| Crashes only in optimized builds | Out-of-bounds access, invalid lifetime, aliasing or overflow assumption | Reproduce with host sanitizers, inspect contracts, then check target code |
| Works on Switch but not Old 3DS | Hidden 64-bit assumption, resource budget, alignment or platform-specific behavior | Check widths and platform layer; test Old 3DS explicitly |
| Rarely corrupted textures | CPU/device visibility or premature buffer reuse | Track producer, cache action, submission and completion |
| Long cleanup chains with leaks | Ownership spread across callers | Consolidate initialization and destruction contracts |
| Hundreds of tiny forwarding functions | Abstraction by naming rather than responsibility | Remove wrappers that provide no boundary or invariant |
| Every game system includes platform headers | Device details leak above the platform layer | Narrow the adapter API and move calls down |
| Fast but impossible to review | Unmeasured tricks and undocumented invariants | Keep baseline, benchmark and record constraints |

## Open questions

Before making this a *binding* standard for a particular game, record: compiler versions on both consoles; the chosen C dialect; which warnings remain clean; how host tests run; how allocations/GPU buffers are managed; and how the project captures hardware benchmarks. These are project decisions and must not be invented by the wiki.

## Sources

- [Coffeepot contributor guidance (`AGENTS.md`)](https://github.com/estyseesghosts/coffeepot/blob/main/AGENTS.md) — document organization, evidence labels, Old 3DS baseline, and verification requirements.
- [devkitPro libctru](https://github.com/devkitPro/libctru) — 3DS user-mode library and devkitARM support.
- [devkitPro 3DS application Makefile](https://github.com/devkitPro/3ds-examples/blob/master/templates/application/Makefile) — ARMv6K target and baseline compiler flags.
- [devkitPro libctru synchronization.h](https://github.com/devkitPro/libctru/blob/master/libctru/include/3ds/synchronization.h) — light locks and synchronization API.
- [switchbrew Switch application Makefile](https://github.com/switchbrew/switch-examples/blob/master/templates/application/Makefile) — ARMv8-A/Cortex-A57 target and application build flags.
- [libnx cache API](https://switchbrew.github.io/libnx/cache_8h.html) — cache clean and flush semantics.
- [libnx mutex implementation](https://github.com/switchbrew/libnx/blob/master/nx/source/kernel/mutex.c) — platform synchronization implementation.
- [GCC optimization manual](https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html) — strict aliasing and optimization constraints.
- [C `restrict` reference](https://en.cppreference.com/c/language/restrict) — aliasing contract and undefined behavior risks.
- [SEI CERT C coding standard](https://cmu-sei.github.io/secure-coding-standards/sei-cert-c-coding-standard/) — preprocessor, integer, memory, expression, and other secure-coding rules.
- [Linux kernel coding style](https://www.kernel.org/doc/html/latest/process/coding-style.html) — reference for naming, scope, function structure, and kernel-oriented tradeoffs (not adopted verbatim).
- [ClangFormat documentation](https://clang.llvm.org/docs/ClangFormat.html) and [style options](https://clang.llvm.org/docs/ClangFormatStyleOptions.html) — formatting automation and configuration.
