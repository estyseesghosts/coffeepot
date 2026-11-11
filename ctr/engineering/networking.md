# Networking from homebrew: sockets, HTTP and TLS

A 3DS program reaches the internet through two system services: a **socket** service (`soc:U`) that gives you the usual BSD-style `socket`/`connect`/`recv` calls, and an **HTTP** service (`http:C`) that does whole HTTP and HTTPS requests for you. This page shows how to start each correctly, how to download a file without ever holding it all in memory, how to use non-blocking sockets with timeouts, and how to make HTTPS actually check the server's certificate. The short answer on certificates: the devkitPro `http` example turns verification off; do not copy that. Add the root certificate you trust and leave verification on.

The services themselves are listed in [CTR services](../software/services.md); how a request travels to them is on [IPC and services](ipc-and-services.md).

## Before you start

- Checked against libctru 2.7.1 at commit `62b7c42c88b1ee837f513358409d2e5c8f9001a9` (local clone) and the `3ds-examples` commit pinned on [source versions](source-versions.md); 3dbrew pages *HTTPC Services* and *SSL Services* fetched in November 2026.
- All four code examples build with devkitARM r68-1 (GCC 16.1.0) and libctru 2.7.1-1, `-Wall -Wextra`, no warnings. **None has been run** on hardware or in an emulator; behaviours that need a network are listed under Open questions.
- Terms: a **root CA** is the certificate authority at the top of a chain of trust; a server's certificate is accepted only if it chains up to a root you trust. **TLS** is the encryption layer under HTTPS. **Non-blocking** means a call returns at once with "would block" instead of waiting.
- A connected Wi-Fi network is required. Without one, `httpcInit` fails (see [Technical details](#technical-details)).

## soc:U initialisation

`socInit(buffer, size)` hands the socket service a block of your memory to use as its working space. The rules are in the header [documented: libctru `soc.h`]:

- `buffer` must be page-aligned (`0x1000`) and `size` a multiple of `0x1000`.
- After `socInit`, **your process can no longer access that block**: its userland permissions are set to no-access. `socExit()` is what gives it back, so free the buffer only after `socExit`.
- libctru registers a `soc:` device and refuses a second `socInit` while it exists (returns -1) [documented: `source/services/soc/soc_init.c`].

On failure, `socInit` closes the memory block and the service handle itself, so you only free your buffer [documented: `soc_init.c`]. The sample programs use a 1 MiB buffer (`SOC_BUFFERSIZE 0x100000`) [documented: `3ds-examples/network/sockets/source/sockets.c`]; what the smallest workable size is has not been measured. The example below uses 128 KiB as an arbitrary smaller value (builds; not run):

```c
#include <3ds.h>
#include <malloc.h>
#include <stdio.h>

#define SOC_ALIGN      0x1000u     // soc.h: page-aligned (0x1000)
#define SOC_BUFFERSIZE 0x20000u    // multiple of 0x1000; 128 KiB is our choice, see Open questions

static u32 *socBuffer;

// Returns 0 on success. On any failure nothing is left allocated.
static Result netStart(void)
{
    socBuffer = memalign(SOC_ALIGN, SOC_BUFFERSIZE);
    if (!socBuffer)
        return MAKERESULT(RL_PERMANENT, RS_OUTOFRESOURCE, RM_APPLICATION, RD_OUT_OF_MEMORY);

    Result rc = socInit(socBuffer, SOC_BUFFERSIZE);
    if (R_FAILED(rc)) {
        free(socBuffer);           // socInit's failure paths close the memory block handle (soc_init.c)
        socBuffer = NULL;
    }
    return rc;
}

static void netStop(void)
{
    socExit();                     // gives the buffer back to the process (soc.h note)
    free(socBuffer);               // only now may the buffer be touched or freed
    socBuffer = NULL;
}

int main(void)
{
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);
    Result rc = netStart();
    printf("socInit: 0x%08lX\n", (unsigned long)rc);
    if (R_SUCCEEDED(rc))
        netStop();
    gfxExit();
    return 0;
}
```

The buffer comes from the normal heap, not the linear heap ([memory and cache](memory-and-cache.md#which-pool-for-which-buffer)). Wrong alignment is the classic failure: `socInit` returns an error from the kernel when creating the memory block, which is the first thing it does.

## Non-blocking sockets, poll and timeouts

Once `socInit` has succeeded, newlib's socket functions work on 3DS sockets. Facts from libctru's source [documented: `source/services/soc/*.c`]:

- `fcntl` accepts only `F_GETFL` and `F_SETFL`, and the only flag it understands is `O_NONBLOCK` (any other flag in `F_SETFL` gives `EINVAL`). `ioctl(FIONBIO)` is built on it.
- `poll()` is implemented (an IPC command with a static buffer, see [IPC](ipc-and-services.md#static-buffers)); `select()` is built on `poll()` and allocates per call. `poll()` with zero descriptors returns `EINVAL`.
- The error table maps service errors to `errno` values including `EINPROGRESS`, `EAGAIN` and `ETIMEDOUT`.
- Whether `SO_RCVTIMEO`/`SO_SNDTIMEO` are honoured is not visible in libctru (`setsockopt` forwards the option to the service); use `poll` deadlines instead.

Pattern: set `O_NONBLOCK`, `connect` (expect `EINPROGRESS`), `poll` for `POLLOUT` with a deadline, read `SO_ERROR`, then read with `recv`, waiting on `POLLIN` when it reports "would block". Builds; not run:

```c
#include <3ds.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

static int waitFor(int fd, short events, int timeoutMs)
{
    struct pollfd p = { .fd = fd, .events = events, .revents = 0 };
    int n = poll(&p, 1, timeoutMs);
    if (n < 0) return -1;                       // errno set by libctru
    if (n == 0) { errno = ETIMEDOUT; return -1; }
    return (p.revents & (events | POLLERR | POLLHUP)) ? 0 : -1;
}

static int connectTimeout(const char *ip, uint16_t port, int timeoutMs)
{
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) return -1;

    int flags = fcntl(fd, F_GETFL, 0);          // libctru fcntl: only F_GETFL/F_SETFL and O_NONBLOCK
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) { close(fd); return -1; }

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &sa.sin_addr) != 1) { close(fd); errno = EINVAL; return -1; }

    if (connect(fd, (struct sockaddr *)&sa, sizeof sa) < 0 && errno != EINPROGRESS) { close(fd); return -1; }
    if (waitFor(fd, POLLOUT, timeoutMs) < 0) { int e = errno; close(fd); errno = e; return -1; }

    int soerr = 0; socklen_t len = sizeof soerr;
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &len) < 0 || soerr != 0) {
        int e = soerr ? soerr : errno; close(fd); errno = e; return -1;
    }
    return fd;
}

// Reads until the peer closes, the cap is hit, or an idle wait expires.
static ssize_t readBounded(int fd, void *buf, size_t cap, int idleMs)
{
    size_t got = 0;
    while (got < cap) {
        ssize_t n = recv(fd, (u8 *)buf + got, cap - got, 0);
        if (n > 0) { got += (size_t)n; continue; }
        if (n == 0) break;                                       // orderly close
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if (waitFor(fd, POLLIN, idleMs) < 0) return got ? (ssize_t)got : -1;
            continue;
        }
        return -1;
    }
    return (ssize_t)got;
}

int main(void)
{
    static u8 buf[1024];
    int fd = connectTimeout("127.0.0.1", 80, 5000);
    ssize_t n = (fd >= 0) ? readBounded(fd, buf, sizeof buf, 5000) : -1;
    if (fd >= 0) close(fd);
    return n < 0;
}
```

Memory bound: the read buffer is fixed (`cap`), so a hostile or buggy peer cannot grow your memory. Timeout: every wait has a deadline. Call these from a worker thread if the main loop must keep drawing ([threading](threading.md)); a blocking `poll` on the main thread freezes the frame.

## HTTP downloads with httpc

`http:C` does DNS, TCP, TLS and HTTP in a system process; you get status, headers and body through IPC. Lifecycle [documented: libctru `httpc.h`, `source/services/httpc.c`]:

1. `httpcInit(sharedmem_size)` — `0` for GET-only use; a page-multiple size reserves shared memory for POST/PUT bodies. It opens a session to `http:C` and sends Initialize.
2. `httpcOpenContext(&ctx, HTTPC_METHOD_GET, url, 1)` — creates a request context and opens a **second session** for it; the last argument non-zero selects the default proxy.
3. Optional: add request headers, set root certificates and SSL options (below).
4. `httpcBeginRequest(&ctx)`, then `httpcGetResponseStatusCode[Timeout]`.
5. Read the body: `httpcReceiveData[Timeout]` into your buffer, with `httpcGetDownloadSizeState` giving bytes so far and `Content-Length` (0 if unknown). `httpcReceiveData` returns `HTTPC_RESULTCODE_DOWNLOADPENDING` (`0xd840a02b`) when your buffer filled and more remains. `httpcDownloadData` loops exactly that way on your behalf.
6. `httpcCloseContext(&ctx)` — closes the per-context session and the context. `httpcExit()` finalises and closes the main session.

Redirects (301, 302, 303, 307, 308) are **not** followed for you: read the `Location` header with `httpcGetResponseHeader`, close the context and open a new one [documented: `3ds-examples/network/http/source/main.c` does this with a 4 KiB `Location` buffer]. `HTTPC_RESULTCODE_TIMEDOUT` (`0xd820a069`) is what the `...Timeout` calls return when they time out [documented: `httpc.h`].

## Bounded-memory download

The `http` example grows a `realloc` buffer to the full file size. That is unsafe for anything large or untrusted: on an Old 3DS the application heap is a few tens of MiB ([memory and cache](memory-and-cache.md#how-much-memory-you-get)). The version below keeps one static 16 KiB chunk, streams it to a `FILE`, enforces a hard byte cap (also refusing early when `Content-Length` exceeds it), applies a 15-second timeout to each receive and to the status wait, limits redirects to 5, refuses to leave HTTPS for HTTP, and only speaks HTTPS when you pass an explicit trust choice. Builds; not run:

```c
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHUNK_SIZE      0x4000u
#define MAX_REDIRECTS   5
#define URL_MAX         2048u
#define TIMEOUT_NS      (15ULL * 1000 * 1000 * 1000)     // 15 s per receive call

typedef struct {
    const u8 *rootDer;              // DER root certificate you trust for this server, or NULL
    u32       rootDerSize;
    SSLC_DefaultRootCert builtin;   // used when rootDer == NULL and builtin != 0
} Trust;

enum { E_URL = -1, E_STATUS = -2, E_NOTRUST = -3, E_REL = -4, E_DOWNGRADE = -5,
       E_TOOBIG = -6, E_WRITE = -7, E_LOOP = -8 };

static bool isRedirect(u32 c) { return c == 301 || c == 302 || c == 303 || c == 307 || c == 308; }
static bool startsWith(const char *s, const char *p) { return strncmp(s, p, strlen(p)) == 0; }

// Writes at most maxBytes into out. Returns 0, a libctru Result, or one of the E_* codes.
static Result download(const char *startUrl, const Trust *trust, FILE *out, u32 maxBytes, u32 *written)
{
    static u8 chunk[CHUNK_SIZE];                 // static: keeps 16 KiB off the thread stack
    char url[URL_MAX];
    *written = 0;
    if (strlen(startUrl) >= sizeof url) return E_URL;
    strcpy(url, startUrl);

    for (int hop = 0; hop <= MAX_REDIRECTS; hop++) {
        httpcContext ctx;
        Result rc = httpcOpenContext(&ctx, HTTPC_METHOD_GET, url, 1);
        if (R_FAILED(rc)) return rc;

        bool https = startsWith(url, "https://");
        if (https) {
            // A fresh context trusts no root CA (3dbrew HTTPC_Services); add exactly what we accept.
            if (trust && trust->rootDer)
                rc = httpcAddTrustedRootCA(&ctx, trust->rootDer, trust->rootDerSize);
            else if (trust && trust->builtin)
                rc = httpcAddDefaultCert(&ctx, trust->builtin);
            else
                rc = E_NOTRUST;                   // refuse https without an explicit trust choice
            if (R_FAILED(rc)) { httpcCloseContext(&ctx); return rc; }
        }

        u32 status = 0;
        rc = httpcBeginRequest(&ctx);
        if (R_SUCCEEDED(rc))
            rc = httpcGetResponseStatusCodeTimeout(&ctx, &status, TIMEOUT_NS);
        if (R_FAILED(rc)) { httpcCloseContext(&ctx); return rc; }

        if (isRedirect(status)) {
            char next[URL_MAX];
            rc = httpcGetResponseHeader(&ctx, "Location", next, sizeof next);
            httpcCloseContext(&ctx);              // see Open questions: close with unread body
            if (R_FAILED(rc)) return rc;
            next[sizeof next - 1] = '\0';
            if (!(startsWith(next, "http://") || startsWith(next, "https://"))) return E_REL;
            if (https && !startsWith(next, "https://")) return E_DOWNGRADE;
            strcpy(url, next);
            continue;
        }
        if (status != 200) { httpcCloseContext(&ctx); return E_STATUS; }

        u32 prev = 0, now = 0, total = 0;
        rc = httpcGetDownloadSizeState(&ctx, &prev, &total);   // total (Content-Length) may be 0
        if (R_SUCCEEDED(rc) && total != 0 && total > maxBytes) rc = E_TOOBIG;   // refuse early
        while (R_SUCCEEDED(rc)) {
            Result rr = httpcReceiveDataTimeout(&ctx, chunk, CHUNK_SIZE, TIMEOUT_NS);
            if (rr != 0 && rr != (Result)HTTPC_RESULTCODE_DOWNLOADPENDING) { rc = rr; break; }
            rc = httpcGetDownloadSizeState(&ctx, &now, NULL);
            if (R_FAILED(rc)) break;
            u32 got = now - prev;                  // bytes delivered by this call
            prev = now;
            if (got > CHUNK_SIZE) got = CHUNK_SIZE;
            if (*written + got > maxBytes) { rc = E_TOOBIG; break; }
            if (got && fwrite(chunk, 1, got, out) != got) { rc = E_WRITE; break; }
            *written += got;
            if (rr == 0) break;                    // finished
        }
        if (R_FAILED(rc)) httpcCancelConnection(&ctx);   // before closing a partly read body
        httpcCloseContext(&ctx);
        return rc;
    }
    return E_LOOP;
}

int main(void)
{
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);
    Result rc = httpcInit(0);                       // 0: no POST shared memory
    if (R_SUCCEEDED(rc)) {
        FILE *f = fopen("sdmc:/example.bin", "wb");
        if (f) {
            u32 n = 0;
            Trust t = { NULL, 0, 0 };
            rc = download("http://example.invalid/file.bin", &t, f, 1u << 20, &n);
            printf("rc=0x%08lX bytes=%lu\n", (unsigned long)rc, (unsigned long)n);
            fclose(f);
        }
        httpcExit();
    }
    gfxExit();
    return 0;
}
```

The chunk-size calculation (`now - prev`) is how libctru's own `httpcDownloadData` derives how many bytes a receive call delivered [documented: `httpc.c`]. Partial downloads: this function writes what it received before an error, so the file may be truncated; write to a temporary name and rename on success, and remember that `*written` is the only length you can trust. The libctru header warns that the **entire** content must be downloaded before `httpcCloseContext`, "otherwise `httpcCloseContext()` will hang" [documented: `httpc.h`]. The error path above therefore calls `httpcCancelConnection` first; whether that prevents the hang is unverified (Open questions).

## Certificate validation

**What the example does wrong.** The devkitPro `http` example calls `httpcSetSSLOpt(&context, SSLCOPT_DisableVerify)` with the comment "This disables SSL cert verification, so https:// will be usable" [documented: `3ds-examples/network/http/source/main.c`]. `SSLCOPT_DisableVerify` is bit 9 in `sslc.h`; 3dbrew describes it as making certificate validation always succeed [reverse-engineered: 3dbrew SSL Services, "SSLOpt"]. With it set, anyone on the network path can impersonate the server, and everything you download is untrusted.

**Why the example needed it.** 3dbrew states that a new HTTP context trusts no root CA at all until you add one explicitly [reverse-engineered: 3dbrew HTTPC Services, "TLS Root CAs"]. Disabling verification is the shortcut around having no roots.

**The secure alternatives that libctru exposes** [documented: `httpc.h`, `sslc.h`]:

| Goal | Call | Notes |
| ---- | ---- | ----- |
| Trust one root you ship with the app | `httpcAddTrustedRootCA(&ctx, der, size)` | DER-encoded certificate, per context. Best choice when you control or know the server |
| Trust a built-in root | `httpcAddDefaultCert(&ctx, SSLC_DefaultRootCert_...)` | Eleven built-in IDs: Nintendo CA/G2/G3, Nintendo Class 2 CA/G2/G3, GTE CyberTrust Global Root, AddTrust External CA Root, COMODO RSA, USERTrust RSA, DigiCert High Assurance EV Root. No ISRG/Let's Encrypt entry exists in this enum |
| Reuse a set of roots across requests | `httpcCreateRootCertChain`, `httpcRootCertChainAddCert` / `AddDefaultCert`, `httpcSelectRootCertChain(&ctx, chain)` | At most 2 chains per process; destroy with `httpcDestroyRootCertChain` |
| Raw TLS over your own socket | `sslcCreateRootCertChain`, `sslcAddTrustedRootCA`, `sslcCreateContext(..., SSLCOPT_Default, hostname)`, `sslcContextSetRootCertChain`, `sslcStartConnection` | Used by `3ds-examples/network/sslc`, which ships `builtin_rootca.der` and does not disable verification |

Rules for a safe client:

- **Never** pass `SSLCOPT_DisableVerify` to `httpcSetSSLOpt` or `sslcCreateContext` in code that downloads anything you will trust.
- Obtain the root as a DER file (convert PEM with `openssl x509 -outform der`), embed it with `bin2s` or read it from RomFS, and pin it: the server's chain must end at that root.
- Pass the real host name to `sslcCreateContext`; per 3dbrew the default option bits when a context is created are `0x1B` (common name, root CA, chain, subject-alt-name checks) [reverse-engineered: 3dbrew SSL Services].
- A verification failure from `http:C` is reported as `0xd8a0a03c` and from sslc as `0xD8A0B814` when the root is not trusted [reverse-engineered: 3dbrew error tables]. Treat both as "do not connect"; do not retry with verification off.

What could **not** be verified here:

- Whether a context really starts with verification enabled and zero roots on every firmware (3dbrew says so; no libctru source states it).
- Whether `httpcAddDefaultCert` roots are still valid certificates today. Several in the list are old; certificates in the system's store are not updated by homebrew.
- Which TLS versions and cipher suites the 3DS stack supports. `SSLCOPT_TLSv10` and 3dbrew's description of option bit 11 suggest an old stack; modern servers that require newer TLS may refuse the connection regardless of certificates.
- 3dbrew notes that the certificate not-before/not-after dates may not be checked against the system clock [reverse-engineered: 3dbrew SSL Services]. Do not rely on expiry checking; see Open questions.

## Technical details

| Item | Value | Source |
| ---- | ----- | ------ |
| `socInit` buffer | `0x1000`-aligned, size multiple of `0x1000`, inaccessible until `socExit` | `soc.h` |
| `soc:U` Initialize | `0x00010044`, process ID + shared memory handle | `soc_init.c` |
| `soc:U` poll | `0x00140084`, static buffers 10 (in) / 0 (out) | `soc_poll.c` |
| `fcntl` | `F_GETFL`/`F_SETFL`, `O_NONBLOCK` only | `soc_fcntl.c` |
| `http:C` Initialize | `0x00010044` | `httpc.c` |
| `httpcInit` shared memory | `memalign(0x1000, size)`, size should be a multiple of `0x1000`, only for POST | `httpc.c`, `httpc.h` |
| Status values | `HTTPC_STATUS_REQUEST_IN_PROGRESS` 5, `HTTPC_STATUS_DOWNLOAD_READY` 7 | `httpc.h` |
| Pending / timeout / not found | `0xd840a02b` / `0xd820a069` / `0xd840a028` | `httpc.h` |
| Root chains / client-cert contexts per process | 2 / 2 | `httpc.h`, 3dbrew |
| SSL option bits | 9 = disable verify, 11 = TLSv1.0 | `sslc.h` |
| `httpcInit` with no network | `0xd8a0a046` (DNS failure at Initialize, e.g. Wi-Fi off) | 3dbrew HTTPC Services |

## Pitfalls

- **Copying the `http` example.** Symptom: HTTPS "works" against anything, including an attacker. Fix: add a root with `httpcAddTrustedRootCA` and drop `SSLCOPT_DisableVerify`.
- **Freeing or reading the `socInit` buffer.** Symptom: data abort. Fix: leave it alone until `socExit` returns.
- **Unaligned `socInit` buffer.** Symptom: `socInit` fails at once. Fix: `memalign(0x1000, n)` with `n` a multiple of `0x1000`.
- **Unbounded `realloc` download.** Symptom: out-of-memory on Old 3DS, or a hostile server filling memory. Fix: stream to the SD card in fixed chunks with a cap.
- **Leaked contexts.** Symptom: later requests hang or fail (each context is a session; see [session limits](ipc-and-services.md#session-lifetime-and-closing)). Fix: `httpcCloseContext` on every path.
- **Blocking network calls on the main thread.** Symptom: frozen screen. Fix: a worker thread, or non-blocking sockets with short `poll` timeouts.
- **Trusting `Content-Length`.** It may be 0 or wrong; the cap in `download` counts real bytes.
- **Following a redirect from HTTPS to HTTP.** Symptom: silent downgrade. Fix: refuse, as the example does.

## Open questions

- Does `httpcCancelConnection` before `httpcCloseContext` avoid the documented hang on a partly read body? And does closing a context after a redirect response (small, unread body) hang? Experiment: run `download` against a server that returns a large body and one that redirects; record firmware and times.
- What is the smallest working `socInit` buffer, and how do buffer size and throughput relate? Experiment: vary `SOC_BUFFERSIZE` from `0x1000` upward and record failures and download times on Old and New 3DS.
- Does `SO_RCVTIMEO` have any effect on `soc:U`? Experiment: set it and time a `recv` on an idle connection.
- Is verification on by default for a context with a trusted root added, and does it reject a wrong host name or an untrusted root? Experiment: connect to a test server with a self-signed certificate with and without its root added; expect `0xd8a0a03c` per 3dbrew.
- Are expired certificates rejected? Experiment: a test server with an expired certificate, with the console clock set correctly.
- Which TLS versions and cipher suites work on current servers, per console firmware?

## Verification status

All four examples (`soc` init, non-blocking connect, bounded download, plus the IPC pages' examples) build against libctru 2.7.1-1 with devkitARM r68-1; emulator, Old 3DS and New 3DS: not run. No `[measured]` numbers appear on this page.

## Sources

- [libctru `httpc.h`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/include/3ds/services/httpc.h) and [`httpc.c`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/source/services/httpc.c) — HTTP API, result codes, download loop, root CA commands.
- [libctru `sslc.h`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/include/3ds/services/sslc.h) — default root CA IDs, option bits, sslc API.
- [libctru `soc.h`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/include/3ds/services/soc.h), [`soc_init.c`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/source/services/soc/soc_init.c), [`soc_fcntl.c`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/source/services/soc/soc_fcntl.c), [`soc_poll.c`](https://github.com/devkitPro/libctru/blob/62b7c42c88b1ee837f513358409d2e5c8f9001a9/libctru/source/services/soc/soc_poll.c) — socket service behaviour.
- [3ds-examples `network/http`](https://github.com/devkitPro/3ds-examples/tree/be2001fee08cf8ec7d3366e095e83f6f75da8942/network/http), [`network/sslc`](https://github.com/devkitPro/3ds-examples/tree/be2001fee08cf8ec7d3366e095e83f6f75da8942/network/sslc), [`network/sockets`](https://github.com/devkitPro/3ds-examples/tree/be2001fee08cf8ec7d3366e095e83f6f75da8942/network/sockets) — the verification-disabling example, the root-CA example, the 1 MiB `soc` buffer.
- [3dbrew: HTTP Services](https://www.3dbrew.org/wiki/HTTP_Services) — root CA behaviour, error codes.
- [3dbrew: SSL Services](https://www.3dbrew.org/wiki/SSL_Services) — SSL option bits, verification notes, error codes.
