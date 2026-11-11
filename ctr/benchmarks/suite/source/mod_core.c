/* Wrapper: compiles corebench's main.c as a module of the New 3DS micro suite (empty in the plain suite). */
#ifdef COFFEEBENCH_N3DS
#define main corebench_run
#include "../../corebench/source/main.c"
#endif
