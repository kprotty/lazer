use super::{thread, CondVar};
use core::ops::{Drop, Deref, DerefMut};
use core::sync::atomic::{AtomicUsize, Ordering, spin_loop_hint};

pub struct SharedLock<'a, T>(&'a RwLock<T>);
pub struct ExclusiveLock<'a, T>(&'a RwLock<T>);

impl<'a, T> Drop for SharedLock<'a, T> {
    fn drop(&mut self) {
        self.0.unlock_read()
    }
}

impl<'a, T> Drop for ExclusiveLock<'a, T> {
    fn drop(&mut self) {
        self.0.unlock_write()
    }
}

impl<'a, T> DerefMut for ExclusiveLock<'a, T> {
    fn deref_mut(&mut self) -> &mut T {
        self.0.get_unchecked_mut()
    }
}

impl<'a, T> Deref for SharedLock<'a, T> {
    type Target = T;
    fn deref(&self) -> &T {
        self.0.get_unchecked_mut()
    }
}

impl<'a, T> Deref for ExclusiveLock<'a, T> {
    type Target = T;
    fn deref(&self) -> &T {
        self.0.get_unchecked_mut()
    }
}

pub struct Mutex<T>(RwLock<T>);

impl<T> Mutex<T> {
    #[inline]
    pub const fn new(value: T) -> Self {
        Self(RwLock::new(value))
    }

    #[inline]
    pub fn lock(&self) -> ExclusiveLock<T> {
        self.0.lock_write()
    }

    #[inline]
    pub fn try_lock(&self) -> Option<ExclusiveLock<T>> {
        self.0.try_lock_write()
    }
}

pub struct RwLock<T> {
    value: T,
    condvar: CondVar,
    lock: AtomicUsize,
}

const WRITE_FREE: usize = core::usize::MIN;
const WRITE_LOCK: usize = core::usize::MAX;

impl<T> RwLock<T> {
    pub const fn new(value: T) -> Self {
        Self {
            value: value,
            condvar: CondVar::new(),
            lock: AtomicUsize::new(WRITE_FREE),
        }
    }

    #[inline]
    pub fn get_unchecked_mut(&self) -> &mut T {
        unsafe { &mut *(&self.value as *const _ as *mut _) }
    }

    #[inline]
    pub fn unlock_write(&self) {
        self.lock.store(WRITE_FREE, Ordering::Release);
        self.condvar.notify_one();
    }

    #[inline]
    pub fn unlock_read(&self) {
        if self.lock.fetch_sub(1, Ordering::Release) == 1 {
            self.condvar.notify_one();
        }
    }

    #[inline]
    pub fn lock_write(&self) -> ExclusiveLock<T> {
        self.adaptive_lock(|| self.try_lock_write())
    }

    #[inline]
    pub fn lock_read(&self) -> SharedLock<T> {
        self.adaptive_lock(|| self.try_lock_read())
    } 

    #[inline]
    pub fn try_lock_write(&self) -> Option<ExclusiveLock<T>> {
        self.cas(WRITE_FREE, WRITE_LOCK).map(|_| ExclusiveLock(self))
    }

    pub fn try_lock_read(&self) -> Option<SharedLock<T>> {
        loop {
            match self.lock.load(Ordering::Relaxed) {
                WRITE_LOCK => return None,
                readers if self.cas(readers, readers + 1).is_some()
                    => return Some(SharedLock(self)),
                _ => {},
            }
        }
    }

    #[inline]
    fn cas(&self, expect: usize, swap_with: usize) -> Option<usize> {
        const SUCCESS: Ordering = Ordering::Acquire;
        const FAILURE: Ordering = Ordering::Relaxed;
        self.lock.compare_exchange_weak(expect, swap_with, SUCCESS, FAILURE).ok()
    }

    #[inline]
    fn adaptive_lock<A, V>(&self, acquire: A) -> V where A: Fn() -> Option<V> {
        const MAX_POLL_ITERS: usize = 10;
        const CPU_BOUND_ITERS: usize = 3;
        assert!(CPU_BOUND_ITERS <= MAX_POLL_ITERS);

        #[inline]
        fn cpu_relax(iterations: usize) {
            for _ in 0..iterations {
                spin_loop_hint();
            }
        }

        loop {
            for iteration in 0..MAX_POLL_ITERS {
                if let Some(value) = acquire() {
                    return value
                } else if iteration <= CPU_BOUND_ITERS {
                    cpu_relax(1 << iteration);
                } else {
                    thread::yield_cpu();
                }
            }
            self.condvar.wait()
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_rwlock_read_lock() {
        let rwlock = RwLock::new(());

        {
            let _first = rwlock.lock_read();
            let _second = rwlock.lock_read();
            assert!(rwlock.try_lock_write().is_none());
        }

        assert!(rwlock.try_lock_write().is_some());
    }

    #[test]
    fn test_rwlock_write_lock() {
        let rwlock = RwLock::new(());

        {
            let _writer = rwlock.lock_write();
            assert!(rwlock.try_lock_read().is_none());
        }

        assert!(rwlock.try_lock_read().is_some());
    }
}