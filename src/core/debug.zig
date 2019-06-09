const std = @import("std");
pub fn warn(comptime fmt: []const u8, args: ...) void {
    // format to a buffer, then write in a single (or as few as possible)
    // posix write calls so that the output from multiple processes
    // doesn't interleave on the same line.
    var buffer: [0x1000]u8 = undefined;
    const prefix_name = blk: {
        const me = std.Thread.getCurrentId();
        for (thread_names) |*maybe_it| {
            if (maybe_it.*) |it| {
                if (it.id == me) break :blk it.name;
            }
        }
        @panic("thread not named");
    };
    std.debug.warn("{}", std.fmt.bufPrint(buffer[0..], "{}: " ++ fmt, prefix_name, args) catch {
        @panic("make the buffer bigger");
    });
}

var mutex: ?std.Mutex = null;
pub fn init() void {
    mutex = std.Mutex.init();
}

const IdAndName = struct {
    id: std.Thread.Id,
    name: []const u8,
};
var thread_names = []?IdAndName{null} ** 100;
pub fn nameThisThread(name: []const u8) void {
    var held = mutex.?.acquire();
    defer held.release();
    const me = std.Thread.getCurrentId();
    for (thread_names) |*maybe_it| {
        if (maybe_it.*) |it| {
            std.debug.assert(it.id != me);
            continue;
        }
        maybe_it.* = IdAndName{
            .id = me,
            .name = name,
        };
        return;
    }
    @panic("too many threads");
}
