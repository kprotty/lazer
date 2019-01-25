#include "qword.h"
#include <string.h>

#define PTR_LOCKED 0x1
#define PTR_UNLOCKED 0x0
#define LZR_FIRST_SET_BIT_64(x) __builtin_ffsll(x)

LZR_INLINE QwordChunk* GetQwordChunkFrom(Qword* qword) {
    return (QwordChunk*) ((char*) qword 
        - ((uintptr_t) qword % LZR_PAGE_SIZE));
}

LZR_INLINE QwordAllocator* GetQwordAllocatorFrom(QwordChunk* chunk) {
    return (QwordAllocator*) (
        ((char*) (chunk - chunk->header.chunk_offset)) 
        - sizeof(QwordAllocator));
}

QwordAllocator* CreateQwordAllocator() {
    const size_t chunks_size = QWORD_BITMAPS * QWORD_CHUNK_SIZE * sizeof(QwordChunk);
    const size_t map_size = sizeof(QwordAllocator) + chunks_size;

    QwordAllocator* allocator = MemoryMap(map_size, false);
    LZR_ASSERT(allocator != NULL);

    MemoryCommit(allocator, sizeof(QwordAllocator));
    memset(allocator, 0, sizeof(QwordAllocator));
    return allocator;
}

QwordChunk* AllocQwordChunkFrom(QwordAllocator* allocator, size_t* offset) {
    ATOMIC(size_t) *bitmaps = allocator->chunk_bitmap;
    QwordChunk* chunks = (QwordChunk*) (allocator + 1);
    
    // check all the bitmaps in the allocator for an open chunk slot
    for (; *offset < QWORD_BITMAPS; (*offset)++) {
        size_t bitmap = AtomicLoad(&bitmaps[*offset], ATOMIC_RELAXED);

        // check if the current bitmap has an open chunk slot
        while ((uintptr_t) bitmap != UINTPTR_MAX) {
            int bitpos = LZR_FIRST_SET_BIT_64(~bitmap) - 1;
            const size_t updated = bitmap | (1ULL << bitpos);

            // atomically try and grab the chunk slot
            if (AtomicCas(&bitmaps[*offset], &bitmap, updated, ATOMIC_ACQUIRE, ATOMIC_RELAXED)) {
                *offset = (*offset) * QWORD_CHUNK_SIZE + bitpos;
                return chunks + (*offset);
            }

            // if failed, that means some other thread did so retry a new slot
            bitmap = AtomicLoad(&bitmaps[*offset], ATOMIC_RELAXED);
        }
    }

    // there were no open chunk slots avaialble
    return NULL;
}

QwordChunk* AllocQwordChunk(QwordAllocator* allocator) {
    size_t offset = 0;
    QwordChunk* chunk;
    QwordAllocator* locked = (QwordAllocator*) PTR_LOCKED;
    QwordAllocator* unlocked = (QwordAllocator*) PTR_UNLOCKED;

    // look for a chunk in the current allocator
    while ((chunk = AllocQwordChunkFrom(allocator, &offset)) == NULL) {
        QwordAllocator* new_allocator;

        // if none available, create a new allocator and link it to this one
        if (AtomicCas(&allocator->next, &unlocked, locked, ATOMIC_ACQUIRE, ATOMIC_RELAXED)) {
            new_allocator = CreateQwordAllocator();
            AtomicStore(&allocator->next, new_allocator, ATOMIC_RELEASE);
        }

        // wait for the new allocator be created and linked
        while ((new_allocator = (QwordAllocator*) 
            AtomicLoad(&allocator->next, ATOMIC_RELAXED)) == locked)
            CpuRelax();

        // try and look for a chunk in the new allocator
        allocator = new_allocator;
    }

    // VirtualAlloc commits in pages so only commit chunks on page boundaries
    if (((uintptr_t) chunk % LZR_PAGE_SIZE) == 0)
        MemoryCommit(chunk, LZR_PAGE_SIZE);

    AtomicStore(&chunk->header.next, NULL, ATOMIC_RELAXED);
    chunk->header.chunk_offset = offset;
    chunk->header.bitmap = 1;
    return chunk;
}

Qword* AllocQword(QwordChunk* chunk) {
    QwordChunk* new_chunk;
    QwordChunk* locked = (QwordChunk*) PTR_LOCKED;
    QwordChunk* unlocked = (QwordChunk*) PTR_UNLOCKED;
    
    while (true) {
        // check the bitmap for an open qword slot
        size_t bitmap = AtomicLoad(&chunk->header.bitmap, ATOMIC_RELAXED);
        while ((uintptr_t) bitmap != UINTPTR_MAX) {
            int bitpos = LZR_FIRST_SET_BIT_64(~bitmap) - 1;
            const size_t updated = bitmap | (1ULL << bitpos);

            // try and grab a qword slot
            if (AtomicCas(&chunk->header.bitmap, &bitmap, updated,
                ATOMIC_ACQUIRE, ATOMIC_RELAXED))
                return &chunk->qwords[bitpos - 1];

            // some other thread got it, retry
            bitmap = AtomicLoad(&chunk->header.bitmap, ATOMIC_RELAXED);
        }

        // failed to find one in the current chunk so try the next chunk
        // if the next chunk is null, allocate a new chunk and use that
        if ((new_chunk = (QwordChunk*) AtomicLoad(&chunk->header.next, ATOMIC_RELAXED)) == NULL) {
            if (AtomicCas(&chunk->header.next, &unlocked, locked, ATOMIC_ACQUIRE, ATOMIC_RELAXED)) {
                new_chunk = AllocQwordChunk(GetQwordAllocatorFrom(chunk));
                AtomicStore(&chunk->header.next, new_chunk, ATOMIC_RELEASE);
            }

            while ((new_chunk = (QwordChunk*)
                AtomicLoad(&chunk->header.next, ATOMIC_RELAXED)) == locked)
                CpuYield();
        }

        // try the next chunk
        chunk = new_chunk;
    }
}

void FreeQword(Qword* qword) {
    size_t bitmap, bitmask;

    // get the owning chunk & bitposition of the qword
    QwordChunk* chunk = GetQwordChunkFrom(qword);
    int bitpos = ((size_t) qword - (size_t) chunk) / sizeof(Qword);
    bitmask = ~(1ULL << bitpos);
    
    // atomically set the bitmap bit as free
    do {
        bitmap = AtomicLoad(&chunk->header.bitmap, ATOMIC_RELAXED);
    } while (!AtomicCas(&chunk->header.bitmap, &bitmap, bitmap & bitmask,
        ATOMIC_RELEASE, ATOMIC_RELAXED));
}
