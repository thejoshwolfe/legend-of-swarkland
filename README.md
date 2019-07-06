# Legend of Swarkland

## Status

Simple turn-based action fantasy puzzle game.

## Design goals

Legend of Swarkland aims to be an interesting, fair, complex, challenging experience.
The primary inspiration for this game is the fabulous complexity of [NetHack](https://www.nethack.org/).
After getting into NetHack, I became frustrated by several core aspects of its design,
and this project was born as an attempt to realize what I believe is even greater potential.

### Client/server architecture

A significant NetHack community grew around the [alt.org](https://alt.org/nethack/) servers,
where players log in via Telnet and play the game in a terminal.
The server facilitates leaderboards, spectating, and even some limited interaction between players.

However, because the interface was limited to a terminal using Telnet, this simultaneously created both
a strong motivation for custom client interfaces and significant friction to implementing such interfaces.
For example, instead of seeing all monsters as single letters like `a` for ant and `b` for blob,
you might want a client with a graphical tileset where ants actually look like ants.
The problem is that `a` is also the letter for bees, `b` is also for slimes, and so on.
This ambiguity can be somewhat mitigated by using terminal colors to distinguish between the different variants,
but that can only take you so far.
There are 28 different `&`s, 58 different `@`s, and 74 `)`s (not counting artifact weapons).
Furthermore, the terminal interface does not distinguish between unexplored squares and dark tiles (that have no items/traps/etc).
In some cases the ambiguity is even intentional, such as the purple `@` Wizard of Yendor summoning a purple `@` elvenking as a decoy.

These ambiguities in NetHack's interface can be further mitigated with the `v` command,
which allows you to move a cursor around the map and inspect each square.
The game will give you a textual description of what you see there.
So it would technically be possible for a custom client interface to constantly use the `v` command in the background
to populate its model of the game state, but this creates tons of problems when it's not clear when the `v` command can be used.

(It's worth noting that a project called [NetHack 4](http://nethack4.org/) addressed many of these concerns
by creating a fork of NetHack that overhauled a significant amount of the game's core.
That's a great thing they did, and I'm happy for them.
But the client/server interface is only one problem I have with NetHack's design.)

Legend of Swarkland considers client/server separation as a core design principle.
It's so fundamental that the official "rules" for what information is fair game is exactly the information in the network protocol.
Down to the sort order of the information presented in the network packets,
it's all deliberate and designed to only reveal exactly what the human player is supposed to know.

Contrast this with [Minecraft](https://www.minecraft.net/),
where, unlike NetHack which sends too little information, the Minecraft server sends the client too much information:
full 3d terrain data in a radius around the player,
including precious information about the location of nearby buried treasure.
However, "X-Ray Texture Packs" that reveal the client's data to the human player are generally considered cheating.

This is even more problematic if someone were to write a fair AI that played Minecraft by using the network protocol
(a project I [dabbled in](https://github.com/PrismarineJS/mineflayer) back in 2011).
The AI would need very complex 3d ray-casting logic to restrict its knowledge to what a human is supposed to know in order to play "fairly".

Of course, simply using an AI to play Legend of Swarkland for you would be considered cheating
just like if you were to do that in a game of online chess.
But writing an AI sophisticated enough to outperform humans should be encouraged as a challenge,
and respectfully asked to be kept out of the humans-only leaderboards.
There should be separate leaderboards for AIs to compete,
and both human and AI players are first-class citizens of the Legend of Swarkland community.

But of course, you shall always be able to play Legend of Swarkland offline thanks to the client program
including all the code for running a server.
(Running everything in a single process also helps debug the program while it's in development.)

As another convenience to server maintainers, the `legend-of-swarkland.headless` executable program has no dependencies on any UI-related libraries,
so it can be built and run on a server without SDL or even X installed.

### Learning the game should not be frustrating

One of the most delightful aspects of NetHack is how many ways there are to lose the game.
You might try praying for help while in Gehennom.
You might get attacked by an electric eel while hovering above water wearing a ring of levitation.
You might try descending stairs while encumbered and wielding a cockatrice corpse.
The NetHack wiki has [an article](https://nethackwiki.com/wiki/Yet_Another_Stupid_Death) devoted to listing hilarious ways to die.

The sting of failure makes victory even sweeter, and comedic misfortune has its place in our hearts.
But when you don't know what you're doing, sudden death sucks.
NetHack is brutally punishing, which is fun when you know what you're doing, and terribly frustrating when you don't.

Most beginners will turn to [the wiki](https://nethackwiki.com/wiki/Strategy) as a guide for learning the game.
Some consider this "cheating" or not within the spirit of the game, but NetHack is so complex, cryptic, tedious, and punishing
that attempting to learn the game through pure trial and error is unreasonably demanding of your time and patience.
Does eating Medusa's corpse grant immunity to petrification or does it instantly stone you?
Answering that question through trial and error would probably be a 4 hour investment for a beginner who manages to make it that far.

NetHack is unplayable without the wiki, but by reading the wiki, you're missing out on the joy of discovery and exploration.

Legend of Swarkland aims to solve this in two big ways.

First, Legend of Swarkland features a practice mode where you get an unlimited undo button.
This is similar to how the old adventure games like [Monkey Island](https://en.wikipedia.org/wiki/Monkey_Island_(series%29)
or [Return to Zork](https://en.wikipedia.org/wiki/Return_to_Zork)
let you save the game anywhere and maintain multiple save files.
Now if you want to know what happens when you eat Medusa's corpse, just try it and undo.
Rather than losing 4 hours of progress, you spend 5 seconds peeking into the game's mechanics.
You are rewarded for experimenting, not punished for it.

When you're ready to play competitively, play in hardcore mode,
which is identical to practice mode, except that there's no undo button.

The second way Legend of Swarkland aims to discourage spoilers from a wiki is by including the wiki in the game itself.
Games with enough complexity really need a wiki to organize information about the game.
Wikis are useful and important and structured perfectly for navigating around to explore concepts and content.
The big downside of a wiki is the spoilers,
but in the builtin wiki for Legend of Swarkland, the wiki will only contain information that you have learned by playing the game.
The article on Medusa won't say what happens when you eat her corpse until you try it and find out.
The wiki starts out empty, and fills itself out as you explore the game.

The wiki's completion will persist across multiple playthroughs, and persist through undo.
The wiki will have vacant placeholders for missing information to give you a sense of progress toward 100%.
For some of the more obscure facts that could not reasonably be learned with first hand experience,
there will be a variety of ways to fill in the gaps such as fortune cookies,
rumours from NPCs, and literal guide books that you can find and read in game.

The UI for the wiki will probably be the system web browser, and the `legend-of-swarkland` binary will serve the content on localhost.
Unlocking wiki content will happen client-side.

For those who really want spoilers, that's fine; there will be a 100% completed wiki readily available on the web somewhere.
Legend of Swarkland is open source, so there's no point in trying to keep any secrets.
The purpose of the slowly unlocking wiki is not to keep the player in the dark;
it's to encourage players to experience the game the intended way without missing out on the structural features that a wiki provides.

The builtin wiki concept is similar to an in-game journal that fills itself out with journal entries as you play the game.
But the problem with the journals in games like [Spelunky](https://spelunky.fandom.com/wiki/Journal/HD) is that they're useless.
Spelunky says about the scorpion enemy "A predatory arachnid with a poisonous stringer on its tail",
but does not mention that it has 2 HP and is therefore possible to sacrifice on altars, and doing so grants 4 points of favor.
The [wiki article](https://spelunky.fandom.com/wiki/Scorpion) mentions all of those things and more.
Legend of Swarkland's in-game "journal" will actually be helpful, not worthless flavor text.

A critical aspect of wikis that has not been mentioned yet is the collaborative editing from the community.
Legend of Swarkland's builtin wiki will be personalized for the player, and the player can add any notes they wish.
The core of the content of the wiki, however, will come from the official, centralized source repository.
Edits to the source of truth would be pull requests or some other curated submission process.
There can be a feature that automates creating reviewable patches from your edits to your personalized wiki to streamline the process for non-programmers.

### Fabulous complexity

TODO elaborate.

 * perception types, blindness, telepathy
 * ambiguous id, shapeshifters, AI dilemmas
 * consistent physics

### Avoid grinding

TODO elaborate.

Grinding is defined by repeating an action to gain some in-game benefit.

 * 1-hit KOs
 * If an entity does take multiple hits to kill, each hit should be unique.
   Perhaps the first hit lops off an arm, which removes a possible action, but changes the AI to be "enraged" or something.
 * Enemy difficulty can be implemented by complete immunity to certain attacks rather than imperfect resistance to them.
   If an enemy is immune to arrows but not swords, then you have to adjust your strategy.
   If an enemy is immune to all attacks, but not falling in lava, that's interesting.
   If an enemy is simply undefeatable with your current abilities, maybe come back later.
 * The navigable space should be relatively small to avoid long boring walks through empty corridors.

### Open source

Legend of Swarkland is a passion project, not a business venture.
I will always keep my contributions to this project free and open source.

Legend of Swarkland is licensed under the MIT License. See `LICENSE.md`.

## Roadmap

### 5.1.0

TBD

## Version History

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
