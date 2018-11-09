const std = @import("std");
pub var prefix_name = "??????";
pub fn warn(comptime fmt: []const u8, args: ...) void {
    // format to a buffer, then write in a single (or as few as possible)
    // posix write calls so that the output from multiple processes
    // doesn't interleave on the same line.
    var buffer: [0x1000]u8 = undefined;
    std.debug.warn("{}", std.fmt.bufPrint(buffer[0..], "{}: " ++ fmt, prefix_name, args) catch {
        @panic("make the buffer bigger");
    });
}
