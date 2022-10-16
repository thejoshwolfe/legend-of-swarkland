const std = @import("std");
const builtin = @import("builtin");
const Builder = std.build.Builder;
const path = std.fs.path;
const sep_str = path.sep_str;
const Target = std.zig.CrossTarget;

pub fn build(b: *Builder) void {
    const mode = b.standardReleaseOptions();
    const target = b.standardTargetOptions(.{
        .whitelist = &[_]Target{
            .{
                .cpu_arch = .x86_64,
                .os_tag = .linux,
                .abi = .musl,
            },
            .{
                .cpu_arch = .x86_64,
                .os_tag = .windows,
                .abi = .gnu,
            },
        },
    });

    const lib = getLibrary(b, mode, target, ".");
    lib.install();

    const exe = b.addExecutable("sdl-zig-demo", "example/main.zig");
    exe.addIncludeDir("include");
    exe.setBuildMode(mode);
    exe.setTarget(target);
    exe.linkLibrary(lib);
    b.default_step.dependOn(&exe.step);
    exe.install();

    const run = b.step("run", "Run the demo");
    const run_cmd = exe.run();
    run.dependOn(&run_cmd.step);
}

pub const Options = struct {
    artifact: *std.build.LibExeObjStep,
    prefix: []const u8,
    gfx: bool = false,
    override_mode: ?std.builtin.Mode = null,
};

pub fn linkArtifact(b: *Builder, options: Options) void {
    const mode = options.override_mode orelse options.artifact.build_mode;
    const lib = getLibrary(b, mode, options.artifact.target, options.prefix);
    options.artifact.addIncludeDir(b.fmt("{s}/zig-prebuilt/include", .{options.prefix}));
    options.artifact.addIncludeDir(b.fmt("{s}/zig-prebuilt/include/SDL2", .{options.prefix}));
    options.artifact.linkLibrary(lib);

    if (options.gfx) {
        const gfx_lib = getLibGfx(b, mode, options.artifact.target, options.prefix);
        options.artifact.addIncludeDir(b.fmt("{s}/include", .{options.prefix}));
        options.artifact.addIncludeDir(b.fmt("{s}/extra/gfx/zig-prebuilt/include", .{options.prefix}));
        options.artifact.linkLibrary(gfx_lib);
    }
}

pub fn getLibGfx(
    b: *Builder,
    mode: std.builtin.Mode,
    target: Target,
    prefix: []const u8,
) *std.build.LibExeObjStep {
    const lib_cflags = &[_][]const u8{"-std=c99"};
    const lib = b.addStaticLibrary("SDL2_gfx", null);
    lib.setBuildMode(mode);
    lib.setTarget(target);
    lib.linkSystemLibrary("c");
    lib.addIncludeDir(b.fmt("{s}/zig-prebuilt/include/SDL2", .{prefix}));
    for (generic_gfx_src_files) |src_file| {
        const full_src_path = path.join(b.allocator, &[_][]const u8{ prefix, "extra", "gfx", src_file }) catch unreachable;

        lib.addCSourceFile(full_src_path, lib_cflags);
    }
    return lib;
}

