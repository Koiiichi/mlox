/**
 * @file debug.h
 * @brief Utilities for disassembling bytecode chunks and instructions.
 */
#ifndef MLOX_DEBUG_H
#define MLOX_DEBUG_H

#include "chunk.h"

/**
 * @brief Disassembles an entire chunk to stdout.
 *
 * @param chunk Chunk to disassemble.
 * @param name Label to display atop the disassembly output.
 */
void disassemble_chunk(Chunk* chunk, const char* name);

/**
 * @brief Disassembles a single instruction within a chunk.
 *
 * @param chunk Chunk containing the instruction.
 * @param offset Byte offset of the instruction to disassemble.
 * @return Offset for the subsequent instruction.
 */
int disassemble_instruction(Chunk* chunk, int offset);

#endif /* MLOX_DEBUG_H */
