const std = @import("std");
const Allocator = std.mem.Allocator;
const core = @import("../index.zig");
const Coord = core.geometry.Coord;

pub fn SparseChunkedMatrix(comptime T: type, comptime default_value: T) type {
    return struct {
        chunks: std.AutoArrayHashMap(Coord, *[chunk_side_length * chunk_side_length]T),
        metrics: Metrics = .{},

        const chunk_shift = 4;
        const chunk_side_length = 1 << chunk_shift;
        const chunk_mask = (1 << chunk_shift) - 1;

        const Metrics = struct {
            min_x: i32 = 0,
            min_y: i32 = 0,
            max_x: i32 = 0,
            max_y: i32 = 0,
        };

        pub fn init(allocator: Allocator) @This() {
            return .{
                .chunks = std.AutoArrayHashMap(Coord, *[chunk_side_length * chunk_side_length]T).init(allocator),
            };
        }
        pub fn deinit(self: *@This()) void {
            var it = self.chunks.iterator();
            while (it.next()) |entry| {
                self.chunks.allocator.free(entry.value_ptr.*);
            }
            self.chunks.deinit();
        }

        pub fn clone(self: @This(), allocator: Allocator) !@This() {
            var other = init(allocator);
            try other.chunks.ensureTotalCapacity(self.chunks.count());
            var it = self.chunks.iterator();
            while (it.next()) |entry| {
                other.chunks.putAssumeCapacity(entry.key_ptr.*, //
                    @ptrCast(*[chunk_side_length * chunk_side_length]T, //
                    (try allocator.dupe(T, entry.value_ptr.*)) //
                    .ptr));
            }
            return other;
        }

        pub fn copyFromSlice(dest: *@This(), source: []const T, source_width: u16, source_height: u16, dx: i32, dy: i32, sx: u16, sy: u16, width: u16, height: u16) !void {
            std.debug.assert(source.len == source_width * source_height);
            std.debug.assert(sx + width <= source_width);
            std.debug.assert(sy + height <= source_height);
            var y: u16 = 0;
            while (y < height) : (y += 1) {
                var x: u16 = 0;
                while (x < width) : (x += 1) {
                    const get_x = sx + x;
                    const get_y = sy + y;
                    try dest.put(dx + x, dy + y, source[get_y * source_width + get_x]);
                }
            }
        }

        pub fn putCoord(self: *@This(), coord: Coord, v: T) !void {
            return self.put(coord.x, coord.y, v);
        }
        pub fn put(self: *@This(), x: i32, y: i32, v: T) !void {
            if (self.chunks.count() == 0) {
                // first put
                self.metrics.min_x = x;
                self.metrics.min_y = y;
                self.metrics.max_x = x;
                self.metrics.max_y = y;
            } else {
                if (x < self.metrics.min_x) self.metrics.min_x = x;
                if (y < self.metrics.min_y) self.metrics.min_y = y;
                if (x > self.metrics.max_x) self.metrics.max_x = x;
                if (y > self.metrics.max_y) self.metrics.max_y = y;
            }

            const chunk_coord = Coord{
                .x = x >> chunk_shift,
                .y = y >> chunk_shift,
            };
            const inner_index = @intCast(usize, (y & chunk_mask) * chunk_side_length + (x & chunk_mask));
            const gop = try self.chunks.getOrPut(chunk_coord);
            if (!gop.found_existing) {
                gop.value_ptr.* = @ptrCast(*[chunk_side_length * chunk_side_length]T, //
                    (try self.chunks.allocator.alloc(T, chunk_side_length * chunk_side_length)) //
                    .ptr);
                std.mem.set(T, gop.value_ptr.*, default_value);
            }

            gop.value_ptr.*[inner_index] = v;
        }

        pub fn getCoord(self: @This(), coord: Coord) T {
            return self.get(coord.x, coord.y);
        }
        pub fn get(self: @This(), x: i32, y: i32) T {
            const chunk_coord = Coord{
                .x = x >> chunk_shift,
                .y = y >> chunk_shift,
            };
            const inner_index = @intCast(usize, (y & chunk_mask) * chunk_side_length + (x & chunk_mask));
            const chunk = self.chunks.get(chunk_coord) orelse return default_value;

            return chunk[inner_index];
        }
    };
}

test "SparseChunkedMatrix" {
    var m = SparseChunkedMatrix(u8, 42).init(std.testing.allocator);
    defer m.deinit();

    try std.testing.expect(m.get(0, 0) == 42);
    try m.put(0, 0, 1);
    try std.testing.expect(m.get(0, 0) == 1);
    try m.put(-9000, 1234, 2);
    try m.put(1, 0, 3);
    try std.testing.expect(m.get(0, 0) == 1);
    try std.testing.expect(m.get(1, 0) == 3);
    try std.testing.expect(m.get(0, 1) == 42);
    try std.testing.expect(m.get(-9000, 1234) == 2);

    var other = try m.clone(std.testing.allocator);
    defer other.deinit();
    try std.testing.expect(other.get(-9000, 1234) == 2);
    try std.testing.expect(other.get(0, 0) == 1);
    try other.put(0, 0, 4);
    try std.testing.expect(other.get(0, 0) == 4);
    try std.testing.expect(m.get(0, 0) == 1);
}
