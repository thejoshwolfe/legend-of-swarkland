const std = @import("std");

const Logger = struct {
    is_enabled: bool,
    show_thread_id: bool,

    pub fn print(comptime self: Logger, comptime fmt: []const u8, args: anytype) void {
        if (!self.is_enabled) return;
        warn(self.show_thread_id, fmt, args);
    }

    pub fn deepPrint(comptime self: Logger, prefix: []const u8, something: anytype) void {
        if (!self.is_enabled) return;
        deep_print(prefix, something);
    }
};

pub const thread_lifecycle = Logger{ .is_enabled = false, .show_thread_id = true };
pub const testing = Logger{ .is_enabled = true, .show_thread_id = true };
pub const happening = Logger{ .is_enabled = false, .show_thread_id = true };
pub const record_macro = Logger{ .is_enabled = false, .show_thread_id = false };

fn warn(comptime show_thread_id: bool, comptime fmt: []const u8, args: anytype) void {
    // format to a buffer, then write in a single (or as few as possible)
    // posix write calls so that the output from multiple processes
    // doesn't interleave on the same line.
    var buffer: [0x1000]u8 = undefined;
    var buffer1: [0x1000]u8 = undefined;
    var buffer2: [0x1000]u8 = undefined;
    if (show_thread_id) {
        const debug_thread_id = blk: {
            const me = std.Thread.getCurrentId();
            for (thread_names.items) |it| {
                if (it.thread_id == me) break :blk it;
            }
            @panic("thread not named");
        };
        const prefix: []const u8 = std.fmt.bufPrint(buffer1[0..], "{s}({})", .{ debug_thread_id.name, debug_thread_id.client_id }) catch @panic("make the buffer bigger");
        const msg: []const u8 = std.fmt.bufPrint(buffer2[0..], fmt, args) catch @panic("make the buffer bigger");
        const line: []const u8 = std.fmt.bufPrint(buffer[0..], "{s}: {s}\n", .{ prefix, msg }) catch @panic("make the buffer bigger");
        std.debug.warn("{s}", .{line});
    } else {
        std.debug.warn("{s}", .{std.fmt.bufPrint(buffer[0..], fmt ++ "\n", args) catch {
            @panic("make the buffer bigger");
        }});
    }
}

var mutex = std.Thread.Mutex{};
pub fn init() void {}

const DebugThreadId = struct {
    thread_id: std.Thread.Id,
    name: []const u8,
    client_id: u32,
};
var _thread_names_buffer = [_]u8{0} ** 0x1000;
var _thread_names_allocator = std.heap.FixedBufferAllocator.init(_thread_names_buffer[0..]);
var thread_names = std.ArrayList(DebugThreadId).init(&_thread_names_allocator.allocator);

pub fn nameThisThread(name: []const u8) void {
    return nameThisThreadWithClientId(name, 0);
}
pub fn nameThisThreadWithClientId(name: []const u8, client_id: u32) void {
    mutex.lock();
    defer mutex.unlock();
    const thread_id = std.Thread.getCurrentId();
    for (thread_names.items) |it| {
        std.debug.assert(it.thread_id != thread_id);
        std.debug.assert(!(std.mem.eql(u8, it.name, name) and it.client_id == client_id));
        continue;
    }
    thread_names.append(DebugThreadId{
        .thread_id = thread_id,
        .name = name,
        .client_id = client_id,
    }) catch @panic("too many threads");
}
pub fn unnameThisThread() void {
    mutex.lock();
    defer mutex.unlock();
    const me = std.Thread.getCurrentId();
    for (thread_names.items) |it, i| {
        if (it.thread_id == me) {
            _ = thread_names.swapRemove(i);
            return;
        }
    } else {
        @panic("thread not named");
    }
}

/// i kinda wish std.fmt did this.
fn deep_print(prefix: []const u8, something: anytype) void {
    std.debug.warn("{}", .{prefix});
    struct {
        pub fn recurse(obj: anytype, comptime indent: comptime_int) void {
            const T = @TypeOf(obj);
            const indentation = ("  " ** indent)[0..];
            if (comptime std.mem.startsWith(u8, @typeName(T), "std.array_list.AlignedArrayList(")) {
                return recurse(obj.items, indent);
            }
            if (comptime std.mem.startsWith(u8, @typeName(T), "std.hash_map.HashMap(u32,")) {
                if (obj.count() == 0) {
                    return std.debug.warn("{{}}", .{});
                }
                std.debug.warn("{{", .{});
                var iterator = obj.iterator();
                while (iterator.next()) |kv| {
                    std.debug.warn("\n{}  {}: ", .{ indentation, kv.key });
                    recurse(kv.value, indent + 1);
                    std.debug.warn(",", .{});
                }
                return std.debug.warn("\n{}}}", .{indentation});
            }
            switch (@typeInfo(T)) {
                .Pointer => |ptr_info| switch (ptr_info.size) {
                    .One => return recurse(obj.*, indent),
                    .Slice => {
                        if (obj.len == 0) {
                            return std.debug.warn("[]", .{});
                        }
                        std.debug.warn("[\n", .{});
                        for (obj) |x| {
                            std.debug.warn("{}  ", .{indentation});
                            recurse(x, indent + 1);
                            std.debug.warn(",\n", .{});
                        }
                        return std.debug.warn("{}]", .{indentation});
                    },
                    else => {},
                },
                .Array => {
                    return recurse(obj[0..], indent);
                },
                .Struct => |StructT| {
                    const multiline = @sizeOf(T) >= 12;
                    std.debug.warn(".{{", .{});
                    inline for (StructT.fields) |field, i| {
                        if (i > 0) {
                            if (!multiline) {
                                std.debug.warn(", ", .{});
                            }
                        } else if (!multiline) {
                            std.debug.warn(" ", .{});
                        }
                        if (multiline) {
                            std.debug.warn("\n{}  ", .{indentation});
                        }
                        std.debug.warn(".{} = ", .{field.name});
                        if (std.mem.eql(u8, field.name, "terrain")) {
                            // hide terrain, because it's so bulky.
                            std.debug.warn("<...>", .{});
                        } else {
                            recurse(@field(obj, field.name), indent + 1);
                        }
                        if (multiline) std.debug.warn(",", .{});
                    }
                    if (multiline) {
                        std.debug.warn("\n{}}}", .{indentation});
                    } else {
                        std.debug.warn(" }}", .{});
                    }
                    return;
                },
                .Union => |info| {
                    if (info.tag_type) |_| {
                        std.debug.warn(".{{ .{} = ", .{@tagName(obj)});
                        inline for (info.fields) |u_field| {
                            if (@enumToInt(obj) == u_field.enum_field.?.value) {
                                recurse(@field(obj, u_field.name), indent);
                            }
                        }
                        std.debug.warn(" }}", .{});
                        return;
                    }
                },
                .Enum => {
                    return std.debug.warn(".{}", .{@tagName(obj)});
                },
                .Void => {
                    return std.debug.warn("{{}}", .{});
                },
                else => {},
            }
            return std.debug.warn("{}", .{obj});
        }
    }.recurse(something, 0);
    std.debug.warn("\n", .{});
}
