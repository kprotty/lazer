#ifndef _LZR_ATOM_H
#define _LZR_ATOM_H

#include "hash.h"

typedef struct {
    uint32_t info;
    Hash hash;
    size_t data;
} Atom;

#define MAX_ATOM_SIZE 0xff

#define GetAtomText(atom) ((atom) + 1)
#define GetAtomHash(atom) ((atom)->hash)
#define GetAtomData(atom) ((atom)->data)
#define GetAtomSize(atom) ((atom)->info & 0xff)

typedef struct {
    // hash map info
    size_t size;
    size_t mask;
    size_t max_atoms;
    Atom** atom_cells;
    Atom** resize_cells;

    // atom heap info
    size_t heap_top;
    size_t heap_committed;
    uint8_t* atom_heap;
} AtomTable;

bool AtomTableInit(AtomTable* table, size_t max_atoms);

Atom* AtomTableFind(AtomTable* table, const uint8_t* bytes, size_t len);

Atom* AtomTableUpsert(AtomTable* table, const uint8_t* bytes, size_t len);

#endif // _LZR_ATOM_H