pub fn getLibrary(
    b: *Builder,
    mode: std.builtin.Mode,
    target: Target,
    prefix: []const u8,
) *std.build.LibExeObjStep {
    const conf_dir = b.fmt("{s}/zig-prebuilt/{s}-{s}-{s}", .{
        prefix,
        @tagName(target.getCpuArch()),
        @tagName(target.getOsTag()),
        @tagName(target.getAbi()),
    });

    const lib_cflags = &[_][]const u8{};
    const lib = b.addStaticLibrary("SDL2", null);
    lib.setBuildMode(mode);
    lib.setTarget(target);
    lib.linkSystemLibrary("c");
    if (target.isWindows()) {
        lib.linkSystemLibrary("setupapi");
        lib.linkSystemLibrary("winmm");
        lib.linkSystemLibrary("gdi32");
        lib.linkSystemLibrary("imm32");
        lib.linkSystemLibrary("version");
        lib.linkSystemLibrary("oleaut32");
        lib.linkSystemLibrary("ole32");
    } else if (target.isDarwin()) {
        lib.linkFramework("OpenGL");
        lib.linkFramework("Metal");
        lib.linkFramework("CoreVideo");
        lib.linkFramework("Cocoa");
        lib.linkFramework("IOKit");
        lib.linkFramework("ForceFeedback");
        lib.linkFramework("Carbon");
        lib.linkFramework("CoreAudio");
        lib.linkFramework("AudioToolbox");
        lib.linkFramework("AVFoundation");
        lib.linkFramework("Foundation");
    }
    lib.addIncludeDir(conf_dir);
    lib.addIncludeDir(b.fmt("{s}/include", .{prefix}));
    lib.addIncludeDir(b.fmt("{s}/src/video/khronos", .{prefix}));
    for (generic_src_files) |src_file| {
        const full_src_path = path.join(b.allocator, &[_][]const u8{ prefix, "src", src_file }) catch unreachable;
        lib.addCSourceFile(full_src_path, lib_cflags);
    }
    if (target.isWindows()) {
        for (windows_src_files) |src_file| {
            const full_src_path = path.join(b.allocator, &[_][]const u8{ prefix, "src", src_file }) catch unreachable;
            lib.addCSourceFile(full_src_path, lib_cflags);
        }
    } else if (target.isDarwin()) {
        for (darwin_src_files) |src_file| {
            const full_src_path = path.join(b.allocator, &[_][]const u8{ prefix, "src", src_file }) catch unreachable;
            lib.addCSourceFile(full_src_path, lib_cflags);
        }
        //{
        //    const full_src_path = path.join(b.allocator, &[_][]const u8{ prefix, "src", "joystick/iphoneos/SDL_mfijoystick.m" }) catch unreachable;
        //    lib.addCSourceFile(full_src_path, &.{"-fobjc-arc"});
        //}
    }
    return lib;
}

