const builtin = @import("builtin");
const linux = @import("std").os.linux;
const onWindows = builtin.os == builtin.Os.windows;

const MADV_WILLNEED = 3;
const MADV_DONTNEED = 4;

const MEM_COMMIT = 0x1000;
const MEM_RESERVE = 0x2000;
const MEM_DECOMMIT = 0x4000;
const PAGE_READWRITE = 0x04;

extern "kernel32" stdcallcc fn VirtualFree(address: usize, bytes: usize, freeType: u32) u32;
extern "kernel32" stdcallcc fn VirtualAlloc(address: usize, bytes: usize, allocType: u32, protect: u32) usize;

pub const MapError = error {
    MapFailed,
    CommitFailed,
    DecommitFailed,
};

pub fn map(comptime T: type, address: usize, amount: usize) ![*]T {
    const bytes = @sizeOf(T) * amount;
    const protect = linux.PROT_READ | linux.PROT_WRITE;
    const flags = linux.MAP_FIXED | linux.MAP_PRIVATE | linux.MAP_ANONYMOUS;

    const addr =
        if (onWindows) VirtualAlloc(address, bytes, MEM_RESERVE, PAGE_READWRITE)
        else linux.syscall6(linux.SYS_mmap, address, bytes, protect, flags, -1, 0);

    return
        if (addr == address) @intToPtr([*]T, addr)
        else MapError.MapFailed;
}

pub fn commit(comptime T: type, address: [*]T, amount: usize) !void {
    const addr = @ptrToInt(address);
    const bytes = @sizeOf(T) * amount;
    
    if (
        if (onWindows) VirtualAlloc(addr, bytes, MEM_COMMIT, PAGE_READWRITE) != addr
        else false // linux.syscall4(linux.SYS_madvise, addr, bytes, MADV_WILLNEED) != 0
    ) return MapError.CommitFailed;
}

pub fn decommit(comptime T: type, address: [*]T, amount: usize) !void {
    const addr = @ptrToInt(address);
    const bytes = @sizeOf(T) * amount;

    if (
        if (onWindows) VirtualFree(addr, bytes, MEM_DECOMMIT) == 0
        else linux.syscall3(linux.SYS_madvise, addr, bytes, MADV_DONTNEED) != 0
    ) return MapError.DecommitFailed;
}

test "memory mapping" {
    const assert = @import("std").debug.assert;
    const memory = try map(u8, 1 << 32, 4096);

    try commit(u8, memory, 4096);
    memory[0] = 0xff;
    assert(memory[0] == 0xff);
    try decommit(u8, memory, 4096);
}