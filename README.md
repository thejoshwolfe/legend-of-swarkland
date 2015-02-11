# legend-of-swarkland

## Status

Basic turn-based, hack-n-slash survival game.

## How Do I Shot Game? (Ubuntu)

add ppa for rucksack:

```
sudo apt-add-repository ppa:andrewrk/rucksack
sudo apt-get update
```

install dependencies:

```
sudo apt-get install librucksack-dev rucksack libsdl2-dev libsdl2-ttf-dev clang libfreeimage-dev
```

build:

```
make
```

run:

```
./build/legend-of-swarkland
```

## Build for Windows on Linux

 0. `git clone https://github.com/andrewrk/mxe`
 0. Follow these instructions: http://mxe.cc/#requirements-debian
 0. In the mxe directory: `make gcc sdl2 sdl2_ttf freeimage rucksack`
 0. In legend-of-swarkland directory: `make MXE_HOME=/path/to/mxe windows`

(You can build for both Windows and native Linux at the same time by appending `native` to the above.)

## Version History

### 2.0.0

 * The map has randomly placed obstacles.
   Vision is limited by line-of-sight.
 * You and the other individuals in the game follow all the same rules, including limited knowledge.
 * Friendly humans spawn.
 * New art.

### 1.0.0

 * Run around a big open space hitting randomly generated monsters.
 * Different monsters have different behavior and movement speed.
 * HP regenerates.
   The game counts your kills.
