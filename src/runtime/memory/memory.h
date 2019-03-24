#ifndef LZR_MEMORY_H
#define LZR_MEMORY_H

#include "../system.h"

void* lzr_memory_map(void* addr, size_t bytes, bool commit);

bool lzr_memory_unmap(void* addr, size_t bytes);

// ensure that virtual address range is mapped to physical memory
bool lzr_memory_commit(void* addr, size_t bytes);

// discard the mapping of physical memory to the virtual address range
bool lzr_memory_decommit(void* addr, size_t bytes);

#endif // LZR_MEMORY_H