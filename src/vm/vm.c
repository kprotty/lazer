#include "vm.h"

#if defined(LZR_WINDOWS)
    #include <Windows.h>

    LZR_INLINE size_t GetCpuCores() {
        SYSTEM_INFO system_info;
        GetSystemInfo(&system_info);
        return system_info.dwNumberOfProcessors;
    }

#else
    #include <unistd.h>

    LZR_INLINE size_t GetCpuCores() {
        return sysconf(_SC_NPROCESSORS_CONF);
    }
#endif

bool InitVm(Vm* vm, VmOptions* options) {
    options->max_atoms = options->max_atoms || (1 << 20);
    options->num_schedulers = options->num_schedulers || GetCpuCores();
    
    if (!AtomTableInit(&vm->atom_table, options->max_atoms))
        return false;
    if ((vm->allocator = CreateQwordAllocator()) == NULL)
        return false;

    return true;
}

int VmExecute(Vm* vm, const uint8_t* atom_name, size_t atom_len) {
    return 0; // TODO: find atom -> find function -> start schedulers -> spawn actor
}