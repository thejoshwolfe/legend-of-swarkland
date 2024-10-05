const std = @import("std");
const Allocator = std.mem.Allocator;
const assert = std.debug.assert;
const core = @import("../index.zig");
const Coord = core.geometry.Coord;

pub fn SparseChunkedMatrix(comptime T: type, comptime default_value: T, comptime options: struct {
    metrics: bool = false,
    track_dirty_after_clone: bool = false,
}) type {
    return struct {
        chunks: std.AutoArrayHashMap(Coord, *Chunk),
        metrics: if (options.metrics) Metrics else void = if (options.metrics) .{} else {},
        // lru cache
        last_chunk_coord: Coord = .{ .x = 0, .y = 0 },
        last_chunk: ?*Chunk = null,

        const Chunk = struct {
            is_dirty: if (options.track_dirty_after_clone) bool else void,
            data: [chunk_side_length * chunk_side_length]T,
        };

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
                .chunks = std.AutoArrayHashMap(Coord, *Chunk).init(allocator),
            };
        }
        pub fn deinit(self: *@This()) void {
            self.clear();
            self.chunks.deinit();
        }
        pub fn clear(self: *@This()) void {
            var it = self.chunks.iterator();
            while (it.next()) |entry| {
                self.chunks.allocator.destroy(entry.value_ptr.*);
            }
            self.chunks.clearRetainingCapacity();
        }

        pub fn clone(self: @This(), allocator: Allocator) !@This() {
            var other = init(allocator);
            try other.chunks.ensureTotalCapacity(self.chunks.count());
            var it = self.chunks.iterator();
            while (it.next()) |entry| {
                const chunk = try self.chunks.allocator.create(Chunk);
                if (options.track_dirty_after_clone) {
                    // chunks are clean after a clone.
                    chunk.is_dirty = false;
                }
                @memcpy(&chunk.data, &entry.value_ptr.*.data);
                other.chunks.putAssumeCapacity(entry.key_ptr.*, chunk);
            }
            other.metrics = self.metrics;
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
            const value_ptr = try self.getOrPut(x, y);
            value_ptr.* = v;
        }

        pub fn getOrPutCoord(self: *@This(), coord: Coord) !*T {
            return self.getOrPut(coord.x, coord.y);
        }
        pub fn getOrPut(self: *@This(), x: i32, y: i32) !*T {
            const chunk_coord = Coord{
                .x = x >> chunk_shift,
                .y = y >> chunk_shift,
            };
            const inner_index = @as(usize, @intCast((y & chunk_mask) * chunk_side_length + (x & chunk_mask)));

            if (self.chunks.count() == 0) {
                // first put
                if (options.metrics) {
                    self.metrics.min_x = x;
                    self.metrics.min_y = y;
                    self.metrics.max_x = x;
                    self.metrics.max_y = y;
                }
            } else {
                if (options.metrics) {
                    if (x < self.metrics.min_x) self.metrics.min_x = x;
                    if (y < self.metrics.min_y) self.metrics.min_y = y;
                    if (x > self.metrics.max_x) self.metrics.max_x = x;
                    if (y > self.metrics.max_y) self.metrics.max_y = y;
                }

                // check the lru cache.
                if (self.last_chunk != null and self.last_chunk_coord.equals(chunk_coord)) {
                    const chunk = self.last_chunk.?;
                    if (options.track_dirty_after_clone) {
                        chunk.is_dirty = true;
                    }
                    return &chunk.data[inner_index];
                }
            }

            const gop = try self.chunks.getOrPut(chunk_coord);
            if (!gop.found_existing) {
                const chunk = try self.chunks.allocator.create(Chunk);
                if (options.track_dirty_after_clone) {
                    chunk.is_dirty = false;
                }
                @memset(&chunk.data, default_value);
                gop.value_ptr.* = chunk;
            }
            const chunk = gop.value_ptr.*;

            // update the lru cache
            self.last_chunk_coord = chunk_coord;
            self.last_chunk = chunk;
            if (options.track_dirty_after_clone) {
                chunk.is_dirty = true;
            }
            return &chunk.data[inner_index];
        }

        pub fn getCoord(self: *@This(), coord: Coord) T {
            return self.get(coord.x, coord.y);
        }
        pub fn get(self: *@This(), x: i32, y: i32) T {
            if (self.chunks.count() == 0) {
                // completely empty
                return default_value;
            }
            const chunk_coord = Coord{
                .x = x >> chunk_shift,
                .y = y >> chunk_shift,
            };
            const inner_index = @as(usize, @intCast((y & chunk_mask) * chunk_side_length + (x & chunk_mask)));
            // check the lru cache
            if (self.last_chunk != null and self.last_chunk_coord.equals(chunk_coord)) {
                return self.last_chunk.?.data[inner_index];
            }
            const chunk = self.chunks.get(chunk_coord) orelse return default_value;

            // update the lru cache
            self.last_chunk_coord = chunk_coord;
            self.last_chunk = chunk;
            return chunk.data[inner_index];
        }

        pub fn getExistingCoord(self: *@This(), coord: Coord) *T {
            return self.getExisting(coord.x, coord.y);
        }
        pub fn getExisting(self: *@This(), x: i32, y: i32) *T {
            const chunk_coord = Coord{
                .x = x >> chunk_shift,
                .y = y >> chunk_shift,
            };
            const inner_index = @as(usize, @intCast((y & chunk_mask) * chunk_side_length + (x & chunk_mask)));

            // check the lru cache
            if (self.last_chunk != null and self.last_chunk_coord.equals(chunk_coord)) {
                const chunk = self.last_chunk.?;
                if (options.track_dirty_after_clone) {
                    chunk.is_dirty = true;
                }
                return &chunk.data[inner_index];
            }
            const chunk = self.chunks.get(chunk_coord) orelse unreachable;

            // update the lru cache
            self.last_chunk_coord = chunk_coord;
            self.last_chunk = chunk;
            if (options.track_dirty_after_clone) {
                chunk.is_dirty = true;
            }
            return &chunk.data[inner_index];
        }

        pub fn chunkCoordAndInnerIndexToCoord(chunk_coord: Coord, inner_index: usize) Coord {
            assert(inner_index < chunk_side_length * chunk_side_length);
            return Coord{
                .x = (chunk_coord.x << chunk_shift) | (@as(i32, @intCast(inner_index)) & chunk_mask),
                .y = (chunk_coord.y << chunk_shift) | (@as(i32, @intCast(inner_index)) >> chunk_shift),
            };
        }
    };
}

test "SparseChunkedMatrix" {
    var m = SparseChunkedMatrix(u8, 42, .{}).init(std.testing.allocator);
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
