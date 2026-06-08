#include "test.h"

#include "../memory.h"

#include <stdint.h>
#include <string.h>

static bool isAligned(const void *ptr, size_t alignment) {
  return ((uintptr_t)ptr % alignment) == 0;
}

static bool initTestHeap(Heap *heap, uint8_t *buffer, size_t size) {
  MemorySpan span = {buffer, size};
  return heapInit(heap, span);
}

static void fillPattern(uint8_t *ptr, size_t size) {
  for (size_t i = 0; i < size; i++) {
    ptr[i] = (uint8_t)(i + 1);
  }
}

static bool checkPattern(const uint8_t *ptr, size_t size) {
  for (size_t i = 0; i < size; i++) {
    CHECK(ptr[i] == (uint8_t)(i + 1));
  }

  return true;
}

static bool test_heap_init(void) {
  uint8_t buffer[4096];
  Heap heap;

  CHECK(initTestHeap(&heap, buffer, sizeof(buffer)));
  CHECK(heap.base == buffer);
  CHECK(heap.size == sizeof(buffer));
  CHECK(heap.firstBlock != NULL);
  CHECK(heap.usedBytes == 0);
  CHECK(heap.allocationCount == 0);
  CHECK(heap.freeCount == 0);
  CHECK(heapValidate(&heap));

  return true;
}

static bool test_heap_init_rejects_too_small_span(void) {
  uint8_t buffer[16];
  Heap heap;
  MemorySpan span = {buffer, sizeof(buffer)};

  CHECK(!heapInit(&heap, span));

  return true;
}

static bool test_heap_alloc_basic(void) {
  uint8_t buffer[4096];
  Heap heap;

  CHECK(initTestHeap(&heap, buffer, sizeof(buffer)));

  void *ptr = heapAlloc(&heap, 24);
  CHECK(ptr != NULL);
  CHECK(heapOwnsPointer(&heap, ptr));
  CHECK(isAligned(ptr, HEAP_ALIGNMENT));
  CHECK(heap.usedBytes >= 24);
  CHECK(heap.allocationCount == 1);
  CHECK(heapValidate(&heap));

  return true;
}

static bool test_heap_alloc_out_of_memory(void) {
  uint8_t buffer[256];
  Heap heap;

  CHECK(initTestHeap(&heap, buffer, sizeof(buffer)));

  CHECK(heapAlloc(&heap, sizeof(buffer)) == NULL);
  CHECK(heap.usedBytes == 0);
  CHECK(heap.allocationCount == 0);
  CHECK(heapValidate(&heap));

  return true;
}

static bool test_heap_free_basic(void) {
  uint8_t buffer[4096];
  Heap heap;

  CHECK(initTestHeap(&heap, buffer, sizeof(buffer)));

  void *ptr = heapAlloc(&heap, 64);
  CHECK(ptr != NULL);

  heapFree(&heap, ptr);
  CHECK(heap.usedBytes == 0);
  CHECK(heap.freeCount == 1);
  CHECK(heapValidate(&heap));

  return true;
}

static bool test_heap_free_null(void) {
  uint8_t buffer[4096];
  Heap heap;

  CHECK(initTestHeap(&heap, buffer, sizeof(buffer)));
  heapFree(&heap, NULL);

  CHECK(heap.usedBytes == 0);
  CHECK(heap.allocationCount == 0);
  CHECK(heap.freeCount == 0);
  CHECK(heapValidate(&heap));

  return true;
}

static bool test_heap_reuse_freed_block(void) {
  uint8_t buffer[4096];
  Heap heap;

  CHECK(initTestHeap(&heap, buffer, sizeof(buffer)));

  void *first = heapAlloc(&heap, 64);
  CHECK(first != NULL);
  heapFree(&heap, first);

  void *second = heapAlloc(&heap, 64);
  CHECK(second == first);
  CHECK(heap.allocationCount == 2);
  CHECK(heap.freeCount == 1);
  CHECK(heapValidate(&heap));

  return true;
}

