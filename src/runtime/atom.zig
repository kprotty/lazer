const std = @import("std");
const assert = std.debug.assert;
const heap = @import("../platform/heap.zig");
const memory = @import("../platform/memory.zig");
const hashBytes = @import("../platform/hash.zig").hashBytes;

pub const AtomError = heap.HeapError || error {
    OutOfAtoms,
    InvalidAtom,
    AtomCommitFailed,
    AtomDecommitFailed,
};

pub const Atom = packed struct {
    const Self = @This();
    pub const MAX_SIZE = 255;

    hash: u32,
    data: u32,
    len: u8,

    pub fn text(self: *const Self) []const u8 {
        const text_offset = @intToPtr([*]u8, @ptrToInt(self) + @sizeOf(Self));
        return memory.ptrToSlice(u8, text_offset, self.len);
    }
};

pub const AtomTable = struct {
    const LOAD_FACTOR = 90;

    size: u32,
    mask: u32,
    capacity: u32,
    cells: [*]AtomCell,
    remap: [*]AtomCell,
    atom_memory: AtomMemory,

    const AtomCell = struct {
        atom_ptr: u32,
        displacement: u32,
    };

    pub fn new(max_atoms: u32, comptime premapped: comptime_int) AtomError! AtomTable {
        assert(max_atoms >= premapped);
        const real_max_atoms = memory.nextPow2(max_atoms);

        const num_bytes = memory.aligned(real_max_atoms * 2 * @sizeOf(AtomCell), heap.CHUNK_SIZE);
        const num_chunks = @truncate(u16, num_bytes / heap.CHUNK_SIZE);

        const cell_memory = memory.ptrCast([*]AtomCell, try heap.alloc(num_chunks));
        const atom_memory = try AtomMemory.new(real_max_atoms);
        if (!memory.commit(@ptrToInt(cell_memory), premapped * @sizeOf(AtomCell)))
            return AtomError.AtomCommitFailed;

        return AtomTable {
            .size = 0,
            .mask = premapped - 1,
            .capacity = max_atoms,
            .atom_memory = atom_memory,
            .cells = cell_memory,
            .remap = cell_memory + max_atoms,
        };
    }

    pub fn find(self: *const AtomTable, atom_name: []const u8) AtomError! ?*const Atom {
        if (atom_name.len > Atom.MAX_SIZE)
            return AtomError.InvalidAtom;
        return self.findAtom(atom_name, hashBytes(atom_name));
    }

    pub fn findOrCreate(self: *AtomTable, atom_name: []const u8) AtomError! *const Atom {
        if (atom_name.len > Atom.MAX_SIZE)
            return AtomError.InvalidAtom;

        const hash = hashBytes(atom_name);
        if (self.findAtom(atom_name, hash)) |atom| {
            return atom;
        }

        const atom = try self.atom_memory.allocAtom(atom_name, hash);
        if (self.size + 1 >= (LOAD_FACTOR * (self.mask + 1)) / 100)
            try self.growTable();
        self.insertAtom(heap.compressPtr(*const Atom, atom));
        return atom;
    }

    fn findAtom(self: *const AtomTable, atom_name: []const u8, hash: u32) ?*const Atom {
        var displacement: usize = 0;
        var index = hash & self.mask;

        while (true) {
            const cell = &self.cells[index];
            const cell_atom = heap.decompressPtr(*const Atom, cell.atom_ptr);

            if (cell.atom_ptr == 0 or displacement > cell.displacement)
                return null;
            if (cell_atom.hash == hash and std.mem.eql(u8, cell_atom.text(), atom_name))
                return cell_atom;
            
            displacement += 1;
            index = (index + 1) & self.mask;
        }
    }

    fn insertAtom(self: *AtomTable, atom_ptr: u32) void {
        var index = heap.decompressPtr(*const Atom, atom_ptr).hash & self.mask;
        var current_cell = AtomCell {
            .displacement = 0,
            .atom_ptr = atom_ptr,
        };

        while (true) {
            const cell = &self.cells[index];
            if (cell.atom_ptr == 0) {
                self.size += 1;
                cell.* = current_cell;
                return;
            } else if (cell.displacement < current_cell.displacement) {
               const temp = current_cell;
               current_cell = cell.*;
               cell.* = temp; 
            }

            current_cell.displacement += 1;
            index = (index + 1) & self.mask;
        }
    }

    fn growTable(self: *AtomTable) AtomError!void {
        const old_cells = memory.ptrToSlice(AtomCell, self.cells, self.mask + 1);
        const new_cells = memory.ptrToSlice(AtomCell, self.remap, old_cells.len * 2);
        if (new_cells.len > self.capacity)
            return AtomError.OutOfAtoms;

        self.size = 0;
        self.cells = new_cells.ptr;
        self.mask = @truncate(u32, new_cells.len - 1);
        if (!memory.commit(@ptrToInt(new_cells.ptr), new_cells.len * @sizeOf(AtomCell)))
            return AtomError.AtomCommitFailed;

        for (old_cells) |old_cell| {
            if (old_cell.atom_ptr != 0)
                self.insertAtom(old_cell.atom_ptr);
        }

        self.remap = old_cells.ptr;
        if (!memory.decommit(@ptrToInt(old_cells.ptr), old_cells.len * @sizeOf(AtomCell)))
            return AtomError.AtomDecommitFailed;
    }

    const AtomMemory = struct {
        const COMMIT_CHUNK_SIZE = 1 * 1024 * 1024;
        const MAX_ATOM_SIZE = @sizeOf(Atom) + Atom.MAX_SIZE;

        commit_at: usize,
        memory_top: usize,
        memory_end: usize,

        pub fn new(max_atoms: usize) AtomError! AtomMemory {
            const num_bytes = memory.aligned(max_atoms * MAX_ATOM_SIZE, heap.CHUNK_SIZE);
            const num_chunks = @truncate(u16, num_bytes / heap.CHUNK_SIZE);

            const atom_memory = @ptrToInt(try heap.alloc(num_chunks));
            if (!memory.commit(atom_memory, COMMIT_CHUNK_SIZE))
                return AtomError.AtomCommitFailed;

            return AtomMemory {
                .memory_top = atom_memory,
                .memory_end = atom_memory + num_bytes,
                .commit_at  = atom_memory + COMMIT_CHUNK_SIZE,
            };
        }

        pub fn allocAtom(self: *AtomMemory, atom_name: []const u8, hash: u32) AtomError! *const Atom {
            const new_top = self.memory_top + memory.aligned(@sizeOf(Atom) + atom_name.len, 8);
            const atom = @intToPtr(*Atom, self.memory_top);

            if (new_top > self.memory_end)
                return AtomError.OutOfAtoms;
            if (new_top > self.commit_at)
                try self.ensureCommitted();

            atom.data = 0;
            atom.hash = hash;
            atom.len = @truncate(u8, atom_name.len);
            @memcpy(memory.ptrCast([*]u8, atom.text().ptr), atom_name.ptr, atom_name.len);

            return atom;
        }

        fn ensureCommitted(self: *AtomMemory) AtomError! void {
            var commit_bytes = self.memory_end - self.commit_at;
            if (commit_bytes > COMMIT_CHUNK_SIZE)
                commit_bytes = COMMIT_CHUNK_SIZE;
            if (!memory.commit(self.commit_at, commit_bytes))
                return AtomError.AtomCommitFailed;
            self.commit_at += commit_bytes;
        }
    };
};

test "atom table" {
    try heap.init();
    defer heap.deinit();

    var table = try AtomTable.new(1024, 4);
    const tests = [][]const u8 {
        "nil",
        "true",
        "false",
        "Kernel.hd/3",
        "Base.decode64",
    };

    for (tests) |test_atom| {
        assert((try table.find(test_atom)) == null);

        var atom = try table.findOrCreate(test_atom);
        assert(std.mem.eql(u8, atom.text(), test_atom));
        
        atom = (try table.find(test_atom)) orelse { assert(false); return; };
        assert(std.mem.eql(u8, atom.text(), test_atom));
    }
    
}