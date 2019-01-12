use core::panic::PanicInfo;

#[panic_handler]
fn panic_handler(_info: &PanicInfo) -> ! {
    unsafe { libc::exit(1) }
}

#[cfg(unix)]
#[no_mangle]
pub extern fn _Unwind_Resume() {}

#[cfg(windows)]
#[no_mangle]
pub extern fn rust_eh_unwind_resume() {}

#[no_mangle]
pub extern fn rust_eh_personality() {}

