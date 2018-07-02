# Legend of Swarkland

## Status

Doesn't work.

## Roadmap

### 5.0.0

 * Complete rewrite from scratch in Zig instead of C++.
 * Client/server separation.
 * Game engine is parallel instead of turn-based.
   * Everyone decides what to do, and then everyone's decisions are resolved.

## Version History

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
