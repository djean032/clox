#include "test.h"

#include "../memory.h"

#include <stdint.h>
#include <string.h>

static bool isAligned(const void *ptr, size_t alignment) {
  return ((uintptr_t)ptr % alignment) == 0;
}

static bool test_arena_init(void) {
  uint8_t buffer[128];
  Arena arena;

  arenaInit(&arena, buffer, sizeof(buffer), "scratch");

  CHECK(arena.base == buffer);
  CHECK(arena.capacity == sizeof(buffer));
  CHECK(arena.offset == 0);
  CHECK(arena.owner == false);
  CHECK(strcmp(arena.name, "scratch") == 0);

  return true;
}

static bool test_arena_create_destroy(void) {
  Arena arena;

  CHECK(arenaCreate(&arena, 128, "owned"));
  CHECK(arena.base != NULL);
  CHECK(arena.capacity == 128);
  CHECK(arena.offset == 0);
  CHECK(arena.owner == true);
  CHECK(strcmp(arena.name, "owned") == 0);

  CHECK(arenaAlloc(&arena, 16, 8) != NULL);
  CHECK(arenaUsed(&arena) >= 16);

  arenaDestroy(&arena);
  CHECK(arena.base == NULL);
  CHECK(arena.capacity == 0);
  CHECK(arena.offset == 0);
  CHECK(arena.owner == false);
  CHECK(arena.name == NULL);

  return true;
}

static bool test_arena_alloc_basic(void) {
  uint8_t buffer[128];
  Arena arena;

  arenaInit(&arena, buffer, sizeof(buffer), "basic");

  void *first = arenaAlloc(&arena, 16, 1);
  void *second = arenaAlloc(&arena, 8, 1);

  CHECK(first == buffer);
  CHECK(second == buffer + 16);
  CHECK(arenaUsed(&arena) == 24);
  CHECK((uint8_t *)first + 16 <= (uint8_t *)second);

  return true;
}

static bool test_arena_alloc_alignment(void) {
  uint8_t buffer[256];
  Arena arena;
  size_t alignments[] = {1, 2, 4, 8, 16, 32};

  arenaInit(&arena, buffer, sizeof(buffer), "alignment");

  for (size_t i = 0; i < sizeof(alignments) / sizeof(alignments[0]); i++) {
    size_t before = arenaUsed(&arena);
    void *ptr = arenaAlloc(&arena, 3, alignments[i]);

    CHECK(ptr != NULL);
    CHECK(isAligned(ptr, alignments[i]));
    CHECK(arenaUsed(&arena) >= before + 3);
  }

  return true;
}

static bool test_arena_alloc_out_of_memory(void) {
  uint8_t buffer[32];
  Arena arena;

  arenaInit(&arena, buffer, sizeof(buffer), "oom");

  void *ptr = arenaAlloc(&arena, sizeof(buffer), 1);
  CHECK(ptr == buffer);
  CHECK(arenaRemaining(&arena) == 0);

  size_t before = arenaUsed(&arena);
  CHECK(arenaAlloc(&arena, 1, 1) == NULL);
  CHECK(arenaUsed(&arena) == before);

  return true;
}

static bool test_arena_alloc_zero(void) {
  uint8_t buffer[64];
  Arena arena;

  memset(buffer, 0xff, sizeof(buffer));
  arenaInit(&arena, buffer, sizeof(buffer), "zero");

  uint8_t *ptr = arenaAllocZero(&arena, 16, 1);
  CHECK(ptr != NULL);
  CHECK(arenaUsed(&arena) == 16);

  for (size_t i = 0; i < 16; i++) {
    CHECK(ptr[i] == 0);
  }

  return true;
}

static bool test_arena_alloc_span(void) {
  uint8_t buffer[32];
  Arena arena;
  MemorySpan span;

  arenaInit(&arena, buffer, sizeof(buffer), "span");

  CHECK(arenaAllocSpan(&arena, 16, 1, &span));
  CHECK(span.base == buffer);
  CHECK(span.size == 16);

  size_t before = arenaUsed(&arena);
  CHECK(!arenaAllocSpan(&arena, 32, 1, &span));
  CHECK(span.base == NULL);
  CHECK(span.size == 0);
  CHECK(arenaUsed(&arena) == before);

  return true;
}

static bool test_arena_mark_restore(void) {
  uint8_t buffer[64];
  Arena arena;

  arenaInit(&arena, buffer, sizeof(buffer), "mark");

  CHECK(arenaAlloc(&arena, 8, 1) == buffer);
  size_t mark = arenaMark(&arena);
  void *temporary = arenaAlloc(&arena, 16, 1);

  CHECK(temporary == buffer + mark);
  CHECK(arenaUsed(&arena) == mark + 16);

  arenaRestore(&arena, mark);
  CHECK(arenaUsed(&arena) == mark);
  CHECK(arenaAlloc(&arena, 16, 1) == temporary);

  return true;
}

static bool test_arena_reset(void) {
  uint8_t buffer[64];
  Arena arena;

  arenaInit(&arena, buffer, sizeof(buffer), "reset");

  CHECK(arenaAlloc(&arena, 8, 1) == buffer);
  CHECK(arenaAlloc(&arena, 8, 1) == buffer + 8);
  CHECK(arenaUsed(&arena) == 16);

  arenaReset(&arena);
  CHECK(arenaUsed(&arena) == 0);
  CHECK(arenaRemaining(&arena) == sizeof(buffer));
  CHECK(arenaAlloc(&arena, 8, 1) == buffer);

  return true;
}

static bool test_memory_constants(void) {
  CHECK(KiB(1) == 1024u);
  CHECK(MiB(1) == 1024u * 1024u);
  CHECK(CLOX_ROOT_ARENA_SIZE > 0);
  CHECK(CLOX_VM_HEAP_SIZE > 0);
  CHECK(CLOX_COMPILER_ARENA_SIZE > 0);

  return true;
}

int registerMemoryArenaTests(TestCase *tests, int max) {
  int count = 0;

  ADD_TEST(test_arena_init);
  ADD_TEST(test_arena_create_destroy);
  ADD_TEST(test_arena_alloc_basic);
  ADD_TEST(test_arena_alloc_alignment);
  ADD_TEST(test_arena_alloc_out_of_memory);
  ADD_TEST(test_arena_alloc_zero);
  ADD_TEST(test_arena_alloc_span);
  ADD_TEST(test_arena_mark_restore);
  ADD_TEST(test_arena_reset);
  ADD_TEST(test_memory_constants);

  return count;
}
