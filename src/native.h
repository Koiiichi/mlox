/**
 * @file native.h
 * @brief Registration functions for built-in native functions.
 */
#ifndef MLOX_NATIVE_H
#define MLOX_NATIVE_H

#include "object.h"

/**
 * @brief Registers all standard native functions with the VM.
 */
void register_core_natives(void);

#endif /* MLOX_NATIVE_H */
