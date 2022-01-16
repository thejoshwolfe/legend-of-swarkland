const std = @import("std");

const Logger = struct {
    is_enabled: bool,
    show_thread_id: bool,

    pub fn print(self: Logger, comptime fmt: []const u8, args: anytype) void {
        if (!self.is_enabled) return;
        warn(self.show_thread_id, fmt, args);
    }

    pub fn deepPrint(self: Logger, prefix: []const u8, something: anytype) void {
        if (!self.is_enabled) return;
        deepPrintImpl(prefix, something);
    }
};

pub const warning = Logger{ .is_enabled = true, .show_thread_id = true };
pub const testing = Logger{ .is_enabled = true, .show_thread_id = true };
pub const thread_lifecycle = Logger{ .is_enabled = false, .show_thread_id = true };
pub const happening = Logger{ .is_enabled = false, .show_thread_id = true };
pub const actions = Logger{ .is_enabled = false, .show_thread_id = false };
pub const render = Logger{ .is_enabled = false, .show_thread_id = false };
pub const animation_compression = Logger{ .is_enabled = false, .show_thread_id = false };
pub const move_counter = Logger{ .is_enabled = false, .show_thread_id = false };
pub const ai = Logger{ .is_enabled = false, .show_thread_id = false };
pub const engine = Logger{ .is_enabled = true, .show_thread_id = false };
pub const auto_action = Logger{ .is_enabled = false, .show_thread_id = false };

fn warn(show_thread_id: bool, comptime fmt: []const u8, args: anytype) void {
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
        std.debug.print("{s}", .{line});
    } else {
        std.debug.print("{s}", .{std.fmt.bufPrint(buffer[0..], fmt ++ "\n", args) catch {
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
var thread_names = std.ArrayList(DebugThreadId).init(_thread_names_allocator.allocator());

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
fn deepPrintImpl(prefix: []const u8, something: anytype) void {
    std.debug.print("{s}", .{prefix});
    struct {
        pub fn recurse(obj: anytype, comptime indent: comptime_int) void {
            const T = @TypeOf(obj);
            const indentation = comptime ("  " ** indent);
            if (comptime std.mem.startsWith(u8, @typeName(T), "std.array_list.AlignedArrayList(")) {
                return recurse(obj.items, indent);
            }
            if (comptime std.mem.startsWith(u8, @typeName(T), "std.array_hash_map.ArrayHashMap(u32,")) {
                if (obj.count() == 0) {
                    return std.debug.print("{{}}", .{});
                }
                std.debug.print("{{", .{});
                var iterator = obj.iterator();
                while (iterator.next()) |kv| {
                    std.debug.print("\n{s}  {}: ", .{ indentation, kv.key_ptr.* });
                    recurse(kv.value_ptr.*, indent + 1);
                    std.debug.print(",", .{});
                }
                return std.debug.print("\n{s}}}", .{indentation});
            }
            switch (@typeInfo(T)) {
                .Pointer => |ptr_info| switch (ptr_info.size) {
                    .One => return recurse(obj.*, indent),
                    .Slice => {
                        if (obj.len == 0) {
                            return std.debug.print("[]", .{});
                        }
                        std.debug.print("[\n", .{});
                        for (obj) |x| {
                            std.debug.print("{s}  ", .{indentation});
                            recurse(x, indent + 1);
                            std.debug.print(",\n", .{});
                        }
                        return std.debug.print("{s}]", .{indentation});
                    },
                    else => {
                        @compileError("shouldn't get here: " ++ @typeName(T));
                    },
                },
                .Array => {
                    return recurse(obj[0..], indent);
                },
                .Struct => |StructT| {
                    const multiline = @sizeOf(T) >= 12;
                    std.debug.print(".{{", .{});
                    inline for (StructT.fields) |field, i| {
                        if (i > 0) {
                            if (!multiline) {
                                std.debug.print(", ", .{});
                            }
                        } else if (!multiline) {
                            std.debug.print(" ", .{});
                        }
                        if (multiline) {
                            std.debug.print("\n{s}  ", .{indentation});
                        }
                        std.debug.print(".{s} = ", .{field.name});
                        if (comptime std.mem.eql(u8, field.name, "terrain")) {
                            // hide terrain, because it's so bulky.
                            std.debug.print("<...>", .{});
                        } else {
                            recurse(@field(obj, field.name), indent + 1);
                        }
                        if (multiline) std.debug.print(",", .{});
                    }
                    if (multiline) {
                        std.debug.print("\n{s}}}", .{indentation});
                    } else {
                        std.debug.print(" }}", .{});
                    }
                    return;
                },
                .Union => |info| {
                    if (info.tag_type) |tag_type| {
                        std.debug.print(".{{ .{s} = ", .{@tagName(obj)});
                        inline for (info.fields) |u_field| {
                            if (@as(tag_type, obj) == @field(tag_type, u_field.name)) {
                                if (comptime (T == @import("../index.zig").protocol.ThingPosition and std.mem.eql(u8, u_field.name, "large"))) {
                                    // XXX: seems to be a miscompilation with this.
                                    std.debug.print("(sorry, compiler machine broke.)", .{});
                                } else {
                                    recurse(@field(obj, u_field.name), indent);
                                }
                            }
                        }
                        std.debug.print(" }}", .{});
                        return;
                    }
                },
                .Enum => {
                    return std.debug.print(".{s}", .{@tagName(obj)});
                },
                .Void => {
                    return std.debug.print("{{}}", .{});
                },
                else => {},
            }
            return std.debug.print("{}", .{obj});
        }
    }.recurse(something, 0);
    std.debug.print("\n", .{});
}
