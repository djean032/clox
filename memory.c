#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"

#define IS_POWER_OF_TWO(x) ((x) != 0 && (((x) & ((x) - 1)) == 0))

static inline size_t alignUpSize(size_t value, size_t alignment) {
  assert(IS_POWER_OF_TWO(alignment));
  return (value + alignment - 1) & ~(size_t)(alignment - 1);
}

static inline uintptr_t alignUpPtr(uintptr_t value, size_t alignment) {
  assert(IS_POWER_OF_TWO(alignment));
  return (value + alignment - 1) & ~(uintptr_t)(alignment - 1);
}

void *reallocate(void *pointer, size_t oldSize, size_t newSize) {
  if (newSize == 0) {
    free(pointer);
    return NULL;
  }

  void *result = realloc(pointer, newSize);
  if (result == NULL)
    exit(1);
  return result;
}

/* Arena struct
typedef struct Arena {
  uint8_t *base;
  size_t capacity;
  size_t offset;
  const char *name;
} Arena;
*/

// Arena methods
bool arenaCreate(Arena *arena, size_t capacity, const char *name) {
  assert(arena != NULL);
  assert(capacity > 0);

  void *memory = malloc(capacity);
  if (memory == NULL)
    return false;

  arenaInit(arena, memory, capacity, name);
  arena->owner = true;

  return true;
}

void arenaInit(Arena *arena, void *memory, size_t capacity, const char *name) {
  assert(arena != NULL);
  assert(memory != NULL);
  assert(capacity > 0);

  arena->base = (uint8_t *)memory;
  arena->capacity = capacity;
  arena->offset = 0;
  arena->owner = false;
  arena->name = name;
}

void arenaDestroy(Arena *arena) {
  assert(arena != NULL);
  if (arena->owner) {
    free(arena->base);
  }
  arena->base = NULL;
  arena->capacity = 0;
  arena->offset = 0;
  arena->owner = false;
  arena->name = NULL;
}

void *arenaAlloc(Arena *arena, size_t size, size_t alignment) {
  assert(arena != NULL);
  assert(arena->base != NULL);
  assert(size > 0);
  assert(IS_POWER_OF_TWO(alignment));

  uintptr_t current = (uintptr_t)arena->base + (uintptr_t)arena->offset;
  uintptr_t aligned = alignUpPtr(current, alignment);

  size_t padding = (size_t)(aligned - current);
  size_t newOffset = arena->offset + padding + size;

  if (padding > arena->capacity - arena->offset) {
    return NULL;
  }

  size_t available = arena->capacity - arena->offset - padding;
  if (size > available) {
    return NULL;
  }

  arena->offset += padding + size;
  return (void *)aligned;
}

void *arenaAllocZero(Arena *arena, size_t size, size_t alignment) {
  void *aligned = arenaAlloc(arena, size, alignment);
  if (aligned != NULL) {
    memset(aligned, 0, size);
  }
  return aligned;
}

bool arenaAllocSpan(Arena *arena, size_t size, size_t alignment,
                    MemorySpan *out) {
  assert(out != NULL);
  void *ptr = arenaAlloc(arena, size, alignment);
  if (ptr == NULL) {
    out->base = NULL;
    out->size = 0;
    return false;
  }
  out->base = (uint8_t *)ptr;
  out->size = size;
  return true;
}

size_t arenaMark(const Arena *arena) {
  assert(arena != NULL);
  return arena->offset;
}

size_t arenaUsed(const Arena *arena) {
  assert(arena != NULL);
  return arena->offset;
}

size_t arenaRemaining(const Arena *arena) {
  assert(arena != NULL);
  return arena->capacity - arena->offset;
}

void arenaRestore(Arena *arena, size_t mark) {
  assert(arena != NULL);
  assert(mark <= arena->capacity);

  arena->offset = mark;
}

void arenaReset(Arena *arena) {
  assert(arena != NULL);
  arena->offset = 0;
}

/*
#define ARENA_PUSH_ARRAY(arena, type, count) \
    (type*)arenaAllocArray((arena), sizeof(type), (count), _Alignof(type))

#define ARENA_PUSH_ARRAY_ZERO(arena, type, count) \
    (type*)arenaAllocArrayZero((arena), sizeof(type), (count), _Alignof(type))

#define ARENA_PUSH_STRUCT(arena, type) \
    ARENA_PUSH_ARRAY((arena), type, 1)

#define ARENA_PUSH_STRUCT_ZERO(arena, type) \
    ARENA_PUSH_ARRAY_ZERO((arena), type, 1)

void* arenaAllocArray(Arena* arena, size_t elemSize, size_t count, size_t
alignment); void* arenaAllocArrayZero(Arena* arena, size_t elemSize, size_t
count, size_t alignment);
*/
