#ifndef LZR_HEAP_H
#define LZR_HEAP_H

#include "memory.h"

/*
lazer memory maps a fixed 32gb heap and allocates chunks off of it at a 2mb granularity.
This is because 32gb worth of address space is 35 bits. When address are aligned by 8 bytes
the bottom 3 bits are always zero so a pointer in this space can be compressed down to
35 - 3 = 32 bits and fit nicely inside a uint32_t. This helps in saving memory later.
*/
#define LZR_HEAP_BEGIN      (1ULL << 35)
#define LZR_HEAP_END        (1ULL << 36)
#define LZR_HEAP_CHUNK_SIZE (2 * 1024 * 1024)

// compression and decompression of 8-byte aligned heap pointers
#define LZR_PTR_ZIP(ptr) ((uint32_t) ((((size_t) (ptr)) - LZR_HEAP_BEGIN) >> 3))
#define LZR_PTR_UNZIP(ptr) ((void*) ((((size_t) (ptr)) << 3) + LZR_HEAP_BEGIN))

// the heap needs to initially be mapped into memory unless alloc/free will segfault
void lzr_heap_init();

// used for reserving static memory that wont be free before committing the heap
void* lzr_heap_reserve(uint16_t num_chunks);

// no more reserving. lock it down and make it safe to use alloc/free
void lzr_heap_commit();

// allocate `num_chunk` amount of chunks (2mb).
// The caller is responsible for committing it.
void* lzr_heap_alloc(uint16_t num_chunks);

// free a 2mb aligned pointer in the lazer heap address range.
// again, the caller is responsible for decommit it.
void lzr_heap_free(void* ptr);

#endif // LZR_HEAP_H