static bool test_heap_get_stats_initial(void) {
  uint8_t buffer[4096];
  Heap heap;
  HeapStats stats;

  CHECK(initTestHeap(&heap, buffer, sizeof(buffer)));
  heapGetStats(&heap, &stats);

  CHECK(stats.totalBytes == sizeof(buffer));
  CHECK(stats.usedBytes == 0);
  CHECK(stats.usedBlockCount == 0);
  CHECK(stats.freeBlockCount == 1);
  CHECK(stats.blockCount == 1);
  CHECK(stats.largestFreeBlock > 0);
  CHECK(stats.allocationCount == 0);
  CHECK(stats.freeCount == 0);
  CHECK(heapValidate(&heap));

  return true;
}

static bool test_heap_split_block(void) {
  uint8_t buffer[4096];
  Heap heap;
  HeapStats stats;

  CHECK(initTestHeap(&heap, buffer, sizeof(buffer)));
  CHECK(heapAlloc(&heap, 64) != NULL);

  heapGetStats(&heap, &stats);
  CHECK(stats.blockCount == 2);
  CHECK(stats.usedBlockCount == 1);
  CHECK(stats.freeBlockCount == 1);
  CHECK(stats.usedBytes >= 64);
  CHECK(stats.freeBytes > 0);
  CHECK(heapValidate(&heap));

  return true;
}

static bool test_heap_coalesce_next(void) {
  uint8_t buffer[4096];
  Heap heap;
  HeapStats stats;

  CHECK(initTestHeap(&heap, buffer, sizeof(buffer)));
  void *first = heapAlloc(&heap, 64);
  void *second = heapAlloc(&heap, 64);
  CHECK(first != NULL);
  CHECK(second != NULL);

  heapFree(&heap, second);
  heapFree(&heap, first);

  heapGetStats(&heap, &stats);
  CHECK(stats.blockCount == 1);
  CHECK(stats.usedBlockCount == 0);
  CHECK(stats.freeBlockCount == 1);
  CHECK(stats.usedBytes == 0);
  CHECK(heap.usedBytes == 0);
  CHECK(heap.freeCount == 2);
  CHECK(heapValidate(&heap));

  return true;
}

static bool test_heap_realloc_null(void) {
  uint8_t buffer[4096];
  Heap heap;

  CHECK(initTestHeap(&heap, buffer, sizeof(buffer)));

  void *ptr = heapRealloc(&heap, NULL, 32);
  CHECK(ptr != NULL);
  CHECK(heapOwnsPointer(&heap, ptr));
  CHECK(heap.usedBytes >= 32);
  CHECK(heap.allocationCount == 1);
  CHECK(heapValidate(&heap));

  return true;
}

static bool test_heap_realloc_null_to_zero(void) {
  uint8_t buffer[4096];
  Heap heap;

  CHECK(initTestHeap(&heap, buffer, sizeof(buffer)));
  CHECK(heapRealloc(&heap, NULL, 0) == NULL);
  CHECK(heap.usedBytes == 0);
  CHECK(heap.allocationCount == 0);
  CHECK(heap.freeCount == 0);
  CHECK(heapValidate(&heap));

  return true;
}

static bool test_heap_realloc_to_zero(void) {
  uint8_t buffer[4096];
  Heap heap;

  CHECK(initTestHeap(&heap, buffer, sizeof(buffer)));
  void *ptr = heapAlloc(&heap, 64);
  CHECK(ptr != NULL);

  CHECK(heapRealloc(&heap, ptr, 0) == NULL);
  CHECK(heap.usedBytes == 0);
  CHECK(heap.allocationCount == 1);
  CHECK(heap.freeCount == 1);
  CHECK(heapValidate(&heap));

  return true;
}

static bool test_heap_realloc_shrink(void) {
  uint8_t buffer[4096];
  Heap heap;

  CHECK(initTestHeap(&heap, buffer, sizeof(buffer)));

  uint8_t *ptr = heapAlloc(&heap, 256);
  CHECK(ptr != NULL);
  fillPattern(ptr, 64);

  size_t before = heap.usedBytes;
  uint8_t *shrunk = heapRealloc(&heap, ptr, 64);
  CHECK(shrunk == ptr);
  CHECK(checkPattern(shrunk, 64));
  CHECK(heap.usedBytes <= before);
  CHECK(heap.usedBytes >= 64);
  CHECK(heapValidate(&heap));

  return true;
}

