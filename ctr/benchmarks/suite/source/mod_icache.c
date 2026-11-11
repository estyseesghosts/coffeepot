/* Wrapper: compiles icachebench's main.c as a module of the micro-benchmark suite. */
#define main icachebench_run
#include "../../icachebench/source/main.c"
