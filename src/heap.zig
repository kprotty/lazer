const memory = @import("memory.zig");
const assert = @import("std").debug.assert;

pub const BEGIN = 1 << 35;
pub const END   = 1 << 36;
pub const CHUNK_SIZE = 2 * 1024 * 1024;

pub inline fn compressPtr(comptime T: type, ptr: T) u32 {
    const address = @ptrToInt(ptr);
    assert(isHeapAddress(address, 8));
    return @intCast(u32, (address - BEGIN) >> 3);
}

pub inline fn decompressPtr(comptime T: type, compressed: u32) T {
    return @intToPtr(T, (usize(compressed) << 3) + BEGIN);
} 

inline fn heapOffset(chunk_offset: u16) usize {
    return BEGIN + usize(chunk_offset) * CHUNK_SIZE;
}

inline fn isHeapAddress(address: usize, comptime alignment: usize) bool {
    return address % alignment == 0 and address >= BEGIN and address < END;
}

pub const HeapError = error {
    MapFailed,
    CommitFailed,
    OutOfMemory,
    InvalidAddress,
};

pub fn init() HeapError!void {
    const heap = @intToPtr(*Heap, heapOffset(0));

    if (memory.map(@ptrToInt(heap), Heap.HEAP_SIZE) == 0)
        return HeapError.MapFailed;
    if (!memory.commit(@ptrToInt(heap), CHUNK_SIZE))
        return HeapError.CommitFailed;

    heap.init();
}

pub fn alloc(chunks: u16) HeapError![*]u8 {
    const heap = @intToPtr(*Heap, heapOffset(0));

    if (heap.findBestFit(chunks)) |free_span| {
        return @intToPtr([*]u8, heapOffset(free_span));
    } else if (heap.bumpAllocate(chunks)) |span| {
        return @intToPtr([*]u8, heapOffset(span));
    } else {
        return HeapError.OutOfMemory;
    }
}

pub fn free(ptr: usize) HeapError!void {
    const heap = @intToPtr(*Heap, heapOffset(0));
    const span = @intCast(u16, (ptr - BEGIN) / CHUNK_SIZE);
    
    if (!isHeapAddress(ptr, CHUNK_SIZE) or heap.spans[span].allocated != 1)
        return HeapError.InvalidAddress;

    heap.coalesceRight(span);
    heap.coalesceLeft(span);
}

const Span = packed struct {
    prev: u16,
    next: u16,
    chunks: u16,
    allocated: u1,
    predecessor: u15,
};

const Heap = struct {
    pub const Self = @This();
    pub const HEAP_SIZE = END - BEGIN;
    pub const MAX_CHUNKS = HEAP_SIZE / CHUNK_SIZE;

    top_span: u16,
    top_chunk: u16,
    spans: [MAX_CHUNKS]Span,
    free_list: struct {
        head: u16,
        tail: u16,
    },
    
    pub fn init(self: *Self) void {
        self.top_chunk = 1;
        self.spans[0].chunks = 1;
        self.spans[0].allocated = 1;
    }

    pub fn findBestFit(self: *Self, chunks: u16) ?u16 {
        var free_span: u16 = 0;
        var span = self.free_list.head;
        
        while (span != 0) {
            if (self.spans[span].chunks >= chunks) {
                if (free_span == 0 or self.spans[span].chunks < self.spans[free_span].chunks)
                    free_span = span;
            }
            span = self.spans[span].next;
        }
        if (free_span == 0)
            return null;

        if (self.spans[free_span].chunks > chunks) {
            const left_over = free_span + chunks;
            self.spans[left_over].chunks = self.spans[free_span].chunks - chunks;
            self.spans[left_over].predecessor = @intCast(u15, free_span);
            self.markSpanFree(left_over);
        }
        
        self.spans[free_span].chunks = chunks;
        self.spans[free_span].allocated = 1;
        self.markSpanUsed(free_span);
        return free_span;
    }

    pub fn bumpAllocate(self: *Self, chunks: u16) ?u16 {
        if (self.top_chunk + chunks >= MAX_CHUNKS)
            return null;
        
        const span = self.top_chunk;
        self.spans[span].allocated = 1;
        self.spans[span].chunks = chunks;
        self.spans[span].predecessor = @intCast(u15, self.top_span);

        self.top_chunk += chunks;
        self.top_span = span;
        return span;
    }

    pub fn coalesceRight(self: *Self, span: u16) void {
        if (span != self.top_span) {
            var free_span = span + self.spans[span].chunks;
            while (free_span < self.top_chunk and self.spans[free_span].allocated == 0) {
                self.markSpanUsed(free_span);
                free_span += self.spans[free_span].chunks;
            }
        }
    }

    pub fn coalesceLeft(self: *Self, span: u16) void {
        var free_span = span;

        while (self.spans[free_span].predecessor != 0) {
            const predecessor = u16(self.spans[free_span].predecessor);
            if (self.spans[predecessor].allocated == 1)
                break;
            self.spans[predecessor].chunks += self.spans[free_span].chunks;
            self.markSpanUsed(predecessor);
            free_span = predecessor;
        }

        self.markSpanFree(free_span);
    }

    pub fn markSpanUsed(self: *Self, span: u16) void {
        const span_ref = &self.spans[span];

        if (self.free_list.head == span)
            self.free_list.head = span_ref.next;
        if (self.free_list.tail == span)
            self.free_list.tail = span_ref.next;

        if (span_ref.prev != 0)
            self.spans[span_ref.prev].next = span_ref.next;
        if (span_ref.next != 0)
            self.spans[span_ref.next].prev = span_ref.prev;
    }

    pub fn markSpanFree(self: *Self, span: u16) void {
        self.spans[span].next = 0;
        self.spans[span].allocated = 0;
        
        if (span + self.spans[span].chunks >= self.top_chunk) {
            self.top_span = u16(self.spans[span].predecessor);
            self.top_chunk = span;

        } else if (self.free_list.tail == 0) {
            self.spans[span].prev = 0;
            self.free_list.head = span;
            self.free_list.tail = span;

        } else {
            self.spans[self.free_list.tail].next = span;
            self.spans[span].prev = self.free_list.tail;
            self.free_list.tail = span;
        }
    }
};

test "Heap allocation" {
    try init();

    // test chunk allocation
    assert(@ptrToInt(try alloc(1)) == heapOffset(1));
    assert(@ptrToInt(try alloc(1)) == heapOffset(2));

    // check p1 reuse + bump allocation after p2
    const p1 = @ptrToInt(try alloc(2));
    const p2 = @ptrToInt(try alloc(1));
    try free(p1);

    assert(@ptrToInt(try alloc(1)) == p1);
    assert(@ptrToInt(try alloc(1)) == p1 + CHUNK_SIZE);
    assert(@ptrToInt(try alloc(1)) == p2 + CHUNK_SIZE);

    // check chunk splitting reuse
    const p3 = @ptrToInt(try alloc(4));
    try free(p3);
    assert(@ptrToInt(try alloc(1)) == p3);
    assert(@ptrToInt(try alloc(3)) == p3 + CHUNK_SIZE);
}