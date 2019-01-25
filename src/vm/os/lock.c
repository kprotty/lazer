#include "lock.h"

#if defined(LZR_WINDOWS)
    #include <Windows.h>

    void InitCondvar(Condvar* cond) {
        *cond = 0;
    }

    void CondvarNotifyOne(Condvar* cond) {
        WakeByAddressSingle(cond);
    }

    void CondvarNotifyAll(Condvar* cond) {
        WakeByAddressAll(cond);
    }

    bool CondvarWait(Condvar* cond, size_t timeout_ms) {
        if (timeout_ms == 0)
            return WaitOnAddress(cond, cond, sizeof(Condvar), INFINITE);

        WaitOnAddress(cond, cond, sizeof(Condvar), (DWORD) timeout_ms);
        return GetLastError() != ERROR_TIMEOUT;
    }

#elif defined(LZR_LINUX)
    #include <errno.h>
    #include <unistd.h>
    #include <limits.h>
    #include <sys/time.h>
    #include <sys/syscall.h>
    #include <linux/futex.h>

    void InitCondvar(Condvar* cond) {
        *cond = 0;
    }

    void CondvarNotifyOne(Condvar* cond) {
        syscall(SYS_futex, cond, FUTEX_WAKE_PRIVATE, 1);
    }

    void CondvarNotifyAll(Condvar* cond) {
        syscall(SYS_futex, cond, FUTEX_WAKE_PRIVATE, INT_MAX);
    }

    bool CondvarWait(Condvar* cond, size_t timeout_ms) {
        if (timeout_ms == 0) {
            syscall(SYS_futex, cond, FUTEX_WAIT_PRIVATE, *cond);
            return true;
        } else {
            struct timespec timeout = {
                .tv_sec = timeout_ms / 1000,
                .tv_nsec = (timeout_ms % 1000) * 1000000
            };
            syscall(SYS_futex, cond, FUTEX_WAIT_PRIVATE, *cond, &timeout);
            return errno != ETIMEDOUT;
        }
    }
#endif

#define WRITE_FREE 0x00
#define WRITE_LOCK 0xff

void InitRwLock(RwLock* lock) {
    lock->state = WRITE_FREE;
    InitCondvar(&lock->condvar);
}

void RwUnlockRead(RwLock* lock) {
    if (AtomicSub(&lock->state, 1, ATOMIC_RELEASE) == 0)
        CondvarNotifyOne(&lock->condvar);
}

void RwUnlockWrite(RwLock* lock) {
    AtomicStore(&lock->state, WRITE_FREE, ATOMIC_RELEASE);
    CondvarNotifyOne(&lock->condvar);
}

LZR_INLINE bool RwLockCas(RwLock* lock, uint8_t expect, uint8_t swap_with) {
    return AtomicCas(&lock->state, &expect, swap_with, ATOMIC_ACQUIRE, ATOMIC_RELAXED);
}

bool TryRwLockWrite(RwLock* lock) {
    return RwLockCas(lock, WRITE_FREE, WRITE_LOCK);
}

bool TryRwLockRead(RwLock* lock) {
    uint8_t readers = AtomicLoad(&lock->state, ATOMIC_RELAXED);
    while (readers != WRITE_LOCK) {
        if (RwLockCas(lock, readers, readers + 1))
            return true;
        readers = AtomicLoad(&lock->state, ATOMIC_RELAXED);
    }
    return false;
}

LZR_INLINE void CpuSpinRelax(size_t iterations) {
    while (iterations--)
        CpuRelax();
}

#define MAX_POLL_ITERATIONS 10
#define CPU_BOUND_ITERATIONS 3
#define RWLOCK_ADAPTIVE_LOCK(lock, try_acquire)                                 \
    while (true) {                                                              \
        size_t iterations = MAX_POLL_ITERATIONS;                                \
        while (iterations--) {                                                  \
            if (try_acquire(lock))                                              \
                return;                                                         \
            else if (iterations >= (MAX_POLL_ITERATIONS - CPU_BOUND_ITERATIONS))\
                CpuSpinRelax(1 << (MAX_POLL_ITERATIONS - iterations));          \
            else                                                                \
                CpuYield();                                                     \
        }                                                                       \
        CondvarWait(&lock->condvar, 0);                                         \
    }

void RwLockRead(RwLock* lock) {
    RWLOCK_ADAPTIVE_LOCK(lock, TryRwLockRead)
}

void RwLockWrite(RwLock* lock) {
    RWLOCK_ADAPTIVE_LOCK(lock, TryRwLockWrite)
}