# Legend of Swarkland

## Status

Simple turn-based action fantasy puzzle sandbox.
See [Version History](#version-history) for latest updates.

## How do I play it

A Windows build can be downloaded from here: https://wolfesoftware.com/legend-of-swarkland/

Here's how to build from source on Linux:

1. Get Zig from [ziglang.org](https://ziglang.org/). Search this project's commit history to find which version of Zig to download.
2. Get other dependencies:
    * NixOS: `nix-shell --pure -p python3 -p git -p clang -p SDL2`
    * Ubuntu: `sudo apt-get install python3 git clang libsdl2-dev`
3. `zig build`
    * Bonus points: `zig test -lc src/index.zig`
    * To build for Windows, add: `-Dtarget=x86_64-windows-gnu -Drelease-fast=true`
4. `./zig-out/bin/legend-of-swarkland`

## Design goals

See [DESIGN.md](DESIGN.md).

## Roadmap

### 5.7.0

 * line of sight
 * items
 * randomly generated world

## Version History

### 5.6.1

 * work around a bug where you get stuck in a wall if you ever move left.

### 5.6.0

 * bring back the puzzle levels. delete the randomly generated room.
   * the old puzzles are back, but now with the new wounded+limping mechanics.
   * add several new puzzles with the new mechanics introduced in this version.
 * save file that remembers which puzzle level you're on. main menu prompts to to resume from the last unlocked puzzle.
 * press `r` to restart the current puzzle.
 * new enemy: blob
   * blobs move by stretching and contracting like inch worms.
   * blob is nearly invincible: immune to attack damage, trample damage, and kicking.
   * blobs can occupy the same space as other individuals.
   * blobs grapple individuals that overlap with it, and can digest and kill individuals.
   * digesting an individual turns a blob into a large blob, which can digest even while stretched out.
   * blobs have limited vision.
   * kicking a blob sucks you into it.
 * new trap: wide polymorph trap
   * polymorph traps only affect individuals that are the right size.
 * restricted which species can kick.
 * animations are less jerky.
   * multiple animation frames will be rendered at the same time as long as an individual isn't doing two different things in the two frames.
   * if you input actions too fast, the lagging animations will be sped up rather than skipped.

### 5.5.0

 * remove puzzle levels, and replace with a large randomly generated room.
   * algorithm is extremely simple, just randomly scattering stuff everywhere.
   * sometimes you can't win. 🤷
 * add wounded status condition.
   * one hit from an attack causes wounded.
   * attack damage while wounded causes death.
   * two hits at once always cause death.
 * add limping status condition.
   * moving (or attempting to move, or being kicked) while wounded causes limping.
   * limping wears off after 1 turn.
   * limping turns move actions into wait actions, effectively halving your move speed.
 * add large diagram of your anatomy that shows status conditions.
 * blue tiles heal status conditions.
 * only orcs and centaurs (via polymorph) exist; removed all other species for now due to AI problems.
 * the game gives you a score when you win. my high score is 4.

### 5.4.0

 * added a polymorph trap in a hallway so you turn into a centaur.
 * adjusted AI so that you team up with your own race.

### 5.3.0

 * overhauled collision physics to make them less bouncy and hopefully more intuitive.
   * moves either succeed or fail. you can no longer be pushed into a space you didn't try to move into.
   * when multiple individuals try to move into the same space, there are some right-of-way rules that determine who wins sometimes.
 * added kicking:
   * does no damage. instead pushes the target 1 space.
   * happens after movement and before attacks.
   * does not affect rhinos.
 * new enemy: kangaroo:
   * doesn't attack. kicks instead of attacking.
   * preemptively kicks when you're 2 spaces away instead of walking into your attack.
   * killing a kangaroo unlocks the tutorial for kicking, although you've had the ability all along.

### 5.2.0

 * added lava: anyone who ends their movement on top of a lava tile dies to lava damage.
 * new enemies:
   * turtle: immune to attack damage.
   * rhino:
     * occupies 2 tiles.
     * head is immune to attack damage.
     * anyone colliding with a rhino dies to trample damage.
     * cannot attack.
     * can move double speed in the "forward" direction relative to its body at the start of the movement.
 * there are now three types of damage:
   * trample damage: happens during collision resolution, which may thwart someone's intention to attack.
   * lava damage: happens simultaneously with attacks, and only happens after all collision resolution has resolved.
   * attack damage: daggers and arrows
 * arrows no longer pierce targets.
 * introduced a bug where the collision resolution can infinite loop crash when too many individuals bump into each other near a wall. ([Issue #56](https://github.com/thejoshwolfe/legend-of-swarkland/issues/56))

### 5.1.0

 * accidentally skipped this version number.

### 5.0.1

 * Support building `legend-of-swarkland.exe` for Windows.

### 5.0.0

 * Deleted all C++ code and started over in Zig.
 * Client/server separation.
   * Headless server with no dependency on SDL.
   * Client that uses SDL and also includes the server.
   * Run as a single process with the server in a separate thread,
     or run the server as a child process and communicate over stdio.
 * All decision makers decide simultaneously on each turn, and all actions are resolved at once.
   * Bouncy collision resolution when multiple entities want to move into the same space at once.
 * Undo button.
 * Individuals:
   * human: equipped with a dagger
   * orc: equipped with a dagger, and wants to kill humans
   * centaur archer: fires arrows that pierce infinitely, and wants to kill humans
 * Sequence of predesigned levels with increasing difficulty, and a win screen at the end.
 * A main menu with a how-to-play blurb.

### 4.6.0 (unfinished)

 * Weapons are equipped and unequipped manually, not automatically, and equipping/unequipping takes an action.
 * Attacking a sucky monster (pink blob, tar elemental, air elemental) with an equipped weapon causes the weapon to be sucked up by the monster instead of the normal attack.
 * New item: a stick equippable as a weapon
   * The stick just pushes the target back 1 space and does no damage.
 * Balance tweaks to snake lunge attack:
   * Must wait to enable lunge attack, which better telegraphs when it will happen and how to avoid getting hit by it.
   * Snakes will camp and wait for you to approach, then get bored if you're out of view.
   * Can still lead to situations where a snake camps in a hallway forever and deals unavoidable damage.
 * This list may be inaccurate; this version was never finished. See the git branch `4.6.0-wip`.

Why the C++ code was abandoned:

 * After pushing a monster, subtle timing details cause significant effects.
   E.g. an ant having faster initiative than you means you can't see the effect of the push, because the ant steps forward before your next turn.
   Initiative is supposed to be an insignificant implementation detail that you never need to think about,
   but now it's a significant factor to whether a stick push is useful or useless.
   This problem led to the timing overhaul favoring atomic, simultaneous decisions from everyone at once.
 * With the introduction of equippable state, 4.6.0 overhauled how location is stored, making it a tagged union.
   Tagged unions and switch statements are such a pain to work with in C++, that this motivated abandoning C++ in favor of Zig.
   This problem and the timing issue above were the last straws in a list of reasons to rewrite everything from scratch.
 * A client/server separation has always been planned, but it wasn't clear how it could be done with incremental changes.
 * Perception relied on "thing ids", which are intuitive for programmers, but not for players or for the theming of the game.
   Every monster was effectively wearing a name tag or id badge that identified it, and your id badge is how the monsters identified who they were supposed to be fighting.
   Id badges allow monsters to identify you despite any polymorphing shenanigans, and generally give the player too much information,
   e.g. the out-of-view placeholder for an individual will disappear when that individual dies.
   See [issue #39](https://github.com/thejoshwolfe/legend-of-swarkland/issues/39) for even more discussion on this.
   Getting rid of id badges would also have been such a significant change that it wasn't clear how to do it with incremental changes.
 * General bad feelings about grinding. See the design goals above.

### 4.5.0

 * Added weapons
   * Weapons can be thrown for more/less range and more damage than other thrown items.
   * Holding a weapon increases your attack power.
 * New items:
   * weapons: dagger, battleaxe
   * potions of: burrowing, levitation
 * Added lava, and lava island rooms.
 * Added point-blank recoil for beam of force. Gotta go fast!
 * Removed random monster spawns over time. You only get what the level starts with now.
 * Snakes get a lunge attack.
 * Scorpions are immune to poison.

### 4.4.0

 * New monsters: tar elemental, shapeshifter.
 * Added dodging: after a move or wait action, you have a chance to avoid incoming attacks, projectiles, magic beams, etc.
 * Window is resizeable, and content scales to fill it while preserving aspect ratio.
 * Enhancements for swarkland developers: removed dependencies on rucksack and libpng. Added dependency on a git submodule.

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
