const std = @import("std");
const builtin = @import("builtin");
const Builder = std.build.Builder;

pub fn build(b: *Builder) void {
    const build_mode = b.standardReleaseOptions();
    const target = b.standardTargetOptions(.{});

    const compile_image_commands = [_]*std.build.RunStep{
        b.addSystemCommand(&[_][]const u8{
            "./tools/compile_spritesheet.py",
            "assets/img32/",
            "--glob=*.png",
            "--tile-size=32",
            "--spritesheet-path=zig-cache/spritesheet32_resource",
            "--defs-path=zig-cache/spritesheet32.zig",
            "--deps=zig-cache/spritesheet32_resource.d",
        }),
        b.addSystemCommand(&[_][]const u8{
            "./tools/compile_spritesheet.py",
            "assets/img200/",
            "--glob=*.png",
            "--tile-size=200",
            "--spritesheet-path=zig-cache/spritesheet200_resource",
            "--defs-path=zig-cache/spritesheet200.zig",
            "--deps=zig-cache/spritesheet200_resource.d",
        }),
        b.addSystemCommand(&[_][]const u8{
            "./tools/compile_spritesheet.py",
            "assets/font/",
            "--glob=*.png",
            "--slice-tiles=12x16",
            "--spritesheet-path=zig-cache/fontsheet_resource",
            "--defs-path=zig-cache/fontsheet.zig",
            "--deps=zig-cache/fontsheet_resource.d",
        }),
        b.addSystemCommand(&[_][]const u8{
            "python3",
            "-c",
            \\import subprocess
            \\tag_str = subprocess.check_output(["git", "describe", "--tags"]).strip()
            \\with open("zig-cache/version.txt", "wb") as f:
            \\    f.write(tag_str)
        }),
    };
    for (compile_image_commands) |compile_image_command| {
        compile_image_command.setEnvironmentVariable("PYTHONPATH", "deps/simplepng.py/");
    }

    const headless_build = make_binary_variant(b, build_mode, target, "legend-of-swarkland_headless", true);
    const gui_build = make_binary_variant(b, build_mode, target, "legend-of-swarkland", false);
    for (compile_image_commands) |compile_image_command| {
        gui_build.dependOn(&compile_image_command.step);
    }

    b.default_step.dependOn(headless_build);
    b.default_step.dependOn(gui_build);

    const do_fmt = b.option(bool, "fmt", "zig fmt before building") orelse true;
    if (do_fmt) {
        const fmt_command = b.addFmt(&[_][]const u8{
            "build.zig",
            "src/core",
            "src/gui",
        });
        headless_build.dependOn(&fmt_command.step);
        gui_build.dependOn(&fmt_command.step);
    }
}

fn make_binary_variant(
    b: *Builder,
    build_mode: builtin.Mode,
    target: std.build.Target,
    name: []const u8,
    headless: bool,
) *std.build.Step {
    const exe = if (headless) b.addExecutable(name, "src/server/server_main.zig") else b.addExecutable(name, "src/gui/gui_main.zig");
    exe.setTarget(target);
    exe.install();
    exe.addPackagePath("core", "src/index.zig");
    if (!headless) {
        if (target.getOsTag() == .windows and target.getAbi() == .gnu) {
            @import("deps/zig-sdl/build.zig").linkArtifact(b, .{
                .artifact = exe,
                .prefix = "deps/zig-sdl",
                .override_mode = .ReleaseFast,
            });
        } else {
            exe.linkSystemLibrary("SDL2");
        }
        exe.linkSystemLibrary("c");
    } else {
        // TODO: only used for malloc
        exe.linkSystemLibrary("c");
    }
    // FIXME: workaround https://github.com/ziglang/zig/issues/855
    exe.setMainPkgPath(".");
    exe.setBuildMode(build_mode);
    return &exe.step;
}
