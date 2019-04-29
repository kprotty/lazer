use core::marker::{Send, Sync};
use core::sync::atomic::{spin_loop_hint, Ordering, AtomicBool};

pub struct Spinlock(AtomicBool);

unsafe impl Send for Spinlock {}
unsafe impl Sync for Spinlock {}

impl Spinlock {
    pub fn new() -> Self {
        Self(AtomicBool::new(false))
    }

    pub fn lock(&self) {
        while self.try_lock().is_none() {
            spin_loop_hint();
        }
    }

    pub fn unlock(&self) {
        self.0.store(false, Ordering::Release);
    }

    #[inline]
    pub fn try_lock(&self) -> Option<()> {
        match self.0.swap(true, Ordering::Acquire) {
            true => None,
            false => Some(())
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use std::thread;
    use std::time::Duration;

    #[test]
    fn spin_lock() {
        let mutex = Spinlock::new();
        let flag = AtomicBool::new(false);

        // test try-lock
        assert!(mutex.try_lock().is_some());
        assert!(mutex.try_lock().is_none());
        mutex.unlock();
        
        // test multi-threaded locking
        flag.store(true, Ordering::Relaxed);
        mutex.lock();

        // try and update the flag from another thread
        let mutex_ref = 
        let waiter = thread::spawn(move || {
            mutex.lock();
            assert!(flag.load(Ordering::Relaxed));
            flag.store(false, Ordering::Relaxed);
            mutex.unlock();
        });

        // test if the thread updated the flag
        thread::sleep(Duration::from_millis(10));
        mutex.unlock();
        waiter.join();
        assert!(!flag.load(Ordering::Relaxed));
    }
}