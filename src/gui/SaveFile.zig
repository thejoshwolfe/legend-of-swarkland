filename: []u8,

// actual data
completed_levels: u32 = 0,

const std = @import("std");
const Allocator = std.mem.Allocator;

/// Fails silently and falls back to an empty save file.
pub fn load(allocator: Allocator) !@This() {
    const dir = try std.fs.cwd().realpathAlloc(allocator, ".");
    defer allocator.free(dir);
    const filename = try std.fs.path.join(allocator, &.{ dir, "save_5_6.swarkland" });
    var save_file = @This(){
        .filename = filename,
    };
    if (std.fs.openFileAbsolute(filename, .{})) |file| {
        defer file.close();
        if (file.reader().readInt(u32, .little)) |completed_levels| {
            save_file.completed_levels = completed_levels;
        } else |_| {}
    } else |_| {}
    return save_file;
}

/// Logs a warning on failure.
pub fn save(save_file: @This()) void {
    var file = std.fs.cwd().createFile(save_file.filename, .{}) catch |err| {
        std.log.warn("error creating save file: {s}", .{@errorName(err)});
        return;
    };
    defer file.close();
    file.writer().writeInt(u32, save_file.completed_levels, .little) catch |err| {
        std.log.warn("error writing save file: {s}", .{@errorName(err)});
        return;
    };
}
