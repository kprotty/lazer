#ifndef _LZR_ATOMIC_H
#define _LZR_ATOMIC_H

#include "system.h"
#include <stdatomic.h>

#define ATOMIC(T) _Atomic T
#define ATOMIC_RELAXED __ATOMIC_RELAXED
#define ATOMIC_CONSUME __ATOMIC_CONSUME
#define ATOMIC_ACQUIRE __ATOMIC_ACQUIRE
#define ATOMIC_RELEASE __ATOMIC_RELEASE
#define ATOMIC_ACQ_REL __ATOMIC_ACQ_REL
#define ATOMIC_SEQ_CST __ATOMIC_SEQ_CST

#define AtomicLoad(ptr, ordering) \
    __atomic_load_n(ptr, ordering)

#define AtomicStore(ptr, value, ordering) \
    __atomic_store_n(ptr, value, ordering)

#define AtomicSwap(ptr, value, ordering) \
    __atomic_exchange_n(ptr, value, ordering)

#define AtomicAdd(ptr, value, ordering) \
    __atomic_add_fetch(ptr, value, ordering)

#define AtomicSub(ptr, value, ordering) \
    __atomic_sub_fetch(ptr, value, ordering)

#define AtomicCas(ptr, expect, swap, success, failure) \
    __atomic_compare_exchange_n(ptr, expect, swap, true, success, failure)

#if defined(LZR_WINDOWS)
    #include <Windows.h>
    #define CpuYield() Sleep(1)
#elif defined(LZR_LINUX)
    #include <sched.h>
    #define CpuYield() sched_yield()
#endif

#if defined(LZR_WINDOWS)
    #define CpuRelax() YieldProcessor()
#elif defined(LZR_X86)
    #define CpuRelax() asm volatile("pause" ::: "memory")
#elif defined(LZR_ARM) && defined(LZR_64)
    #define CpuRelax() asm volatile("yield" ::: "memory")
#else
    #define CpuRelax()
#endif

#endif // _LZR_ATOMIC_H