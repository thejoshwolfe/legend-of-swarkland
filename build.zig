const std = @import("std");
const builtin = @import("builtin");
const Builder = std.build.Builder;

pub fn build(b: *Builder) !void {
    const build_options = b.standardReleaseOptions();

    try b.env_map.set("PYTHONPATH", "deps/simplepng.py/");
    const compile_spritesheet = b.addSystemCommand([][]const u8{
        "./tools/compile_spritesheet.py",
        "assets/img/",
        "--glob=*.png",
        "--tile-size=32",
        "--spritesheet-path=zig-cache/spritesheet_resource",
        "--defs-path=zig-cache/spritesheet.zig",
        "--deps=zig-cache/spritesheet_resource.d",
    });
    const compile_fontsheet = b.addSystemCommand([][]const u8{
        "./tools/compile_spritesheet.py",
        "assets/font/",
        "--glob=*.png",
        "--slice-tiles=12x16",
        "--spritesheet-path=zig-cache/fontsheet_resource",
        "--defs-path=zig-cache/fontsheet.zig",
        "--deps=zig-cache/fontsheet_resource.d",
    });

    const headless_build = make_binary_variant(b, build_options, "legend-of-swarkland_headless", true);
    const gui_build = make_binary_variant(b, build_options, "legend-of-swarkland", false);
    gui_build.dependOn(&compile_spritesheet.step);
    gui_build.dependOn(&compile_fontsheet.step);

    b.default_step.dependOn(headless_build);
    b.default_step.dependOn(gui_build);
}

fn make_binary_variant(b: *Builder, build_options: builtin.Mode, name: []const u8, headless: bool) *std.build.Step {
    const exe = if (headless) b.addExecutable(name, "src-core/server_main.zig") else b.addExecutable(name, "src-gui/gui_main.zig");
    exe.addPackagePath("core", "src-core/index.zig");
    if (!headless) {
        exe.linkSystemLibrary("SDL2");
        exe.linkSystemLibrary("c");
    } else {
        // TODO: only used for malloc
        exe.linkSystemLibrary("c");
    }
    exe.setBuildMode(build_options);
    return &exe.step;
}
