#include "memory.h"

#if defined(LZR_WINDOWS)
    #include <Windows.h>

    void* lzr_memory_map(void* addr, size_t bytes, bool commit) {
        DWORD alloc_type = MEM_RESERVE | (commit ? MEM_COMMIT : 0);
        DWORD protect = commit ? PAGE_NOACCESS : PAGE_READWRITE;

        void* address = VirtualAlloc(addr, bytes, alloc_type, protect);
        assert(address != NULL);
        return address;
    }

    void lzr_memory_unmap(void* addr, size_t bytes) {
        BOOL unmapped = VirtualFree(addr, 0, MEM_RELEASE);
        assert(unmapped == TRUE);
    }

    void lzr_memory_commit(void* addr, size_t bytes) {
        void* address = VirtualAlloc(addr, bytes, MEM_COMMIT, PAGE_READWRITE);
        assert(address == addr);
    }

    void lzr_memory_decommit(void* addr, size_t bytes) {
        BOOL decommitted = VirtualFree(addr, bytes, MEM_DECOMMIT);
        assert(decommitted == TRUE);
    }

#else
    #include <sys/mman.h>

    void* lzr_memory_map(void* addr, size_t bytes, bool commit) {
        int protect = PROT_READ | PROT_WRITE;
        int flags = MAP_PRIVATE | MAP_ANONYMOUS | (addr != NULL ? MAP_FIXED : 0);

        addr = mmap(addr, bytes, protect, flags, -1, 0);
        assert(addr != MAP_FAILED);
        return addr;
    }

    void lzr_memory_unmap(void* addr, size_t bytes) {
        int unmapped = munmap(addr, bytes);
        assert(unmapped == 0);
    }

    void lzr_memory_commit(void* addr, size_t bytes) {
        // linux over-commits memory by default
    }

    void lzr_memory_decommit(void* addr, size_t bytes) {
        int decommitted = madvise(addr, bytes, MADV_DONTNEED);
        assert(decommitted == 0);
    }
    
#endif