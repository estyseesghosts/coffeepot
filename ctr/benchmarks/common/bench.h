// bench.h: shared output + metadata helpers for the coffeepot CTR benchmarks.
//
// Every benchmark writes the same machine-readable CSV-style lines both to
// stdout (the console) and to sdmc:/coffeepot-<program>.csv. Line kinds:
//   meta,<key>,<value>
//   result,<program>,<test>,<variant>,<bytes_or_n>,<runs>,<min_us>,<med_us>,<max_us>
//   note,<text>
//   error,<where>,<detail>
// Header-only, all functions static inline, so each program can include it.
#ifndef COFFEEPOT_BENCH_H
#define COFFEEPOT_BENCH_H

#include <3ds.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *bench_fp;
static int bench_error_count;

static inline void bench_line(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static inline void bench_line(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    putchar('\n');
    if (bench_fp) {
        va_start(ap, fmt);
        vfprintf(bench_fp, fmt, ap);
        va_end(ap);
        fputc('\n', bench_fp);
        fflush(bench_fp);
    }
}

// Record a failed call. Does not abort; callers decide whether to continue.
static inline void bench_error(const char *where, Result rc)
{
    bench_error_count++;
    bench_line("error,%s,rc=0x%08lx", where, (unsigned long)rc);
}

static inline int bench_cmp_u64(const void *a, const void *b)
{
    u64 x = *(const u64 *)a, y = *(const u64 *)b;
    return (x > y) - (x < y);
}

// Sorts t (n ticks) and emits one result line in microseconds.
static inline void bench_report(const char *program, const char *test, const char *variant,
                                unsigned long count, u64 *t, int n)
{
    qsort(t, n, sizeof(u64), bench_cmp_u64);
    bench_line("result,%s,%s,%s,%lu,%d,%.2f,%.2f,%.2f", program, test, variant, count, n,
               t[0] / CPU_TICKS_PER_USEC, t[n / 2] / CPU_TICKS_PER_USEC,
               t[n - 1] / CPU_TICKS_PER_USEC);
}

// Opens the sdmc output file and prints the metadata header. Call after
// consoleInit(). Returns the New 3DS flag through *isNew. A failure to open
// the file is reported but not fatal (stdout still works).
//
// Not available from the libraries at run time, so recorded as "not-readable":
//   - libctru / citro3d version (no version macro in the installed headers;
//     record them from ctr/engineering/source-versions.md),
//   - New 3DS 804 MHz speed-up and L2 cache state (libctru has only a setter,
//     osSetSpeedupEnable). Define BENCH_SPEEDUP=0 or 1 at build time to have
//     the program set it (New 3DS only) and record the requested value.
static inline void bench_begin(const char *program, bool *isNew)
{
    static const char *const model_names[] = {
        "3DS", "3DS XL", "New 3DS", "2DS", "New 3DS XL", "New 2DS XL"
    };
    char path[96];
    snprintf(path, sizeof(path), "sdmc:/coffeepot-%s.csv", program);
    bench_fp = fopen(path, "w");
    if (!bench_fp) {
        bench_error_count++;
        printf("error,sdmc_open,errno=%d (%s)\n", errno, path);
    }

    bool newflag = false;
    Result rc = APT_CheckNew3DS(&newflag);
    if (R_FAILED(rc)) bench_error("APT_CheckNew3DS", rc);
    *isNew = newflag;

    u8 model = 0xFF;
    rc = cfguInit();
    if (R_SUCCEEDED(rc)) {
        rc = CFGU_GetSystemModel(&model);
        if (R_FAILED(rc)) bench_error("CFGU_GetSystemModel", rc);
        cfguExit();
    } else {
        bench_error("cfguInit", rc);
    }

    bench_line("meta,program,%s", program);
    bench_line("meta,build_date,%s %s", __DATE__, __TIME__);
    bench_line("meta,compiler,%s", __VERSION__);
    bench_line("meta,libctru_version,not-readable");
    bench_line("meta,citro3d_version,not-readable");
    if (model < sizeof(model_names) / sizeof(model_names[0]))
        bench_line("meta,model,%s", model_names[model]);
    else
        bench_line("meta,model,unknown(%u)", (unsigned)model);
    bench_line("meta,new3ds,%d", newflag ? 1 : 0);
    u32 fw = osGetFirmVersion(), kv = osGetKernelVersion();
    bench_line("meta,firm_version,%lu.%lu.%lu", (unsigned long)GET_VERSION_MAJOR(fw),
               (unsigned long)GET_VERSION_MINOR(fw), (unsigned long)GET_VERSION_REVISION(fw));
    bench_line("meta,kernel_version,%lu.%lu.%lu", (unsigned long)GET_VERSION_MAJOR(kv),
               (unsigned long)GET_VERSION_MINOR(kv), (unsigned long)GET_VERSION_REVISION(kv));
#if defined(BENCH_SPEEDUP)
    if (newflag) osSetSpeedupEnable(BENCH_SPEEDUP ? true : false);
    bench_line("meta,speedup_requested,%d", newflag ? (BENCH_SPEEDUP ? 1 : 0) : -1);
#else
    bench_line("meta,speedup_requested,untouched");
#endif
    bench_line("meta,speedup_l2_state,not-readable");
    bench_line("meta,cpu_ticks_per_usec,%.4f", CPU_TICKS_PER_USEC);
    bench_line("meta,linear_free_bytes,%lu", (unsigned long)linearSpaceFree());
}

// Waits for START, so the screen stays readable.
static inline void bench_wait_exit(void)
{
    bench_line("note,done errors=%d; press START to exit", bench_error_count);
    if (bench_fp) { fclose(bench_fp); bench_fp = NULL; }
    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & KEY_START) break;
        gspWaitForVBlank();
    }
}

#endif
