const std = @import("std");
const Builder = std.build.Builder;

pub fn build(b: *Builder) void {
    const optimize = b.standardOptimizeOption(.{});
    const target = b.standardTargetOptions(.{});

    const use_llvm = b.option(bool, "use-llvm", "use the LLVM backend");
    const headless_build = make_binary_variant(b, optimize, target, "legend-of-swarkland_headless", true, use_llvm);
    const gui_build = make_binary_variant(b, optimize, target, "legend-of-swarkland", false, use_llvm);

    addCompileSpritesheet(b, gui_build, .{
        .dir = "assets/img32/",
        .tile_arg = "--tile-size=32",
        .basename = "spritesheet32",
        .module_name = "sprites",
    });
    addCompileSpritesheet(b, gui_build, .{
        .dir = "assets/img200/",
        .tile_arg = "--tile-size=200",
        .basename = "spritesheet200",
        .module_name = "large_sprites",
    });
    addCompileSpritesheet(b, gui_build, .{
        .dir = "assets/font12x16/",
        .tile_arg = "--slice-tiles=12x16",
        .basename = "fontsheet12x16",
        .module_name = "fonts12x16",
    });
    addCompileSpritesheet(b, gui_build, .{
        .dir = "assets/font6x10/",
        .tile_arg = "--slice-tiles=6x10",
        .basename = "fontsheet6x10",
        .module_name = "fonts6x10",
    });

    b.installArtifact(headless_build);
    b.installArtifact(gui_build);

    const do_fmt = b.option(bool, "fmt", "zig fmt before building") orelse true;
    if (do_fmt) {
        const fmt_command = b.addFmt(.{
            .paths = &.{
                "build.zig",
                "src/core",
                "src/gui",
            },
        });
        headless_build.step.dependOn(&fmt_command.step);
        gui_build.step.dependOn(&fmt_command.step);
    }

    const config = b.addOptions();
    config.addOption([]const u8, "version", v: {
        const git_describe_untrimmed = b.run(&.{
            "git", "-C", b.build_root.path orelse ".", "describe", "--tags",
        });
        break :v std.mem.trim(u8, git_describe_untrimmed, " \n\r");
    });
    gui_build.addOptions("config", config);
}

fn make_binary_variant(
    b: *Builder,
    optimize: std.builtin.Mode,
    target: std.zig.CrossTarget,
    name: []const u8,
    headless: bool,
    use_llvm: ?bool,
) *std.build.Step.Compile {
    const exe = b.addExecutable(.{
        .name = name,
        .root_source_file = .{
            .path = if (headless) "src/server/server_main.zig" else "src/gui/gui_main.zig",
        },
        .target = target,
        .optimize = optimize,
    });
    exe.use_llvm = use_llvm;
    exe.use_lld = use_llvm;
    b.installArtifact(exe);

    const core = b.addModule("core", .{
        .source_file = .{ .path = "src/index.zig" },
    });
    const server = b.addModule("server", .{
        .source_file = .{ .path = "src/server/game_server.zig" },
        .dependencies = &.{
            .{ .name = "core", .module = core },
        },
    });
    const client = b.addModule("client", .{
        .source_file = .{ .path = "src/client/main.zig" },
        .dependencies = &.{
            .{ .name = "core", .module = core },
            .{ .name = "server", .module = server },
        },
    });

    exe.addModule("core", core);
    exe.addModule("server", server);
    exe.addModule("client", client);

    if (!headless) {
        if ((target.getOsTag() == .windows and target.getAbi() == .gnu) or target.getOsTag() == .macos) {
            const zig_sdl = b.dependency("zig_sdl", .{
                .target = target,
                .optimize = .ReleaseFast,
            });
            exe.linkLibrary(zig_sdl.artifact("SDL2"));
        } else {
            exe.linkSystemLibrary("SDL2");
        }
        exe.linkLibC();
    } else {
        // TODO: only used for malloc
        exe.linkLibC();
    }
    return exe;
}

const CompileSpritesheetOptions = struct {
    dir: []const u8,
    tile_arg: []const u8,
    basename: []const u8,
    module_name: []const u8,
};

fn addCompileSpritesheet(
    b: *std.Build,
    gui: *std.Build.Step.Compile,
    options: CompileSpritesheetOptions,
) void {
    const run = b.addSystemCommand(&.{
        "./tools/compile_spritesheet.py",
        options.dir,
        "--glob=*.png",
        options.tile_arg,
    });

    run.setEnvironmentVariable("PYTHONPATH", "deps/simplepng.py/");

    const zig_name = b.fmt("{s}.zig", .{options.basename});
    gui.addAnonymousModule(options.module_name, .{
        .source_file = run.addPrefixedOutputFileArg("--defs-path=", zig_name),
        .dependencies = &.{
            .{
                .name = "core",
                .module = gui.modules.get("core").?,
            },
        },
    });

    // TODO: add support to zig build system for directories as dependencies.
    // run.addPrefixedDepFileOutputArg("--deps=", b.fmt("{s}.d", .{options.basename}));
}
