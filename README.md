# Legend of Swarkland

## Status

Simple turn-based action fantasy puzzle game.

## How do I play it

A Windows build can be downloaded from here: https://wolfesoftware.com/legend-of-swarkland/

Here's how to build from source on Linux:

1. Get a very recent build of [zig](https://ziglang.org/).
   I'm developing off of zig's master branch, so check my commit log for "updated to zig xxxxxx" commits to know which version to use.
2. `sudo apt-get install libsdl2-dev`
3. `git submodule update --init`
4. `zig build`
5. `./zig-cache/bin/legend-of-swarkland`

## Design goals

Legend of Swarkland aims to be an interesting, fair, complex, challenging experience.
The primary inspiration for this game is the fabulous complexity of [NetHack](https://www.nethack.org/).
After getting into NetHack, I became frustrated by several core aspects of its design,
and this project was born as an attempt to realize what I believe is even greater potential.

1. Client/server architecture
2. Learning the game should not be frustrating
3. Fabulous complexity
4. Avoid grinding
5. Seeded randomness
6. Open source

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
This is similar to how the old adventure games like [Monkey Island](https://en.wikipedia.org/wiki/Monkey_Island_%28series%29)
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

Legend of Swarkland aims to have comparable complexity to the gold standard of complex game design, NetHack.
Although the details of Legend of Swarkland's complexity have not yet been fully fleshed out,
it will be heavily inspired by what NetHack has already accomplished.

To illustrate a glimpse of the fabulous complexity of NetHack, here is an excerpt from the wiki article on [mirrors](https://nethackwiki.com/wiki/Mirror):

> You can apply a mirror to a monster to scare it; this only works if they can see it (i.e. are not blind, have eyes, you and the monster are not invisible (unless the monster can see invisible), etc.). [...] Humanoid foes (and unicorns) are not fooled by a mirror and won't be frightened. A cursed mirror only works half the time, the rest of the time it will "fog up" instead.

Here is another excerpt from the [guide](https://nethackwiki.com/wiki/Curse_removal) for how to remove cursed equipment that is stuck to you:

> 1. If the cursed object is armor, let an incubus or succubus remove it with their seduction attack.
> 2. If the cursed object is causing you to levitate, float over a sink. You will crash to the floor, and the item will be removed.
> 3. If the cursed object is armor or a weapon, polymorph into something that can't wear armor or wield weapons. Changing into were-form also works; werewolves will destroy armor they are wearing, while werejackals and wererats will shrink out of it.

This should give you a sense for just how sophisticated the simulation is in NetHack.

However, NetHack has a problem with its complexity.
The game rules are almost consistent, but several rules are different for you and for enemies.
While polymorphed into a vampire, you can perform the vampire's life-draining attack on your enemies,
but as an incubus you cannot perform the seduction attack.
When you polymorph into a nymph, you can perform the item stealing maneuver just like an enemy nymph would against you,
but you cannot teleport away afterward like an enemy nymph would after stealing from you.

A clever player might think to polymorph into a nymph in order to steal an item from an enemy, and that intuition would be rewarded.
But if the same player polymorphs into a nymph intending to teleport, that intuition would be punished.
Why can't players teleport while polymorphed into a nymph? I don't know.
It seems arbitrary and unfair.

In Legend of Swarkland, one of the most important game design principles is that
everything should be fair to everyone in the game world; you are not special.
Starting in version 5.0.0, the code that runs the game rules is so fair that it isn't even aware of
which individual in the world is being controlled by the player.
(There may end up being a few practical exceptions to this, such as only simulating the world around the player.)

To accomplish this, the server-side code is divided into two components:

1. The facilitator, which accepts connections from player clients, creates new games,
   and manages the lifecycle of AI clients that control enemies.
2. The game engine, which implements all the complex game rules,
   but has no concept of which individual is being controlled by which decision maker.

The player client along with the AI clients are collectively known as decision makers.
They get information about what their characters can see,
and then have to decide what to do next and send that decision to the server.

The server-side facilitator collects the decisions that everyone is making and sends them all to the game engine.
The game engine runs a (pure) function that determines the consequences of those decisions
including personalized observational information for each decision maker from their perspective.
The facilitator then distributes that information back to each decision maker, and the cycle repeats.
All communication with decision makers happens through a serial protocol that is the same for AI clients and the player client.

The AIs do not have privileged information about the game state;
they're playing by the same rules you are.
The only reason why the monsters want to attack you instead of each other,
is that the AIs are programmed to attack humans and not any other species.
Note that the AIs are *not* part of the game engine, but instead are managed by the facilitator.

One consequence of this fair design that already exists in version 5.0.0 is that
the game will only unlock the stairs to the next level when all individuals but one have been killed,
but that last standing individual doesn't have to be you.
If you get yourself killed while the last living enemy is already standing on the stairs,
the game will advance that enemy to the next level.
As what I would call an emergent easter egg, you can even manage to see the "you win" screen with a lone enemy standing triumphant
over your (not shown) dead corpse.

Legend of Swarkland's rigorous dedication to fairness will have several consequences on the game design.
It will be generally good for making the game engine make sense to players,
but it will also become impossible to implement several beloved features of NetHack.
For example, the "conflict" effect in NetHack causes all monsters to attack everything in sight instead of just you.
It's not possible to mind control the human player (without removing their agency),
and so because conflict cannot be used against the player, it would not be fair to use it against the AI.
An additional consideration is that super powerful abilities, like genocide, need careful balance scrutiny
if monsters can use that super powerful ability against you.

In the now-deleted code of Legend of Swarkland 4.5.0,
we were starting to see several good ideas emerge from the fairness oriented game design.
Most of the examples involved enemies having limited perception, just like you have.
Monsters (and you) can have blindness, invisibility, telepathic vision, or any combination of the above,
which led to situations where monsters could be convinced to attack each other if they couldn't tell who exactly they were attacking.
Additionally, item identification was fair for both the player and the monsters.
You wouldn't know if the wand in your hand was a helpful or harmful wand until you observed it in action,
and the same goes for the enemies.
This led to situations like an ogre guessing that an unidentified wand is harmful, and angrily zapping it at you,
only to find by seeing the effect on you that it's a helpful wand of speed, so the ogre then zaps themself with the wand.
All these ideas worked very well in a fair game engine, and will probably return in the new code of version 5+.

There is a long list of features I'm considering adding to the game someday,
but this is a discussion of design goals, and is not the place for an exhaustive list.
If I formalize a list of planned features someday, I will link it here.
For now, it's mostly just a bunch of ideas floating around in my head.

### Avoid grinding

Grinding is defined by the player repeating an action to gain some in-game benefit.
Grinding is pervasive in video game design, and it is often used to make games more compelling to play.
RPGs are notorious for being rife with grindy elements, like fishing for hours trying to get that 1-in-2000 rare fish for a quest.
NetHack has its own [problems](https://nethackwiki.com/wiki/Death_by_boredom) with grinding,
but they're certainly not the worst industry has seen.

Legend of Swarkland aims to avoid grinding by design.

One red flag in game design that there might be grinding is simply having large numbers, where large means above 6 or so.
Some common places for these numbers: hit points (hundreds?), gold pieces (thousands?), experience points (tens of thousands?).
The trope of "weapon damage vs health bar" is so prevalent in action game design
that people seem to accept that that's just the way action games should be designed.
But consider instead the Mr Freeze fight in [Batman: Arkham City](https://en.wikipedia.org/wiki/Batman:_Arkham_City),
in which you must outsmart the enemy by using 8 or so *different* attacks on him.
This isn't a boss encounter where you pelt bullets repeatedly until some number in memory eventually reduces to zero.
Each hit on the boss is unique content; no grinding.

Legend of Swarkland currently has the ambitious goal of having no health bars anywhere.

When health bars are in an action game, they usually serve a few distinct purposes:

1. Your health bar can enable you to make a limited number of mistakes without reaching a failure state.
   e.g. [The Legend of Zelda](https://en.wikipedia.org/wiki/The_Legend_of_Zelda),
   [Crypt of the Necrodancer](https://en.wikipedia.org/wiki/Crypt_of_the_NecroDancer)
2. Your health bar can additionally be a resource that you spend and manage strategically.
   e.g. [The Binding of Isaac](https://en.wikipedia.org/wiki/The_Binding_of_Isaac_%28video_game%29),
   [Slay the Spire](https://en.wikipedia.org/wiki/Slay_the_Spire)
3. An enemy health bar can be used to elongate the encounter while you constantly pelt the enemy with attacks.
   e.g. [Contra](https://en.wikipedia.org/wiki/Contra_%28series%29),
   [Metroid](https://en.wikipedia.org/wiki/Metroid)
4. An enemy health bar can additionally function as a "level check" barrier
   that you cannot get past without leveling up elsewhere first.
   e.g. [Final Fantasy](https://en.wikipedia.org/wiki/Final_Fantasy),
   [Dark Souls](https://en.wikipedia.org/wiki/Dark_Souls)

Legend of Swarkland aims to achieve all those goals without relying on health bars.
Here are some alternative solutions:

1. For mistake forgiveness, a mistake may inflict you with a slow inevitable death
   that you can recover from by using life saving resources such as drinking potions or calling in a divine favor.
   In practice mode, you can always use the undo button.
   In hardcore mode, you don't necessarily need much mistake forgiveness.
2. There will be plenty of resources to manage strategically that aren't a health bar.
3. Elongating boss encounters can be done through boss design.
   Perhaps the first hit lops off an arm, which changes the available attacks and enrages the AI.
   Perhaps the goal of the boss encounter is to navigate them around obstacles toward a pit of lava and get them to fall in.
   And it's ok for there to be obscure powerful strategies that instakill certain bosses, like throwing cockatrice eggs.
4. Level check barriers can be done by making an enemy simply immune to all of your available attacks.
   Then instead of theoretically being able to grind in place and eventually get past the enemy,
   you have to go looking for a weapon or trick that will get past them.
   This makes it more of a metroidvania or puzzle barrier than a big-numbers check.

Legend of Swarkland will have lots of 1-hit kills when the attacks are with weapons like swords or axes.
More variety in combat can happen in the form of inflicting status conditions rather than outright killing opponents.
Perhaps a giant mosquito infects you with a disease, and then attempts to flee.
Perhaps a dog bite causes a bleeding condition, and subsequent attacks can cause limping.
Perhaps getting hit with a thrown rock causes a dizzy condition.

There is also lots of opportunity for content in enemy AI design.
Instead of everything being dead set on killing humans, perhaps some monsters are simply territorial, and running away deaggros them.
Perhaps some monsters will retreat from attacking you if you demonstrate to them that you are a formidable threat.
There can even be monsters that naturally fight each other, like a mongoose attacking a snake, or elves attacking dwarves.

By resisting the game design tropes of big number stats, Legend of Swarkland will naturally avoid boring filler content
like NetHack's [rock troll](https://nethackwiki.com/wiki/Troll#Rock_Troll), which is just a troll but with bigger numbers.
Every addition to the game must be justified with unique content.

Another more subtle source of grinding in games does not take the form of big numbers but of big spaces.
Walking down a hallway in NetHack can cause a "Death by Boredom" if you hold down the movement key
and don't react quickly enough when a floating eye shows up in the darkness just in front of you.
In this example, pressing the "move to the right" button repeatedly is considered grinding.

NetHack also has a problem with Gehennom having too many long boring maze levels.
A good strategy for Gehennom is to meticulously pickaxe through the walls to connect the stairs with a straight shot path.
The amount of time that takes depends on the number of randomly generated maze levels in Gehennom,
which is something the devs just typed into a text file.
There's no difference in the amount of content between 5 maze levels and 10 maze levels.
There's only a difference in the amount of time it takes a player to get through them.

Legend of Swarkland will try to avoid long corridors, and instead have smaller, more interesting spaces where every move counts.

Legend of Swarkland also intends to have a large open world design, so to avoid long boring walks,
there will need to be lots of fast travel available.
Teleportation is easy to implement, but I want to try to avoid teleportation for the sake of balance and fairness.
Perhaps you will open [Portal](https://en.wikipedia.org/wiki/Portal_%28series%29)-style portals
that connect areas that you or anyone else can walk through.
(This would also mean that sufficiently capable enemies could also open such portals.)

### Seeded randomness

A well established feature of modern roguelikes is seeded playthroughs.
This is useful if players wish to race each other in real time
and don't want the random nature of roguelikes to favor one player over another.
A seeded playthrough will guarantee that both racers see the same world
with the same items and enemies generated in the same places.

Legend of Swarkland will have multiple uses for randomness in the game,
and they should all be deterministic from the initial world seed and the sequence of player decisions.
In this way, any playthrough of the game can be recorded and replayed using only the world seed and the sequence of decisions.
This means that TAS tools are effectively builtin in the form of practice mode.
In the now-deleted code of Legend of Swarkland 4.5.0,
this reply feature powered the mechanism for restoring saved games and even for running unit tests.

One of the notable implications of deterministic randomness and client/server separation
is that enemy AIs have to be specifically programmed to be deterministic as well,
especially in the event that a player in practice mode hits undo.

### Open source

Legend of Swarkland is a passion project, not a business venture.
I will always keep my contributions to this project free and open source.

Legend of Swarkland is licensed under the MIT License. See `LICENSE.md`.

## Roadmap

### 5.2.0

???

## Version History

### 5.1.0

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
 * introduced a bug where the collision resolution can infinite loop crash when too many individuals bump into each other near a wall.

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
