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

// the heap needs to initially be mapped into memory unless alloc/free will segfault
void lzr_heap_init();

// allocate `num_chunk` amount of chunks (2mb).
// The caller is responsible for committing it.
void* lzr_heap_alloc(uint16_t num_chunks);

// free a 2mb aligned pointer in the lazer heap address range.
// again, the caller is responsible for decommit it.
void lzr_heap_free(void* ptr);

#endif // LZR_HEAP_H