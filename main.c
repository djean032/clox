#include "chunk.h"
#include "common.h"
#include "debug.h"
#include <stdio.h>

int main(int argc, const char *argv[]) {
  Chunk chunk;
  initChunk(&chunk);
  printf("Chunk initialized\n");

  int constant = addConstant(&chunk, 1.2);
  writeChunk(&chunk, OP_CONSTANT, 123);
  printf("Chunk 1 written\n");
  writeChunk(&chunk, constant, 123);
  printf("Chunk 2 written\n");
  writeChunk(&chunk, OP_RETURN, 123);
  printf("Chunk 3 written\n");
  disassembleChunk(&chunk, "test chunk");
  printf("Chunk disassembled\n");
  freeChunk(&chunk);

  return 0;
}
