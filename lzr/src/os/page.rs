use super::ffi::*;
use super::SystemInfo;
use self::page_flags::*;

use core::marker::PhantomData;
use core::slice::from_raw_parts;
use core::slice::from_raw_parts_mut;

pub mod page_flags {
    pub const PAGE_EXEC: usize = 1 << 0;
    pub const PAGE_READ: usize = 1 << 1;
    pub const PAGE_WRITE: usize = 1 << 2;
    pub const PAGE_COMMIT: usize = 1 << 3;
    pub const PAGE_DECOMMIT: usize = 1 << 4;
    pub const PAGE_SEQUENTIAL: usize = 1 << 5;
}

#[derive(Default)]
pub struct Page<T: Sized> {
    size: usize,
    addr: usize,
    phantom: PhantomData<T>,
}

impl<T> From<(*mut T, usize)> for Page<T> {
    fn from((address, size): (*mut T, usize)) -> Self {
        Self {
            size: size,
            addr: address as _,
            phantom: PhantomData,
        }
    }
}

impl<T> Page<T> {
    #[inline]
    pub fn empty() -> Self {
        Self {
            size: 0,
            addr: 0,
            phantom: PhantomData,
        }
    }

    #[inline]
    pub const fn len(&self) -> usize {
        self.size
    }

    #[inline]
    pub const fn offset(&self, offset: usize) -> *mut T {
        (self.addr + (size_of::<T>() * offset)) as *mut T
    }

    #[inline]
    pub fn as_slice(&self) -> &[T] {
        unsafe { from_raw_parts(self.addr as *const _, self.size) }
    }

    #[inline]
    pub fn as_slice_mut(&self) -> &mut [T] {
        unsafe { from_raw_parts_mut(self.addr as *mut _, self.size) }
    }
}

#[cfg(target_os = "windows")]
impl<T> Page<T> {
    fn get_protect_flags(flags: usize) -> DWORD {
        match flags & (PAGE_EXEC | PAGE_READ | PAGE_WRITE) {
            PAGE_EXEC => PAGE_EXECUTE,
            PAGE_READ => PAGE_READONLY,
            PAGE_WRITE => PAGE_READWRITE,
            p if p == (PAGE_READ | PAGE_WRITE) => PAGE_READWRITE,
            p if p == (PAGE_READ | PAGE_EXEC) => PAGE_EXECUTE_READ,
            p if p == (PAGE_EXEC | PAGE_WRITE) => PAGE_EXECUTE_READWRITE,
            p if p == (PAGE_READ | PAGE_EXEC | PAGE_WRITE) => PAGE_EXECUTE_READWRITE,
            _ => PAGE_NOACCESS,
        }
    }

    pub fn mmap(mut size: usize, flags: usize) -> Option<Self> {
        const NULL: *mut u8 = null_mut();
        size = align(size, SystemInfo::get().allocation_granularity);
    
        let mut memory_flags = MEM_RESERVE;
        let alloc_size = size * size_of::<T>();
        let protect_flags = Self::get_protect_flags(flags);

        if (flags & PAGE_COMMIT) != 0 {
            memory_flags |= MEM_COMMIT;
        }

        match unsafe { VirtualAlloc(null_mut(), alloc_size, memory_flags, protect_flags) } {
            NULL => None,
            addr => Some(Self {
                size: size,
                addr: addr as _,
                phantom: PhantomData,
            })
        }
    }

    #[inline]
    pub fn advise(&self, size: usize, flags: usize) {
        unsafe {
            let addr = self.addr as *mut _;
            let size = size * size_of::<T>();

            if (flags & PAGE_COMMIT) != 0 {
                let protect = Self::get_protect_flags(flags);
                VirtualAlloc(addr, size, MEM_COMMIT, protect);
            } else if (flags & PAGE_DECOMMIT) != 0 {
                VirtualFree(addr, size, MEM_DECOMMIT);
            }
        }
    }

    #[inline]
    pub fn protect(&self, size: usize, flags: usize) {
        unsafe {
            let addr = self.addr as *mut _;
            let size = size * size_of::<T>();
            
            let mut old_protect = uninitialized();
            let protect = Self::get_protect_flags(flags);
            VirtualProtect(addr, size, protect, &mut old_protect);
        }
    }

    #[inline]
    pub fn unmap(self) {
        unsafe { VirtualFree(self.addr as *mut _, 0, MEM_RELEASE); }
    }
}

#[cfg(target_os = "linux")]
impl<T> Page<T> {
    fn get_protect_flags(flags: usize) -> c_int {
        let mut protect_flags = 0;

        if (flags & (PAGE_EXEC | PAGE_READ | PAGE_WRITE)) == 0 {
            protect_flags = PROT_NONE;
        } else {
            if (flags & PAGE_EXEC) != 0 {
                protect_flags |= PROT_EXEC;
            }
            if (flags & PAGE_READ) != 0 {
                protect_flags |= PROT_READ;
            }
            if (flags & PAGE_WRITE) != 0 {
                protect_flags |= PROT_WRITE;
            }
        }

        protect_flags
    }

    pub fn mmap(mut size: usize, flags: usize) -> Option<Self> {
        size = align(size, SystemInfo::get().allocation_granularity);

        let alloc_size = size * size_of::<T>();
        let memory_flags = MAP_PRIVATE | MAP_ANONYMOUS;
        let protect_flags = Self::get_protect_flags(flags);

        match unsafe { mmap(null_mut(), alloc_size, protect_flags, memory_flags, -1, 0) } {
            MAP_FAILED => None,
            addr => Some(Self {
                size: size,
                addr: addr as _,
                phantom: PhantomData,
            })
        }
    }

    #[inline]
    pub fn advise(&self, size: usize, flags: usize) {
        unsafe {
            let mut advise_flags = 0;
            let addr = self.addr as *mut _;
            let size = size * size_of::<T>();

            if (flags & PAGE_DECOMMIT) != 0 {
                advise_flags |= MADV_DONTNEED;
            }
            if (flags & PAGE_SEQUENTIAL) != 0 {
                advise_flags |= MADV_SEQUENTIAL;
            }

            madvise(addr, size, advise_flags);
        }
    }

    #[inline]
    pub fn protect(&self, size: usize, flags: usize) {
        unsafe {
            let size = size * size_of::<T>();
            mprotect(self.addr as *mut _, size, Self::get_protect_flags(flags));    
        }
    }

    #[inline]
    pub fn unmap(self) {
        unsafe { munmap(self.addr as *mut _, self.size * size_of::<T>()); }
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_virtual_memory() {
        const FLAGS: usize = PAGE_READ | PAGE_WRITE | PAGE_COMMIT;
        let alignment = SystemInfo::get().allocation_granularity;
        let page = Page::<u8>::mmap(64, FLAGS).unwrap();
        
        assert_eq!(page.len(), alignment);
        assert!(!page.offset(0).is_null());

        page.unmap();
    }

}