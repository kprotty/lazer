#include "lock.h"

#if defined(LZR_WINDOWS)
    #include <Windows.h>
    #define cpu_yield() Sleep(1)
    #define cpu_relax() YieldProcessor()

#elif defined(LZR_LINUX)
    #include <sched.h>
    #include <immintrin.h>
    #define cpu_relax() _mm_pause()
    #define cpu_yield() sched_yield()
#endif

void lzr_spinlock_init(lzr_spinlock_t* self) {
    atomic_flag_clear_explicit(self, memory_order_relaxed);
}

void lzr_spinlock_lock(lzr_spinlock_t* self) {
    size_t contended = 0;
    while (!atomic_flag_test_and_set_explicit(self, memory_order_relaxed)) {
        if (++contended > 20) {
            cpu_relax();
        } else {
            cpu_yield();
        }
    }

    atomic_thread_fence(memory_order_release);
}

void lzr_spinlock_unlock(lzr_spinlock_t* self) {
    atomic_thread_fence(memory_order_acquire);
    atomic_flag_clear_explicit(self, memory_order_relaxed);
}