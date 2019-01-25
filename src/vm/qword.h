#ifndef _LZR_QWORD_H
#define _LZR_QWORD_H

#include "atomic.h"
#include "memory.h"

#define QWORD_CHUNK_SIZE (sizeof(size_t) * 8)
#define QWORD_BITMAPS ((LZR_PAGE_SIZE / sizeof(size_t)) - 2)

typedef struct QwordAllocator {
    ATOMIC(struct QwordAllocator*) next;
    size_t reserved;
    ATOMIC(size_t) chunk_bitmap[QWORD_BITMAPS];
} QwordAllocator;
_Static_assert(sizeof(QwordAllocator) == LZR_PAGE_SIZE, "Bad size QwordAllocator");

struct QwordChunk;
typedef struct {
    ATOMIC(struct QwordChunk*) next;
    ATOMIC(size_t) bitmap;
    size_t chunk_offset;
    size_t reserved;
} Qword;

typedef struct QwordChunk {
    Qword header;
    Qword qwords[QWORD_CHUNK_SIZE - 1];
} QwordChunk;

QwordAllocator* CreateQwordAllocator();

QwordChunk* AllocQwordChunk(QwordAllocator* allocator);

Qword* AllocQword(QwordChunk* chunk);

void FreeQword(Qword* qword);

#endif // _LZR_QWORD_H