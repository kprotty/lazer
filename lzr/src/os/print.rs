use super::{ffi::*, Mutex};

use core::fmt;
use core::marker::{Send, Sync};
use core::sync::atomic::{AtomicBool, ATOMIC_BOOL_INIT, Ordering, spin_loop_hint};

macro_rules! impl_console {
    ($d:tt $print:ident $println:ident $name:ident $init:ident) => {
        pub static mut $name: ConsoleOutput = ConsoleOutput::$init();

        #[inline]
        pub fn $print(args: fmt::Arguments) {
            let _ = fmt::write(unsafe { &mut $name }, args);
        }

        #[cfg_attr(not(test), macro_export)]
        macro_rules! $print {
            ($d($d args:tt)*) => 
                ($crate::os::print::$print(format_args!($d($d args)*)));
        }

        #[cfg_attr(not(test), macro_export)]
        macro_rules! $println {
            () => ($print!("\n"));
            ($d($d args:tt)*) => ($print!("{}\n", format_args!($d($d args)*)));
        }
    };
}

const CONSOLE_BUFFER_SIZE: usize = 4096;
impl_console!($ print  println  STDOUT stdout);
impl_console!($ eprint eprintln STDERR stderr);

pub struct ConsoleOutput {
    handle: usize,
    buffer_top: usize,
    created: AtomicBool,
    buffer: Mutex<[u8; CONSOLE_BUFFER_SIZE]>,
}

unsafe impl Send for ConsoleOutput {}
unsafe impl Sync for ConsoleOutput {}

impl fmt::Write for ConsoleOutput {
    fn write_str(&mut self, string: &str) -> fmt::Result {
        unsafe { self.write(string.as_bytes()); }
        Ok(())
    }
}

impl ConsoleOutput {
    #[inline]
    pub const fn stderr() -> Self {
        Self::new(core::usize::MAX - 1)
    }

    #[inline]
    pub const fn stdout() -> Self {
        Self::new(core::usize::MAX)
    }

    #[inline]
    pub const fn new(handle: usize) -> Self {
        Self {
            buffer_top: 0,
            handle: handle,
            created: ATOMIC_BOOL_INIT,
            buffer: Mutex::new([0; CONSOLE_BUFFER_SIZE]),
        }
    }

    #[inline]
    unsafe fn should_flush(&self, buffer: &[u8], input: &[u8]) -> bool {
        let input_ptr = input.as_ptr() as *const _;
        self.buffer_top + input.len() > buffer.len() ||
        !memchr(input_ptr, b'\n' as _, input.len()).is_null()
    }

    #[inline]
    unsafe fn ensure_handle_created(&mut self) {
        const ORDER: Ordering = Ordering::Relaxed;
        if self.created.compare_exchange_weak(false, true, ORDER, ORDER).is_ok() {
            self.handle = match self.handle {
                core::usize::MAX => Self::get_stdout_handle(),
                _ => Self::get_stderr_handle(),
            };
        }

        while self.handle >= core::usize::MAX - 1 {
            spin_loop_hint();
        }
    }

    #[inline]
    unsafe fn write(&mut self, input: &[u8]) {
        if input.len() < 1 {
            return
        }

        self.ensure_handle_created();
        let mut buffer = self.buffer.lock();

        if self.should_flush(&* buffer, input) {
            self.flush(&buffer[0..self.buffer_top], input);
            self.buffer_top = 0;
        } else {
            let offset = buffer.get_unchecked_mut(self.buffer_top);
            memcpy(offset, input.as_ptr(), input.len());
            self.buffer_top += input.len();
        }
    }
}

#[cfg(target_os = "windows")]
impl ConsoleOutput {
    #[inline]
    unsafe fn get_stderr_handle() -> usize {
        GetStdHandle(STD_ERROR_HANDLE) as usize
    }

    #[inline]
    unsafe fn get_stdout_handle() -> usize {
        GetStdHandle(STD_OUTPUT_HANDLE) as usize
    }

    #[inline]
    unsafe fn flush_buffer(handle: HANDLE, ptr: *const u8, len: usize) {
        WriteConsoleA(handle, ptr as *mut _, len as _, null_mut(), null_mut());
    }

    #[inline]
    unsafe fn flush(&self, buffer: &[u8], string: &[u8]) {
        Self::flush_buffer(self.handle as _, buffer.as_ptr(), buffer.len());
        Self::flush_buffer(self.handle as _, string.as_ptr(), string.len());
    }
}

#[cfg(target_os = "linux")]
impl ConsoleOutput {
    #[inline]
    unsafe fn get_stderr_handle() -> usize {
        STDERR_FILENO as usize
    }

    #[inline]
    unsafe fn get_stdout_handle() -> usize {
        STDOUT_FILENO as usize
    }

    #[inline]
    unsafe fn flush(&self, buffer: &[u8], string: &[u8]) {
        let iovecs = [
            iovec {
                iov_len: buffer.len(),
                iov_base: buffer.as_ptr() as *mut _,
            },
            iovec {
                iov_len: string.len(),
                iov_base: string.as_ptr() as *mut _,
            },
        ];
        writev(self.handle as _, iovecs.as_ptr(), iovecs.len() as c_int);
    }
}