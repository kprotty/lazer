#ifndef LZR_ATOMIC_H
#define LZR_ATOMIC_H

#include "system.h"
#include <stdatomic.h>

#define LZR_ATOMIC(T) _Atomic T

#define LZR_ATOMIC_SEQ_CST memory_order_seq_cst
#define LZR_ATOMIC_RELAXED memory_order_relaxed
#define LZR_ATOMIC_ACQUIRE memory_order_acquire
#define LZR_ATOMIC_RELEASE memory_order_release

#define lzr_atomic_store atomic_store_explicit
#define lzr_atomic_swap atomic_exchange_explicit
#define lzr_atomic_cas atomic_compare_exchange_weak_explicit

typedef atomic_flag lzr_atomic_lock_t;
#define lzr_atomic_flag_init(ptr) atomic_flag_clear_explicit(ptr, LZR_ATOMIC_RELAXED)
#define lzr_atomic_lock(ptr) atomic_flag_test_and_set_explicit(ptr, LZR_ATOMIC_ACQUIRE)
#define lzr_atomic_unlock(ptr) atomic_flag_clear_explicit(ptr, LZR_ATOMIC_RELEASE)

#if defined(LZR_WINDOWS)
    #include <Windows.h>
    #define lzr_cpu_yield() Sleep(1)
    #define lzr_cpu_relax() YieldProcessor()
#else
    #include <sched.h>
    #define lzr_cpu_yield() sched_yield()
    #define lzr_cpu_relax() asm volatile("pause" ::: "memory")
#endif

#endif // LZR_ATOMIC_H