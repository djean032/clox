#include <stdio.h>

#include "lines.h"
#include "memory.h"

void initLineData(LineData *array) {
  array->count = 0;
  array->capacity = 0;
  array->lineNum = NULL;
  array->offset = NULL;
}

void writeLineData(LineData *array, int lineNum, int offset) {
  if (array->capacity < array->count + 1) {
    int oldCapacity = array->capacity;
    array->capacity = GROW_CAPACITY(oldCapacity);
    array->lineNum =
        GROW_ARRAY(int, array->lineNum, oldCapacity, array->capacity);
    array->offset =
        GROW_ARRAY(int, array->offset, oldCapacity, array->capacity);
  }

  array->lineNum[array->count] = lineNum;
  array->offset[array->count] = offset;
  array->count++;
}

void freeLineData(LineData *array) {
  FREE_ARRAY(int, array->lineNum, array->capacity);
  FREE_ARRAY(int, array->offset, array->capacity);
  initLineData(array);
}
