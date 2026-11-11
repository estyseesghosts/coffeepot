# Writing a correct service client (IPC)

Almost everything a 3DS program does beyond computing — reading the SD card, asking which console model it runs on, playing sound, opening a socket — is done by sending a small message to a system **service** and reading the reply. libctru wraps the common services, but sooner or later you need a command it does not wrap, or you need to understand why a wrapper misbehaves. This page shows how to write a correct client from scratch: how the message is laid out, what the two different "result codes" mean, how buffers and handles travel inside a message, when to close a session, and how to test the failure paths without crashing the console. The worked example is one real command, `cfg:u` `GetSystemModel`.

Background and the list of services: [CTR services](../software/services.md). The system calls involved are described on the [kernel](../software/kernel.md) page; the memory a message points into is covered in [memory and cache](memory-and-cache.md).

## The whole thing in 10 lines

1. Get a session: `srvGetServiceHandle(&h, "name")` (names are at most 8 characters). Zero `h` first.
2. Take the command buffer: `u32 *cmd = getThreadCommandBuffer();`
3. Write the header with `IPC_MakeHeader(id, normalWords, translateWords)`.
4. Write the normal parameters, then each translate parameter (descriptor word, then its pointer or handles).
5. Send immediately: `svcSendSyncRequest(h)`. Do no other IPC-using call between filling the buffer and sending.
6. If that returns an error, stop: the reply is not valid.
7. Otherwise read `cmd[1]`, the service's own result; if it is an error, the output words are not valid.
8. Only then read the outputs, immediately.
9. Close the session exactly once with `svcCloseHandle`, and zero your variable.
10. Never send a descriptor the kernel documents as fatal (rights 0, oversized static buffer, move plus process ID).

## Before you start

