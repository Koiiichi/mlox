#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"

typedef enum { // -- To make our integer const more readable
    OP_RETURN, // 'return from current function'
} OpCode;

typedef struct {
    int count;
    int capacity;
    uint8_t* code;
} Chunk;

void initChunk(Chunk *chunk)

#endif

