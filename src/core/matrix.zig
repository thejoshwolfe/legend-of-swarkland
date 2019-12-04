const std = @import("std");
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
        fn init(allocator: *std.mem.Allocator, width: u16, height: u16) !Self {
            return Self{
                .width = width,
                .height = height,
                .data = try allocator.alloc(T, width * height),
            };
        }
        pub fn initFill(allocator: *std.mem.Allocator, width: u16, height: u16, fill_value: T) !Self {
            var self = try Self.init(allocator, width, height);
            self.fill(fill_value);
            return self;
        }

        pub fn clone(self: Self, allocator: *std.mem.Allocator) !Self {
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
    };
}

test "Matrix" {
    var m = try Matrix(u8).initFill(std.debug.global_allocator, 3, 2, 0);
    std.testing.expect(m.at(-1, 0) == null);
    std.testing.expect(m.get(0, -1) == null);
    std.testing.expect(m.get(3, 0) == null);
    std.testing.expect(m.at(0, 2) == null);

    std.testing.expect(m.at(0, 0).?.* == 0);
    std.testing.expect(m.at(2, 1).?.* == 0);
    std.testing.expect(m.get(1, 1).? == 0);
    m.atUnchecked(1, 1).* = 3;
    std.testing.expect(m.at(1, 1).?.* == 3);

    var other = try m.clone(std.debug.global_allocator);
    std.testing.expect(other.at(0, 0).?.* == 0);
    std.testing.expect(other.get(1, 1).? == 3);
    m.atUnchecked(1, 1).* = 4;
    std.testing.expect(m.get(1, 1).? == 4);
    std.testing.expect(other.get(1, 1).? == 3);
}
