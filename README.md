# Legend of Swarkland

## Status

Doesn't work.

## Roadmap

### 5.0.0

 * Complete rewrite from scratch in Zig instead of C++.
 * Client/server separation.
   * A headless "server" binary that only knows the game logic and a protocol. no UI-related dependencies.
   * A GUI-enabled binary that uses SDL, which also includes all the code in the headless "server".
   * The GUI can either run everything in its process, which helps debuggers,
     or it can spawn a headless child process and communicate with stdio to demonstrate and test the client/server separation.
     Remote communication with TCP is planned but not yet implemented.
   * The plan is that all information and apis available in the protocol are fair game wrt online play (leaderboards, pvp, etc.).
     So an AI that uses the API directly is no more privaledged than a human using a GUI.
     Also, the official GUI will be as helpful as possible for exposing all the features you might want.
 * All decision makers decide simultaneously on each turn, and all actions are resolved at once.
   * Introduces complex collision resolution when multiple entities want to move into the same space at once.
   * Removes slow and fast movement speeds in favor of temporary stun statuses (slow) and move actions that can leap farther than normal (fast).
 * Undo button.
   * Always available in "practice" mode, which is currently the only mode.
     ("Hardcore" mode would simply disable the undo button, and that's all.
     Hardcore mode would be preferred for any kind of competitive play, such as leaderboards.)
   * D&D-style dice-roll randomness makes a lot less sense with undo.
     Instead of attacks doing random damage, for example, all attacks do predictable damage.
   * Spending resources to learn information that would persist through an undo makes a lot less sense.
     The current plan is to do it anyway, which would cause a pretty significant divergence in playstyle between practice mode and hardcore mode.
     In practice mode, you drink every potion you find, then undo, to identify what potion it is.
     In hardcore mode, you need to be more careful.
 * Try to eliminate grinding (where "grinding" is defined by repeating an action to gain some in-game benefit).
   * No more wait-to-win HP or MP regeneration.
     Favor a playstyle where you can feasibly avoid all damage.
     (More like Crypt of the Necrodancer than Nethack in this regard.)
   * All entities have greatly reduced HP, usually one-hit kills all around.
     This avoids smacking a boss 20 times until it dies, for example.
   * If an entity does take multiple hits to kill, each hit should be unique.
     Perhaps the first hit lops off an arm, which removes a possible action, but changes the AI to be "enraged" or something.
   * Enemy difficulty can be implemented by complete immunity to certain attacks rather than imperfect resistance to them.
     If an enemy is immune to arrows but not swords, then you have to adjust your strategy.
     If an enemy is immune to all attacks, but not falling in lava, that's interesting.
     If an enemy is simply undefeatable with your current abilities, maybe come back later.
   * Make the navigable space smaller. This avoids long boring walks through empty corridors.
 * Overall, more of a puzzle feel than an action feel.

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
