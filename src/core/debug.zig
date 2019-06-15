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
    std.debug.warn("{}", std.fmt.bufPrint(buffer[0..], "{}: " ++ fmt ++ "\n", prefix_name, args) catch {
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
var thread_names = [_]?IdAndName{null} ** 100;
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

/// i kinda wish std.fmt did this.
pub fn deep_print(prefix: []const u8, something: var) void {
    std.debug.warn("{}", prefix);
    struct {
        pub fn recurse(obj: var, comptime indent: comptime_int) void {
            const T = @typeOf(obj);
            const indentation = ("  " ** indent)[0..];
            if (comptime std.mem.startsWith(u8, @typeName(T), "std.array_list.AlignedArrayList(")) {
                return recurse(obj.toSliceConst(), indent);
            }
            if (comptime std.mem.startsWith(u8, @typeName(T), "std.hash_map.HashMap(u32,")) {
                if (obj.count() == 0) {
                    return std.debug.warn("{{}}");
                }
                std.debug.warn("{{\n{}", indentation);
                var iterator = obj.iterator();
                while (iterator.next()) |kv| {
                    std.debug.warn("{}  {}: ", indentation, kv.key);
                    recurse(kv.value, indent + 1);
                    std.debug.warn(",\n");
                }
                return std.debug.warn("{}}}", indentation);
            }
            switch (@typeInfo(T)) {
                .Pointer => |ptr_info| switch (ptr_info.size) {
                    .One => return recurse(obj.*, indent),
                    .Slice => {
                        if (obj.len == 0) {
                            return std.debug.warn("[]");
                        }
                        std.debug.warn("[\n");
                        for (obj) |x| {
                            std.debug.warn("{}  ", indentation);
                            recurse(x, indent + 1);
                            std.debug.warn(",\n");
                        }
                        return std.debug.warn("{}]", indentation);
                    },
                    else => {},
                },
                .Array => {
                    return recurse(obj[0..], indent);
                },
                .Struct => {
                    const multiline = @sizeOf(T) >= 20;
                    comptime var field_i = 0;
                    std.debug.warn("{{");
                    inline while (field_i < @memberCount(T)) : (field_i += 1) {
                        if (field_i > 0) {
                            if (!multiline) {
                                std.debug.warn(", ");
                            }
                        } else if (!multiline) {
                            std.debug.warn(" ");
                        }
                        if (multiline) {
                            std.debug.warn("\n{}  ", indentation);
                        }
                        std.debug.warn(".{} = ", @memberName(T, field_i));
                        recurse(@field(obj, @memberName(T, field_i)), indent + 1);
                        if (multiline) std.debug.warn(",");
                    }
                    if (multiline) {
                        std.debug.warn("\n{}}}", indentation);
                    } else {
                        std.debug.warn(" }}");
                    }
                    return;
                },
                .Union => |info| {
                    if (info.tag_type) |UnionTagType| {
                        std.debug.warn("{}.{} = ", @typeName(T), @tagName(UnionTagType(obj)));
                        inline for (info.fields) |u_field| {
                            if (@enumToInt(UnionTagType(obj)) == u_field.enum_field.?.value) {
                                recurse(@field(obj, u_field.name), indent);
                            }
                        }
                        return;
                    }
                },
                .Enum => |info| {
                    return std.debug.warn(".{}", @tagName(obj));
                },
                else => {},
            }
            return std.debug.warn("{}", obj);
        }
    }.recurse(something, 0);
    std.debug.warn("\n");
}
