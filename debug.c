#include <stdio.h>

#include "chunk.h"
#include "debug.h"
#include "lines.h"
#include "value.h"

void disassembleChunk(Chunk *chunk, const char *name) {
  printf("== %s ==\n", name);

  for (int offset = 0; offset < (int)chunk->count;) {
    offset = disassembleInstruction(chunk, offset);
  }
}

static int constantInstruction(const char *name, Chunk *chunk, size_t offset) {
  uint8_t constant = chunk->code[offset + 1];
  printf("%-16s %4d '", name, constant);
  printValue(chunk->constants.values[constant]);
  printf("'\n");
  return offset + 2;
}

static int simpleInstruction(const char *name, size_t offset) {
  printf("%s\n", name);
  return offset + 1;
}

static int getLine(LineData *lines, size_t offset) {
  if (lines->count == 0) {
    return -1;
  }
  size_t low = 0;
  size_t high = lines->count - 1;
  size_t resultIndex = SIZE_MAX;
  while (low <= high) {
    size_t mid = low + (high - low) / 2;
    if ((size_t)lines->offset[mid] <= offset) {
      resultIndex = mid;
      low = mid + 1;
    } else {
      if (mid == 0) {
        break;
      }
      high = mid - 1;
    }
  }
  return lines->lineNum[resultIndex];
}

int disassembleInstruction(Chunk *chunk, int offset) {
  printf("%04d ", offset);
  int line = getLine(&chunk->lines, offset);
  if (offset > 0 && line == getLine(&chunk->lines, offset - 1)) {
    printf("   | ");
  } else {
    printf("%4d ", line);
  }

  uint8_t instruction = chunk->code[offset];
  switch (instruction) {
  case OP_RETURN:
    return simpleInstruction("OP_RETURN", offset);
  case OP_CONSTANT:
    return constantInstruction("OP_CONSTANT", chunk, offset);
  default:
    printf("Unknown opcode %d\n", instruction);
    return offset + 1;
  }
}
