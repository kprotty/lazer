const memory = @import("os/memory.zig");
const assert = @import("std").debug.assert;

pub const BEGIN = 1 << 35;
pub const END   = 1 << 36;
pub const CHUNK_SIZE = 2 * 1024 * 1024;

inline pub fn compressPtr(comptime Ptr: type, ptr: Ptr) u32 {
    const addresss = @ptrToInt(ptr);
    assert(address % 8 == 0);
    assert(address >= BEGIN and address < END);
    return u32((address - BEGIN) >> 3);
}

inline pub fn decompressPtr(comptime Ptr: type, ptr: u32) Ptr {
    return @intToPtr(Ptr, (usize(ptr) << 3) + BEGIN);
}

pub const HeapError = error {
    MapFailed,
    OutOfMemory,
    CommitFailed,
    Uninitialized,
    InvalidAddress,
};

var heap_chunk_offset: u16 = 0;
var global_heap: ?*Heap = null;

pub fn init() HeapError!void {
    assert(global_heap == null);
    assert(heap_chunk_offset == 0);

    if (memory.map(BEGIN, END - BEGIN) != 0)
        return HeapError.MapFailed;
}

pub fn reserve(chunks: u16) HeapError![*]u8 {
    const address = heap_chunk_offset * CHUNK_SIZE;
    const next_offset = heap_chunk_offset + chunks;

    if (next_offset >= (END - BEGIN) / CHUNK_SIZE)
        return HeapError.OutOfMemory;

    heap_chunk_offset = next_offset;
    return @intToPtr([*]u8, address);
}

pub fn commit() HeapError!void {
    assert(global_heap == null);
    global_heap = @intToPtr(?*Heap, BEGIN + usize(heap_chunk_offset));

    if (!memory.commit(@ptrToInt(global_heap), CHUNK_SIZE))
        return HeapError.CommitFailed;
}

inline pub fn alloc(chunks: u16) HeapError![*]u8 {
    return (global_heap orelse return HeapError.Uninitialized).alloc(chunks);
}

inline pub fn free(ptr: usize) HeapError!void {
    return (global_heap orelse return HeapError.Uninitialized).free(ptr);
}

const Heap = struct {
    const Self = @This();

    pub fn alloc(self: *Self, chunks: u16) HeapError![*]u8 {

    }

    pub fn free(self: *Self, ptr: usize) HeapError![*]u8 {
        
    }
}