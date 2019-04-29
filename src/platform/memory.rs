use core::mem::size_of;
use core::num::NonZeroUsize;
use core::slice::from_raw_parts_mut;

pub fn map<T: Sized>(address: usize, amount: usize) -> Option<&'static mut[T]> {
    unsafe {
        self::memory
            ::map(address, size_of::<T>() * amount)
            .map(|ptr| from_raw_parts_mut(ptr.get() as *mut _, amount))
    }
}

pub fn unmap<T: Sized>(memory: &[T]) -> Option<()> {
    unsafe { 
        self::memory::unmap(
            memory.as_ptr() as usize,
            memory.len() * size_of::<T>()
        )
    }
}

pub fn commit<T: Sized>(memory: &[T]) -> Option<()> {
    unsafe {
        self::memory::commit(
            memory.as_ptr() as usize,
            memory.len() * size_of::<T>()
        )
    }
}

pub fn decommit<T: Sized>(memory: &[T]) -> Option<()> {
    unsafe {
        self::memory::decommit(
            memory.as_ptr() as usize,
            memory.len() * size_of::<T>()
        )
    }
}

#[cfg(unix)]
mod memory {
    use libc::*;
    use super::NonZeroUsize;

    pub unsafe fn map(address: usize, bytes: usize) -> Option<NonZeroUsize> {
        let prot = PROT_READ | PROT_WRITE;
        let flags = MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS;
        match mmap(address as *mut _, bytes, prot, flags, -1, 0) as usize {
            ptr if ptr != address => None,
            ptr => Some(NonZeroUsize::new_unchecked(ptr)),
        }
    }

    pub unsafe fn unmap(address: usize, bytes: usize) -> Option<()> {
        match munmap(address as *mut _, bytes) {
            0 => None,
            _ => Some(()),
        }
    }

    pub unsafe fn commit(address: usize, bytes: usize) -> Option<()> {
        // linux over-commits by default
        Some(())
    }

    pub unsafe fn decommit(address: usize, bytes: usize) -> Option<()> {
        match madvise(address as *mut _, bytes, MADV_DONTNEED) {
            0 => Some(()),
            _ => None,
        }
    }
}

#[cfg(windows)]
mod memory {
    use super::NonZeroUsize;
    use winapi::shared::minwindef::*;
    use winapi::um::{winnt::*, memoryapi::*};

    pub unsafe fn map(address: usize, bytes: usize) -> Option<NonZeroUsize> {
        match VirtualAlloc(address as *mut _, bytes, MEM_RESERVE, PAGE_READWRITE) as usize {
            0 => None,
            ptr => Some(NonZeroUsize::new_unchecked(ptr)),
        }
    }

    pub unsafe fn unmap(address: usize, bytes: usize) -> Option<()> {
        match VirtualFree(address as *mut _, 0, MEM_RELEASE) {
            TRUE => Some(()),
            _ => None,
        }
    }

    pub unsafe fn commit(address: usize, bytes: usize) -> Option<()> {
        match VirtualAlloc(address as *mut _, bytes, MEM_COMMIT, PAGE_READWRITE) {
            ptr if ptr.is_null() => None,
            _ => Some(()),
        }
    }

    pub unsafe fn decommit(address: usize, bytes: usize) -> Option<()> {
        match VirtualFree(address as *mut _, bytes, MEM_DECOMMIT) {
            TRUE => Some(()),
            _ => None,
        }
    }
}