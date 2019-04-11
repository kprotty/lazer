#ifndef LZR_LOCK_H
#define LZR_LOCK_H

#include "../system.h"
#include <stdatomic.h>

typedef atomic_flag lzr_spinlock_t;

void lzr_spinlock_init(lzr_spinlock_t* self);

void lzr_spinlock_lock(lzr_spinlock_t* self);

void lzr_spinlock_unlock(lzr_spinlock_t* self);

#endif // LZR_LOCK_H
