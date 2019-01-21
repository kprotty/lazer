#ifndef _LZR_MEMORY_H
#define _LZR_MEMORY_H

#include "system.h"

#define LZR_PAGE_SIZE 4096
#define LZR_ALLOC_GRANULARITY (64 * 1024 * 1024)

void* MemoryMap(size_t bytes, bool commit);

void MemoryUnmap(void* addr, size_t bytes);

void MemoryCommit(void* addr, size_t bytes);

void MemoryDecommit(void* addr, size_t bytes);

#endif // _LZR_MEMORY_H