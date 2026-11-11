// coffeebench: the micro-benchmarks in one binary. They run one after another in a fixed order; each
// writes its own CSV exactly as the standalone program does, into one session directory:
//   sdmc:/coffeepot/s<NNN>/<program>.csv
// Order: icachebench first (touches nothing but its own code), then membench, flushbench and
// drawbench (these allocate linear memory and use the GPU). Every module sets up and tears down
// its own graphics state, so they do not share state with each other. Each module's source is
// the standalone program's main.c, compiled with `main` renamed (see mod_*.c) and with
// COFFEEBENCH_SUITE defined (bench.h then skips the per-module wait for START).
//
// make N3DS=1 builds coffeebench-n3ds, the New 3DS version: it refuses to run on an Old 3DS, adds corebench
// (threads on cores 0, 2 and 3), and runs the whole list twice in one boot, first with the CPU forced to
// 268 MHz (osSetSpeedupEnable(false)) and then at 804 MHz with the L2 cache (osSetSpeedupEnable(true)).
// The files are named <program>-sp0.csv and <program>-sp1.csv, so a pair is a same-boot, same-binary A/B.
// meta,clock_probe_us records which clock each module really had.
//
// Run it from the Homebrew Menu; it needs no input. START at the end exits.
#include <3ds.h>
#include <stdio.h>
#include "session.h"

int icachebench_run(void);
int membench_run(void);
int flushbench_run(void);
int drawbench_run(void);
#ifdef COFFEEBENCH_N3DS
int corebench_run(void);
#endif

int coffee_speed_mode = -1;   // see bench.h

static void wait_start(void)
{
    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & KEY_START) break;
        gspWaitForVBlank();
    }
}

int main(void)
{
    int session = coffee_session_new();
    int ran = 0, total = 0;

#ifdef COFFEEBENCH_N3DS
    bool isNew = false;
    APT_CheckNew3DS(&isNew);
    if (!isNew) {
        gfxInitDefault();
        consoleInit(GFX_TOP, NULL);
        printf("coffeebench-n3ds needs a New 3DS (804 MHz, L2 cache, cores 2 and 3).\nUse coffeebench on an Old 3DS.\nPress START to exit.\n");
        wait_start();
        gfxExit();
        return 0;
    }
    int (*const modules[])(void) = { icachebench_run, membench_run, flushbench_run, drawbench_run, corebench_run };
    static const int modes[] = { 0, 1 };
#else
    int (*const modules[])(void) = { icachebench_run, membench_run, flushbench_run, drawbench_run };
    static const int modes[] = { -1 };
#endif
    int nmodules = (int)(sizeof modules / sizeof modules[0]);
    int nmodes = (int)(sizeof modes / sizeof modes[0]);
    total = nmodules * nmodes;
    for (int m = 0; m < nmodes; m++) {
        coffee_speed_mode = modes[m];
        for (int i = 0; i < nmodules; i++) {
            if (!aptMainLoop()) goto done;   // HOME menu closed the application
            modules[i]();
            ran++;
        }
    }
done:
    coffee_speed_mode = -1;
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);
    printf("coffeebench: %d of %d module runs finished.\nSession %d.\nResults: sdmc:/coffeepot/s%03d/\n", ran, total, session, session);
    printf("Check each CSV ends with note,done errors=0.\nPress START to exit.\n");
    wait_start();
    gfxExit();
    return 0;
}
