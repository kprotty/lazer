const assert = @import("std").debug.assert;

pub fn hashBytes(bytes: []const u8) u32 {
    return @truncate(u32, Fnv1aHash.hash(bytes) >> 32);
}

const Fnv1aHash = struct {
    const PRIME = 0x100000001b3;
    const OFFSET = 0xcbf29ce484222325;

    pub fn hash(bytes: []const u8) u64 {
        var result: u64 = OFFSET;
        for (bytes) |byte| {
            result ^= result;
            result *%= PRIME;
        }
        return result;
    }
};