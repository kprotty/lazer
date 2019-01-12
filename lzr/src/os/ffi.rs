pub use self::ffi::*;
pub use libc::{*, memcpy as _memcpy, memmove as _memmove};
pub use core::ptr::{self, null, null_mut, write_bytes as memset};
pub use core::mem::{size_of, replace, swap, zeroed, transmute, uninitialized};

#[inline(always)]
pub const fn align(value: usize, alignment: usize) -> usize {
    value + (alignment - (value % alignment))
}

#[inline(always)]
pub unsafe fn memmove<T>(dst: *mut T, src: *const T, size: usize) {
    core::ptr::copy(src, dst, size)
}

#[inline(always)]
pub unsafe fn memcpy<T>(dst: *mut T, src: *const T, size: usize) {
    core::ptr::copy_nonoverlapping(src, dst, size)
}

#[cfg(not(target_os = "windows"))]
mod ffi {}

#[cfg(target_os = "windows")]
mod ffi {
    use super::*;

    pub type WORD = c_ushort;
    pub type DWORD = c_uint;
    pub type HANDLE = *mut u8;
    
    pub const PAGE_NOACCESS: DWORD = 0x01;
    pub const PAGE_READONLY: DWORD = 0x02;
    pub const PAGE_READWRITE: DWORD = 0x04;
    pub const PAGE_EXECUTE: DWORD = 0x10;
    pub const PAGE_EXECUTE_READ: DWORD = 0x20;
    pub const PAGE_EXECUTE_READWRITE: DWORD = 0x40;

    pub const MEM_COMMIT: DWORD = 0x1000;
    pub const MEM_DECOMMIT: DWORD = 0x4000;
    pub const MEM_RESERVE: DWORD = 0x2000;
    pub const MEM_RELEASE: DWORD = 0x8000;
    pub const MEM_LARGE_PAGES: DWORD = 0x20000000;

    pub const INFINITE: DWORD = -1i32 as DWORD;
    pub const STD_ERROR_HANDLE: DWORD = -12i32 as DWORD;
    pub const STD_OUTPUT_HANDLE: DWORD = -11i32 as DWORD;

    #[link(name = "synchronization")]
    extern "system" {
        pub fn WakeByAddressAll(address: *mut u8);
        pub fn WakeByAddressSingle(address: *mut u8);
        pub fn WaitOnAddress(
            address: *mut u8,
            compare: *mut u8,
            address_size: usize,
            timeout: DWORD
        ) -> bool;
    }

    extern "system" {
        pub fn GetSystemInfo(info: *mut u8);
        pub fn GetModuleHandleA(name: *const u8) -> HANDLE;
        pub fn GetProcAddress(handle: HANDLE, name: *const u8) -> *mut u8;

        pub fn GetStdHandle(handle: DWORD) -> HANDLE;
        pub fn CloseHandle(handle: HANDLE) -> bool;
        pub fn WriteConsoleA(
            handle: HANDLE,
            buffer: *const u8,
            write_chars: DWORD,
            written_chars: *mut DWORD,
            reserved: *mut u8,
        ) -> bool;

        pub fn Sleep(timeout: DWORD);
        pub fn WaitForSingleObject(handle: HANDLE, timeout: DWORD) -> DWORD;
        pub fn CreateThread(
            attributes: *mut u8,
            stack_size: usize,
            func_address: *mut u8,
            func_parameter: *mut u8,
            creation_flags: DWORD,
            thread_id: *mut DWORD,
        ) -> HANDLE;

        pub fn VirtualFree(
            address: *mut u8,
            alloc_size: usize,
            free_type: DWORD
        ) -> bool;
        pub fn VirtualAlloc(
            address: *mut u8,
            alloc_size: usize,
            alloc_type: DWORD,
            page_protect: DWORD,
        ) -> *mut u8;
        pub fn VirtualProtect(
            address: *mut u8,
            alloc_size: usize,
            new_protect: DWORD,
            old_protect: *mut DWORD,
        ) -> bool;
    }
}