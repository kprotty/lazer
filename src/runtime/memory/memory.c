#include "memory.h"

#if defined(LZR_WINDOWS)
    #include <Windows.h>

    void* lzr_memory_map(void* addr, size_t bytes, bool commit) {
        DWORD alloc_type = MEM_RESERVE | (commit ? MEM_COMMIT : 0);
        DWORD protect = commit ? PAGE_NOACCESS : PAGE_READWRITE;
        return VirtualAlloc(addr, bytes, alloc_type, protect);
    }

    bool lzr_memory_unmap(void* addr, size_t bytes) {
        return VirtualFree(addr, 0, MEM_RELEASE);
    }

    bool lzr_memory_commit(void* addr, size_t bytes) {
        return VirtualAlloc(addr, bytes, MEM_COMMIT, PAGE_READWRITE) == addr;
    }

    bool lzr_memory_decommit(void* addr, size_t bytes) {
        return VirtualFree(addr, bytes, MEM_DECOMMIT);
    }

#else
    #include <sys/mman.h>

    void* lzr_memory_map(void* addr, size_t bytes, bool commit) {
        int protect = PROT_READ | PROT_WRITE;
        int flags = MAP_PRIVATE | MAP_ANONYMOUS | (addr != NULL ? MAP_FIXED : 0);
        addr = mmap(addr, bytes, protect, flags, -1, 0);
        return addr == MAP_FAILED ? NULL : addr;
    }

    bool lzr_memory_unmap(void* addr, size_t bytes) {
        return munmap(addr, bytes) == 0;
    }

    bool lzr_memory_commit(void* addr, size_t bytes) {
        // linux over-commits memory by default
        return true;
    }

    bool lzr_memory_decommit(void* addr, size_t bytes) {
        return madvise(addr, bytes, MADV_DONTNEED) == 0;
    }
    
#endif