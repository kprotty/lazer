#include "lock.h"

#if defined(LZR_WINDOWS)
    #include <Windows.h>
    #define cpu_relax() YieldProcessor()

#elif defined(LZR_LINUX)
    #include <immintrin.h>
    #define cpu_relax() _mm_pause()
#endif

void lzr_spinlock_init(lzr_spinlock_t* self) {
    atomic_flag_clear_explicit(self, memory_order_relaxed);
}

void lzr_spinlock_lock(lzr_spinlock_t* self) {
    while (!atomic_flag_test_and_set_explicit(self, memory_order_relaxed))
        cpu_relax();
    atomic_thread_fence(memory_order_release);
}

void lzr_spinlock_unlock(lzr_spinlock_t* self) {
    atomic_thread_fence(memory_order_acquire);
    atomic_flag_clear_explicit(self, memory_order_relaxed);
}