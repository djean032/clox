#ifndef clox_memory_h
#define clox_memory_h

#include "common.h"
#include "memory_constants.h"
#include <stddef.h>

#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, oldCount, newCount)                          \
  (type *)reallocate(pointer, sizeof(type) * (oldCount),                       \
                     sizeof(type) * (newCount))

#define FREE_ARRAY(type, pointer, oldCount)                                    \
  reallocate(pointer, sizeof(type) * (oldCount), 0)

void *reallocate(void *pointer, size_t oldSize, size_t newSize);
/*
root malloc()
└── Arena
    ├── MemorySpan: VM Heap
    │   └── General allocator
    │       ├── physical block list
    │       ├── segregated free lists
    │       ├── large/variable allocations
    │       └── slab pages
    │           └── Slab allocator
    │               ├── class 32  → slabs → slots
    │               ├── class 64  → slabs → slots
    │               ├── class 128 → slabs → slots
    │               ├── class 256 → slabs → slots
    │               └── class 512 → slabs → slots
    │
    ├── MemorySpan: Compiler scratch arena
    │   └── bump allocations, reset after compilation
    │
    └── remaining unused arena space
  */
// Arena implementations
//
// Spans inside Arena

typedef struct MemorySpan {
  uint8_t *base;
  size_t size;
} MemorySpan;

// Arena struct
typedef struct Arena {
  uint8_t *base;
  size_t capacity;
  size_t offset;
  bool owner;
  const char *name;
} Arena;

// Arena methods
void arenaInit(Arena *arena, void *memory, size_t capacity, const char *name);
bool arenaCreate(Arena *arena, size_t capacity, const char *name);
void arenaDestroy(Arena *arena);
void *arenaAlloc(Arena *arena, size_t size, size_t alignment);
void *arenaAllocZero(Arena *arena, size_t size, size_t alignment);
bool arenaAllocSpan(Arena *arena, size_t size, size_t alignment,
                    MemorySpan *out);
size_t arenaMark(const Arena *arena);
size_t arenaUsed(const Arena *arena);
size_t arenaRemaining(const Arena *arena);
void arenaRestore(Arena *arena, size_t pos);
void arenaReset(Arena *arena);

// Heaps

typedef struct HeapBlock HeapBlock;

typedef struct HeapStats {
  size_t totalBytes;
  size_t usedBytes;
  size_t freeBytes;

  size_t largestFreeBlock;
  size_t blockCount;
  size_t usedBlockCount;
  size_t freeBlockCount;

  size_t allocationCount;
  size_t freeCount;
} HeapStats;

typedef struct Heap {
  uint8_t *base;
  size_t size;

  HeapBlock *firstBlock;
  HeapBlock *freeLists[HEAP_NUM_BUCKETS];

  size_t usedBytes;
  size_t allocationCount;
  size_t freeCount;
} Heap;

bool heapInit(Heap *heap, MemorySpan span);
void heapDestroy(Heap *heap);
void *heapAlloc(Heap *heap, size_t size);
void heapFree(Heap *heap, void *ptr);
void *heapRealloc(Heap *heap, void *ptr, size_t newSize);
bool heapOwnsPointer(const Heap *heap, const void *ptr);
void heapGetStats(const Heap *heap, HeapStats *out);
void heapDump(const Heap *heap);
void heapDumpFreeLists(const Heap *heap);
bool heapValidate(const Heap *heap);

// Slabs

typedef struct Slab Slab;
typedef struct SlabPageChunk SlabPageChunk;

typedef struct SlabClass {
  size_t slotSize;

  Slab *partialSlabs;
  Slab *fullSlabs;

  size_t slabCount;
  size_t usedSlots;
  size_t freeSlots;
} SlabClass;

typedef struct SlabStats {
  size_t pageSize;

  size_t slabCount;
  size_t fullSlabCount;
  size_t partialSlabCount;

  size_t totalSlots;
  size_t usedSlots;
  size_t freeSlots;

  size_t allocationCount;
  size_t freeCount;
} SlabStats;

typedef struct SlabAllocator {
  Heap *heap;

  SlabPageChunk *chunks;
  SlabClass classes[SLAB_CLASS_COUNT];

  size_t allocationCount;
  size_t freeCount;
} SlabAllocator;

bool slabInit(SlabAllocator *slabs, Heap *heap);
void slabDestroy(SlabAllocator *slabs);
bool slabCanAlloc(size_t size);
void *slabAlloc(SlabAllocator *slabs, size_t size);
void slabFree(SlabAllocator *slabs, void *ptr);
void *slabRealloc(SlabAllocator *slabs, void *ptr, size_t oldSize,
                  size_t newSize);
bool slabOwnsPointer(const SlabAllocator *slabs, const void *ptr);
void slabGetStats(const SlabAllocator *slabs, SlabStats *out);
void slabDump(const SlabAllocator *slabs);
bool slabValidate(const SlabAllocator *slabs);
#endif // !clox_memory_h
