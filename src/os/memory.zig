const std = @import("std");
const builtin = @import("builtin");

const linux = std.os.linux;
const windows = builtin.os == builtin.Os.windows;

const MADV_DONTNEED = 4;
const MEM_COMMIT = 0x1000;
const MEM_RESERVE = 0x2000;
const MEM_DECOMMIT = 0x4000;
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

test "memory mapping" {
    const assert = @import("std").debug.assert;
    const memory = @intToPtr([*]u8, map(1 << 32, 4096));
    
    assert(commit(@ptrToInt(memory), 4096));
    defer assert(decommit(@ptrToInt(memory), 4096));

    memory[0] = 0xff;
    assert(memory[0] == 0xff);
}