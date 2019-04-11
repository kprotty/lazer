#include "heap.h"
#include "lock.h"

#define HEAP_SIZE (LZR_HEAP_END - LZR_HEAP_BEGIN)
#define HEAP_PTR(offset) (LZR_HEAP_BEGIN + ((offset) * LZR_HEAP_CHUNK_SIZE))

typedef struct {
    uint16_t head;
    uint16_t tail;
} freelist_t;

typedef struct {
    uint16_t prev;
    uint16_t next;
    uint16_t size;
    bool allocated: 1;
    uint16_t predecessor: 14;
} chunk_info_t;

typedef struct {
    lzr_spinlock_t lock;
    uint16_t top_heap;
    uint16_t top_chunk;
    freelist_t free_list;
    chunk_info_t chunks[HEAP_SIZE / LZR_HEAP_CHUNK_SIZE];
} heap_t;

static uint32_t heap_offset = 0;
static bool heap_committed = false;
static bool heap_initialized = false;

// memory map the entire 32gb heap into the address space (uncommitted)
void lzr_heap_init() {
    assert(heap_initialized == false);
    lzr_memory_map((void*) HEAP_PTR(0), HEAP_SIZE, false);
    heap_initialized = true;
}

// reserve a specific amount of the heap for static data.
// needs to be done befoer lzr_heap_commit()
void* lzr_heap_reserve(uint16_t num_chunks) {
    assert(heap_committed == false);
    void* address = (void*) HEAP_PTR(heap_offset);
    heap_offset += num_chunks * LZR_HEAP_CHUNK_SIZE;
    return address;
}

// locks down the heap for future lzr_heap_reserve's and initializes it.
// any further allocates need to be done through lzr_heap_alloc/lzr_heap_free
void lzr_heap_commit() {
    heap_t* heap = (heap_t*) HEAP_PTR(heap_offset);
    
    assert(heap_committed == false);
    lzr_memory_commit((void*) heap, sizeof(heap_t));
    heap_committed = true;

    heap->top_heap = 1;
    heap->top_chunk = 0;
    heap->free_list.head = 0;
    heap->free_list.tail = 0;
    lzr_spinlock_init(&heap->lock);

    heap->chunks[0].prev = 0;
    heap->chunks[0].next = 0;
    heap->chunks[0].size = 0;
    heap->chunks[0].allocated = true;
    heap->chunks[0].predecessor = 0;
}

void freelist_append(heap_t* heap, uint16_t chunk) {
    heap->chunks[chunk].next = 0;

    if (heap->free_list.tail == 0) {
        heap->chunks[chunk].prev = 0;
        heap->free_list.tail = heap->free_list.head = chunk;
    } else {
        heap->chunks[heap->free_list.tail].next = chunk;
        heap->chunks[chunk].prev = heap->free_list.tail;
        heap->free_list.tail = chunk;
    }
}

void freelist_remove(heap_t* heap, uint16_t chunk) {
    chunk_info_t* chunk_info = &heap->chunks[chunk];

    if (heap->free_list.head == chunk)
        heap->free_list.head = chunk_info->next;
    if (heap->free_list.tail == chunk)
        heap->free_list.tail = chunk_info->next;
    if (chunk_info->prev != 0)
        heap->chunks[chunk_info->prev].next = chunk_info->next;
    if (chunk_info->next != 0)
        heap->chunks[chunk_info->next].prev = chunk_info->prev;
}

