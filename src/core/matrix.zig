const std = @import("std");
const Allocator = std.mem.Allocator;
const core = @import("../index.zig");
const Coord = core.geometry.Coord;

pub fn Matrix(comptime T: type) type {
    return struct {
        const Self = @This();

        width: u16,
        height: u16,
        data: []T,

        pub fn initEmpty() Self {
            return Self{
                .width = 0,
                .height = 0,
                .data = &[_]T{},
            };
        }
        fn init(allocator: Allocator, width: u16, height: u16) !Self {
            return Self{
                .width = width,
                .height = height,
                .data = try allocator.alloc(T, width * height),
            };
        }
        pub fn initFill(allocator: Allocator, width: u16, height: u16, fill_value: T) !Self {
            var self = try Self.init(allocator, width, height);
            self.fill(fill_value);
            return self;
        }
        pub fn deinit(self: Self, allocator: Allocator) void {
            allocator.free(self.data);
        }

        pub fn clone(self: Self, allocator: Allocator) !Self {
            var other = try Self.init(allocator, self.width, self.height);
            std.mem.copy(T, other.data, self.data);
            return other;
        }

        pub fn fill(self: Self, fill_value: T) void {
            for (self.data) |*x| {
                x.* = fill_value;
            }
        }

        pub fn atCoord(self: Self, coord: Coord) ?*T {
            return self.at(coord.x, coord.y);
        }
        pub fn at(self: Self, x: i32, y: i32) ?*T {
            if (0 <= x and x < @as(i32, self.width) and
                0 <= y and y < @as(i32, self.height))
            {
                return self.atUnchecked(@intCast(usize, x), @intCast(usize, y));
            }
            return null;
        }
        pub fn atUnchecked(self: Self, x: usize, y: usize) *T {
            return &self.data[y * self.width + x];
        }

        pub fn getCoord(self: Self, coord: Coord) ?T {
            return self.get(coord.x, coord.y);
        }
        pub fn get(self: Self, x: i32, y: i32) ?T {
            if (0 <= x and x < @as(i32, self.width) and
                0 <= y and y < @as(i32, self.height))
            {
                return self.getUnchecked(@intCast(usize, x), @intCast(usize, y));
            }
            return null;
        }
        pub fn getUnchecked(self: Self, x: usize, y: usize) T {
            return self.atUnchecked(x, y).*;
        }

        pub fn indexToCoord(self: Self, index: usize) Coord {
            return Coord{
                .x = @intCast(i32, index % self.width),
                .y = @intCast(i32, index / self.width),
            };
        }

        pub fn copy(dest: Self, source: []const T, source_width: u16, source_height: u16, dx: u16, dy: u16, sx: u16, sy: u16, width: u16, height: u16) void {
            std.debug.assert(source.len == source_width * source_height);
            std.debug.assert(dx + width <= dest.width);
            std.debug.assert(dy + height <= dest.height);
            std.debug.assert(sx + width <= source_width);
            std.debug.assert(sy + height <= source_height);

            var y: u16 = 0;
            while (y < height) : (y += 1) {
                var x: u16 = 0;
                while (x < width) : (x += 1) {
                    const get_x = sx + x;
                    const get_y = sy + y;
                    dest.atUnchecked(dx + x, dy + y).* = source[get_y * source_width + get_x];
                }
            }
        }
    };
}

test "Matrix" {
    var m = try Matrix(u8).initFill(std.testing.allocator, 3, 2, 0);
    defer m.deinit(std.testing.allocator);
    try std.testing.expect(m.at(-1, 0) == null);
    try std.testing.expect(m.get(0, -1) == null);
    try std.testing.expect(m.get(3, 0) == null);
    try std.testing.expect(m.at(0, 2) == null);

    try std.testing.expect(m.at(0, 0).?.* == 0);
    try std.testing.expect(m.at(2, 1).?.* == 0);
    try std.testing.expect(m.get(1, 1).? == 0);
    m.atUnchecked(1, 1).* = 3;
    try std.testing.expect(m.at(1, 1).?.* == 3);

    var other = try m.clone(std.testing.allocator);
    defer other.deinit(std.testing.allocator);
    try std.testing.expect(other.at(0, 0).?.* == 0);
    try std.testing.expect(other.get(1, 1).? == 3);
    m.atUnchecked(1, 1).* = 4;
    try std.testing.expect(m.get(1, 1).? == 4);
    try std.testing.expect(other.get(1, 1).? == 3);
}

pub fn SparseChunkedMatrix(comptime T: type, comptime default_value: T) type {
    return struct {
        chunks: std.AutoArrayHashMap(Coord, *[chunk_side_length * chunk_side_length]T),

        const chunk_shift = 4;
        const chunk_side_length = 1 << chunk_shift;
        const chunk_mask = (1 << chunk_shift) - 1;

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

        pub fn putCoord(self: *@This(), coord: Coord, v: T) !void {
            return self.put(coord.x, coord.y, v);
        }
        pub fn put(self: *@This(), x: i32, y: i32, v: T) !void {
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
