use core::mem::size_of;
use core::num::NonZeroUsize;
use core::slice::from_raw_parts_mut;

pub fn map<'a, T: Sized>(address: usize, amount: usize, commit: bool, executable: bool) -> Option<&'a mut [T]> {
    self::memory
        ::map(address, size_of::<T>() * amount, commit, executable)
        .map(|ptr| unsafe { from_raw_parts_mut(ptr.get() as *mut T, amount) })
}

pub fn commit<T: Sized>(address_range: &[T], executable: bool) -> bool {
    self::memory::commit(
        address_range.as_ptr() as usize,
        address_range.len() * size_of::<T>(),
        executable,
    )
}

pub fn decommit<T: Sized>(address_range: &[T]) -> bool {
    self::memory::decommit(
        address_range.as_ptr() as usize,
        address_range.len() * size_of::<T>(),
    )
}

#[cfg(target_os = "linux")]
mod memory {
    use super::NonZeroUsize;
    use libc::{
        mmap, madvise,
        MADV_DONTNEED,
        PROT_READ, PROT_WRITE, PROT_EXEC,
        MAP_PRIVATE, MAP_ANONYMOUS, MAP_FIXED, MAP_FAILED,
    };

    pub fn map(address: usize, bytes: usize, _commit: bool, executable: bool) -> Option<NonZeroUsize> {
        let flags = MAP_PRIVATE | MAP_ANONYMOUS | if address > 0 { MAP_FIXED } else { 0 };
        let protect = PROT_READ | PROT_WRITE | if executable { PROT_EXEC } else { 0 };
        
        match unsafe { mmap(address as *mut _, bytes, protect, flags, -1, 0) } {
            MAP_FAILED => None,
            ptr if ptr as usize != address => None,
            ptr => Some(unsafe { NonZeroUsize::new_unchecked(ptr as usize) })
        }
    }

    pub fn commit(_address: usize, _bytes: usize, _executable: bool) -> bool {
        // linux over-commits by default so no need to manually do it
        // madvise(address, bytes, MADV_WILLNEED)
        true
    }

    pub fn decommit(address: usize, bytes: usize) -> bool {
        unsafe { madvise(address as *mut _, bytes, MADV_DONTNEED) == 0 }
    }
}

#[cfg(target_os = "windows")]
mod memory {
    use super::NonZeroUsize;
    use winapi::shared::minwindef::TRUE;
    use winapi::um::{
        memoryapi::{VirtualAlloc, VirtualFree},
        winnt::{
            MEM_COMMIT, MEM_DECOMMIT, MEM_RESERVE,
            PAGE_READWRITE, PAGE_EXECUTE_READWRITE,
        },
    };

    pub fn map(address: usize, bytes: usize, commit: bool, executable: bool) -> Option<NonZeroUsize> {
        let protect = if executable { PAGE_EXECUTE_READWRITE } else { PAGE_READWRITE };
        let alloc_type = MEM_RESERVE | if commit { MEM_COMMIT } else { 0 };
        
        match unsafe { VirtualAlloc(address as *mut _, bytes, alloc_type, protect) } {
            ptr if ptr.is_null() || ptr as usize != address => None,
            ptr => Some(unsafe { NonZeroUsize::new_unchecked(ptr as usize) })
        }
    }

    pub fn commit(address: usize, bytes: usize, executable: bool) -> bool {
        let protect = if executable { PAGE_EXECUTE_READWRITE } else { PAGE_READWRITE };
        unsafe { VirtualAlloc(address as *mut _, bytes, MEM_COMMIT, protect) as usize == address }
    }

    pub fn decommit(address: usize, bytes: usize) -> bool {
        unsafe { VirtualFree(address as *mut _, bytes, MEM_DECOMMIT) == TRUE }
    }
}
