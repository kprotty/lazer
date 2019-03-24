#ifndef LZR_HEAP_H
#define LZR_HEAP_H

#include "memory.h"

#define LZR_HEAP_START      (1ULL << 35)
#define LZR_HEAP_END        (1ULL << 36)
#define LZR_HEAP_CHUNK_SIZE (2 * 1024 * 1024)

void lzr_heap_init();

void lzr_heap_free(void* ptr);

void* lzr_heap_alloc(size_t num_chunks);

#endif // LZR_HEAP_H