#![cfg_attr(not(test), no_std)]

#![feature(
    start,
    lang_items,
    try_blocks,
    core_intrinsics,
    optin_builtin_traits,
)]

#[cfg(target_os="windows")]
extern crate winapi;
#[cfg(target_os="linux")]
extern crate libc;
#[cfg(not(test))]
mod panic;

mod runtime;
mod platform;
mod compiler;

#[cfg(not(any(
    all(target_os="linux", any(target_arch="x86_64")),
    all(target_os="windows", any(target_arch="x86_64")),
)))] compile_error!("Platform not supported");

#[cfg(not(test))]
#[start]
fn start(_argc: isize, _argv: *const *const u8) -> isize {
    0
}