const generic_src_files = [_][]const u8{
    "SDL.c",
    "SDL_assert.c",
    "SDL_dataqueue.c",
    "SDL_error.c",
    "SDL_hints.c",
    "SDL_log.c",
    "atomic/SDL_atomic.c",
    "atomic/SDL_spinlock.c",
    "audio/SDL_audio.c",
    "audio/SDL_audiocvt.c",
    "audio/SDL_audiodev.c",
    "audio/SDL_audiotypecvt.c",
    "audio/SDL_mixer.c",
    "audio/SDL_wave.c",
    "audio/arts/SDL_artsaudio.c",
    "audio/directsound/SDL_directsound.c",
    "audio/disk/SDL_diskaudio.c",
    "audio/dsp/SDL_dspaudio.c",
    "audio/dummy/SDL_dummyaudio.c",
    "audio/esd/SDL_esdaudio.c",
    "audio/fusionsound/SDL_fsaudio.c",
    "audio/nas/SDL_nasaudio.c",
    "audio/netbsd/SDL_netbsdaudio.c",
    "audio/paudio/SDL_paudio.c",
    "audio/qsa/SDL_qsa_audio.c",
    "audio/sndio/SDL_sndioaudio.c",
    "audio/sun/SDL_sunaudio.c",
    "audio/wasapi/SDL_wasapi.c",
    "audio/wasapi/SDL_wasapi_win32.c",
    "audio/winmm/SDL_winmm.c",
    "cpuinfo/SDL_cpuinfo.c",
    "events/SDL_clipboardevents.c",
    "events/SDL_displayevents.c",
    "events/SDL_dropevents.c",
    "events/SDL_events.c",
    "events/SDL_gesture.c",
    "events/SDL_keyboard.c",
    "events/SDL_mouse.c",
    "events/SDL_quit.c",
    "events/SDL_touch.c",
    "events/SDL_windowevents.c",
    "file/SDL_rwops.c",
    "haptic/SDL_haptic.c",
    "haptic/dummy/SDL_syshaptic.c",
    "joystick/SDL_gamecontroller.c",
    "joystick/SDL_joystick.c",
    "joystick/dummy/SDL_sysjoystick.c",
    "joystick/hidapi/SDL_hidapi_gamecube.c",
    "joystick/hidapi/SDL_hidapi_luna.c",
    "joystick/hidapi/SDL_hidapi_ps4.c",
    "joystick/hidapi/SDL_hidapi_ps5.c",
    "joystick/hidapi/SDL_hidapi_rumble.c",
    "joystick/hidapi/SDL_hidapi_stadia.c",
    "joystick/hidapi/SDL_hidapi_switch.c",
    "joystick/hidapi/SDL_hidapi_xbox360.c",
    "joystick/hidapi/SDL_hidapi_xbox360w.c",
    "joystick/hidapi/SDL_hidapi_xboxone.c",
    "joystick/hidapi/SDL_hidapijoystick.c",
    "joystick/steam/SDL_steamcontroller.c",
    "joystick/virtual/SDL_virtualjoystick.c",
    "libm/e_atan2.c",
    "libm/e_exp.c",
    "libm/e_fmod.c",
    "libm/e_log.c",
    "libm/e_log10.c",
    "libm/e_pow.c",
    "libm/e_rem_pio2.c",
    "libm/e_sqrt.c",
    "libm/k_cos.c",
    "libm/k_rem_pio2.c",
    "libm/k_sin.c",
    "libm/k_tan.c",
    "libm/s_atan.c",
    "libm/s_copysign.c",
    "libm/s_cos.c",
    "libm/s_fabs.c",
    "libm/s_floor.c",
    "libm/s_scalbn.c",
    "libm/s_sin.c",
    "libm/s_tan.c",
    "loadso/dlopen/SDL_sysloadso.c",
    "power/SDL_power.c",
    "render/SDL_d3dmath.c",
    "render/SDL_render.c",
    "render/SDL_yuv_sw.c",
    "render/opengl/SDL_render_gl.c",
    "render/opengl/SDL_shaders_gl.c",
    "render/opengles/SDL_render_gles.c",
    "render/opengles2/SDL_render_gles2.c",
    "render/opengles2/SDL_shaders_gles2.c",
    "render/software/SDL_blendfillrect.c",
    "render/software/SDL_blendline.c",
    "render/software/SDL_blendpoint.c",
    "render/software/SDL_drawline.c",
    "render/software/SDL_drawpoint.c",
    "render/software/SDL_render_sw.c",
    "render/software/SDL_rotate.c",
    "render/software/SDL_triangle.c",
    "sensor/SDL_sensor.c",
    "sensor/dummy/SDL_dummysensor.c",
    "stdlib/SDL_crc32.c",
    "stdlib/SDL_getenv.c",
    "stdlib/SDL_iconv.c",
    "stdlib/SDL_malloc.c",
    "stdlib/SDL_qsort.c",
    "stdlib/SDL_stdlib.c",
    "stdlib/SDL_string.c",
    "thread/SDL_thread.c",
    "thread/generic/SDL_syscond.c",
    //"thread/generic/SDL_sysmutex.c",
    //"thread/generic/SDL_syssem.c",
    //"thread/generic/SDL_systhread.c",
    //"thread/generic/SDL_systls.c",
    "timer/SDL_timer.c",
    "timer/dummy/SDL_systimer.c",
    "video/SDL_RLEaccel.c",
    "video/SDL_blit.c",
    "video/SDL_blit_0.c",
    "video/SDL_blit_1.c",
    "video/SDL_blit_A.c",
    "video/SDL_blit_N.c",
    "video/SDL_blit_auto.c",
    "video/SDL_blit_copy.c",
    "video/SDL_blit_slow.c",
    "video/SDL_bmp.c",
    "video/SDL_clipboard.c",
    "video/SDL_egl.c",
    "video/SDL_fillrect.c",
    "video/SDL_pixels.c",
    "video/SDL_rect.c",
    "video/SDL_shape.c",
    "video/SDL_stretch.c",
    "video/SDL_surface.c",
    "video/SDL_video.c",
    "video/SDL_vulkan_utils.c",
    "video/SDL_yuv.c",
    "video/directfb/SDL_DirectFB_WM.c",
    "video/directfb/SDL_DirectFB_dyn.c",
    "video/directfb/SDL_DirectFB_events.c",
    "video/directfb/SDL_DirectFB_modes.c",
    "video/directfb/SDL_DirectFB_mouse.c",
    "video/directfb/SDL_DirectFB_opengl.c",
    "video/directfb/SDL_DirectFB_render.c",
    "video/directfb/SDL_DirectFB_shape.c",
    "video/directfb/SDL_DirectFB_video.c",
    "video/directfb/SDL_DirectFB_window.c",
    "video/dummy/SDL_nullevents.c",
    "video/dummy/SDL_nullframebuffer.c",
    "video/dummy/SDL_nullvideo.c",
    "video/kmsdrm/SDL_kmsdrmdyn.c",
    "video/kmsdrm/SDL_kmsdrmevents.c",
    "video/kmsdrm/SDL_kmsdrmmouse.c",
    "video/kmsdrm/SDL_kmsdrmopengles.c",
    "video/kmsdrm/SDL_kmsdrmvideo.c",
    "video/pandora/SDL_pandora.c",
    "video/pandora/SDL_pandora_events.c",
    "video/raspberry/SDL_rpievents.c",
    "video/raspberry/SDL_rpimouse.c",
    "video/raspberry/SDL_rpiopengles.c",
    "video/raspberry/SDL_rpivideo.c",
    "video/yuv2rgb/yuv_rgb.c",
};

