// chain.h: start another .3dsx from the one that is running, without the Homebrew Menu.
//
// Used by the tile-renderer suite: each binary measures one variant, writes its CSV, and
// launches the next. The IPC follows devkitPro/3ds-hbmenu source/loaders/rosalina.c: connect to
// Luma3DS's "hb:ldr" port, set argv (command 3), set the target path (command 2), then ask the
// APT library to chainload to this same application (aptSetChainloaderToSelf), so that exiting
// reopens the loader with the new target. It needs Luma3DS's Rosalina loader; on anything else
// the call returns an error and the caller must stop and tell the user.
//
// Status: builds; confirmed on hardware (20 chained binaries on an Old 3DS under Luma3DS 13.0.2
// and a New 2DS XL under 13.3.3). Adapted from the hgss-3ds chainload smoke test.
//
// argv passed to the next program: argv[0] = its own path (sdmc:-prefixed), then the strings
// in extra[]. Header-only; all functions are static inline.
#ifndef COFFEEPOT_CHAIN_H
#define COFFEEPOT_CHAIN_H

#include <3ds.h>
#include <stdio.h>
#include <string.h>

static inline Result chain_command(Handle h, unsigned id, const void *data, size_t size)
{
    u32 *cmdbuf = getThreadCommandBuffer();
    cmdbuf[0] = IPC_MakeHeader(id, 0, 2);
    cmdbuf[1] = IPC_Desc_StaticBuffer(size, id == 2 ? 0 : 1);
    cmdbuf[2] = (u32)data;
    Result rc = svcSendSyncRequest(h);
    return R_FAILED(rc) ? rc : (Result)cmdbuf[1];
}

// path: absolute SD path without the "sdmc:" prefix, e.g. "/3ds/coffeepot-tiles/t02.3dsx".
// extra: up to 4 argument strings (may be NULL entries at the end). Returns 0 on success; on
// success the caller should clean up and return from main so the chainload takes effect.
static inline Result chain_launch(const char *path, const char *const *extra, int nextra)
{
    static u32 args[0x400 / sizeof(u32)];   // Luma's loader consumes a 0x400-byte argv block
    static char target[0x400];              // and a PATH_MAX path
    Handle h;
    Result rc = svcConnectToPort(&h, "hb:ldr");
    if (R_FAILED(rc)) return rc;
    memset(target, 0, sizeof target);
    snprintf(target, sizeof target, "%s", path);
    memset(args, 0, sizeof args);
    args[0] = 1 + nextra;
    char *p = (char *)&args[1];
    char *end = (char *)args + sizeof args;
    int n = snprintf(p, end - p, "sdmc:%s", path);
    p += n + 1;
    for (int i = 0; i < nextra && p < end; i++) {
        n = snprintf(p, end - p, "%s", extra[i]);
        p += n + 1;
    }
    // argv first: a failure must not leave a new target selected.
    rc = chain_command(h, 3, args, sizeof args);
    if (R_SUCCEEDED(rc)) rc = chain_command(h, 2, target, sizeof target);
    svcCloseHandle(h);
    if (R_SUCCEEDED(rc)) aptSetChainloaderToSelf();
    return rc;
}

#endif
