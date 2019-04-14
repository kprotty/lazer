use core::marker::PhantomData;

/// Lazer memory-maps a fixed 32gb of heap and allocates chunks off of it at a 2mb granularity.
/// This is because 32bg worth of address space is 35 bits. When addresses are aligned by 8 bytes
/// the bottom 3 bits are always zero so a  pointer in this space can be compressed down to
/// 35 - 3 = 32 bits and fit nicely inside a `u32`. This helps in saving memory elsewhere.
pub const HEAP_BEGIN: usize = 1 << 35;
pub const HEAP_END:   usize = 1 << 36;
pub const HEAP_SIZE:  usize = HEAP_END - HEAP_BEGIN;
pub const CHUNK_SIZE: usize = 2 * 1024 * 1024;

pub struct Ptr<T> {
    compressed: u32,
    marker: PhantomData<T>,
}

impl<T> Ptr<T> {
    pub fn zip(ptr: *mut T) -> Self {
        // ensure that its 8-byte aligned & lives on the lazer heap
        let ptr = ptr as usize;
        debug_assert!(
            ptr % 8 == 0 &&
            ptr < HEAP_END &&
            ptr >= HEAP_BEGIN
        );

        Self {
            marker: PhantomData,
            compressed: (ptr / 8 - HEAP_BEGIN) as u32,
        }
    }

    pub fn unzip(&self) -> *mut T {
        (self.compressed as usize * 8 + HEAP_BEGIN) as *mut T
    }
}