const std = @import("std");
const builtin = @import("builtin");

const linux = std.os.linux;
const windows = builtin.os == builtin.Os.windows;

const MADV_DONTNEED = 4;
const MEM_COMMIT = 0x1000;
const MEM_RESERVE = 0x2000;
const MEM_DECOMMIT = 0x4000;
const MEM_RELEASE = 0x8000;
const PAGE_READWRITE = 0x04;
const INVALID = std.math.maxInt(usize);

extern "kernel32" stdcallcc fn VirtualFree(address: usize, bytes: usize, free_type: u32) bool;
extern "kernel32" stdcallcc fn VirtualAlloc(address: usize, bytes: usize, alloc_type: u32, protect: u32) usize;

pub fn map(address: usize, bytes: usize) usize {
    if (windows)
        return VirtualAlloc(address, bytes, MEM_RESERVE, PAGE_READWRITE);

    const prot = linux.PROT_READ | linux.PROT_WRITE;
    const flags = linux.MAP_FIXED | linux.MAP_PRIVATE | linux.MAP_ANONYMOUS;
    const addr = linux.syscall6(linux.SYS_mmap, address, bytes, prot, flags, INVALID, 0);

    return if (addr == address) address else 0;
}

pub fn unmap(address: usize, bytes: usize) bool {
    if (windows)
        return VirtualFree(address, 0, MEM_RELEASE);
    return linux.syscall2(linux.SYS_munmap, address, bytes) == 0;
}

pub fn commit(address: usize, bytes: usize) bool {
    if (windows)
        return VirtualAlloc(address, bytes, MEM_COMMIT, PAGE_READWRITE) == address;
    return true; // linux overcommits by default
}

pub fn decommit(address: usize, bytes: usize) bool {
    if (windows)
        return VirtualFree(address, bytes, MEM_DECOMMIT);
    return linux.syscall3(linux.SYS_madvise, address, bytes, MADV_DONTNEED) == 0;
}

pub inline fn ptrCast(comptime To: type, from: var) To {
    return @intToPtr(To, @ptrToInt(from));
}

pub fn aligned(value: usize, comptime alignment: usize) usize {
    return value + (alignment - (value % alignment));
}

pub fn nextPow2(value: u32) u32 {
    const shift: u32 = 32 - @clz(value - 1);
    return u32(1) << @truncate(u5, shift);
}

pub fn ptrToSlice(comptime T: type, ptr: [*]T, size: usize) []T {
    // WARNING: zig hack
    // this relies on the structure layout of a slice since theres no way to unsafe-ly create one
    return @bitCast([]T, [2]usize { @ptrToInt(ptr), size });
}

test "memory mapping" {
    const assert = @import("std").debug.assert;
    
    const memory = @intToPtr([*]u8, map(1 << 32, 4096));
    defer assert(unmap(1 << 32, 4096));
    
    assert(commit(@ptrToInt(memory), 4096));
    defer assert(decommit(@ptrToInt(memory), 4096));

    memory[0] = 0xff;
    assert(memory[0] == 0xff);
}