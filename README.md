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
