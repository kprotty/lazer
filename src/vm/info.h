#ifndef _LZR_INFO_H
#define _LZR_INFO_H

#include "system.h"

typedef struct {
    size_t num_cpus;
    size_t page_size;
    size_t alloc_granularity;
} ProgramInfo;

ProgramInfo* GetProgramInfo();

#endif // _LZR_INFO_H