#include "heap.h"
#include "../atomic.h"

#define BUDDY_FREE      0
#define BUDDY_EXTENSION 1
#define BUDDY_ALLOCATED 2
#define BUDDY_TAG(tag) ((tag) << 14)
#define BUDDY_GET_TAG(buddy) ((buddy) >> 14)
#define BUDDY_GET_OFFSET(buddy) ((buddy) & 0xC000)

#define LZR_HEAP_SIZE (LZR_HEAP_END - LZR_HEAP_START)
#define HEAP_PTR(offset) ((void*) (LZR_HEAP_START + (offset)))

typedef struct {
    uint16_t prev;
    uint16_t next;
    uint16_t size;
    uint16_t buddy;
} chunk_info_t;

typedef struct {
    uint32_t heap_top;
    uint16_t top_chunk;
    lzr_atomic_lock_t lock;
    struct {
        chunk_info_t* head;
        chunk_info_t* tail;
    } free_list;
    chunk_info_t chunks[LZR_HEAP_SIZE / LZR_HEAP_CHUNK_SIZE];
} heap_t;

void lzr_heap_init() {
    // map the entire heap (32 GB) into the process (uncommitted)
    heap_t* heap = lzr_memory_map(HEAP_PTR(0), LZR_HEAP_SIZE, false);
    assert(heap != NULL);

    // commit the central heap structure into the pagefile
    bool heap_committed = lzr_memory_commit(heap, sizeof(heap_t));
    assert(heap_committed == true);

    // initialize the heap structure
    heap->top_chunk = 0;
    heap->free_list.head = NULL;
    heap->free_list.tail = NULL;
    lzr_atomic_flag_init(&heap->lock);
    heap->heap_top = LZR_HEAP_CHUNK_SIZE;
}

void* lzr_heap_alloc(size_t num_chunks) {
    void* address;
    heap_t* heap = HEAP_PTR(0);
    
    // lock the heap
    while (!lzr_atomic_lock(&heap->lock))
        lzr_cpu_yield();

    // best-fit scan for a free heap chunk
    chunk_info_t* free_chunk = NULL;
    for (chunk_info_t* chunk = heap->free_list.head; chunk != NULL; chunk = &heap->chunks[chunk->next])
        if (chunk->size >= num_chunks)
            if (free_chunk == NULL || free_chunk->size > chunk->size)
                free_chunk = chunk;

    // found a free heap chunk
    if (free_chunk != NULL) {
        uint16_t next_chunk = free_chunk->next;
        uint16_t left_over = free_chunk->size - num_chunks;
        uint16_t chunk_offset = (free_chunk - (chunk_info_t*) &heap->chunks);
        
        // it was bigger than we needed, add the rest back 
        if (left_over > 0) {
            next_chunk = chunk_offset + num_chunks;
            heap->chunks[next_chunk] = *free_chunk;
            heap->chunks[next_chunk].size = left_over;
        }

        // fix the doubly-linked-list chains
        chunk_info_t* next_heap_chunk = next_chunk == 0 ? NULL : &heap->chunks[next_chunk];
        if (heap->free_list.tail == free_chunk)
            heap->free_list.tail = next_heap_chunk;
        if (heap->free_list.head == free_chunk)
            heap->free_list.head = next_heap_chunk;
        if (free_chunk->prev != 0)
            heap->chunks[free_chunk->prev].next = next_chunk;

        // mark the chunk as taken & compute the address
        free_chunk->buddy |= BUDDY_TAG(BUDDY_ALLOCATED);
        address = HEAP_PTR(chunk_offset * LZR_HEAP_CHUNK_SIZE);

    // no free chunk was found, grab one via bump-allocation from the heap top
    } else {
        uint32_t offset = heap->heap_top;
        heap->heap_top += num_chunks * LZR_HEAP_CHUNK_SIZE;
        assert(heap->heap_top > offset); // check for overflow
        address = HEAP_PTR(offset);

        // set as the new top chunk
        uint16_t old_heap_chunk = heap->top_chunk;
        heap->top_chunk = offset / LZR_HEAP_CHUNK_SIZE;

        // register the newly allocated chunk
        chunk_info_t* new_chunk = &heap->chunks[heap->top_chunk];
        new_chunk->buddy = old_heap_chunk | BUDDY_TAG(BUDDY_ALLOCATED);
        new_chunk->size = num_chunks;
    }

    lzr_atomic_unlock(&heap->lock);
    return address;
}

void lzr_heap_free(void* ptr) {
    // ensure that the pointer is from the heap and aligned to the CHUNK_SIZE boundary
    assert(((size_t) ptr > LZR_HEAP_START) && ((size_t) ptr < LZR_HEAP_END));
    assert(((size_t) ptr & (LZR_HEAP_CHUNK_SIZE - 1)) == 0);
    heap_t* heap = HEAP_PTR(0);

    // get the heap chunk info from the ptr
    uint16_t chunk_offset = (((size_t) ptr) - LZR_HEAP_START) / LZR_HEAP_CHUNK_SIZE;
    chunk_info_t* chunk = &heap->chunks[chunk_offset];

    // lock the heap
    while (!lzr_atomic_lock(&heap->lock))
        lzr_cpu_yield();

    // try and simply coalesce two free buddy chunks together
    uint16_t buddy_offset = BUDDY_GET_OFFSET(chunk->buddy);
    if (buddy_offset != 0 && BUDDY_GET_TAG(chunk->buddy) == BUDDY_FREE) {
        heap->chunks[buddy_offset].size += chunk->size;
        chunk->buddy |= BUDDY_TAG(BUDDY_EXTENSION);
        chunk->prev = buddy_offset;

    // cant coalesce. add the chunk to the free_list
    } else {
        chunk_info_t** head = &heap->free_list.head;
        chunk_info_t** tail = &heap->free_list.tail;
        
        // mark the chunk as free & setup its linked-list pointers
        chunk->next = 0;
        chunk->buddy |= BUDDY_TAG(BUDDY_FREE);
        chunk->prev = *tail == NULL ? 0 : (*tail - (chunk_info_t*) &heap->chunks);
        
        // add it to the appropriate links
        if (*tail == NULL) {
            *head = *tail = chunk;
        } else {
            *tail = &heap->chunks[(*tail)->next = chunk_offset];
        }
    }

    lzr_atomic_unlock(&heap->lock);
}