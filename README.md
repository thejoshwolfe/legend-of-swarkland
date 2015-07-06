# Legend of Swarkland

## Status

Basic turn-based, hack-n-slash survival game.

## How Do I Run It?

### Windows

[Latest Windows Build](http://superjoe.s3.amazonaws.com/temp/legend-of-swarkland.html)

### Ubuntu

add ppa for rucksack:

```
sudo apt-add-repository ppa:andrewrk/rucksack
sudo apt-get update
```

install dependencies:

```
sudo apt-get install librucksack-dev rucksack libsdl2-dev libsdl2-ttf-dev libpng12-dev
```

build:

```
make
```

run:

```
./build/native/legend-of-swarkland
```

run with gdb from the command line:

```
gdb -iex "add-auto-load-safe-path ./" -ex run -ex quit ./build/native/legend-of-swarkland
```

## Build for Windows on Linux

 0. `git clone https://github.com/mxe/mxe`
 0. Follow these instructions: http://mxe.cc/#requirements-debian
 0. In the mxe directory: `make gcc sdl2 sdl2_ttf libpng rucksack`
 0. In legend-of-swarkland directory: `make MXE_HOME=/path/to/mxe windows`

(You can build for both Windows and native Linux at the same time by appending `native` to the above.)

See [this project](https://github.com/andrewrk/www.legend-of-swarkland) for
source code to the website.

## Eclipse Environment Setup on Linux

Josh develops this project in Eclipse on Linux, but of course it's optional.
These instructions were last updated with Eclipse Luna.

Search Google for "eclipse luna" to get to right Eclipse downloads page (there are lots of wrong download pages.).
Avoid the download for Java; get the one for C/C++ (if you don't see one for Java, you're probably on the wrong page.).
The description should confirm that it's for Linux.
You should get a binary archive on the order of 200MB with an executable at the top level called `eclipse`.

```
sudo apt-get install default-jdk
```

In eclipse, File -> New -> Project... -> C/C++ -> Makefile Project with Existing Code.
Browse to this directory.
**Click the "Linux GCC" toolchain.**

Now you should be able to open `main.cpp` and see no error or warning annotations.
You should be able to select a system include (`#include <...>`), and F3 to see its source.

### Optional Tweaks

Project -> Properties -> C/C++ Build -> Behavior Tab.
Uncheck "Stop on first build error".
Check "Enable parallel build".
Check "Build on resource save (Auto build)".
Blank out the textboxes under "Make build target" that say "all".

Run -> Debug Configurations... -> C/C++ Application -> New launch configuration.
Main Tab: Project -> Browse... -> select the one.
C/C++ Application: `build/native/legend-of-swarkland`
Debugger Tab: Uncheck "Stop on startup at".
GDB command file: `${workspace_loc:legend-of-swarkland}/.gdbinit`

Project -> Properties -> C/C++ General -> Formatter -> Edit...
Indentation Tab: Indentation policy: Spaces only.
Check "Statements within a 'switch' body".
Line Wrapping Tab: Maximum line width: 9999.
Default indentation for wrapped lines: 1.

## Roadmap

### 5.0.0

 * Secret dungeon features:
   * Secret passage ways.
   * Acid traps.
   * Polymorph traps.

### 4.1.0

 * Interface improvements, such as [zooming out or scrolling](https://github.com/thejoshwolfe/legend-of-swarkland/issues/8).

### 4.0.0

 * Roguelike dungeon crawler.
   * Rooms and corridors with stairs down to the next level (no stairs up).
   * Experience levels.
   * Monster difficulty depends on your experience level and dungeon depth.
 * Speediness only affects movement; fighting and other actions are standard speed for everyone.
 * Added potions and quaffing.
 * New items: wand of speed, wand of remedy, potion of healing, potion of poison, potion of ethereal vision, potion of cogniscopy.
 * New monsters: ant, bee, beetle, scorpion, snake.
 * New status effects: fast, poisoned, ethereal-visioned, cogniscopic.

## Version History

### 3.1.0

 * Color text.
 * The game is now a self-contained executable (no more resources.bundle).
 * Builtin help and version number.
 * Bug fixes

### 3.0.0

 * Items: wand of confusion, wand of striking, wand of digging.
   * Pick up, drop, throw, zap wands.
   * Some species try using wands. Air elementals suck up wands and fling them around randomly.
   * Wands can explode when they are thrown.
 * Building for Windows.

### 2.0.0

 * The map has randomly placed obstacles.
   Vision is limited by line-of-sight.
 * You and the other individuals in the game follow all the same rules, including limited knowledge.
 * Friendly humans spawn.
 * New art.
 * Some TAS support via `--script` and `--dump-script`

### 1.0.0

 * Run around a big open space as a human hitting randomly generated monsters.
 * Different monsters have different behavior and movement speed: pink blob, ogre, dog, air elemental.
 * HP regenerates.
   The game counts your kills.
