// coffeebench: the four micro-benchmarks (icachebench, membench, flushbench, drawbench) in one
// binary. They run one after another in a fixed order; each writes its own CSV exactly as the
// standalone program does, into one session directory:
//   sdmc:/coffeepot/s<NNN>/<program>.csv
// Order: icachebench first (touches nothing but its own code), then membench, flushbench and
// drawbench (these allocate linear memory and use the GPU). Every module sets up and tears down
// its own graphics state, so they do not share state with each other. Each module's source is
// the standalone program's main.c, compiled with `main` renamed (see mod_*.c) and with
// COFFEEBENCH_SUITE defined (bench.h then skips the per-module wait for START).
//
// Run it from the Homebrew Menu; it needs no input. START at the end exits.
#include <3ds.h>
#include <stdio.h>
#include "session.h"

int icachebench_run(void);
int membench_run(void);
int flushbench_run(void);
int drawbench_run(void);

int main(void)
{
    int session = coffee_session_new();
    int (*const modules[])(void) = { icachebench_run, membench_run, flushbench_run, drawbench_run };
    static const char *const names[] = { "icachebench", "membench", "flushbench", "drawbench" };
    int ran = 0;
    for (unsigned i = 0; i < sizeof modules / sizeof modules[0]; i++) {
        if (!aptMainLoop()) break;   // HOME menu closed the application
        modules[i]();
        ran++;
    }
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);
    printf("coffeebench: %d of %d modules finished.\nSession %d.\nResults: sdmc:/coffeepot/s%03d/\n", ran,
           (int)(sizeof names / sizeof names[0]), session, session);
    printf("Check each CSV ends with note,done errors=0.\nPress START to exit.\n");
    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & KEY_START) break;
        gspWaitForVBlank();
    }
    gfxExit();
    return 0;
}
