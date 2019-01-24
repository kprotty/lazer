#ifndef _LZR_VM_H
#define _LZR_VM_H

#include "atom.h"
#include "qword.h"

typedef struct {
    AtomTable atom_table;
    QwordAllocator* allocator;
} Vm;

typedef struct {
    size_t max_atoms;
    size_t num_schedulers;
} VmOptions;

bool InitVm(Vm* vm, VmOptions* options);

int VmExecute(Vm* vm, const uint8_t* atom_name, size_t atom_len);

#endif // _LZR_VM_H