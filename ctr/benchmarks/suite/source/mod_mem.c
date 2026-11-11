/* Wrapper: compiles membench's main.c as a module of the micro-benchmark suite. */
#define main membench_run
#include "../../membench/source/main.c"
