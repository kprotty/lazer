#![no_std]
#![no_main]

#[macro_use]
extern crate lzr;

mod panic;

#[no_mangle]
pub extern fn main(_argc: i32, _argv: *const *const u8) -> i32 {
    println!("Hello world\n");
    0
}