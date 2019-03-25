#include "heap.h"

#define HEAP_SIZE (LZR_HEAP_END - LZR_HEAP_BEGIN)
#define HEAP_PTR(offset) (LZR_HEAP_BEGIN + ((offset) * LZR_HEAP_CHUNK_SIZE))

typedef struct {
    uint16_t head;
    uint16_t tail;
} freelist_t;

typedef struct {
    // the previous chunk in the free list
    uint16_t prev;
    // the next chunk in the free list
    uint16_t next;
    // the number of LZR_HEAP_CHUNK_SIZEs this chunk occupies
    uint16_t size;
    // if this chunk is allocated or not
    bool allocated: 1;
    // the left-side neighbooring chunk if any
    uint16_t predecessor: 14;
} chunk_info_t;

typedef struct {
    // pointer to the top LZR_HEAP_CHUNK in the heap (like a program break)
    uint16_t top_heap;
    // the allocated or free chunk right before the top_heap
    uint16_t top_chunk;
    // list of non-contiguous free chunks in the heap
    freelist_t free_list;
    // metadata descriptions for heap LZR_HEAP_CHUNK
    chunk_info_t chunks[HEAP_SIZE / LZR_HEAP_CHUNK_SIZE];
} heap_t;

void lzr_heap_init() {
    // map the entire 32gb heap in one go
    heap_t* heap = (heap_t*) lzr_memory_map((void*) HEAP_PTR(0), HEAP_SIZE, false);
    assert(heap != NULL);

    // commit into physical memory only the size of the heap structure needed for metadata
    bool committed = lzr_memory_commit((void*) heap, sizeof(heap_t));
    assert(committed == true);

    // the first LZR_HEAP_CHUNK (2mb) is used for the heap structure so start the top_heap at 1
    heap->top_heap = 1;
    heap->top_chunk = 0;
    heap->free_list.head = 0;
    heap->free_list.tail = 0;

    // the first chunk[0] is special as it represents the heap structure and therefor NULL
    heap->chunks[0].prev = 0;
    heap->chunks[0].next = 0;
    heap->chunks[0].size = 0;
    heap->chunks[0].allocated = true;
    heap->chunks[0].predecessor = 0;
}

// add a chunk to the end of the free list
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

// remove a chunk from the free list
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
    heap_t* heap = (heap_t*) HEAP_PTR(0);
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
        address = (void*) HEAP_PTR(free_chunk);

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
        address = (void*) HEAP_PTR(top);
    }

    return address;
}

void lzr_heap_free(void* ptr) {
    // assert that the ptr is in the heap & aligned to LZR_HEAP_CHUNK_SIZE (its a valid chunk)
    assert((size_t) ptr > LZR_HEAP_BEGIN && (size_t) ptr < LZR_HEAP_END);
    assert(((size_t) ptr % LZR_HEAP_CHUNK_SIZE) == 0);
    heap_t* heap = (heap_t*) HEAP_PTR(0);

    // get the chunk info and make sure that it was allocated before freeing
    uint16_t ptr_chunk = ((size_t) ptr - LZR_HEAP_BEGIN) / LZR_HEAP_CHUNK_SIZE;
    chunk_info_t* ptr_chunk_info = &heap->chunks[ptr_chunk];
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
}