static bool test_heap_realloc_grow_in_place(void) {
  uint8_t buffer[4096];
  Heap heap;

  CHECK(initTestHeap(&heap, buffer, sizeof(buffer)));

  uint8_t *first = heapAlloc(&heap, 64);
  void *second = heapAlloc(&heap, 128);
  CHECK(first != NULL);
  CHECK(second != NULL);
  fillPattern(first, 64);

  heapFree(&heap, second);
  uint8_t *grown = heapRealloc(&heap, first, 160);

  CHECK(grown == first);
  CHECK(checkPattern(grown, 64));
  CHECK(heap.usedBytes >= 160);
  CHECK(heap.allocationCount == 2);
  CHECK(heap.freeCount == 1);
  CHECK(heapValidate(&heap));

  return true;
}

static bool test_heap_realloc_grow_moves(void) {
  uint8_t buffer[4096];
  Heap heap;

  CHECK(initTestHeap(&heap, buffer, sizeof(buffer)));

  uint8_t *first = heapAlloc(&heap, 64);
  void *second = heapAlloc(&heap, 512);
  CHECK(first != NULL);
  CHECK(second != NULL);
  fillPattern(first, 64);

  uint8_t *grown = heapRealloc(&heap, first, 1024);
  CHECK(grown != NULL);
  CHECK(grown != first);
  CHECK(checkPattern(grown, 64));
  CHECK(heapOwnsPointer(&heap, grown));
  CHECK(heap.allocationCount == 3);
  CHECK(heap.freeCount == 1);
  CHECK(heapValidate(&heap));

  return true;
}

static bool test_heap_get_stats_after_alloc_free(void) {
  uint8_t buffer[4096];
  Heap heap;
  HeapStats stats;

  CHECK(initTestHeap(&heap, buffer, sizeof(buffer)));
  void *first = heapAlloc(&heap, 64);
  void *second = heapAlloc(&heap, 128);
  CHECK(first != NULL);
  CHECK(second != NULL);

  heapFree(&heap, first);
  heapGetStats(&heap, &stats);

  CHECK(stats.totalBytes == sizeof(buffer));
  CHECK(stats.blockCount == 3);
  CHECK(stats.usedBlockCount == 1);
  CHECK(stats.freeBlockCount == 2);
  CHECK(stats.usedBytes >= 128);
  CHECK(stats.freeBytes > 0);
  CHECK(stats.allocationCount == 2);
  CHECK(stats.freeCount == 1);
  CHECK(heapValidate(&heap));

  return true;
}

static bool test_heap_destroy(void) {
  uint8_t buffer[4096];
  Heap heap;

  CHECK(initTestHeap(&heap, buffer, sizeof(buffer)));
  CHECK(heapAlloc(&heap, 64) != NULL);

  heapDestroy(&heap);
  CHECK(heap.base == NULL);
  CHECK(heap.size == 0);
  CHECK(heap.firstBlock == NULL);
  CHECK(heap.usedBytes == 0);
  CHECK(heap.allocationCount == 0);
  CHECK(heap.freeCount == 0);

  return true;
}

int registerMemoryHeapTests(TestCase *tests, int max) {
  int count = 0;

  ADD_TEST(test_heap_init);
  ADD_TEST(test_heap_init_rejects_too_small_span);
  ADD_TEST(test_heap_alloc_basic);
  ADD_TEST(test_heap_alloc_out_of_memory);
  ADD_TEST(test_heap_free_basic);
  ADD_TEST(test_heap_free_null);
  ADD_TEST(test_heap_reuse_freed_block);
  ADD_TEST(test_heap_get_stats_initial);
  ADD_TEST(test_heap_split_block);
  ADD_TEST(test_heap_coalesce_next);
  ADD_TEST(test_heap_realloc_null);
  ADD_TEST(test_heap_realloc_null_to_zero);
  ADD_TEST(test_heap_realloc_to_zero);
  ADD_TEST(test_heap_realloc_shrink);
  ADD_TEST(test_heap_realloc_grow_in_place);
  ADD_TEST(test_heap_realloc_grow_moves);
  ADD_TEST(test_heap_get_stats_after_alloc_free);
  ADD_TEST(test_heap_destroy);

  return count;
}
