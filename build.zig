const std = @import("std");
const builtin = @import("builtin");
const Builder = std.build.Builder;

pub fn build(b: *Builder) !void {
    const build_options = b.standardReleaseOptions();

    try b.env_map.set("PYTHONPATH", "deps/simplepng.py/");
    const compile_spritesheet = b.addCommand(".", b.env_map, [][]const u8{
        "./tools/compile_spritesheet.py",
        "assets/img/",
        "--glob=*.png",
        "--tilesize=32",
        "--spritesheet=zig-cache/spritesheet_resource",
        "--header=zig-cache/spritesheet.zig",
        "--deps=zig-cache/spritesheet_resource.d",
    });

    const headless_build = make_binary_variant(b, build_options, "legend-of-swarkland_headless", true);
    const client_build = make_binary_variant(b, build_options, "legend-of-swarkland", false);
    client_build.dependOn(&compile_spritesheet.step);

    const do_fmt = b.option(bool, "fmt", "zig fmt before building") orelse true;
    if (do_fmt) {
        const fmt_command = b.addCommand(".", b.env_map, [][]const u8{
            "zig-self-hosted",
            "fmt",
            "build.zig",
            "src",
        });
        headless_build.dependOn(&fmt_command.step);
        client_build.dependOn(&fmt_command.step);
    }

    b.default_step.dependOn(headless_build);
    b.default_step.dependOn(client_build);
}

fn make_binary_variant(b: *Builder, build_options: builtin.Mode, name: []const u8, headless: bool) *std.build.Step {
    const exe = b.addExecutable(name, "src/main.zig");
    exe.addBuildOption(bool, "headless", headless);
    if (!headless) {
        exe.linkSystemLibrary("SDL2");
        exe.linkSystemLibrary("c");
    }
    exe.setBuildMode(build_options);
    return &exe.step;
}
