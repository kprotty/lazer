#ifndef LZR_ATOM_H
#define LZR_ATOM_H

#include "hash.h"

typedef struct {
    uint32_t hash;
    uint32_t data;
    uint32_t actor;
} lzr_atom_t;

#define MAX_ATOM_TEXT 255
#define MAX_ATOM_SIZE (sizeof(lzr_atom_t) + sizeof(uint8_t) + MAX_ATOM_TEXT)

#define lzr_atom_len(atom) (*lzr_atom_len_ptr(atom))
#define lzr_atom_len_ptr(atom) ((uint8_t*) ((atom) + 1))
#define lzr_atom_text_ptr(atom) (((char*) ((atom) + 1)) + 1)

typedef struct lzr_atom_table_t lzr_atom_table_t;

void lzr_atom_table_init(lzr_atom_table_t* self, size_t max_atoms);

lzr_atom_t* lzr_atom_table_find(lzr_atom_table_t* self, const char* key, size_t key_len);

lzr_atom_t* lzr_atom_table_upsert(lzr_atom_table_t* self, const char* key, size_t key_len);

#endif // LZR_ATOM_H