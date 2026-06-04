#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "lines.h"
#include "memory.h"
#include "value.h"

void initChunk(Chunk *chunk) {
  chunk->count = 0;
  chunk->capacity = 0;
  chunk->code = NULL;
  initValueArray(&chunk->constants);
  initLineData(&chunk->lines);
}

void writeChunk(Chunk *chunk, uint8_t byte, int line) {
  if (chunk->capacity < chunk->count + 1) {
    int oldCapacity = chunk->capacity;
    chunk->capacity = GROW_CAPACITY(oldCapacity);
    chunk->code =
        GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
  }

  chunk->code[chunk->count] = byte;

  if (chunk->lines.count == 0 ||
      line > chunk->lines.lineNum[chunk->lines.count - 1]) {
    writeLineData(&chunk->lines, line, chunk->count);
  } else if (line < chunk->lines.lineNum[chunk->lines.count]) {
    // Shouldn't be possible, but just in case.
    printf("Unrecoverable error at line: %d\n",
           chunk->lines.lineNum[chunk->lines.count]);
    exit(1);
  }
  chunk->count++;
}

int addLine(Chunk *chunk, int line, int offset) {
  writeLineData(&chunk->lines, line, offset);
  return chunk->lines.count - 1;
}

int addConstant(Chunk *chunk, Value value) {
  writeValueArray(&chunk->constants, value);
  return chunk->constants.count - 1;
}

void freeChunk(Chunk *chunk) {
  FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
  freeValueArray(&chunk->constants);
  freeLineData(&chunk->lines);
  initChunk(chunk);
}
