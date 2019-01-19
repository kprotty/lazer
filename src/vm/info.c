#include "info.h"
#include "atomic.h"

void InitializeProgramInfo(ProgramInfo* info);

ProgramInfo* GetProgramInfo() {
    static ProgramInfo program_info;
    static ATOMIC(bool) created = false;
    static ATOMIC(bool) initialized = false;

    bool uninitialized = false;
    const int ordering = ATOMIC_RELAXED;
    if (AtomicCas(&initialized, &uninitialized, true, ordering, ordering)) {
        InitializeProgramInfo(&program_info);
        AtomicStore(&created, true, ordering);
    }

    while (!AtomicLoad(&created, ordering))
        CpuRelax();
    return &program_info;
}

#if defined(LZR_WINDOWS)
    #include <Windows.h>

    void InitializeProgramInfo(ProgramInfo* info) {
        SYSTEM_INFO sys_info;
        GetSystemInfo(&sys_info);

        info->page_size = sys_info.dwPageSize;
        info->num_cpus = sys_info.dwNumberOfProcessors;
        info->alloc_granularity = sys_info.dwAllocationGranularity;
    }

#else
    #include <unistd.h>

    void InitializeProgramInfo(ProgramInfo* info) {
        info->page_size = sysconf(_SC_PAGESIZE);
        info->num_cpus = sysconf(_SC_NPROCESSORS_CONF);
        info->alloc_granularity = info->page_size;
    }
#endif