#include "atom.h"
#include <string.h>
#include "memory/heap.h"

#define MAX_ATOM_TE

typedef struct {
    uint32_t displacement;
    uint32_t atom_ptr;
} atom_cell_t;

typedef struct {
    lzr_atom_t* heap;
    size_t commit_at;
} atom_heap_t;

struct lzr_atom_table_t {
    size_t size;
    size_t mask;
    size_t capacity;
    atom_cell_t* cells;
    atom_cell_t* remap;
    atom_heap_t atom_heap;
};

#define ATOM_CELL_LOAD_FACTOR 90
#define ATOM_CELL_COMMIT_DEFAULT 1024
#define ATOM_HEAP_COMMIT_SIZE (1 * 1024 * 1024)

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define NEXT_POW_2(value) (1ULL << (64 - __builtin_clzl((value) - 1)))
#define ALIGN(value, alignment) ((value) + ((alignment) - (((value) % (alignment)))))

void atom_heap_init(atom_heap_t* self, size_t max_atoms) {
    const size_t heap_size = ALIGN(MAX_ATOM_SIZE * max_atoms, LZR_HEAP_CHUNK_SIZE);
    self->heap = (lzr_atom_t*) lzr_heap_reserve(heap_size / LZR_HEAP_CHUNK_SIZE);

    self->commit_at = ((size_t) self->heap) + ATOM_HEAP_COMMIT_SIZE;
    lzr_memory_commit((void*) self->heap, ATOM_HEAP_COMMIT_SIZE);
}

lzr_atom_t* atom_heap_alloc(atom_heap_t* self, uint32_t hash, const char* text, size_t len) {
    const size_t atom_size = ALIGN(sizeof(lzr_atom_t) + 1 + len, 8);
    if (((size_t) self->heap) + atom_size > self->commit_at) {
        lzr_memory_commit((void*) self->commit_at, ATOM_HEAP_COMMIT_SIZE);
        self->commit_at += ATOM_HEAP_COMMIT_SIZE;
    }

    lzr_atom_t* atom = (lzr_atom_t*) self->heap;
    atom->data = 0;
    atom->actor = 0;
    atom->hash = hash;
    *lzr_atom_len_ptr(atom) = (len & 0xff);
    memcpy(lzr_atom_text_ptr(atom), text, lzr_atom_len(atom));

    self->heap = (lzr_atom_t*) (((size_t) self->heap) + atom_size);
    return atom;
}

bool table_compare_eq(uint32_t atom_ptr, uint32_t hash, const char* text, size_t len) {
    lzr_atom_t* atom = (lzr_atom_t*) LZR_PTR_UNZIP(atom_ptr);

    return hash == atom-> hash &&
           len == lzr_atom_len(atom) &&
           memcmp(lzr_atom_text_ptr(atom), text, len) == 0;
}

atom_cell_t* table_find(lzr_atom_table_t* self, uint32_t hash, const char* text, size_t len) {
    uint32_t displacement = 0;
    size_t index = hash & self->mask;

    while (true) {
        atom_cell_t* cell = &self->cells[index];
        if (cell->atom_ptr == 0 || displacement > cell->displacement)
            return NULL;
        if (table_compare_eq(cell->atom_ptr, hash, text, len))
            return cell;

        displacement++;
        index = (index + 1) & self->mask;
    }
}

atom_cell_t* table_insert(lzr_atom_table_t* self, uint32_t atom_ptr) {
    size_t index = ((lzr_atom_t*) LZR_PTR_UNZIP(atom_ptr))->hash & self->mask;
    atom_cell_t *inserted_cell = NULL;
    atom_cell_t current_cell;

    current_cell.displacement = 0;
    current_cell.atom_ptr = atom_ptr;

    while (true) {
        atom_cell_t* cell = &self->cells[index];
        if (cell->atom_ptr == 0) {
            self->size++;
            *cell = current_cell;
            return inserted_cell != NULL ? inserted_cell : cell;

        } else if (cell->displacement < current_cell.displacement) {
            if (current_cell.atom_ptr == atom_ptr)
                inserted_cell = cell;
            atom_cell_t temp = current_cell;
            current_cell = *cell;
            *cell = temp;
        }

        current_cell.displacement++;
        index = (index + 1) & self->mask;
    }
}

void table_grow(lzr_atom_table_t* self) {
    atom_cell_t* old_cells = self->cells;
    size_t old_capacity = self->mask + 1;
    size_t new_capacity = old_capacity << 1;
    assert(new_capacity <= self->capacity);

    self->size = 0;
    self->cells = self->remap;
    self->mask = new_capacity - 1;
    lzr_memory_commit((void*) self->cells, new_capacity * sizeof(atom_cell_t));

    for (size_t cell_index = 0; cell_index < old_capacity; cell_index++) {
        uint32_t atom_ptr = old_cells[cell_index].atom_ptr;
        if (atom_ptr != 0)
            table_insert(self, atom_ptr);
    }
    
    lzr_memory_decommit((void*) self->remap, old_capacity * sizeof(atom_cell_t));
    self->remap = old_cells;
}

void lzr_atom_table_init(lzr_atom_table_t* self, size_t max_atoms) {
    // allocate the cell tables for both the main cells and the remap cells
    // the remap are used as a copying-gc like space when resizing
    const size_t cell_size = NEXT_POW_2(MAX(max_atoms, ATOM_CELL_COMMIT_DEFAULT));
    const size_t cell_heap_size = ALIGN(cell_size, LZR_HEAP_CHUNK_SIZE) / LZR_HEAP_CHUNK_SIZE;
    self->cells = (atom_cell_t*) lzr_heap_reserve(cell_heap_size);
    self->remap = (atom_cell_t*) lzr_heap_reserve(cell_heap_size);

    const size_t cells_to_commit = MIN(cell_size, ATOM_CELL_COMMIT_DEFAULT * sizeof(atom_cell_t));
    lzr_memory_commit((void*) self->cells, cells_to_commit);
    
    atom_heap_init(&self->atom_heap, max_atoms);
    self->mask = cells_to_commit - 1;
    self->capacity = cell_size;
    self->size = 0;
}

lzr_atom_t* lzr_atom_table_find(lzr_atom_table_t* self, const char* key, size_t key_len) {
    // TODO: Add read-lock here

    uint32_t hash = lzr_hash_bytes(key, key_len);
    atom_cell_t* cell = table_find(self, hash, key, key_len);
    return (lzr_atom_t*) (cell != NULL ? LZR_PTR_UNZIP(cell->atom_ptr) : NULL);
}

lzr_atom_t* lzr_atom_table_upsert(lzr_atom_table_t* self, const char* key, size_t key_len) {
    // TODO: Add write-lock here

    uint32_t hash = lzr_hash_bytes(key, key_len);
    atom_cell_t* cell = table_find(self, hash, key, key_len);

    if (cell == NULL) {
        if (++self->size >= (ATOM_CELL_LOAD_FACTOR * self->capacity) / 100)
            table_grow(self);
        lzr_atom_t* atom = atom_heap_alloc(&self->atom_heap, hash, key, key_len);
        cell = table_insert(self, LZR_PTR_ZIP(atom));
    }

    return (lzr_atom_t*) LZR_PTR_UNZIP(cell->atom_ptr);
}