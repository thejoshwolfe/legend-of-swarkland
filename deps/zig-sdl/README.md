# zig-sdl

Self-contained SDL2 package for Zig.

## Current Status

On Linux, I was able to build libSDL2.a and then link an application against it,
but there would be no video support:

```
$ zig build run
INFO: Unable to initialize SDL: No available video device
```

So then I tried enabling X11, and running into this:

```
/home/andy/dev/zig-sdl/src/sensor/../events/../video/./khronos/vulkan/./vk_platform.h:113:10: fatal error: 
      'X11/Xlib.h' file not found
#include <X11/Xlib.h>
         ^~~~~~~~~~~~
1 error generated.
```

Which means that I need to create an X11 package that this one will depend on.
Recursive package dependencies is starting to get into territory where we need
Zig package manager to continue.

## How to Use this Package

I'm still experimenting. Don't try to use it yet.

## Updating to a Newer SDL Version

First, download the new tarball and overwrite all the source files with the new versions.
Look at the git diff and make sure that everything that has been overwritten is SDL stuff,
not stuff from this package, which is these files:

 * README.md
 * build.zig
 * zig-prebuilt/
 * example/

Revert the dynapi stuff which is in `src/dynapi/SDL_dynapi.h`. For an example,
see `15c22726a80ce7f842a2d929eb8457ed3ac76134`.

Next, you'll need access to each supported OS. The current list is:

 * Linux
 * Windows
 * macOS

### Linux

TODO docs for configuring

### Windows

On a Windows machine, install Microsoft Visual Studio and CMake.
Run **x64 Native Tools Command Prompt for VS 2019** and execute these commands:

```
mkdir build-debug
cd build-debug
cmake .. -Thost=x64 -A x64 -DCMAKE_BUILD_TYPE=Debug -DSDL_SHARED=OFF
cd ..
mkdir build-release
cd build-release
cmake .. -Thost=x64 -A x64 -DCMAKE_BUILD_TYPE=Release -DSDL_SHARED=OFF
```

Next, diff `build-debug/include/SDL_config.h` and `build-release/include/SDL_config.h`.
When I did this, there were no differences. So, the directories are the same. Delete
one of them and use the other one.

Ensure the appropriate subdirectory exists within zig-prebuilt. For example, on 64-bit
Windows, it is `x86_64-windows-msvc`. Copy `build-release/include/SDL_config.h` to the
appropriate subdirectory within `zig-prebuilt`.

Next, look at a `git diff` and use your critical thinking skills to determine if anything
not mentioned in this README needs to be done.

### macOS

TODO docs for configuring
