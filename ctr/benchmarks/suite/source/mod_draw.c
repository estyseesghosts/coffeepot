/* Wrapper: compiles drawbench's main.c as a module of the micro-benchmark suite. */
#define main drawbench_run
#include "../../drawbench/source/main.c"
