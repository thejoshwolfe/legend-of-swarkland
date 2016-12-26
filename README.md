# Legend of Swarkland

## Status

Very short (5 levels) turn-based roguelike with a final boss and randomized items.

## How Do I Run It?

### Windows

[Latest Windows Build](http://superjoe.s3.amazonaws.com/temp/legend-of-swarkland.html)

### Ubuntu

install dependencies:

```
git submodule init && git submodule update
sudo apt-get install libsdl2-dev libsdl2-ttf-dev upx-ucl
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
 0. In the mxe directory: `make gcc sdl2 sdl2_ttf`
 0. In legend-of-swarkland directory: `make MXE_HOME=/path/to/mxe windows`

(You can build for both Windows and native Linux at the same time by appending `native` to the above.)

See [this project](https://github.com/thejoshwolfe/www.legend-of-swarkland) for
source code to the website.

## Eclipse Environment Setup on Linux

Josh develops this project in Eclipse on Linux, but of course it's optional.
These instructions were last updated with Eclipse Mars.

Search Google for "eclipse download" to get to the right Eclipse downloads page (there are lots of wrong download pages.).
You should see several options such as "Eclipse IDE for Java Developers".
Get "Eclipse IDE for C/C++ Developers".
You should get a binary archive on the order of 200MB with an executable at the top level called `eclipse`.

In my experience, the Eclipse Installer utility sucks.
It selected a slow mirror from another country,
took 20 minutes to download stuff into the /tmp directory,
crashed with a "Widget is disposed" error,
deleted its download cache,
and didn't save any of my install path preferences for the next attempt.
Worthless.

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

## VIM syntax setup

To get vim syntax highlighting for the .swarkland file format:

```
ln -s /path/to/legend-of-swarkland/vim/syntax/swarkland.vim ~/.vim/syntax/swarkland.vim
```

and add this to your `.vimrc`:

```
autocmd BufNewFile,BufRead *.swarkland set filetype=swarkland
```

## Roadmap

### 5.0.0

 * Stairs up.

### 4.5.0

 * New items:
   * spellbooks of: charm, fear, life syphon, slowing
   * potions of: fruit juice, mana

### 4.4.0

 * New monsters: tar elemental, shapeshifter.
 * Added dodging: after a move or wait action, you have a chance to avoid incoming attacks, projectiles, magic beams, etc.
 * Window is resizeable, and content scales to fill it while preserving aspect ratio.
 * Enhancements for swarkland developers: removed dependencies on rucksack and libpng. Added dependency on a git submodule.

## Version History

### 4.3.0

 * Spells and MP.
 * New items:
   * spellbooks of: assume form, magic bullet, force, mapping, speed
   * wands of: blinding, force, invisibility, magic bullet, slowing
 * Renamed striking to magic missile.

### 4.2.0
 * New monster: cobra that spits blinding venom.
 * Added a rest button. (issue [#22](https://github.com/thejoshwolfe/legend-of-swarkland/issues/22))
 * Limited perception of status effects. (i.e. You can't identify cogniscopy by throwing it at enemies, etc.)
 * AI doesn't wander off when someone gets in their way anymore.

### 4.1.0

 * Invisibility and blindness.
 * New items: potion of blinding, potion of invisibility.
 * Balance:
   * scorpions have a 1/4 chance of poison on each hit instead of 100% chance.
   * give the final boss 5 random potions, and teach him how to use them.
   * monsters always start at their minimum XP level. (issue [#19](https://github.com/thejoshwolfe/legend-of-swarkland/issues/19))

### 4.0.0

 * Roguelike dungeon crawler.
   * Rooms and corridors with stairs down to the next level (no stairs up).
   * Experience levels.
   * The monsters that spawn depend on dungeon depth.
 * Added potions and quaffing.
 * New items: wand of speed, wand of remedy, potion of healing, potion of poison, potion of ethereal vision, potion of cogniscopy.
 * New monsters: ant, bee, beetle, scorpion, snake.
 * New status effects: fast, poisoned, ethereal-visioned, cogniscopic.
 * Species have different base move speeds, but fighting and other actions are standard speed for everyone.

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
