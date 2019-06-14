# Legend of Swarkland

## Status

Doesn't work.

## Roadmap

### 5.0.0

TODO: separate release notes from design goals when 5.0.0 is "released" in some form.

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
 * Perceived information is no longer accompanied by unambiguous "index" fields.
   See [issue #39](https://github.com/thejoshwolfe/legend-of-swarkland/issues/39).
   No more "id badges" or absolute coordinates.
   * If you see a monster with normal vision, the sensation is accompanied by where the monster is relative to you.
     Its relative position is sufficient to render the monster on a 2d grid representation of the world.
   * If you hear rushing water, the sensation is accompanied by the general direction the sound is coming from, and a vague distance.
     This is *not* sufficient to render the source of the sound on a 2d grid precisely.
     Client implementations will need to represent the information somehow, TBD.
   * If you hear someone speaking to you, the sensation will be accompanied by some kind of id number to identify who is doing the speaking.
     While location may be easy enough to encode in cartesian coordinates, the identity of a speaker is less mathematically intuitive,
     but intelligent beings nonetheless have a keen sense for speaker identities.
     In the sensation protocol, this speaker identity will be represented by an arbitrary id number,
     and if the client has observed the speaker introduce themselves and associate a name to the id number,
     then the client will use that name to tell the player who is speaking.
   * Polymorphing or other status effects (even getting an ear cut off in battle) can effect the sensitivity of your senses.
     E.g. a canine nose gives you a sense of smell identity for nearly all biological things.
   * Even the special identity fields are not guaranteed to be unique.
     Identity collisions can be caused by shapeshifters or simply by the random identity generator, which might be deliberately coarse.
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
 * General bad feelings about grinding. See the 5.0.0 notes for more discussion.

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
