use core::panic::PanicInfo;
use core::intrinsics::abort;

#[lang = "eh_personality"]
pub extern fn rust_eh_personality() {}

#[lang = "eh_unwind_resume"]
pub extern fn rust_eh_unwind_resume() {}

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    unsafe { abort() }
}