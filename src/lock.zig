const builtin = @import("builtin");
const assert = @import("std").debug.assert;

const Op = builtin.AtomicRmwOp;
const Order = builtin.AtomicOrder;

pub inline fn cpuPause() void {
    asm volatile("pause" ::: "memory");
}

pub const Spinlock = struct {
    const Self = @This();
    lock: u8,

    pub fn new() Self {
        return Self { .lock = 0 };
    }

    pub fn acquire(self: *Self) void {
        assert(@atomicLoad(u8, &self.lock, Order.Unordered) == 0);
        while (@atomicRmw(u8, &self.lock, Op.Xchg, 1, Order.SeqCst) != 0)
            cpuPause();
    }

    pub fn release(self: *Self) void {
        assert(@atomicRmw(u8, &self.lock, Op.Xchg, 0, Order.SeqCst) == 1);
    }
};