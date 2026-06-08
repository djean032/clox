#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "memory_constants.h"

#define SLAB_MAGIC 0x51AB51ABu
#define IS_POWER_OF_TWO(x) ((x) != 0 && (((x) & ((x) - 1)) == 0))

static const size_t slabClassSizes[SLAB_CLASS_COUNT] = {32, 64, 128, 256, 512};

static inline size_t alignUpSize(size_t value, size_t alignment) {
  assert(IS_POWER_OF_TWO(alignment));
  return (value + alignment - 1) & ~(size_t)(alignment - 1);
}

static inline uintptr_t alignUpPtr(uintptr_t value, size_t alignment) {
  assert(IS_POWER_OF_TWO(alignment));
  return (value + alignment - 1) & ~(uintptr_t)(alignment - 1);
}

void *reallocate(void *pointer, size_t oldSize, size_t newSize) {
  (void)oldSize;

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

// Heap

struct HeapBlock {
  size_t size;
  bool free;

  struct HeapBlock *prevPhys;
  struct HeapBlock *nextPhys;

  struct HeapBlock *prevFree;
  struct HeapBlock *nextFree;
};

static void *payloadFromBlock(HeapBlock *block) {
  assert(block != NULL);
  return (void *)((uint8_t *)block + sizeof(HeapBlock));
}

static void *blockFromPayload(void *ptr) {
  assert(ptr != NULL);
  return (HeapBlock *)((uint8_t *)ptr - sizeof(HeapBlock));
}

static int bucketForSize(size_t size) {
  int bucket = 0;
  size_t classSize = HEAP_ALIGNMENT;
  while (bucket < (HEAP_NUM_BUCKETS - 1) && size > classSize) {
    classSize = classSize << 1;
    bucket = bucket + 1;
  }

  return bucket;
}

static void insertFreeBlock(Heap *heap, HeapBlock *block) {
  assert(heap != NULL);
  assert(block != NULL);
  assert(block->free);

  int bucket = bucketForSize(block->size);

  block->prevFree = NULL;
  block->nextFree = heap->freeLists[bucket];

  if (block->nextFree != NULL) {
    block->nextFree->prevFree = block;
  }

  heap->freeLists[bucket] = block;
}

static void removeFreeBlock(Heap *heap, HeapBlock *block) {
  assert(heap != NULL);
  assert(block != NULL);
  assert(block->free);

  int bucket = bucketForSize(block->size);

  if (block->prevFree != NULL) {
    block->prevFree->nextFree = block->nextFree;
  } else {
    heap->freeLists[bucket] = block->nextFree;
  }

  if (block->nextFree != NULL) {
    block->nextFree->prevFree = block->prevFree;
  }

  block->prevFree = NULL;
  block->nextFree = NULL;
}

static void splitBlock(Heap *heap, HeapBlock *block, size_t wantedSize) {
  assert(heap != NULL);
  assert(block != NULL);
  assert(block->size >= wantedSize);

  size_t remaining = block->size - wantedSize;

  if (remaining < (sizeof(HeapBlock) + HEAP_MIN_SPLIT_SIZE)) {
    return;
  }

  uint8_t *newBlockAddress = (uint8_t *)block + sizeof(HeapBlock) + wantedSize;

  HeapBlock *newBlock = (HeapBlock *)newBlockAddress;
  newBlock->size = remaining - sizeof(HeapBlock);
  newBlock->free = true;

  newBlock->prevPhys = block;
  newBlock->nextPhys = block->nextPhys;
  newBlock->prevFree = NULL;
  newBlock->nextFree = NULL;

  if (newBlock->nextPhys != NULL) {
    newBlock->nextPhys->prevPhys = newBlock;
  }

  if (newBlock->nextPhys != NULL && newBlock->nextPhys->free) {
    HeapBlock *next = newBlock->nextPhys;

    removeFreeBlock(heap, next);

    newBlock->size += sizeof(HeapBlock) + next->size;
    newBlock->nextPhys = next->nextPhys;

    if (newBlock->nextPhys != NULL) {
      newBlock->nextPhys->prevPhys = newBlock;
    }
  }

  block->nextPhys = newBlock;
  block->size = wantedSize;

  insertFreeBlock(heap, newBlock);
}

static HeapBlock *coalesceBlock(Heap *heap, HeapBlock *block) {
  assert(heap != NULL);
  assert(block != NULL);
  assert(block->free);

  if (block->prevPhys != NULL && block->prevPhys->free) {
    HeapBlock *prev = block->prevPhys;

    removeFreeBlock(heap, prev);

    prev->size += sizeof(HeapBlock) + block->size;
    prev->nextPhys = block->nextPhys;

    if (prev->nextPhys != NULL) {
      prev->nextPhys->prevPhys = prev;
    }

    block = prev;
  }

  if (block->nextPhys != NULL && block->nextPhys->free) {
    HeapBlock *next = block->nextPhys;

    removeFreeBlock(heap, next);

    block->size = block->size + sizeof(HeapBlock) + next->size;
    block->nextPhys = next->nextPhys;

    if (block->nextPhys != NULL) {
      block->nextPhys->prevPhys = block;
    }
  }

  block->prevFree = NULL;
  block->nextFree = NULL;

  return block;
}

static uint8_t *blockEnd(HeapBlock *block) {
  assert(block != NULL);

  return (uint8_t *)block + sizeof(HeapBlock) + block->size;
}

bool heapOwnsPointer(const Heap *heap, const void *ptr) {
  assert(heap != NULL);
  assert(heap->base != NULL);
  if (ptr == NULL)
    return false;

  const uint8_t *p = (const uint8_t *)ptr;
  const uint8_t *start = (const uint8_t *)heap->base;
  const uint8_t *end = (const uint8_t *)(heap->base + heap->size);

  return p >= start && p < end;
}
// TODO: insertfreeblock func

bool heapInit(Heap *heap, MemorySpan span) {
  assert(heap != NULL);

  if (span.base == NULL)
    return false;

  if (span.size <= (sizeof(HeapBlock) + HEAP_MIN_SPLIT_SIZE)) {
    return false;
  }

  memset((void *)heap, 0, sizeof(*heap));

  heap->base = (uint8_t *)span.base;
  heap->size = span.size;

  HeapBlock *block = (HeapBlock *)span.base;

  block->size = span.size - sizeof(HeapBlock);
  block->free = true;

  block->prevPhys = NULL;
  block->nextPhys = NULL;
  block->prevFree = NULL;
  block->nextFree = NULL;

  heap->firstBlock = block;

  insertFreeBlock(heap, block);

  return true;
}

void *heapAlloc(Heap *heap, size_t size) {
  assert(heap != NULL);
  assert(size > 0);

  size_t wantedSize = alignUpSize(size, HEAP_ALIGNMENT);
  int startBucket = bucketForSize(wantedSize);

  for (int i = startBucket; i < HEAP_NUM_BUCKETS; i++) {
    HeapBlock *block = heap->freeLists[i];

    while (block != NULL) {
      HeapBlock *next = block->nextFree;

      if (block->size >= wantedSize) {
        removeFreeBlock(heap, block);

        splitBlock(heap, block, wantedSize);

        block->free = false;
        block->prevFree = NULL;
        block->nextFree = NULL;

        heap->usedBytes += block->size;
        heap->allocationCount++;

        return payloadFromBlock(block);
      }

      block = next;
    }
  }

  return NULL;
}
void heapFree(Heap *heap, void *ptr) {
  assert(heap != NULL);

  if (ptr == NULL)
    return;

  assert(heapOwnsPointer(heap, ptr));

  HeapBlock *block = blockFromPayload(ptr);

  assert(block != NULL);
  assert(!block->free);

  heap->usedBytes = heap->usedBytes - block->size;
  heap->freeCount++;

  block->free = true;
  block->prevFree = NULL;
  block->nextFree = NULL;

  block = coalesceBlock(heap, block);

  insertFreeBlock(heap, block);
}

void *heapRealloc(Heap *heap, void *ptr, size_t newSize) {
  assert(heap != NULL);

  if (ptr == NULL) {
    if (newSize == 0) {
      return NULL;
    }

    return heapAlloc(heap, newSize);
  }

  if (newSize == 0) {
    heapFree(heap, ptr);
    return NULL;
  }

  assert(heapOwnsPointer(heap, ptr));

  HeapBlock *block = blockFromPayload(ptr);
  assert(block != NULL);
  assert(!block->free);

  size_t wantedSize = alignUpSize(newSize, HEAP_ALIGNMENT);

  // Shrinking

  if (wantedSize <= block->size) {
    size_t oldBlockSize = block->size;

    splitBlock(heap, block, wantedSize);

    if (block->size < oldBlockSize) {
      heap->usedBytes -= (oldBlockSize - block->size);
    }

    return ptr;
  }

  // Growing into next physical block

  HeapBlock *next = block->nextPhys;

  if (next != NULL && next->free) {
    size_t combinedSize = block->size + sizeof(HeapBlock) + next->size;

    if (combinedSize >= wantedSize) {
      size_t oldBlockSize = block->size;

      removeFreeBlock(heap, next);

      block->size = combinedSize;
      block->nextPhys = next->nextPhys;

      if (block->nextPhys != NULL) {
        block->nextPhys->prevPhys = block;
      }

      splitBlock(heap, block, wantedSize);

      if (block->size > oldBlockSize) {
        heap->usedBytes += (block->size - oldBlockSize);
      }

      return payloadFromBlock(block);
    }
  }

  // Allocate a new block

  void *newPtr = heapAlloc(heap, newSize);
  if (newPtr == NULL)
    return NULL;

  size_t copySize = block->size < newSize ? block->size : newSize;
  memcpy(newPtr, ptr, copySize);

  heapFree(heap, ptr);

  return newPtr;
}

void heapGetStats(const Heap *heap, HeapStats *out) {
  assert(heap != NULL);
  assert(out != NULL);

  memset((void *)out, 0, sizeof(*out));

  out->totalBytes = heap->size;
  out->allocationCount = heap->allocationCount;
  out->freeCount = heap->freeCount;

  for (HeapBlock *block = heap->firstBlock; block != NULL;
       block = block->nextPhys) {
    out->blockCount++;

    if (block->free) {
      out->freeBlockCount++;
      out->freeBytes += block->size;

      if (block->size > out->largestFreeBlock) {
        out->largestFreeBlock = block->size;
      }
    } else {
      out->usedBlockCount++;
      out->usedBytes += block->size;
    }
  }
}

void heapDump(const Heap *heap) {
  assert(heap != NULL);

  printf("HEAP DUMP\n");

  int index = 0;

  for (HeapBlock *block = heap->firstBlock; block != NULL;
       block = block->nextPhys) {
    printf("block %d: addr=%p size=%zu %s prevPhys=%p nextPhys=%p\n", index,
           (void *)block, block->size, block->free ? "free" : "used",
           (void *)block->prevPhys, (void *)block->nextPhys);

    index++;
  }
}

void heapDumpFreeLists(const Heap *heap) {
  assert(heap != NULL);

  printf("HEAP FREE LISTS\n");

  for (int i = 0; i < HEAP_NUM_BUCKETS; i++) {
    printf("bucket %d:\n", i);

    for (HeapBlock *block = heap->freeLists[i]; block != NULL;
         block = block->nextFree) {
      printf("   addr=%p size=%zu prevFree=%p nextFree=%p\n", (void *)block,
             block->size, (void *)block->prevFree, (void *)block->nextFree);
    }
  }
}

bool heapValidate(const Heap *heap) {
  assert(heap != NULL);
  assert(heap->base != NULL);

  const uint8_t *heapStart = heap->base;
  const uint8_t *heapEnd = (const uint8_t *)(heap->base + heap->size);

  size_t physicalFreeCount = 0;
  size_t freeListCount = 0;

  HeapBlock *prev = NULL;

  for (HeapBlock *block = heap->firstBlock; block != NULL;
       block = block->nextPhys) {
    const uint8_t *blockStart = (const uint8_t *)block;
    const uint8_t *headerEnd = (const uint8_t *)block + sizeof(HeapBlock);
    const uint8_t *end = (const uint8_t *)blockEnd(block);

    if (blockStart < heapStart) {
      return false;
    }

    if (headerEnd > heapEnd) {
      return false;
    }

    if (end > heapEnd) {
      return false;
    }

    if (block->prevPhys != prev) {
      return false;
    }

    if (prev != NULL) {
      if (blockEnd(prev) != (uint8_t *)block) {
        return false;
      }
    }

    if (block->free) {
      physicalFreeCount++;

      if (block->nextPhys != NULL && block->nextPhys->free) {
        return false;
      }
    }

    prev = block;
  }

  for (int bucket = 0; bucket < HEAP_NUM_BUCKETS; bucket++) {
    HeapBlock *prevFree = NULL;

    for (HeapBlock *block = heap->freeLists[bucket]; block != NULL;
         block = block->nextFree) {
      if (!block->free) {
        return false;
      }

      if (!heapOwnsPointer(heap, (const void *)block)) {
        return false;
      }

      if (block->prevFree != prevFree) {
        return false;
      }

      int actualBucket = bucketForSize(block->size);
      if (actualBucket != bucket) {
        return false;
      }

      freeListCount++;
      prevFree = block;
    }
  }

  if (physicalFreeCount != freeListCount) {
    return false;
  }

  return true;
}

void heapDestroy(Heap *heap) {
  assert(heap != NULL);

  memset(heap, 0, sizeof(*heap));
}

// Slabs

struct SlabPageChunk {
  void *rawAllocation;
  uint8_t *firstPage;
  size_t pageCount;
  size_t usedPages;

  struct SlabPageChunk *next;
};

struct Slab {
  uint32_t magic;

  uint8_t *pageStart;
  uint8_t *slotStart;

  size_t slotSize;
  size_t totalSlots;
  size_t usedSlots;

  void *freeList;

  SlabClass *class;

  Slab *prev;
  Slab *next;
};

static bool isPageAligned(const void *ptr) {
  return ((uintptr_t)ptr % SLAB_PAGE_SIZE) == 0;
}

// Class helpers:
static int slabClassIndexForSize(size_t size) {
  if (!slabCanAlloc(size)) {
    return -1;
  }
  for (int i = 0; i < SLAB_CLASS_COUNT; i++) {
    if (size <= slabClassSizes[i]) {
      return i;
    }
  }
  return -1;
}
static SlabClass *slabClassForSize(SlabAllocator *slabs, size_t size) {
  assert(slabs != NULL);

  int index = slabClassIndexForSize(size);
  return index < 0 ? NULL : &slabs->classes[index];
}
// List helpers:
static void insertSlab(Slab **list, Slab *slab) {
  assert(list != NULL);
  assert(slab != NULL);
  assert(slab->prev == NULL);
  assert(slab->next == NULL);

  slab->prev = NULL;
  slab->next = *list;

  if (*list != NULL) {
    (*list)->prev = slab;
  }
  *list = slab;
}

static void removeSlab(Slab **list, Slab *slab) {
  assert(list != NULL);
  assert(*list != NULL);
  assert(slab != NULL);

  if (slab->prev != NULL) {
    slab->prev->next = slab->next;
  } else {
    *list = slab->next;
  }
  if (slab->next != NULL) {
    slab->next->prev = slab->prev;
  }
  slab->prev = NULL;
  slab->next = NULL;
}

static void moveSlab(Slab **from, Slab **to, Slab *slab) {
  assert(from != NULL);
  assert(to != NULL);
  assert(slab != NULL);

  removeSlab(from, slab);
  insertSlab(to, slab);
}

// Chunk / page helpers:
static bool allocateSlabChunk(SlabAllocator *slabs) {
  assert(slabs != NULL);
  assert(slabs->heap != NULL);
  assert(IS_POWER_OF_TWO(SLAB_PAGE_SIZE));
  assert(SLAB_CHUNK_PAGE_COUNT > 0);

  size_t pageBytes = SLAB_CHUNK_PAGE_COUNT * SLAB_PAGE_SIZE;
  size_t rawSize = sizeof(SlabPageChunk) + pageBytes + (SLAB_PAGE_SIZE - 1);

  void *raw = heapAlloc(slabs->heap, rawSize);
  if (raw == NULL)
    return false;

  SlabPageChunk *chunk = (SlabPageChunk *)raw;
  uint8_t *afterHeader = (uint8_t *)raw + sizeof(SlabPageChunk);
  uint8_t *firstPage =
      (uint8_t *)alignUpPtr((uintptr_t)afterHeader, SLAB_PAGE_SIZE);
  assert(isPageAligned(firstPage));

  chunk->rawAllocation = raw;
  chunk->firstPage = firstPage;
  chunk->pageCount = SLAB_CHUNK_PAGE_COUNT;
  chunk->usedPages = 0;
  chunk->next = slabs->chunks;
  slabs->chunks = chunk;
  return true;
}

static uint8_t *allocateSlabPage(SlabAllocator *slabs) {
  assert(slabs != NULL);
  assert(slabs->heap != NULL);

  for (SlabPageChunk *chunk = slabs->chunks; chunk != NULL;
       chunk = chunk->next) {
    if (chunk->usedPages < chunk->pageCount) {
      uint8_t *page = chunk->firstPage + (chunk->usedPages * SLAB_PAGE_SIZE);
      chunk->usedPages++;
      assert(isPageAligned(page));
      return page;
    }
  }

  if (!allocateSlabChunk(slabs)) {
    return NULL;
  }
  SlabPageChunk *chunk = slabs->chunks;
  assert(chunk != NULL);
  assert(chunk->usedPages < chunk->pageCount);

  uint8_t *page = chunk->firstPage + chunk->usedPages * SLAB_PAGE_SIZE;
  chunk->usedPages++;
  assert(isPageAligned(page));
  return page;
}

static void releaseSlabChunks(SlabAllocator *slabs);

// Pointer helpers:
static bool slabContainsPointer(const Slab *slab, const void *ptr) {
  if (ptr == NULL || slab == NULL) {
    return false;
  }
  const uint8_t *p = ptr;
  return p >= slab->slotStart && p < (slab->pageStart + SLAB_PAGE_SIZE);
}

static bool slabPointerIsSlotAligned(const Slab *slab, const void *ptr) {
  if (!slabContainsPointer(slab, ptr)) {
    return false;
  }
  uintptr_t offset = (uintptr_t)((const uint8_t *)ptr - slab->slotStart);
  return offset % slab->slotSize == 0;
}

static bool slabOwnsSlot(const Slab *slab, const void *ptr) {
  return slab != NULL && slabContainsPointer(slab, ptr) &&
         slabPointerIsSlotAligned(slab, ptr);
}

// Slab/slot helpers:
static void initSlotFreeList(Slab *slab) {
  assert(slab != NULL);
  assert(slab->pageStart != NULL);
  assert(slab->slotStart != NULL);
  assert(slab->slotSize >= sizeof(void *));
  assert(slab->totalSlots > 0);
  assert(slab->usedSlots == 0);

  slab->freeList = NULL;
  for (size_t i = 0; i < slab->totalSlots; i++) {
    uint8_t *slot = slab->slotStart + (i * slab->slotSize);
    *(void **)slot = slab->freeList;
    slab->freeList = slot;
  }
}

static Slab *initSlabPage(uint8_t *pageBase, SlabClass *class) {
  assert(pageBase != NULL);
  assert(class != NULL);
  assert(class->slotSize > 0);
  assert(class->slotSize <= SLAB_MAX_ALLOC_SIZE);
  assert((class->slotSize % HEAP_ALIGNMENT) == 0);
  assert(isPageAligned(pageBase));

  Slab *slab = (Slab *)pageBase;
  memset(slab, 0, sizeof(*slab));

  slab->magic = SLAB_MAGIC;
  slab->pageStart = pageBase;
  slab->slotSize = class->slotSize;
  slab->class = class;
  slab->slotStart =
      (uint8_t *)alignUpPtr((uintptr_t)pageBase + sizeof(Slab), HEAP_ALIGNMENT);
  slab->totalSlots =
      (SLAB_PAGE_SIZE - (size_t)(slab->slotStart - pageBase)) / slab->slotSize;
  slab->usedSlots = 0;

  assert(slab->slotStart >= slab->pageStart);
  assert(slab->slotStart < slab->pageStart + SLAB_PAGE_SIZE);
  assert(((uintptr_t)slab->slotStart % HEAP_ALIGNMENT) == 0);
  assert(slab->totalSlots > 0);

  initSlotFreeList(slab);
  return slab;
}

static void *popFreeSlot(Slab *slab) {
  assert(slab != NULL);
  assert(slab->usedSlots < slab->totalSlots);

  void *slot = slab->freeList;
  if (slot == NULL)
    return NULL;
  slab->freeList = *(void **)slot;
  slab->usedSlots++;
  return slot;
}

static void pushFreeSlot(Slab *slab, void *ptr) {
  assert(slab != NULL);
  assert(ptr != NULL);
  assert(slabOwnsSlot(slab, ptr));
  assert(slab->usedSlots > 0);

  *(void **)ptr = slab->freeList;
  slab->freeList = ptr;
  slab->usedSlots--;
}

static Slab *ensurePartialSlab(SlabAllocator *slabs, SlabClass *class) {
  assert(slabs != NULL);
  assert(slabs->heap != NULL);
  assert(class != NULL);
  assert(class->slotSize > 0);

  if (class->partialSlabs != NULL) {
    return class->partialSlabs;
  }
  uint8_t *page = allocateSlabPage(slabs);
  if (page == NULL) {
    return NULL;
  }
  Slab *slab = initSlabPage(page, class);
  insertSlab(&class->partialSlabs, slab);

  class->slabCount++;
  class->freeSlots += slab->totalSlots;
  assert(slab->freeList != NULL);
  assert(slab->usedSlots < slab->totalSlots);
  return slab;
}

static bool slabPageBelongsToChunk(const SlabAllocator *slabs,
                                   const uint8_t *pageBase) {
  assert(slabs != NULL);
  if (pageBase == NULL) {
    return false;
  }

  for (SlabPageChunk *chunk = slabs->chunks; chunk != NULL;
       chunk = chunk->next) {
    const uint8_t *start = chunk->firstPage;
    const uint8_t *end = start + chunk->usedPages * SLAB_PAGE_SIZE;

    if (pageBase >= start && pageBase < end) {
      size_t offset = (size_t)(pageBase - start);
      return (offset % SLAB_PAGE_SIZE) == 0;
    }
  }
  return false;
}

static Slab *slabFromPointer(const SlabAllocator *slabs, const void *ptr) {
  if (slabs == NULL || ptr == NULL)
    return NULL;

  if (slabs->heap == NULL || !heapOwnsPointer(slabs->heap, ptr)) {
    return NULL;
  }

  uintptr_t pageAddress = (uintptr_t)ptr & ~(uintptr_t)(SLAB_PAGE_SIZE - 1);
  uint8_t *pageBase = (uint8_t *)pageAddress;

  if (!slabPageBelongsToChunk(slabs, pageBase)) {
    return NULL;
  }

  Slab *slab = (Slab *)pageBase;
  if (slab->magic != SLAB_MAGIC)
    return NULL;
  return slab;
}

// Validation helpers:
static size_t countFreeSlots(const Slab *slab) {
  size_t count = 0;
  uint8_t *slot = slab->freeList;
  while (slot != NULL) {
    if (!slabOwnsSlot(slab, (void *)slot)) {
      return SIZE_MAX;
    }
    count++;
    if (count > slab->totalSlots) {
      return SIZE_MAX;
    }
    slot = *(void **)slot;
  }
  return count;
}
static bool validateSlab(const Slab *slab) {
  if (slab == NULL) {
    return false;
  }

  if (slab->magic != SLAB_MAGIC) {
    return false;
  }

  if (slab->pageStart == NULL || !isPageAligned(slab->pageStart)) {
    return false;
  }

  if (slab->slotStart == NULL) {
    return false;
  }

  if (slab->slotStart < slab->pageStart) {
    return false;
  }

  if (slab->slotStart >= slab->pageStart + SLAB_PAGE_SIZE) {
    return false;
  }

  if (((uintptr_t)slab->slotStart % HEAP_ALIGNMENT) != 0) {
    return false;
  }

  if (slab->slotSize == 0 || slab->slotSize > SLAB_MAX_ALLOC_SIZE) {
    return false;
  }

  if ((slab->slotSize % HEAP_ALIGNMENT) != 0) {
    return false;
  }

  if (slab->totalSlots == 0) {
    return false;
  }

  if (slab->usedSlots > slab->totalSlots) {
    return false;
  }

  size_t freeSlots = countFreeSlots(slab);
  if (freeSlots == SIZE_MAX) {
    return false;
  }
  if (freeSlots != slab->totalSlots - slab->usedSlots) {
    return false;
  }

  return true;
}

static bool validateSlabList(const SlabClass *class, const Slab *list,
                             bool expectFull, size_t *slabCount,
                             size_t *usedSlots, size_t *freeSlots) {
  const Slab *prev = NULL;
  for (const Slab *slab = list; slab != NULL; slab = slab->next) {
    if (!validateSlab(slab)) {
      return false;
    }

    if (slab->class != class) {
      return false;
    }

    if (slab->prev != prev) {
      return false;
    }

    if (slab->slotSize != class->slotSize) {
      return false;
    }

    if (expectFull) {
      if (slab->usedSlots != slab->totalSlots) {
        return false;
      }
    } else {
      if (slab->usedSlots >= slab->totalSlots) {
        return false;
      }
    }
    size_t slabFreeSlots = slab->totalSlots - slab->usedSlots;

    if (slabCount != NULL) {
      (*slabCount)++;
    }

    if (usedSlots != NULL) {
      *usedSlots += slab->usedSlots;
    }

    if (freeSlots != NULL) {
      *freeSlots += slabFreeSlots;
    }

    prev = slab;
  }

  return true;
}

bool slabValidate(const SlabAllocator *slabs) {
  if (slabs == NULL || slabs->heap == NULL) {
    return false;
  }

  for (SlabPageChunk *chunk = slabs->chunks; chunk != NULL;
       chunk = chunk->next) {
    if (chunk->rawAllocation == NULL) {
      return false;
    }

    if (chunk->firstPage == NULL || !isPageAligned(chunk->firstPage)) {
      return false;
    }

    if (chunk->pageCount == 0) {
      return false;
    }

    if (chunk->usedPages > chunk->pageCount) {
      return false;
    }

    if (!heapOwnsPointer(slabs->heap, chunk->rawAllocation)) {
      return false;
    }
  }

  for (int i = 0; i < SLAB_CLASS_COUNT; i++) {
    const SlabClass *class = &slabs->classes[i];

    if (class->slotSize != slabClassSizes[i]) {
      return false;
    }

    size_t slabCount = 0;
    size_t usedSlots = 0;
    size_t freeSlots = 0;

    if (!validateSlabList(class, class->partialSlabs, false, &slabCount,
                          &usedSlots, &freeSlots)) {
      return false;
    }

    if (!validateSlabList(class, class->fullSlabs, true, &slabCount, &usedSlots,
                          &freeSlots)) {
      return false;
    }

    if (slabCount != class->slabCount) {
      return false;
    }

    if (usedSlots != class->usedSlots) {
      return false;
    }

    if (freeSlots != class->freeSlots) {
      return false;
    }

    for (const Slab *slab = class->partialSlabs; slab != NULL;
         slab = slab->next) {
      if (!slabPageBelongsToChunk(slabs, slab->pageStart)) {
        return false;
      }
    }
    for (const Slab *slab = class->fullSlabs; slab != NULL; slab = slab->next) {
      if (!slabPageBelongsToChunk(slabs, slab->pageStart)) {
        return false;
      }
    }
  }

  return true;
}

bool slabInit(SlabAllocator *slabs, Heap *heap) {
  assert(slabs != NULL);
  assert(heap != NULL);

  memset(slabs, 0, sizeof(*slabs));
  slabs->heap = heap;

  for (int i = 0; i < SLAB_CLASS_COUNT; i++) {
    slabs->classes[i].slotSize = slabClassSizes[i];
  }

  return true;
}

void *slabAlloc(SlabAllocator *slabs, size_t size) {
  assert(slabs != NULL);
  SlabClass *class = slabClassForSize(slabs, size);
  if (class == NULL) {
    return NULL;
  }

  Slab *slab = ensurePartialSlab(slabs, class);
  if (slab == NULL) {
    return NULL;
  }

  void *slot = popFreeSlot(slab);
  assert(slot != NULL);

  class->usedSlots++;
  class->freeSlots--;
  slabs->allocationCount++;

  if (slab->usedSlots == slab->totalSlots) {
    moveSlab(&class->partialSlabs, &class->fullSlabs, slab);
  }

  return slot;
}
void slabDestroy(SlabAllocator *slabs) {}
bool slabCanAlloc(size_t size) {
  return size > 0 && size <= SLAB_MAX_ALLOC_SIZE;
}
void slabFree(SlabAllocator *slabs, void *ptr);
void *slabRealloc(SlabAllocator *slabs, void *ptr, size_t oldSize,
                  size_t newSize);
bool slabOwnsPointer(const SlabAllocator *slabs, const void *ptr);
void slabGetStats(const SlabAllocator *slabs, SlabStats *out);
void slabDump(const SlabAllocator *slabs);

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
