/**
 * @file native.c
 * @brief Defines built-in native functions exposed to MLOX scripts.
 */
#include "native.h"

#include <time.h>

#include "vm.h"

/**
 * @brief Native implementation returning the current time in seconds.
 *
 * @param arg_count Number of supplied arguments (expected to be 0).
 * @param args Argument array.
 * @return Number of seconds as a double value.
 */
static Value clock_native(int arg_count, Value* args);

/**
 * @see register_core_natives
 */
void register_core_natives(void) {
  vm_define_native("clock", clock_native);
}

/**
 * @see clock_native
 */
static Value clock_native(int arg_count, Value* args) {
  (void)arg_count;
  (void)args;
  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}
