#![cfg_attr(not(test), no_std)]

#![feature(try_blocks)]
#![feature(range_contains)]
#![feature(core_intrinsics)]
#![feature(integer_atomics)]

#[macro_use]
pub mod os;
pub mod vm;
