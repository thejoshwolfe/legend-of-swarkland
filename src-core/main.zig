const std = @import("std");
const build_options = @import("build_options");
const displayMain = @import("../src-client/main.zig").displayMain;

pub fn main() error!void {
    if (build_options.headless) {
        return headlessMain();
    } else {
        return displayMain();
    }
}

fn headlessMain() !void {
    var in_adapter = (try std.io.getStdIn()).inStream();
    var in_stream = &in_adapter.stream;
    var out_adapter = (try std.io.getStdOut()).outStream();
    var out_stream = &out_adapter.stream;

    var position: i32 = 0;
    while (true) {
        {
            var buffer: [1]u8 = undefined;
            try in_stream.readNoEof(buffer[0..]);
            const direction = @bitCast(i8, buffer[0]);
            position += direction;
        }
        {
            var buffer: [4]u8 = undefined;
            std.mem.writeIntBE(i32, &buffer, position);
            try out_stream.write(buffer[0..]);
        }
    }
}
