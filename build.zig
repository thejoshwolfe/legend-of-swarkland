const Builder = @import("std").build.Builder;

pub fn build(b: *Builder) !void {
    const exe = b.addExecutable("legend-of-swarkland", "src/main.zig");
    exe.linkSystemLibrary("SDL2");
    exe.linkSystemLibrary("c");
    exe.setBuildMode(b.standardReleaseOptions());
    b.default_step.dependOn(&exe.step);

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
    exe.step.dependOn(&compile_spritesheet.step);
}
