#ifndef LZR_MEMORY_H
#define LZR_MEMORY_H

#include "../system.h"

// virtually map memory (TODO: add support for executable memory down the road)
void* lzr_memory_map(void* addr, size_t bytes, bool commit);

// tbh i dont think this will ever be used, but its here if need be.
bool lzr_memory_unmap(void* addr, size_t bytes);

// ensure that virtual address range is mapped to physical memory
bool lzr_memory_commit(void* addr, size_t bytes);

// discard the mapping of physical memory to the virtual address range
bool lzr_memory_decommit(void* addr, size_t bytes);

#endif // LZR_MEMORY_H