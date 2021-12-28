completed_levels: u32 = 0,

const std = @import("std");

pub const filename = "save_5_6.swarkland";

/// Fails silently and falls back to an empty save file.
pub fn load() @This() {
    var file = std.fs.cwd().openFile(filename, .{}) catch return @This(){};
    defer file.close();
    return @This(){
        .completed_levels = file.reader().readIntLittle(u32) catch return @This(){},
    };
}

/// Fails silently.
pub fn save(save_file: @This()) void {
    var file = std.fs.cwd().createFile(filename, .{}) catch return;
    defer file.close();
    file.writer().writeIntLittle(u32, save_file.completed_levels) catch return;
}
