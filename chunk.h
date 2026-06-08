#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "lines.h"
#include "value.h"
#include <stdint.h>

typedef enum {
  OP_CONSTANT,
  OP_RETURN,
} OpCode;

typedef struct {
  size_t count;
  size_t capacity;
  uint8_t *code;
  LineData lines;
  ValueArray constants;
} Chunk;

void initChunk(Chunk *chunk);

void writeChunk(Chunk *chunk, uint8_t byte, int line);
int addLine(Chunk *chunk, int line, int offset);
int addConstant(Chunk *chunk, Value value);

void freeChunk(Chunk *chunk);

#endif // !clox_chunk_h