const linux_src_files = [_][]const u8{
    "thread/pthread/SDL_syscond.c",
    "thread/pthread/SDL_systls.c",
    "thread/pthread/SDL_sysmutex.c",
    "thread/pthread/SDL_systhread.c",
    "thread/pthread/SDL_syssem.c",
    "hidapi/libusb/hid.c",
    "timer/unix/SDL_systimer.c",
    "haptic/linux/SDL_syshaptic.c",
    "haptic/darwin/SDL_syshaptic.c",
    "core/unix/SDL_poll.c",
    "power/macosx/SDL_syspower.c",
    "hidapi/mac/hid.c",
    "power/linux/SDL_syspower.c",
    "hidapi/linux/hid.c",
    "core/linux/SDL_ime.c",
    "core/linux/SDL_fcitx.c",
    "core/linux/SDL_udev.c",
    "core/linux/SDL_dbus.c",
    "core/linux/SDL_ibus.c",
    "core/linux/SDL_evdev_kbd.c",
    "core/linux/SDL_evdev.c",
    "filesystem/unix/SDL_sysfilesystem.c",
    "audio/pulseaudio/SDL_pulseaudio.c",
    "audio/jack/SDL_jackaudio.c",
    "audio/alsa/SDL_alsa_audio.c",
    "video/qnx/gl.c",
    "video/qnx/keyboard.c",
    "video/qnx/video.c",
    "power/psp/SDL_syspower.c",
    "audio/psp/SDL_pspaudio.c",
    "timer/psp/SDL_systimer.c",
    "thread/psp/SDL_syscond.c",
    "thread/psp/SDL_sysmutex.c",
    "thread/psp/SDL_systhread.c",
    "thread/psp/SDL_syssem.c",
    "render/psp/SDL_render_psp.c",
    "joystick/psp/SDL_sysjoystick.c",
    "video/psp/SDL_pspmouse.c",
    "video/psp/SDL_pspgl.c",
    "video/psp/SDL_pspevents.c",
    "video/psp/SDL_pspvideo.c",
    "joystick/linux/SDL_sysjoystick.c",
    "joystick/bsd/SDL_sysjoystick.c",
    "joystick/darwin/SDL_sysjoystick.c",
    "joystick/android/SDL_sysjoystick.c",
    "filesystem/android/SDL_sysfilesystem.c",
    "haptic/android/SDL_syshaptic.c",
    "core/android/SDL_android.c",
    "power/android/SDL_syspower.c",
    "sensor/android/SDL_androidsensor.c",
    "audio/android/SDL_androidaudio.c",
    "video/android/SDL_androidkeyboard.c",
    "video/android/SDL_androidvulkan.c",
    "video/android/SDL_androidtouch.c",
    "video/android/SDL_androidmouse.c",
    "video/android/SDL_androidevents.c",
    "video/android/SDL_androidvideo.c",
    "video/android/SDL_androidclipboard.c",
    "video/android/SDL_androidwindow.c",
    "video/android/SDL_androidmessagebox.c",
    "video/android/SDL_androidgl.c",
    "power/emscripten/SDL_syspower.c",
    "audio/emscripten/SDL_emscriptenaudio.c",
    "filesystem/emscripten/SDL_sysfilesystem.c",
    "joystick/emscripten/SDL_sysjoystick.c",
    "video/emscripten/SDL_emscriptenframebuffer.c",
    "video/emscripten/SDL_emscriptenevents.c",
    "video/emscripten/SDL_emscriptenmouse.c",
    "video/emscripten/SDL_emscriptenopengles.c",
    "video/emscripten/SDL_emscriptenvideo.c",
    "audio/nacl/SDL_naclaudio.c",
    "filesystem/nacl/SDL_sysfilesystem.c",
    "video/nacl/SDL_naclwindow.c",
    "video/nacl/SDL_naclvideo.c",
    "video/nacl/SDL_naclglue.c",
    "video/nacl/SDL_naclopengles.c",
    "video/nacl/SDL_naclevents.c",
    "video/mir/SDL_mirvulkan.c",
    "video/mir/SDL_mirevents.c",
    "video/mir/SDL_mirmouse.c",
    "video/mir/SDL_mirwindow.c",
    "video/mir/SDL_mirdyn.c",
    "video/mir/SDL_mirframebuffer.c",
    "video/mir/SDL_mirvideo.c",
    "video/mir/SDL_miropengl.c",
    "video/x11/SDL_x11shape.c",
    "video/x11/imKStoUCS.c",
    "video/x11/SDL_x11framebuffer.c",
    "video/x11/SDL_x11events.c",
    "video/x11/SDL_x11opengles.c",
    "video/x11/SDL_x11touch.c",
    "video/x11/SDL_x11video.c",
    "video/x11/edid-parse.c",
    "video/x11/SDL_x11mouse.c",
    "video/x11/SDL_x11dyn.c",
    "video/x11/SDL_x11modes.c",
    "video/x11/SDL_x11messagebox.c",
    "video/x11/SDL_x11window.c",
    "video/x11/SDL_x11vulkan.c",
    "video/x11/SDL_x11keyboard.c",
    "video/x11/SDL_x11clipboard.c",
    "video/x11/SDL_x11opengl.c",
    "video/x11/SDL_x11xinput2.c",
    "video/vivante/SDL_vivantevideo.c",
    "video/vivante/SDL_vivanteplatform.c",
    "video/vivante/SDL_vivanteopengles.c",
    "video/wayland/SDL_waylandvideo.c",
    "video/wayland/SDL_waylandclipboard.c",
    "video/wayland/SDL_waylandvulkan.c",
    "video/wayland/SDL_waylandtouch.c",
    "video/wayland/SDL_waylandmouse.c",
    "video/wayland/SDL_waylandevents.c",
    "video/wayland/SDL_waylandwindow.c",
    "video/wayland/SDL_waylandopengles.c",
    "video/wayland/SDL_waylanddyn.c",
    "video/wayland/SDL_waylanddatamanager.c",
    "hidapi/SDL_hidapi.c",
};

