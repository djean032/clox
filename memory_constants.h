#ifndef CLOX_MEMORY_CONSTANTS_H
#define CLOX_MEMORY_CONSTANTS_H

#include <stddef.h>

#define KiB(n) ((size_t)(n) * 1024u)
#define MiB(n) ((size_t)(n) * 1024u * 1024u)

/*
 * Top-level VM memory budget.
 */
#ifndef CLOX_ROOT_ARENA_SIZE
#define CLOX_ROOT_ARENA_SIZE MiB(128)
#endif

#ifndef CLOX_VM_HEAP_SIZE
#define CLOX_VM_HEAP_SIZE MiB(64)
#endif

#ifndef CLOX_COMPILER_ARENA_SIZE
#define CLOX_COMPILER_ARENA_SIZE MiB(8)
#endif

/*
 * General heap allocator.
 */
#ifndef HEAP_ALIGNMENT
#define HEAP_ALIGNMENT 16
#endif

#ifndef HEAP_MIN_SPLIT_SIZE
#define HEAP_MIN_SPLIT_SIZE 32
#endif

#ifndef HEAP_NUM_BUCKETS
#define HEAP_NUM_BUCKETS 24
#endif

/*
 * Slab allocator.
 */
#ifndef SLAB_PAGE_SIZE
#define SLAB_PAGE_SIZE KiB(4)
#endif

#ifndef SLAB_CLASS_COUNT
#define SLAB_CLASS_COUNT 5
#endif

#ifndef SLAB_MAX_ALLOC_SIZE
#define SLAB_MAX_ALLOC_SIZE 512
#endif

#ifndef SLAB_CHUNK_PAGE_COUNT
#define SLAB_CHUNK_PAGE_COUNT 16
#endif

/*
 * Debugging.
 */
#ifndef HEAP_DEBUG
#define HEAP_DEBUG 0
#endif

#endif
