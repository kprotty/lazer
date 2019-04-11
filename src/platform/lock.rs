use core::sync::atomic::{self, Ordering, AtomicBool};

pub struct Spinlock(AtomicBool);

impl !Send for Spinlock {}
impl !Sync for Spinlock {}

impl Spinlock {
    pub fn new() -> Self {
        Self(AtomicBool::new(false))
    }

    pub fn lock(&self) {
        while self.0.swap(true, Ordering::Relaxed) {
            atomic::spin_loop_hint();
        }
        atomic::fence(Ordering::Release);
    }

    pub fn unlock(&self) {
        atomic::fence(Ordering::Acquire);
        self.0.store(false, Ordering::Relaxed);
    }
}
