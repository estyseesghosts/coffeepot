// session.h: where benchmark results go, and who ran them.
//
// Every run belongs to a numbered session. Results are written to
//   sdmc:/coffeepot/s<NNN>/<name>.csv
// so a second session never appends to, or overwrites, the first. The counter lives in
// sdmc:/coffeepot/session. Whoever starts a run (a standalone program, the micro-benchmark
// suite, or the first tile-suite binary) calls coffee_session_new(); the rest of a suite
// reads the current number with coffee_session_read().
//
// coffee_provenance() emits the facts the CSVs previously lacked: the repository commit
// (pass -DBENCH_COMMIT=<hash> at build time, see tools/build_benchmarks.sh), the Luma3DS
// version (svcGetSystemInfo type 0x10000, a Luma3DS extension), and the system version
// string. Anything the console will not tell us is recorded as "not-readable", never guessed.
// Header-only; all functions are static inline.
#ifndef COFFEEPOT_SESSION_H
#define COFFEEPOT_SESSION_H

#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define COFFEE_ROOT "sdmc:/coffeepot"

#define COFFEE_STR2(x) #x
#define COFFEE_STR(x) COFFEE_STR2(x)

static inline int coffee_session_read(void)
{
    int n = 0;
    FILE *f = fopen(COFFEE_ROOT "/session", "r");
    if (f) {
        if (fscanf(f, "%d", &n) != 1) n = 0;
        fclose(f);
    }
    return n;
}

// Starts a new session and returns its number (0 if the counter could not be written,
// in which case results go to s000 and the caller's file open will usually fail loudly).
static inline int coffee_session_new(void)
{
    mkdir(COFFEE_ROOT, 0777);
    int n = coffee_session_read() + 1;
    FILE *f = fopen(COFFEE_ROOT "/session", "w");
    if (!f) return 0;
    fprintf(f, "%d\n", n);
    fclose(f);
    return n;
}

// Formats sdmc:/coffeepot/s<NNN>/<name>.csv and creates the directories.
static inline void coffee_result_path(char *out, size_t size, int session, const char *name)
{
    char dir[64];
    mkdir(COFFEE_ROOT, 0777);
    snprintf(dir, sizeof dir, COFFEE_ROOT "/s%03d", session);
    mkdir(dir, 0777);
    if (snprintf(out, size, "%s/%s.csv", dir, name) >= (int)size) out[0] = 0;   // too long: open fails loudly
}

// emit(key, value) is called once per provenance fact.
static inline void coffee_provenance(void (*emit)(const char *key, const char *value))
{
    char buf[64];
#ifdef BENCH_COMMIT
    emit("repo_commit", COFFEE_STR(BENCH_COMMIT));
#else
    emit("repo_commit", "not-recorded");
#endif
    s64 lv = 0;
    if (R_SUCCEEDED(svcGetSystemInfo(&lv, 0x10000, 0))) {
        snprintf(buf, sizeof buf, "%d.%d.%d", (int)((lv >> 24) & 0xFF), (int)((lv >> 16) & 0xFF),
                 (int)((lv >> 8) & 0xFF));
        emit("luma3ds_version", buf);
    } else {
        emit("luma3ds_version", "not-readable");
    }
    char sys[32] = { 0 };
    if (R_SUCCEEDED(osGetSystemVersionDataString(NULL, NULL, sys, sizeof sys)))
        emit("system_version", sys);
    else
        emit("system_version", "not-readable");
}

#endif
