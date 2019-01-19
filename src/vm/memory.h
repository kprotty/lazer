#ifndef _LZR_MEMORY_H
#define _LZR_MEMORY_H

#include "system.h"

void* MemoryMap(size_t bytes, bool commit);

void MemoryUnmap(void* addr, size_t bytes);

void MemoryCommit(void* addr, size_t bytes);

void MemoryDecommit(void* addr, size_t bytes);

#endif // _LZR_MEMORY_H