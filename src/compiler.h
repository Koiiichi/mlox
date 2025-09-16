/**
 * @file compiler.h
 * @brief Pratt parser and bytecode compiler entry points.
 */
#ifndef MLOX_COMPILER_H
#define MLOX_COMPILER_H

#include "object.h"
#include "vm.h"

/**
 * @brief Compiles source code into a function object representing the script.
 *
 * @param source Null-terminated source code string.
 * @return Function object containing compiled bytecode, or NULL on error.
 */
ObjFunction* compile(const char* source);

/**
 * @brief Marks compiler roots to protect them during garbage collection.
 */
void mark_compiler_roots(void);

#endif /* MLOX_COMPILER_H */
