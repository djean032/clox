#ifndef clox_lines_h
#define clox_lines_h

#include "common.h"

typedef struct {
  size_t count;
  size_t capacity;
  int *lineNum;
  int *offset;
} LineData;

void initLineData(LineData* array);
void writeLineData(LineData* array, int line, int offset);
void freeLineData(LineData* array);


#endif // !clox_lines_h
