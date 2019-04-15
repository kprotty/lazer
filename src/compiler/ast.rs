use core::mem::transmute;

#[repr(u8)]
#[derive(Copy, Clone)]
pub enum ExprKind {
    Id,
    Number,

    Func,
    Unary,
    Binary,
}

#[derive(Copy, Clone)]
pub struct ExprTag(u32);

impl ExprTag {
    pub fn new(kind: ExprKind, value: u32) -> Self {
        debug_assert!(value <= 0xffffff);
        Self(((kind as u32) << 24) | value)
    }

    pub fn kind(&self) -> ExprKind {
        unsafe { transmute((self.0 >> 24) as u8) }
    }

    pub fn value(&self) -> u32 {
        self.0 & 0xffffff
    }
}