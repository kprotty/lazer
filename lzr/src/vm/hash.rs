use super::os::ffi::*;
use core::ops::BitXor;

pub type Hash = u32;

pub trait Hashable {
    fn hashed(&self) -> Hash;
}

impl Hashable for str {
    fn hashed(&self) -> Hash {
        self.as_bytes().hashed()
    }
}

impl Hashable for f64 {
    fn hashed(&self) -> Hash {
        unsafe {
            let rounded = core::intrinsics::roundf64(*self);
            splitmix_hash(transmute(rounded as i64))
        }
    }
}

impl Hashable for [u8] {
    fn hashed(&self) -> Hash {
        if self.len() <= 256 as usize {
            fnv1a_hash(self)
        } else {
            fx_hash(self)
        }
    }
}

#[inline]
fn splitmix_hash(mut input: u64) -> Hash {
    const SEED1: u64 = 0xbf58476d1ce4e5b9;
    const SEED2: u64 = 0x94d049bb133111eb;

    input = input.bitxor(input.wrapping_shr(30)).wrapping_mul(SEED1);
    input = input.bitxor(input.wrapping_shr(27)).wrapping_mul(SEED2);
    input.bitxor(input.wrapping_shr(31)) as Hash
}

#[inline]
fn fnv1a_hash(input: &[u8]) -> Hash {
    const FNV_PRIME: Hash = 0x1000193;
    const FNV_OFFSET: Hash = 0x811c9dc5;

    input.iter().fold(
        FNV_OFFSET, |hash, byte|
            hash.bitxor(*byte as Hash).wrapping_mul(FNV_PRIME))
}

#[inline]
fn fx_hash(mut input: &[u8]) -> Hash {
    #[cfg(target_pointer_width = "32")]
    const SEED: usize = 0x27220a95;
    #[cfg(target_pointer_width = "64")]
    const SEED: usize = 0x517cc1b727220a95;

    let mut hash = 0usize;
    macro_rules! fx_hash_loop {
        ($type:ty) => {
            while input.len() >= size_of::<$type>() {
                let word = unsafe { *(input.as_ptr() as *const $type) as usize };
                hash = hash.rotate_left(5).bitxor(word).wrapping_mul(SEED);
                input = input.split_at(size_of::<$type>()).1;
            }
        };
    }
    
    fx_hash_loop!(usize);
    #[cfg(target_pointer_width = "64")]
    fx_hash_loop!(u32);
    fx_hash_loop!(u8);
    
    hash as Hash
}