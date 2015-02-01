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
