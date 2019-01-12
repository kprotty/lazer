use super::ffi::*;
use core::sync::atomic::{AtomicBool, Ordering, spin_loop_hint};

pub struct SystemInfo {
    pub num_cpus: usize,
    pub page_size: usize,
    pub allocation_granularity: usize,
}

impl SystemInfo {
    pub fn get() -> &'static SystemInfo {
        const ORDER: Ordering = Ordering::Relaxed;

        static mut INITIALIZED: AtomicBool = AtomicBool::new(false);
        static mut CREATED: AtomicBool = AtomicBool::new(false);
        static mut SYS_INFO: SystemInfo = SystemInfo {
            num_cpus: 0,
            page_size: 0,
            allocation_granularity: 0,
        };

        unsafe {
            if CREATED.compare_exchange_weak(false, true, ORDER, ORDER).is_ok() {
                SYS_INFO = Self::get_system_info();
                INITIALIZED.store(true, ORDER);
            } else {
                while !INITIALIZED.load(ORDER) {
                    spin_loop_hint();
                }
            }
            &SYS_INFO
        }
    }
}

#[cfg(target_os = "linux")]
impl SystemInfo {
    #[inline]
    unsafe fn get_system_info() -> SystemInfo {
        let page_size = sysconf(_SC_PAGESIZE) as usize;
        let num_cpus = sysconf(_SC_NPROCESSORS_CONF) as usize;

        SystemInfo {
            num_cpus,
            page_size,
            allocation_granularity: page_size,
        }
    }
}

#[cfg(target_os = "windows")]
impl SystemInfo {
    #[inline]
    unsafe fn get_system_info() -> SystemInfo {

        #[repr(C)]
        #[allow(non_snake_case)]
        struct SYSTEM_INFO {
            dwOemId: DWORD,
            dwPageSize: DWORD,
            lpMinimumApplicationAddress: usize,
            lpMaximumApplicationAddress: usize,
            dwActiveProcessorMask: *const DWORD,
            dwNumberOfProcessors: DWORD,
            dwProcessorType: DWORD,
            dwAllocationGranularity: DWORD,
            wProcessorLevel: WORD,
            wProcessorRevision: DWORD,
        }

        let mut sys_info: SYSTEM_INFO = uninitialized();
        GetSystemInfo(&mut sys_info as *mut _ as *mut _);

        SystemInfo {
            page_size: sys_info.dwPageSize as usize,
            num_cpus: sys_info.dwNumberOfProcessors as usize,
            allocation_granularity: sys_info.dwAllocationGranularity as usize,
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_system_info() {
        let info = SystemInfo::get();
        assert!(info.num_cpus >= 1);
        assert!(info.page_size >= 1);
        assert!(info.allocation_granularity >= info.page_size);
    }
}