const windows_src_files = [_][]const u8{
    "render/direct3d11/SDL_shaders_d3d11.c",
    "render/direct3d11/SDL_render_d3d11.c",
    "render/direct3d/SDL_shaders_d3d.c",
    "render/direct3d/SDL_render_d3d.c",
    "sensor/windows/SDL_windowssensor.c",
    "hidapi/windows/hid.c",
    "timer/windows/SDL_systimer.c",
    "loadso/windows/SDL_sysloadso.c",
    "power/windows/SDL_syspower.c",
    "core/windows/SDL_windows.c",
    "core/windows/SDL_xinput.c",
    "core/windows/SDL_hid.c",
    "thread/windows/SDL_systls.c",
    "thread/windows/SDL_sysmutex.c",
    "thread/windows/SDL_systhread.c",
    "thread/windows/SDL_syssem.c",
    "thread/windows/SDL_syscond_cv.c",
    "haptic/windows/SDL_dinputhaptic.c",
    "haptic/windows/SDL_xinputhaptic.c",
    "haptic/windows/SDL_windowshaptic.c",
    "filesystem/windows/SDL_sysfilesystem.c",
    "joystick/windows/SDL_windowsjoystick.c",
    "joystick/windows/SDL_xinputjoystick.c",
    "joystick/windows/SDL_dinputjoystick.c",
    "joystick/windows/SDL_rawinputjoystick.c",
    //"joystick/windows/SDL_windows_gaming_input.c",
    "video/windows/SDL_windowsclipboard.c",
    "video/windows/SDL_windowsmessagebox.c",
    "video/windows/SDL_windowsmouse.c",
    "video/windows/SDL_windowsvideo.c",
    "video/windows/SDL_windowsopengl.c",
    "video/windows/SDL_windowsvulkan.c",
    "video/windows/SDL_windowsshape.c",
    "video/windows/SDL_windowskeyboard.c",
    "video/windows/SDL_windowswindow.c",
    "video/windows/SDL_windowsmodes.c",
    "video/windows/SDL_windowsopengles.c",
    "video/windows/SDL_windowsframebuffer.c",
    "video/windows/SDL_windowsevents.c",
    "hidapi/SDL_hidapi.c",
};

