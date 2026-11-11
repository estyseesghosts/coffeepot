/* Wrapper: compiles flushbench's main.c as a module of the micro-benchmark suite. */
#define main flushbench_run
#include "../../flushbench/source/main.c"
