#![no_std]
#![feature(
    start,
    lang_items,
    core_intrinsics,
)]

#[cfg(windows)]
extern crate winapi;
#[cfg(unix)]
extern crate libc;

mod panic;
mod runtime;
mod compiler;

#[start]
fn start(_argc: isize, _argv: *const *const u8) -> isize {
    0
}