void* lzr_heap_alloc(uint16_t num_chunks) {
    heap_t* heap = (heap_t*) HEAP_PTR(heap_offset);
    lzr_spinlock_lock(&heap->lock);
    void* address;

    // using Best-Fit search, try and find a free chunk in the free list
    uint16_t free_chunk = 0;
    for (uint16_t chunk = heap->free_list.head; chunk != 0; chunk = heap->chunks[chunk].next)
        if (heap->chunks[chunk].size >= num_chunks)
            if (free_chunk == 0 || heap->chunks[free_chunk].size > heap->chunks[chunk].size)
                free_chunk = chunk;

    // a free chunk was found, use it
    if (free_chunk != 0) {
        chunk_info_t* free_chunk_info = &heap->chunks[free_chunk];
        free_chunk_info->allocated = true;
        free_chunk_info->next = 0;

        // remove it from the free list and add back the remaining if it was bigget than needed
        freelist_remove(heap, free_chunk);
        if (free_chunk_info->size > num_chunks) {
            uint16_t left_over = free_chunk + num_chunks;
            heap->chunks[left_over].size = free_chunk_info->size - num_chunks;
            heap->chunks[left_over].predecessor = free_chunk;
            heap->chunks[left_over].allocated = false;
            freelist_append(heap, left_over);
        }

        free_chunk_info->size = num_chunks;
        address = (void*) HEAP_PTR(heap_offset + free_chunk);

    // no free chunk was found, bump-allocate from the top of the heap
    } else {
        uint16_t top = heap->top_heap;
        heap->top_heap += num_chunks;
        assert(heap->top_heap > top);

        // register a newly made chunk
        heap->chunks[top].predecessor = heap->top_chunk;
        heap->chunks[top].size = num_chunks;
        heap->chunks[top].allocated = true;

        heap->top_chunk = top;
        address = (void*) HEAP_PTR(heap_offset + top);
    }

    lzr_spinlock_unlock(&heap->lock);
    return address;
}

void lzr_heap_free(void* ptr) {
    // assert that the ptr is in the heap & aligned to LZR_HEAP_CHUNK_SIZE (its a valid chunk)
    assert((size_t) ptr > LZR_HEAP_BEGIN && (size_t) ptr < LZR_HEAP_END);
    assert(((size_t) ptr % LZR_HEAP_CHUNK_SIZE) == 0);
    heap_t* heap = (heap_t*) HEAP_PTR(heap_offset);

    // get the chunk info and make sure that it was allocated before freeing
    uint16_t ptr_chunk = ((size_t) ptr - heap_offset - LZR_HEAP_BEGIN) / LZR_HEAP_CHUNK_SIZE;
    chunk_info_t* ptr_chunk_info = &heap->chunks[ptr_chunk];
    
    lzr_spinlock_lock(&heap->lock);
    assert(ptr_chunk_info->allocated == true);
    ptr_chunk_info->allocated = false;

    // coalesce free chunks from left to right (removing them from the free list)    
    if (ptr_chunk != heap->top_chunk) {
        uint16_t chunk = ptr_chunk + ptr_chunk_info->size;
        while (!heap->chunks[chunk].allocated) {
            freelist_remove(heap, chunk);
            ptr_chunk_info->size += heap->chunks[chunk].size;
            chunk += heap->chunks[chunk].size;
            if (chunk >= heap->top_heap)
                break;
        }
    }

    // coalesce free chunks right to left following the chain of predecessors
    while (ptr_chunk_info->predecessor != 0) {
        chunk_info_t* predecessor = &heap->chunks[ptr_chunk_info->predecessor];
        if (predecessor->allocated)
            break;
        freelist_remove(heap, ptr_chunk_info->predecessor);
        predecessor->size += ptr_chunk_info->size;
        ptr_chunk = ptr_chunk_info->predecessor;
        ptr_chunk_info = predecessor;
    }

    // the freed chunk is on the top of the heap:
    // move the heap top back down to the freed chunk and discard it
    if (ptr_chunk + ptr_chunk_info->size >= heap->top_heap) {
        heap->top_chunk = ptr_chunk_info->predecessor;
        heap->top_heap = ptr_chunk;

    // the freed chunk is somewhere else in the heap:
    // add it to the free list ("add it to the list Helvian")
    } else {
        freelist_append(heap, ptr_chunk);
    }

    lzr_spinlock_unlock(&heap->lock);
}