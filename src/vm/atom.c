#include "atom.h"
#include "info.h"
#include "memory.h"
#include <string.h>

#define GetAtomDisplacement(atom) ((atom)->info >> 8)
#define BumpAtomDisplacement(atom) ((atom)->info = \
    GetAtomSize(atom) | ((GetAtomDisplacement(atom) + 1) << 8))

bool TableGrow(AtomTable* table);
void TableInsert(AtomTable* table, Atom* atom);
Atom* TableFind(AtomTable* table, Hash hash, const uint8_t* bytes, size_t len);

bool AtomCompareEq(const Atom* atom, Hash hash, const uint8_t* bytes, size_t len);
Atom* AllocAtom(AtomTable* table, Hash hash, const uint8_t* bytes, size_t len);

bool AtomTableInit(AtomTable* table, size_t max_atoms) {
    ProgramInfo* info = GetProgramInfo();
    max_atoms = LZR_MAX(max_atoms, 8);
    max_atoms = LZR_NEXTPOW2(max_atoms);
    table->max_atoms = max_atoms;

    size_t alloc_size = max_atoms * 2 * sizeof(Atom*);
    alloc_size = LZR_ALIGN(alloc_size, info->alloc_granularity);

    // allocate atom heap
    table->heap_top = 0;
    table->heap_committed = 0;
    table->heap_size = LZR_ALIGN(info->alloc_granularity, (2 * 1024 * 1024));
    table->atom_heap = MemoryMap(table->heap_size, false);

    // allocate atom mapping memory
    table->size = 0;
    table->mask = LZR_MIN(max_atoms, 1024) - 1;
    table->atom_cells = MemoryMap(alloc_size, false);
    table->resize_cells = table->atom_cells + (alloc_size / 2 / sizeof(Atom*));
    MemoryCommit(table->atom_cells, (table->mask + 1) * sizeof(Atom*));

    return table->atom_heap != NULL && table->atom_cells != NULL;
}

Atom* AtomTableFind(AtomTable* table, const uint8_t* bytes, size_t len) {
    if (LZR_UNLIKELY(len > MAX_ATOM_SIZE))
        return NULL;
    return TableFind(table, HashBytes(bytes, len), bytes, len);
}

Atom* AtomTableUpsert(AtomTable* table, const uint8_t* bytes, size_t len) {
    if (LZR_UNLIKELY(len > MAX_ATOM_SIZE))
        return NULL;

    const Hash hash = HashBytes(bytes, len);
    Atom* atom = TableFind(table, hash, bytes, len);

    if (atom != NULL)
        return atom;
    if (table->size >= table->mask + 1 && !TableGrow(table))
        return NULL;
    if ((atom = AllocAtom(table, hash, bytes, len)) == NULL)
        return NULL;

    TableInsert(table, atom);
    return atom;
}

bool AtomCompareEq(const Atom* atom, Hash hash, const uint8_t* bytes, size_t len) {
    return GetAtomHash(atom) == hash &&
           GetAtomSize(atom) == len  &&
           !strncmp((const char*) GetAtomText(atom), (const char*) bytes, len);
}

Atom* TableFind(AtomTable* table, Hash hash, const uint8_t* bytes, size_t len) {
    size_t index = hash & table->mask;
    size_t displacement = 0;

    while (true) {
        Atom* atom = table->atom_cells[index];
        if (atom == NULL || displacement > GetAtomDisplacement(atom))
            return NULL;
        else if (AtomCompareEq(atom, hash, bytes, len))
            return atom;

        displacement++;
        index = (index + 1) & table->mask;
    }
}

void TableInsert(AtomTable* table, Atom* current_atom) {
    current_atom->info &= 0xff;
    size_t index = GetAtomHash(current_atom) & table->mask;

    while (true) {
        Atom** atom = &table->atom_cells[index];
        if (*atom == NULL) {
            table->size++;
            *atom = current_atom;
            return;
        } else if (GetAtomDisplacement(*atom) < GetAtomDisplacement(current_atom)) {
            Atom* temp = current_atom;
            current_atom = *atom;
            *atom = temp;
        }
        
        index = (index + 1) & table->mask;
        BumpAtomDisplacement(current_atom);
    }
}

bool TableGrow(AtomTable* table) {
    size_t old_capacity = table->mask + 1;
    size_t new_capacity = old_capacity << 1;
    Atom** old_atom_cells = table->atom_cells;

    if (new_capacity > table->max_atoms)
        return false;

    table->size = 0;
    table->mask = new_capacity - 1;
    table->atom_cells = table->resize_cells;
    table->resize_cells = old_atom_cells;

    MemoryCommit(table->atom_cells, new_capacity * sizeof(Atom*));
    for (size_t atom_index = 0; atom_index < old_capacity; atom_index++)
        if (old_atom_cells[atom_index] != NULL)
            TableInsert(table, old_atom_cells[atom_index]);

    MemoryDecommit(old_atom_cells, old_capacity * sizeof(Atom*));
    return true;
}

Atom* AllocAtom(AtomTable* table, Hash hash, const uint8_t* bytes, size_t len) {
    const size_t atom_size = sizeof(Atom) + len;
    ProgramInfo* program_info = GetProgramInfo();

    if (table->heap_top + atom_size > table->heap_size) {
        if ((table->atom_heap = MemoryMap(table->heap_size, false)) == NULL)
            return NULL;
        table->heap_top = 0;
        table->heap_committed = 0;        
    }

    table->heap_top += atom_size;
    if (table->heap_committed < table->heap_top) {
        size_t commit_size = table->heap_top - table->heap_committed;
        commit_size = LZR_ALIGN(commit_size, program_info->page_size);
        MemoryCommit(table->atom_heap + table->heap_committed, commit_size);
        table->heap_committed += commit_size;
    }

    Atom* atom = (Atom*) &table->atom_heap[table->heap_top - atom_size];
    memcpy(GetAtomText(atom), bytes, len);
    atom->hash = hash;
    atom->info = len;

    return atom;
}