- Checked against libctru 2.7.1 at commit `62b7c42c88b1ee837f513358409d2e5c8f9001a9` (the pin in [source versions](source-versions.md)), read from a local clone at that commit, and against 3dbrew's *IPC* page (the old name *IPC Command Structure* redirects there), fetched in November 2026 (re-read on 10 November 2026 as raw wikitext; the local `3dbrew` mirror does not contain the IPC, Config Services, HTTP or SSL pages).
- Code compiles with devkitARM r68 (GCC 16.1.0) and libctru 2.7.1-1 with `-Wall -Wextra`, no warnings. **Nothing here has been run on hardware or in an emulator.** See [Verification status](#verification-status).
- Terms used below, defined once: a **session** is a private two-ended message channel between your process and a service; the **handle** is the number your process uses to refer to its end of it. The **command buffer** is a 64-word area in each thread's **thread-local storage** (TLS, a small per-thread memory page) where a message is written. The **kernel** copies or rewrites parts of the message on its way to the service (see [IPC](../../glossary.md#ipc) and [service](../glossary.md#service)).

## The shape of every request

A request is an array of 32-bit words written to the command buffer at `TLS + 0x80`, then sent with the system call `svcSendSyncRequest(handle)`. When the call returns, the service's reply has been written over the same buffer [reverse-engineered: 3dbrew IPC, "Message Structure"; documented: libctru `svc.h`, `getThreadCommandBuffer()` returns `TLS + 0x80`].

| Word | Contents |
| ---- | -------- |
| 0 | Header: command ID, number of normal words, number of translate words |
| 1 .. x | Normal parameters: plain 32-bit values, delivered unchanged |
| 1+x .. | Translate parameters: each starts with a descriptor word; the kernel rewrites these |

The header is built by `IPC_MakeHeader(command_id, normal_params, translate_params)` in `<3ds/ipc.h>` [documented: libctru `include/3ds/ipc.h`]:

| Bits | Field |
| ---- | ----- |
| 31..16 | Command ID |
| 15..12 | Unused |
| 11..6 | Number of normal parameter words (0..63) |
| 5..0 | Total size in words of the translate parameters, **including their descriptor words** (0..63) |

So `IPC_MakeHeader(0x5, 0, 0)` is `0x00050000`, and `IPC_MakeHeader(0x1, 2, 2)` is `0x00010082`. The same layout is used for replies, where word 1 is by convention the service's result code [reverse-engineered: 3dbrew IPC].

The most common mistake is counting translate words wrong: a mapped-buffer parameter is **two** words (descriptor plus pointer), so a command with one buffer has `translate_params = 2`, not 1.

## Worked example: `cfg:u` GetSystemModel

`GetSystemModel` asks the config service which console model this is. 3dbrew lists it as `cfg:u` command `0x00050000`, available from system version 5.0.0-11, with no request parameters and one reply value: a `u8` model [reverse-engineered: 3dbrew Config Services and `Cfg:GetSystemModel`]. libctru's implementation is the reference [documented: libctru `source/services/cfgu.c` at the pinned commit]:

```c
Result CFGU_GetSystemModel(u8* model)
{
	Result ret = 0;
	u32 *cmdbuf = getThreadCommandBuffer();

	cmdbuf[0] = IPC_MakeHeader(0x5,0,0); // 0x50000

	if(R_FAILED(ret = svcSendSyncRequest(cfguHandle)))return ret;

	*model = (u8)cmdbuf[2] & 0xFF;

	return (Result)cmdbuf[1];
}
```

Model values (reply word 2; values 0, 1 and 3 are Old 3DS family models and 2, 4 and 5 are New 3DS family models; the Model column keeps libctru's `CFG_SystemModel` names) [documented: libctru `cfgu.h` `CFG_SystemModel`; reverse-engineered: 3dbrew]:

| Value | Model | Codename |
| ----- | ----- | -------- |
| 0 | Old 3DS | CTR |
| 1 | Old 3DS XL | SPR |
| 2 | New 3DS | KTR |
| 3 | Old 2DS | FTR |
| 4 | New 3DS XL | RED |
| 5 | New 2DS XL | JAN |

Written as a standalone client without the libctru `cfgu` wrapper (builds; not run):

```c
#include <3ds.h>
#include <stdio.h>

// One raw cfg:u call, written out by hand (no libctru cfgu wrapper).
// Command 0x5 = GetSystemModel: request header only, reply = header, result, u8 model.
static Result rawGetSystemModel(Handle session, u8 *model)
{
    u32 *cmdbuf = getThreadCommandBuffer();     // TLS + 0x80
    cmdbuf[0] = IPC_MakeHeader(0x5, 0, 0);      // 0x00050000

    Result transport = svcSendSyncRequest(session);
    if (R_FAILED(transport))
        return transport;                       // the request never completed

    Result service = (Result)cmdbuf[1];         // the service's own verdict
    if (R_FAILED(service))
        return service;                         // cmdbuf[2] is not valid output

    *model = (u8)(cmdbuf[2] & 0xFF);
    return 0;
}

int main(void)
{
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

    Handle cfg = 0;
    Result rc = srvGetServiceHandle(&cfg, "cfg:u");   // libctru's __appInit already ran srvInit()
    if (R_SUCCEEDED(rc)) {
        u8 model = 0xFF;
        rc = rawGetSystemModel(cfg, &model);
        if (R_SUCCEEDED(rc))
            printf("model id %u\n", model);
        svcCloseHandle(cfg);                          // you opened the session, you close it
    }
    if (R_FAILED(rc))
        printf("failed: 0x%08lX (%s)\n", (unsigned long)rc, osStrError(rc));

    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & KEY_START) break;
        gspWaitForVBlank();
    }
    gfxExit();
    return 0;
}
```

What to expect: it prints `model id N` with N from the table. How it fails: if the service handle cannot be obtained, `rc` is the `srv:` error (see [GetServiceHandle](#getting-a-session-srv-getservicehandle)); if the command is unknown to the running system version, the service should reply with an error in word 1 [hypothesis: 3dbrew lists the command as added in 5.0.0-11; the exact error on older systems is unrecorded — it would be settled by sending `0x00050000` on a console below 5.0.0, see Open questions].

One deliberate difference from libctru: the wrapper writes `*model` **before** it returns `cmdbuf[1]`, so after an error the output variable holds `cmdbuf[2]`, which is not meaningful. The example above only touches the output on success.

### Transport result versus service result

There are two independent results, and a correct client checks both, in order:

1. **Transport result** — the return value of `svcSendSyncRequest`. It says whether the kernel delivered the request and got a reply. If it failed, the command buffer holds no valid reply, so do not read word 1 or later [documented: every wrapper in libctru, e.g. `cfgu.c`, returns immediately on this failure].
2. **Service result** — `cmdbuf[1]` of the reply. It is the service's verdict on the command: bad argument, wrong state, not allowed, and so on. Output words (`cmdbuf[2]...`) are only meaningful if this is a success [reverse-engineered: 3dbrew IPC says the second reply word is by convention an error code, 0 on success].

Both are 32-bit `Result` values; negative means failure (`R_FAILED`). `<3ds/result.h>` exposes the bit fields: `R_LEVEL` (bits 31..27), `R_SUMMARY` (26..21), `R_MODULE` (counted by `R_MODULE(res)` from bit 10, 8 bits) and `R_DESCRIPTION` (low 10 bits) [documented: libctru `result.h`]. `osStrError(rc)` turns the summary into text. Keep the raw hex in your logs: module and description identify the service and the reason, and 3dbrew's error-code pages list many of them.

## Header encoding in practice

Compute the header with the helpers, never by hand. These values are the ones the `ipc.h` helpers produce and the ones libctru's own comments quote; they were checked on a PC (see [Verification status](#verification-status)):

| Expression | Value | Used by |
| ---------- | ----- | ------- |
| `IPC_MakeHeader(0x5,0,0)` | `0x00050000` | `cfg:u` GetSystemModel |
| `IPC_MakeHeader(0x1,2,2)` | `0x00010082` | `cfg:u` GetConfigInfoBlk2 (one mapped buffer) |
| `IPC_MakeHeader(0x1,1,4)` | `0x00010044` | `soc:U` and `http:C` Initialize (process ID + shared handle) |
| `IPC_MakeHeader(0x1E,3,4)` | `0x001E00C4` | `http:C` GetResponseHeader (static buffer + mapped buffer) |

## Buffers and handles inside a message

Translate parameters are how a message refers to memory and kernel objects. Each begins with a descriptor word; the kernel validates it and rewrites the following word(s) for the receiver. Descriptor builders in `<3ds/ipc.h>` [documented: libctru `ipc.h`; semantics reverse-engineered: 3dbrew IPC]:

| Builder | Descriptor | Words that follow | What the kernel does |
| ------- | ---------- | ----------------- | -------------------- |
| `IPC_Desc_Buffer(size, rights)` | `(size<<4) \| 8 \| rights` | pointer | **Mapped buffer**: maps your memory into the service for the duration of the call |
| `IPC_Desc_StaticBuffer(size, id)` | `(size<<14) \| (id<<10) \| 2` | pointer | **Static buffer**: copies `size` bytes into a buffer the receiver pre-registered under `id` |
| `IPC_Desc_SharedHandles(n)` | `(n-1)<<26` | n handles | Copies the handles; you keep yours |
| `IPC_Desc_MoveHandles(n)` | `((n-1)<<26) \| 0x10` | n handles | Gives the handles to the receiver and closes yours |
| `IPC_Desc_CurProcessId()` | `0x20` | 1 placeholder word | Kernel writes your process ID into the placeholder |
| `IPC_Desc_PXIBuffer(...)` | `(size<<8) \| (id<<4) \| 4 or 6` | pointer | Physical-address table for the ARM9 side; used by `fs`/PXI code, rarely by applications |

### Mapped buffers

Use a mapped buffer for anything big or variable-size. `IPC_BUFFER_R` (bit 1) means the service may read it, `IPC_BUFFER_W` (bit 2) means it may write it, `IPC_BUFFER_RW` both. The descriptor values are therefore `0xA`, `0xC` and `0xE` with the size in bits 4 and up. A real command with an output buffer is `cfg:u` GetConfigInfoBlk2 (`0x00010082`): parameters are the size and the config block ID, and the service fills your buffer [documented: libctru `cfgu.c` `CFGU_GetConfigInfoBlk2`]:

```c
cmdbuf[0] = IPC_MakeHeader(0x1,2,2); // 0x10082
cmdbuf[1] = size;
cmdbuf[2] = blkID;
cmdbuf[3] = IPC_Desc_Buffer(size,IPC_BUFFER_W);
cmdbuf[4] = (u32)outData;
```

Rules from the 3dbrew description of the mechanism [reverse-engineered: 3dbrew IPC]:

- The service sees the buffer at a virtual address from `0x04000000` upward, always read-write there, whatever rights you gave. Your rights are checked against your own mapping, not enforced afterwards.
- The first and last partial pages are copied through kernel-allocated memory; whole pages in between are mapped directly. A buffer that starts and ends on a `0x1000` boundary is mapped without copying. For small buffers the cost is therefore a copy, not a mapping.
- A descriptor with access bits 0 (that is, `0x8` alone) causes a kernel panic. Never build a descriptor from a rights value you did not take from `IPC_BufferRights`.
- The call is synchronous, so the buffer is no longer in use when `svcSendSyncRequest` returns. If another thread of yours can modify the same memory while the call is in flight, that is a data race you own.

### Static buffers

A static buffer is a copy, not a mapping. The *receiver* registers up to 16 buffers in its own TLS at offset `0x180 + id*8` (`getThreadStaticBuffers()` in libctru's `svc.h`), each entry being a descriptor of the same format plus a pointer. A sender then names the `id`; the kernel copies the data to the registered address. Asking for more bytes than the registered buffer holds panics the kernel [reverse-engineered: 3dbrew IPC].

As a client you use static buffers in two ways, and libctru's `soc` code shows both in one command. `poll()` sends the pollfd array through static buffer 10 and receives the updated array back through static buffer 0, which it must register in its own TLS first and restore afterwards [documented: libctru `source/services/soc/soc_poll.c`]:

```c
cmdbuf[0] = IPC_MakeHeader(0x14,2,4); // 0x140084
cmdbuf[1] = (u32)nfds;
cmdbuf[2] = (u32)timeout;
cmdbuf[3] = IPC_Desc_CurProcessId();
cmdbuf[5] = IPC_Desc_StaticBuffer(size,10);
cmdbuf[6] = (u32)tmp_fds;

u32 * staticbufs = getThreadStaticBuffers();
saved_threadstorage[0] = staticbufs[0];
saved_threadstorage[1] = staticbufs[1];

staticbufs[0] = IPC_Desc_StaticBuffer(size,0);
staticbufs[1] = (u32)tmp_fds;

ret = svcSendSyncRequest(SOCU_handle);

staticbufs[0] = saved_threadstorage[0];
staticbufs[1] = saved_threadstorage[1];
```

The save and restore matter: the static buffer table is per thread and other libctru code on the same thread may rely on its entries. `http:C` GetResponseHeader mixes both kinds: the header name goes in static buffer 3, the output goes to a mapped buffer [documented: libctru `source/services/httpc.c`].

### Handle copy and move

`IPC_Desc_SharedHandles(n)` duplicates handles into the receiver; `IPC_Desc_MoveHandles(n)` also closes yours. A handle value of 0 is passed through as 0 without a transfer. Setting both the "move" and "process ID" bits in one descriptor panics the kernel [reverse-engineered: 3dbrew IPC]. Do not pass `n = 0`: the builders compute `(n - 1) << 26` in unsigned arithmetic, which for 0 produces a descriptor claiming 64 handles [derived from `ipc.h`].

Examples in libctru: `soc:U` and `http:C` Initialize *copy* a memory-block handle so the service can map your memory [documented: `soc_init.c`, `httpc.c`]; `am` ImportTwlBackup *moves* a file handle so the service owns it afterwards [documented: `source/services/am.c`, `AM_ImportTwlBackup`]. After a move, treat your copy of the handle as closed: do not use it and do not close it again.

### Process ID translation

Some services want proof of who is calling. `IPC_Desc_CurProcessId()` followed by one placeholder word makes the kernel write your process ID into the placeholder; you cannot lie about it. The pair counts as 2 translate words in the header. `SOCU_Initialize` shows the whole pattern: header `(0x1,1,4)` is one normal word, then two translate pairs (process ID, shared handle) [documented: libctru `soc_init.c`]:

```c
cmdbuf[0] = IPC_MakeHeader(0x1,1,4); // 0x10044
cmdbuf[1] = memsize;
cmdbuf[2] = IPC_Desc_CurProcessId();
cmdbuf[4] = IPC_Desc_SharedHandles(1);
cmdbuf[5] = memhandle;
```

`cmdbuf[3]` is the placeholder and is left untouched.

## Getting a session: srv: GetServiceHandle

Services are not connected to directly. You ask the service manager over the global port `srv:` [reverse-engineered: 3dbrew IPC, "Services"]. libctru's `srvInit()` (run for you by the default `__appInit`) connects to `srv:` and registers the process with `RegisterClient` (`0x00010002`) [documented: libctru `source/srv.c`].

`srvGetServiceHandle(&handle, "cfg:u")` then does this [documented: libctru `srv.c`; request layout from 3dbrew `SRV:GetServiceHandle`]:

1. Look in the service list the loader handed to the process (`envGetHandle`). Homebrew started from the Homebrew Menu may receive some service handles that way; if the name is found, the handle is duplicated and returned with no `srv:` call.
2. Otherwise send `0x00050100`: words 1-2 hold the name as 8 bytes, word 3 the name length, word 4 the flags (bit 0: return an error at once if the service has no free session slot, instead of waiting). libctru sets the flag from the per-thread policy `srvSetBlockingPolicy()`; the default is to wait.
3. On success word 3 of the reply is your session handle. On failure libctru writes 0 to `*out`.

Consequences for a client:

- **Names are at most 8 characters.** libctru copies with `strncpy(..., 8)` and `strnlen(name, 8)`; a longer name is silently cut, so you would ask for a different service. Check the length yourself (the wrapper below does).
- **Access is checked per service name.** The kernel-level check uses the service-access list in the title's extended header, so a program can be denied a service it names correctly [reverse-engineered: 3dbrew IPC]. Under [Luma3DS](../homebrew/luma3ds.md) the service manager is replaced to lift these limits ([services](../software/services.md#system-titles-that-implement-services)). The exact error for a denied or unknown name is not recorded here; see Open questions.
- **Fallback chains are normal.** `cfguInit` tries `cfg:i`, then `cfg:s`, then `cfg:u`, because the first has the most commands [documented: libctru `cfgu.c`]. If you need only a `cfg:u` command, open `cfg:u` only: fewer privileges, and a failure means something else.
- Before system version 11.0 the kernel filled the output handle with junk when `svcConnectToPort` failed [documented: comment in libctru `srvInit`]. `srvGetServiceHandleDirect` itself writes 0 to `*out` on any failure, but the other path of `srvGetServiceHandle` (a handle from the loader's service list, duplicated with `svcDuplicateHandle`) writes nothing on failure [documented: `srv.c`]. So zero a handle variable before the call and only use or close it after success.

## Session lifetime and closing

A session handle is a kernel object that you own until you close it with `svcCloseHandle`. Rules:

- **One owner, one close.** Close each session exactly once, from a thread that is not mid-call on it, then set your variable to 0. Never reuse a closed handle number: handle values can be handed out again.
- **Reference-count shared sessions.** libctru's `cfguInit`/`cfguExit` use an atomic counter so that only the first init opens the session and only the last exit closes it [documented: `cfgu.c`]. Copy that pattern if two parts of your program need the same service.
- **Sessions are a finite resource.** A service accepts a limited number of simultaneous sessions. With the default blocking policy, `GetServiceHandle` waits for a free slot [reverse-engineered: 3dbrew `SRV:GetServiceHandle` flag; documented: libctru policy default], so a leak shows up as a program that hangs on its Nth open, not as an error. `httpcOpenContext` opens a **new** session per context (and `httpcCloseContext` closes it), so an HTTP loop that forgets to close contexts leaks sessions [documented: libctru `httpc.c`].
- **What the other side sees.** When the last client handle closes, the server's next `svcReplyAndReceive` returns the error `0xC920181A` ("session closed by remote") [reverse-engineered: 3dbrew IPC server example]. You do not have to tell the service you are leaving; some services nevertheless have their own finalize command (`http:C` Finalize `0x00390000`, `soc:U` Shutdown `0x00190000`) that libctru's `*Exit` functions send *before* closing.
- **Shared memory handed to a service stays tied to the session.** For `soc:U`, the buffer given to `socInit` is unusable by your process until `socExit` ([networking](networking.md#socu-initialisation)).

## A more advanced wrapper

The wrapper below adds what repeated hand-written calls need: it validates the service name length, refuses to send on a closed session, folds both results into one, and validates buffer arguments *before* they reach the kernel. It then implements `GetConfigInfoBlk2` on top (builds; not run):

```c
#include <3ds.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    Handle handle;        // 0 when closed
} Session;

// Service names are sent as 8 bytes; srvGetServiceHandle copies at most 8 characters,
// so a longer name would silently be cut.
static Result sessionOpen(Session *s, const char *name)
{
    s->handle = 0;
    if (strlen(name) > 8)
        return MAKERESULT(RL_PERMANENT, RS_INVALIDARG, RM_APPLICATION, RD_OUT_OF_RANGE);
    return srvGetServiceHandle(&s->handle, name);
}

static void sessionClose(Session *s)
{
    if (s->handle) {
        svcCloseHandle(s->handle);
        s->handle = 0;
    }
}

// Sends whatever is already in the command buffer and folds the two result codes into one.
static Result sessionSend(const Session *s)
{
    if (!s->handle)
        return MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, RM_APPLICATION, RD_NOT_INITIALIZED);
    Result transport = svcSendSyncRequest(s->handle);
    if (R_FAILED(transport))
        return transport;
    return (Result)getThreadCommandBuffer()[1];
}

#define CFG_BLOCK_MAX 0x1000u   // our own sanity limit, not a service limit

// Reads a config block (e.g. blkID 0x000A0002 = language) into out[0..size).
static Result cfgReadBlock(const Session *s, u32 blkID, void *out, u32 size)
{
    if (!out || size == 0 || size > CFG_BLOCK_MAX)
        return MAKERESULT(RL_PERMANENT, RS_INVALIDARG, RM_APPLICATION, RD_OUT_OF_RANGE);

    u32 *cmdbuf = getThreadCommandBuffer();
    cmdbuf[0] = IPC_MakeHeader(0x1, 2, 2);               // 0x00010082
    cmdbuf[1] = size;                                    // normal parameter 1
    cmdbuf[2] = blkID;                                   // normal parameter 2
    cmdbuf[3] = IPC_Desc_Buffer(size, IPC_BUFFER_W);     // translate: mapped buffer, service writes
    cmdbuf[4] = (u32)out;
    return sessionSend(s);
}

int main(void)
{
    Session cfg;
    Result rc = sessionOpen(&cfg, "cfg:u");
    u8 language = 0xFF;
    if (R_SUCCEEDED(rc))
        rc = cfgReadBlock(&cfg, 0xA0002, &language, sizeof language);
    sessionClose(&cfg);
    return R_SUCCEEDED(rc) ? language : 1;
}
```

Block `0xA0002` is the language byte; libctru's `CFGU_GetSystemLanguage` calls `CFGU_GetConfigInfoBlk2(1, 0xA0002, language)` [documented: `cfgu.c`]. Fill the command buffer **immediately** before sending (as `cfgReadBlock` does) and compute everything else first: the buffer is one per thread, and any libctru call between filling it and sending that itself does IPC — including `printf` when output is routed over a socket ([debugging](debugging.md#logging-over-the-network-3dslink)) — overwrites it.

## Testing a client

Test design, from cheapest to riskiest. Only the first group is safe by construction; the second asks the real service questions whose answers this wiki has not recorded.

**1. Pure tests, no console.** Header and descriptor builders are `static inline` arithmetic. Compile a copy of the body of `ipc.h` on the PC and assert the values in the [table above](#header-encoding-in-practice), plus: `IPC_Desc_Buffer(0x10, IPC_BUFFER_R) == 0x10A`, `..._W == 0x10C`, `..._RW == 0x10E`, `IPC_Desc_MoveHandles(2) == 0x04000010`, `IPC_Desc_StaticBuffer(5,3) == (5<<14)|(3<<10)|2`. A wrapper's argument validation (name length, size limits, closed session) can likewise be tested by compiling it with the system calls replaced by stubs.

**2. Failure paths on a console** (build only; read the caveats before running):

| Case | Test | Assert | Notes |
| ---- | ---- | ------ | ----- |
| Service not available | `srvGetServiceHandle(&h, "zzzz:u")` | `R_FAILED(rc)` and `h == 0` | The zeroing is documented in `srv.c`; the exact error code is **printed, not asserted** |
| Invalid command ID | Header `IPC_MakeHeader(0x7F00,0,0)` on a `cfg:u` handle | Print transport and service result | Outcome unknown: do not assert. A service may reply with an error or behave unexpectedly |
| Bad buffer size | Valid `GetConfigInfoBlk2` with a block ID that should not exist (`0xDEADBEEF`), size 1 | Transport succeeds; print service result | Tests the service's validation without malformed descriptors |
| Oversized request | Wrapper called with size 0 and size above its own limit | Returns `RS_INVALIDARG` without any IPC | Pure test of your validation |

Do **not** write tests that send a descriptor the kernel documents as fatal: a mapped-buffer descriptor with access bits 0, a static-buffer size larger than the receiver's registered buffer, or both "move" and "process ID" bits in one handle descriptor. 3dbrew reports each as a kernel panic [reverse-engineered: 3dbrew IPC], which reboots the console and can lose data on the SD card. If you need those results, use an emulator that models the kernel and record that the answer is from an emulator, not hardware.

The harness (builds; not run):

```c
#include <3ds.h>
#include <stdio.h>

static int failures;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL line %d: %s\n", __LINE__, #cond); failures++; } } while (0)

static void testServiceNotAvailable(void)
{
    Handle h = 0xDEADBEEF;
    Result rc = srvGetServiceHandle(&h, "zzzz:u");   // not a real service name
    printf("unknown service: rc=0x%08lX\n", (unsigned long)rc);   // exact code: unrecorded
    CHECK(R_FAILED(rc));
    CHECK(h == 0);   // srv.c writes 0 on failure (srvGetServiceHandleDirect)
}

static void testInvalidCommandId(Handle cfg)
{
    u32 *cmdbuf = getThreadCommandBuffer();
    cmdbuf[0] = IPC_MakeHeader(0x7F00, 0, 0);       // not a documented cfg:u command
    Result transport = svcSendSyncRequest(cfg);
    printf("bad cmd id: transport=0x%08lX service=0x%08lX\n",
           (unsigned long)transport, (unsigned long)cmdbuf[1]);   // outcome: unknown, record it
}

static void testUnknownBlock(Handle cfg)
{
    u8 b = 0;
    u32 *cmdbuf = getThreadCommandBuffer();
    cmdbuf[0] = IPC_MakeHeader(0x1, 2, 2);
    cmdbuf[1] = 1;
    cmdbuf[2] = 0xDEADBEEF;                          // block id that should not exist
    cmdbuf[3] = IPC_Desc_Buffer(1, IPC_BUFFER_W);
    cmdbuf[4] = (u32)&b;
    Result transport = svcSendSyncRequest(cfg);
    CHECK(R_SUCCEEDED(transport));
    printf("unknown block: service=0x%08lX\n", (unsigned long)cmdbuf[1]);
}

int main(void)
{
    testServiceNotAvailable();
    Handle cfg = 0;
    if (R_SUCCEEDED(srvGetServiceHandle(&cfg, "cfg:u"))) {
        testInvalidCommandId(cfg);
        testUnknownBlock(cfg);
        svcCloseHandle(cfg);
    }
    return failures;
}
```

Treat the printed result codes as data to record in [source versions](source-versions.md) or a benchmark README with console model and firmware, which turns them from open questions into `[measured]` facts. See [testing on hardware](testing-on-hardware.md) for the test loop.

## Troubleshooting

| Symptom | Likely cause | Check |
| ------- | ------------ | ----- |
| `srvGetServiceHandle` fails | Name wrong or longer than 8 characters, service not permitted for your title, or service absent on this firmware | Print the raw `Result`; try from a title with broader access; see Open questions |
| Program hangs inside `srvGetServiceHandle` | Session slots of that service exhausted by leaked handles | Count opens and closes; reference-count sessions |
| Transport succeeds but outputs are zero or garbage | You read outputs without checking `cmdbuf[1]` | Check the service result first |
| Reply words belong to another command | Another IPC call ran between your send and your read | Read outputs immediately; avoid `printf` over sockets in between |
| Console reboots when the call is made | Fatal descriptor (rights 0, static buffer too big, move with process ID bit) | Build descriptors with the `IPC_Desc_*` helpers only |
| Kernel rejects the request | Wrong translate-word count in the header | Count descriptor words: buffer 2, process ID 2, `n` handles `n + 1` |
| Works on one firmware, fails on older | Command added in a later system version | Check the version column on the 3dbrew service page |

## Not covered yet

This page covers the client side only. Writing a *server* (a `svcReplyAndReceive` loop), PXI buffers in practice, and the exact error values for malformed requests are not covered or measured. For service-specific walkthroughs see [networking](networking.md) (`soc:U`, `http:C`) and [application lifecycle](application-lifecycle.md) (`APT`). Local wireless over `nwm::UDS` (a service client like any other, needing shared memory handles and event handles) and input/sensor services (`hid:USER`, `ir`) have no engineering page yet; start from [CTR services](../software/services.md).

## Technical details

| Item | Value | Source |
| ---- | ----- | ------ |
| Command buffer | TLS + 0x80, 64 words | `svc.h` `getThreadCommandBuffer`; 3dbrew IPC |
| Static buffer table | TLS + 0x180, 16 entries of 2 words | `svc.h` `getThreadStaticBuffers`; 3dbrew IPC |
| Header | `(cmd << 16) \| (normal << 6) \| translate` (6-bit counts) | `ipc.h` |
| Max handles per descriptor | 64 | `ipc.h` comments |
| Mapped buffer region in receiver | from `0x04000000`, one unmapped guard page before and after each | 3dbrew IPC |
| `srv:` GetServiceHandle | `0x00050100`; name 8 bytes + length + flags | `srv.c`; 3dbrew |
| `srv:` RegisterClient | `0x00010002`, process ID translate | `srv.c` |
| HTTPC "download pending" result | `0xd840a02b` | `httpc.h` |

3dbrew documents translation types 0 (handles), 1 (static buffers), 2 and 3 (PXI buffers) and the mapped-buffer type selected by bit 3. For replies the kernel accepts only types 0, 1, 5, 6 and 7, so a *server* must zero type-2 descriptors before replying; this matters only if you write a service, not a client [reverse-engineered: 3dbrew IPC].

## Pitfalls

- **Reading output without checking `cmdbuf[1]`.** Symptom: random values after a failed call, because libctru's own wrappers write outputs even on failure. Fix: check the service result first, as in the raw example.
- **Using the buffer after another IPC call.** Symptom: reply words that belong to a different command. Fix: read all reply words you need immediately after the send, before any other libctru call.
- **Wrong translate size in the header.** Symptom: the kernel rejects the request or the service reads garbage. Fix: count descriptor words and placeholders; a buffer is 2, a process ID is 2, `n` handles are `n + 1`.
- **Service name longer than 8 characters.** Symptom: a different service or a confusing failure. Fix: validate before the call.
- **Leaking sessions.** Symptom: a hang in `srvGetServiceHandle` after many opens. Fix: pair every open with a close; reference-count shared sessions.
- **Closing a moved handle.** Symptom: the second close fails or closes an unrelated object. Fix: after `IPC_Desc_MoveHandles`, set your variable to 0.
- **Rights 0 on a mapped buffer, oversized static buffer.** Symptom: console reboots. Fix: build descriptors only from the helpers and validate sizes.

## Open questions

- What are the exact `Result` values for: an unknown service name, a service the title is not allowed to use, a command ID the service does not implement, a mapped buffer that violates its rights, and a block ID `cfg:u` does not know? Experiment: run the harness above on an Old 3DS and a New 3DS and record firmware and Luma3DS versions.
- What does `cfg:u` `0x00050000` return on a system older than 5.0.0-11? 3dbrew lists the version but not the error.
- Does the default blocking policy really hang when the session limit of a service is reached, and what is the limit for `cfg:u`? Experiment: open sessions in a loop with `srvSetBlockingPolicy(true)` and count.
- Is it safe for two threads to send on the same session handle at once? The kernel behaviour is not recorded here.

## Verification status

| Item | Status |
| ---- | ------ |
| The three examples with a `main` on this page, and the three in [networking](networking.md) | Build with devkitARM r68-1 (GCC 16.1.0) and libctru 2.7.1-1, `-Wall -Wextra`, no warnings; linked with `3dsx.specs` into `.elf` files |
| Header/descriptor value assertions | Compiled and run on a PC (host), copy of `ipc.h` body; all pass. This checks the arithmetic only |
| Emulator, Old 3DS, New 3DS | not run |

## Sources

- [3dbrew: IPC](https://www.3dbrew.org/wiki/IPC) — message structure, translate descriptors, ports and services, server loop (fetched as raw wikitext in November 2026; `IPC_Command_Structure` redirects here).
- [3dbrew: Config Services](https://www.3dbrew.org/wiki/Config_Services) and [Cfg:GetSystemModel](https://www.3dbrew.org/wiki/Cfg:GetSystemModel) — command list, model values.
- [3dbrew: SRV:GetServiceHandle](https://www.3dbrew.org/wiki/SRV:GetServiceHandle) — request/response layout and flags.
- [libctru `ipc.h`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/include/3ds/ipc.h) — header and descriptor builders.
- [libctru `cfgu.c`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/source/services/cfgu.c) — `GetSystemModel`, `GetConfigInfoBlk2`, init fallback chain.
- [libctru `srv.c`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/source/srv.c) — `srvInit`, `srvGetServiceHandle`, blocking policy.
- [libctru `soc_poll.c`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/source/services/soc/soc_poll.c), [`soc_init.c`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/source/services/soc/soc_init.c), [`httpc.c`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/source/services/httpc.c), [`am.c`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/source/services/am.c) — static, mapped, shared and moved handle examples.
- [libctru `svc.h`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/include/3ds/svc.h) and [`result.h`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/include/3ds/result.h) — TLS accessors, result bit fields.
