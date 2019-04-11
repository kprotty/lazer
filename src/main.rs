#![no_std]
#![feature(
    start,
    lang_items,
    core_intrinsics,
)]

#[cfg(target_os = "windows")]
extern crate winapi;
#[cfg(target_os = "linux")]
extern crate libc;

mod panic;
mod runtime;
mod platform;
mod compiler;

#[cfg(not(all(
    any(target_os = "windows", target_os = "linux"),
    any(target_arch = "x86_64", target_arch = "aarch64"),
)))] compile_error!("Platform not supported");

#[start]
fn start(_argc: isize, _argv: *const *const u8) -> isize {
    0
}