const darwin_src_files = [_][]const u8{
    "file/cocoa/SDL_rwopsbundlesupport.m",
    "misc/macosx/SDL_sysurl.m",
    "audio/coreaudio/SDL_coreaudio.m",
    "joystick/darwin/SDL_iokitjoystick.c",
    "haptic/darwin/SDL_syshaptic.c",
    "power/macosx/SDL_syspower.c",
    "locale/macosx/SDL_syslocale.m",
    "timer/unix/SDL_systimer.c",
    "filesystem/cocoa/SDL_sysfilesystem.m",
    "thread/pthread/SDL_systhread.c",
    "thread/pthread/SDL_sysmutex.c",
    "thread/pthread/SDL_syscond.c",
    "thread/pthread/SDL_syssem.c",
    "thread/pthread/SDL_systls.c",
    "render/metal/SDL_render_metal.m",
    "video/cocoa/SDL_cocoaclipboard.m",
    "video/cocoa/SDL_cocoaevents.m",
    "video/cocoa/SDL_cocoakeyboard.m",
    "video/cocoa/SDL_cocoamessagebox.m",
    "video/cocoa/SDL_cocoametalview.m",
    "video/cocoa/SDL_cocoamodes.m",
    "video/cocoa/SDL_cocoamouse.m",
    "video/cocoa/SDL_cocoaopengl.m",
    "video/cocoa/SDL_cocoaopengles.m",
    "video/cocoa/SDL_cocoashape.m",
    "video/cocoa/SDL_cocoavideo.m",
    "video/cocoa/SDL_cocoavulkan.m",
    "video/cocoa/SDL_cocoawindow.m",
};

const generic_gfx_src_files = [_][]const u8{
    "SDL2_imageFilter.c",
    "SDL2_framerate.c",
    "SDL2_gfxPrimitives.c",
    "SDL2_rotozoom.c",
};
