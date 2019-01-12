use super::ffi::*;
use core::time::Duration;
pub use self::condvar::*;

#[cfg(target_os = "windows")]
mod condvar {
    use super::*;

    #[derive(Default)]
    pub struct CondVar(u8);

    impl CondVar {
        #[inline]
        pub const fn new() -> Self {
            Self(0)
        }

        #[inline]
        fn addr(&self) -> *mut u8 {
            &self.0 as *const _ as *mut u8
        }

        #[inline]
        pub fn notify_all(&self) {
            unsafe { WakeByAddressAll(self.addr()) }
        }

        #[inline]
        pub fn notify_one(&self) {
            unsafe { WakeByAddressSingle(self.addr()) }
        }

        #[inline]
        pub fn wait(&self) {
            self.wait_on_address(INFINITE)
        }

        #[inline]
        pub fn wait_timeout(&self, timeout: Duration) {
            self.wait_on_address(timeout.as_millis() as DWORD)
        }

        #[inline]
        fn wait_on_address(&self, timeout: DWORD) {
            let address = self.addr();
            let size = size_of::<Self>();
            unsafe { WaitOnAddress(address, address, size, timeout); }
        }
    }
}

#[cfg(target_os = "linux")]
mod condvar {
    use super::*;

    #[derive(Default)]
    pub struct CondVar(u32);

    const FUTEX_PRIVATE: c_int = 128;
    const FUTEX_WAIT: c_int = 0 | FUTEX_PRIVATE;
    const FUTEX_WAKE: c_int = 1 | FUTEX_PRIVATE;

    impl CondVar {
        #[inline]
        pub const fn new() -> Self {
            Self(0)
        }

        #[inline]
        pub fn notify_all(&self) {
            unsafe { syscall(SYS_futex, &self.0, FUTEX_WAKE, INT_MAX); }
        }

        #[inline]
        pub fn notify_one(&self) {
            unsafe { syscall(SYS_futex, &self.0, FUTEX_WAKE, 1); }
        }

        #[inline]
        pub fn wait(&self) {
            unsafe { syscall(SYS_futex, &self.0, FUTEX_WAIT, self.0, 0); }
        }

        #[inline]
        pub fn wait_timeout(&self, timeout: Duration) {
            let timeout = timespec {
                tv_sec: timeout.as_secs() as _,
                tv_nsec: timeout.subsec_nanos() as _,
            };
            unsafe { syscall(SYS_futex, &self.0, FUTEX_WAIT, self.0, &timeout); }
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;

    use std::thread;
    use std::sync::Arc;
    use std::sync::atomic::{AtomicBool, Ordering};

    #[test]
    fn test_condition_variable() {
        let info = Arc::new((CondVar::new(), AtomicBool::new(false)));
        let thread_info = info.clone();

        thread::spawn(move || {
            let &(ref condvar, ref complete) = &* thread_info;
            complete.store(true, Ordering::Relaxed);
            condvar.notify_one();
        });

        let &(ref condvar, ref complete) = &* info;
        while !complete.load(Ordering::Relaxed) {
            condvar.wait();
        }

        assert!(complete.load(Ordering::Relaxed));
    }
}