use super::ffi::*;
use core::time::Duration;
pub use self::thread::{*, sleep};

#[cfg(target_os = "windows")]
mod thread {
    use super::*;
    pub struct JoinHandle(HANDLE);

    impl JoinHandle {
        #[inline]
        pub fn join(&self) {
            unsafe {
                WaitForSingleObject(self.0, INFINITE);
                CloseHandle(self.0);
            }
        }
    }

    #[inline]
    pub fn spawn<T>(func: fn(*mut T), arg: *mut T) -> JoinHandle {
        unsafe {
            let mut thread_id = uninitialized();
            JoinHandle(CreateThread(
                null_mut(),
                0,
                transmute(func),
                transmute(arg),
                0,
                &mut thread_id,
            ))
        }
    }

    #[inline]
    pub fn sleep(duration: Duration) {
        unsafe { Sleep(duration.as_millis() as DWORD); }
    }

    #[inline]
    pub fn yield_cpu() {
        sleep(Duration::from_millis(0))
    }
}

#[cfg(target_os = "linux")]
mod thread {
    use super::*;
    pub struct JoinHandle(pthread_t);

    impl JoinHandle {
        #[inline]
        pub fn join(&self) {
            unsafe { pthread_join(self.0, null_mut()); }
        }
    }

    #[inline]
    pub fn spawn<T>(func: fn(*mut T), arg: *mut T) -> JoinHandle {
        unsafe {
            let mut handle = uninitialized();
            pthread_create(
                &mut handle,
                null_mut(),
                transmute(func),
                transmute(arg),
            );
            JoinHandle(handle)
        }
    }

    #[inline]
    pub fn sleep(duration: Duration) {
        unsafe {
            nanosleep(&mut timespec {
                tv_sec: duration.as_secs() as _,
                tv_nsec: duration.subsec_nanos() as _,
            }, null_mut());
        }
    }

    #[inline]
    pub fn yield_cpu() {
        unsafe { sched_yield(); }
    }
}

#[cfg(test)]
mod test {
    use std::time::{Duration, Instant};
    use std::sync::atomic::{AtomicBool, Ordering};

    #[test]
    fn test_thread_create() {
        let mut complete = AtomicBool::new(false);
        super::spawn(test_thread_func, &mut complete).join();
        assert!(complete.load(Ordering::Relaxed));

        fn test_thread_func(complete: *mut AtomicBool) {
            let complete = unsafe { &* complete };
            complete.store(true, Ordering::Relaxed);
        }
    }

    #[test]
    fn test_thread_sleep() {
        const DELAY: u64 = 500;
        const THRESHOLD: u64 = 200;
        
        let now = Instant::now();
        super::sleep(Duration::from_millis(DELAY));
        let elapsed = now.elapsed().as_millis() as u64;
        
        let range = (DELAY - THRESHOLD)..=(DELAY + THRESHOLD);
        assert!(range.contains(&elapsed));
    }
}