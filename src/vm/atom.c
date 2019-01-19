#include "atom.h"
#include "memory.h"
#include <string.h>

#define PAGE_SIZE 4096
#define ATOM_MEMORY_PAGE_SIZE (2 * 1024 * 1024)
#define GetAtomDisplacement(atom) ((atom)->info >> 8)
#define BumpAtomDisplacement(atom) ((atom)->info |= (GetAtomDisplacement(atom) + 1) << 8)

bool TableGrow(AtomTable* table);
void TableInsert(AtomTable* table, Atom* atom);
Atom* AllocAtom(AtomTable* table, Hash hash, const uint8_t* bytes, size_t len);
Atom* TableFind(AtomTable* table, Hash hash, const uint8_t* bytes, size_t len);
bool AtomCompareEq(const Atom* atom, Hash hash, const uint8_t* bytes, size_t len);

bool AtomTableInit(AtomTable* table, size_t max_atoms) {
    table->capacity = LZR_NEXTPOW2(max_atoms);
    table->mask = LZR_MIN(table->capacity, 1024) - 1;
    table->memory = MemoryMap(ATOM_MEMORY_PAGE_SIZE, false);
    table->atoms = MemoryMap(table->capacity * 2 * sizeof(Atom*), false);

    table->size = 0;
    table->mem_top = 0;
    table->mem_committed = 0;
    table->mapping = table->atoms;

    if ((table->memory == NULL) || (table->atoms == NULL))
        return false;

    MemoryCommit(table->atoms, (table->mask + 1) * sizeof(Atom*));
    return true;
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
        Atom** atom = &table->atoms[index];
        if (*atom == NULL || displacement > GetAtomDisplacement(*atom))
            return NULL;
        else if (AtomCompareEq(*atom, hash, bytes, len))
            return *atom;

        displacement++;
        index = (index + 1) & table->mask;
    }
}

void TableInsert(AtomTable* table, Atom* current_atom) {
    current_atom->info &= 0xff;
    size_t index = GetAtomHash(current_atom) & table->mask;

    while (true) {
        Atom** atom = &table->atoms[index];
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
    Atom** old_atoms = table->atoms;
    size_t old_capacity = table->mask;
    size_t new_capacity = old_capacity << 1;

    if (new_capacity > table->capacity)
        return false;
    
    table->size = 0;
    table->mask = new_capacity - 1;
    table->atoms = &table->mapping[old_atoms == table->mapping ? table->capacity : 0];

    MemoryCommit(table->atoms, new_capacity * sizeof(Atom*));
    for (size_t atom_index = 0; atom_index < old_capacity; atom_index++)
        if (old_atoms[atom_index] != NULL)
            TableInsert(table, old_atoms[atom_index]);
    MemoryDecommit(old_atoms, old_capacity * sizeof(Atom*));
}

Atom* AllocAtom(AtomTable* table, Hash hash, const uint8_t* bytes, size_t len) {
    const size_t atom_size = sizeof(Atom) + len;

    if (table->mem_top + atom_size > ATOM_MEMORY_PAGE_SIZE) {
        if ((table->memory = MemoryMap(ATOM_MEMORY_PAGE_SIZE, false)) == NULL)
            return NULL;
        table->mem_top = 0;
        table->mem_committed = 0;        
    }

    table->mem_top += atom_size;
    if (table->mem_committed < table->mem_top) {
        size_t commit_size = LZR_ALIGN(table->mem_top - table->mem_committed, PAGE_SIZE);
        MemoryCommit(table->memory + table->mem_committed, commit_size);
        table->mem_committed += commit_size;
    }

    Atom* atom = (Atom*) &table->memory[table->mem_top - atom_size];
    memcpy(GetAtomText(atom), bytes, len);
    atom->hash = hash;
    atom->info = len;
    return atom;
}