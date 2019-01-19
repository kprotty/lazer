#include "memory.h"

#ifdef LZR_WINDOWS
    #include <Windows.h>

    void* MemoryMap(size_t bytes, bool commit) {
        const DWORD allocType = MEM_RESERVE | (commit ? MEM_COMMIT : 0);
        return VirtualAlloc(NULL, bytes, allocType, PAGE_READWRITE);
    }

    void MemoryUnmap(void* addr, size_t bytes) {
        VirtualFree(addr, 0, MEM_RELEASE);
    }

    void MemoryCommit(void* addr, size_t bytes) {
        VirtualAlloc(addr, bytes, MEM_COMMIT, PAGE_READWRITE);
    }

    void MemoryDecommit(void* addr, size_t bytes) {
        VirtualFree(addr, bytes, MEM_DECOMMIT);
    }

#else
    #include <sys/mman.h>

    void* MemoryMap(size_t bytes, bool commit) {
        const int prot = PROT_READ | PROT_WRITE;
        const int flags = MAP_PRIVATE | MAP_ANONYMOUS;
        void* addr = mmap(NULL, bytes, prot, flags, -1, 0);
        return addr != MAP_FAILED ? addr : NULL;
    }

    void MemoryUnmap(void* addr, size_t bytes) {
        munmap(addr, bytes);
    }

    void MemoryCommit(void* addr, size_t bytes) {
        // unnecessary syscall madvise(addr, bytes, MADV_WILLNEED);
    }

    void MemoryDecommit(void* addr, size_t bytes) {
        madvise(addr, bytes, MADV_DONTNEED);
    }
#endif