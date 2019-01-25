#ifndef _LZR_LOCK_H
#define _LZR_LOCK_H

#include "atomic.h"

#if defined(LZR_WINDOWS)
    typedef uint8_t Condvar;
#elif defined(LZR_LINUX)
    typedef int Condvar;
#endif

void InitCondvar(Condvar* cond);

void CondvarNotifyOne(Condvar* cond);

void CondvarNotifyAll(Condvar* cond);

bool CondvarWait(Condvar* cond, size_t timeout_ms);

typedef struct {
    Condvar condvar;
    ATOMIC(uint8_t) state;
} RwLock;

void InitRwLock(RwLock* lock);

void RwLockRead(RwLock* lock);

void RwLockWrite(RwLock* lock);

void RwUnlockRead(RwLock* lock);

void RwUnlockWrite(RwLock* lock);

bool TryRwLockRead(RwLock* lock);

bool TryRwLockWrite(RwLock* lock);

#endif // _LZR_LOCK_H