use core::mem::size_of;
use core::num::NonZeroUsize;
use core::slice::from_raw_parts_mut;

pub fn map<T: Sized>(address: usize, amount: usize, commit: bool) -> Option<&'static mut [T]> {
    self::memory
        ::map(address, size_of::<T>() * amount, commit)
        .map(|ptr| unsafe { from_raw_parts_mut(ptr.get() as *mut T, amount) })
}

pub fn commit<T: Sized>(address_range: &[T], executable: bool) -> Option<()> {
    self::memory::commit(
        address_range.as_ptr() as usize,
        address_range.len() * size_of::<T>(),
        executable,
    )
}

pub fn decommit<T: Sized>(address_range: &[T]) -> Option<()> {
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

    pub fn map(address: usize, bytes: usize, _commit: bool) -> Option<NonZeroUsize> {
        let flags = MAP_PRIVATE | MAP_ANONYMOUS | if address > 0 { MAP_FIXED } else { 0 };
        
        match unsafe { mmap(address as *mut _, bytes, PROT_READ | PROT_WRITE, flags, -1, 0) } {
            MAP_FAILED => None,
            ptr if ptr as usize != address => None,
            ptr => Some(unsafe { NonZeroUsize::new_unchecked(ptr as usize) })
        }
    }

    pub fn commit(address: usize, bytes: usize, executable: bool) -> Option<()> {
        // linux over-commits by default so madvise call not required
        let flags = PROT_READ | PROT_WRITE | PROT_EXEC;

        if executable && unsafe { mprotect(address as *mut _, bytes, flags) == 0 } {
            Some(())
        } else {
            None
        }
    }

    pub fn decommit(address: usize, bytes: usize) -> Option<()> {
        match unsafe { madvise(address as *mut _, bytes, MADV_DONTNEED) } {
            0 => Some(()),
            _ => None,
        }
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

    pub fn map(address: usize, bytes: usize, commit: bool) -> Option<NonZeroUsize> {
        let alloc_type = MEM_RESERVE | if commit { MEM_COMMIT } else { 0 };
        
        match unsafe { VirtualAlloc(address as *mut _, bytes, alloc_type, PAGE_READWRITE) } {
            ptr if ptr.is_null() || ptr as usize != address => None,
            ptr => Some(unsafe { NonZeroUsize::new_unchecked(ptr as usize) })
        }
    }

    pub fn commit(address: usize, bytes: usize, executable: bool) -> Option<()> {
        let protect = if executable { PAGE_EXECUTE_READWRITE } else { PAGE_READWRITE };

        match unsafe { VirtualAlloc(address as *mut _, bytes, MEM_COMMIT, protect) } {
            ptr if ptr as usize == address => Some(()),
            _ => None,
        }
    }

    pub fn decommit(address: usize, bytes: usize) -> Option<()> {
        match unsafe { VirtualFree(address as *mut _, bytes, MEM_DECOMMIT) } {
            TRUE => Some(()),
            _ => None,
        }
    }
}
