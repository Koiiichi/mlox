/**
 * @file chunk.h
 * @brief Bytecode chunk representation and manipulation utilities.
 */
#ifndef MLOX_CHUNK_H
#define MLOX_CHUNK_H

#include "common.h"
#include "value.h"

/**
 * @brief Opcode identifiers for MLOX bytecode instructions.
 */
typedef enum {
  OP_CONSTANT,
  OP_NIL,
  OP_TRUE,
  OP_FALSE,
  OP_POP,
  OP_GET_LOCAL,
  OP_SET_LOCAL,
  OP_GET_GLOBAL,
  OP_DEFINE_GLOBAL,
  OP_SET_GLOBAL,
  OP_GET_UPVALUE,
  OP_SET_UPVALUE,
  OP_GET_PROPERTY,
  OP_SET_PROPERTY,
  OP_GET_SUPER,
  OP_EQUAL,
  OP_GREATER,
  OP_LESS,
  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  OP_NOT,
  OP_NEGATE,
  OP_PRINT,
  OP_JUMP,
  OP_JUMP_IF_FALSE,
  OP_LOOP,
  OP_CALL,
  OP_INVOKE,
  OP_SUPER_INVOKE,
  OP_CLOSURE,
  OP_CLOSE_UPVALUE,
  OP_RETURN,
  OP_CLASS,
  OP_INHERIT,
  OP_METHOD
} OpCode;

/**
 * @brief Dynamic array storing bytecode instructions alongside metadata.
 */
typedef struct {
  uint8_t* code;        /**< Raw instruction bytes. */
  int* lines;           /**< Source line number per instruction. */
  int count;            /**< Number of bytes currently stored. */
  int capacity;         /**< Allocated capacity of the instruction buffer. */
  ValueArray constants; /**< Pool of constant values referenced by the chunk. */
} Chunk;

/**
 * @brief Initializes an empty chunk instance.
 *
 * @param chunk Chunk to initialize.
 */
void init_chunk(Chunk* chunk);

/**
 * @brief Releases memory owned by the chunk and resets it to empty.
 *
 * @param chunk Chunk to clean up.
 */
void free_chunk(Chunk* chunk);

/**
 * @brief Appends a single bytecode instruction to the chunk.
 *
 * @param chunk Target chunk instance.
 * @param byte Instruction byte to append.
 * @param line Source line number corresponding to the instruction.
 */
void write_chunk(Chunk* chunk, uint8_t byte, int line);

/**
 * @brief Adds a constant to the chunk's constant pool.
 *
 * @param chunk Chunk that will own the constant.
 * @param value Constant value to append.
 * @return Index within the constant pool for the added value.
 */
int add_constant(Chunk* chunk, Value value);

#endif /* MLOX_CHUNK_H */
