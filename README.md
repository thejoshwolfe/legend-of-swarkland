# Legend of Swarkland

## Status

Basic turn-based, hack-n-slash survival game.

## How Do I Shot Game?

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
sudo apt-get install librucksack-dev rucksack libsdl2-dev libsdl2-ttf-dev clang libpng12-dev
```

build:

```
make
```

run:

```
./build/native/legend-of-swarkland
```

## Build for Windows on Linux

 0. `git clone https://github.com/andrewrk/mxe`
 0. Follow these instructions: http://mxe.cc/#requirements-debian
 0. In the mxe directory: `make gcc sdl2 sdl2_ttf libpng rucksack`
 0. In legend-of-swarkland directory: `make MXE_HOME=/path/to/mxe windows`

(You can build for both Windows and native Linux at the same time by appending `native` to the above.)

See [this project](https://github.com/andrewrk/www.legend-of-swarkland) for
source code to the website.

## Roadmap

### 5.0.0

 * Secret dungeon features:
   * Secret passage ways.
   * Acid traps.
   * Polymorph traps.

### 4.0.0

 * Roguelike dungeon crawler.
   * Rooms and corridors with stairs down to the next level (no stairs up).
   * Experience levels.
   * Monster difficulty depends on your experience level and dungeon depth.

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

 * Run around a big open space hitting randomly generated monsters.
 * Different monsters have different behavior and movement speed.
 * HP regenerates.
   The game counts your kills.
