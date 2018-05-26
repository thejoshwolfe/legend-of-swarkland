#ifndef PRIMITIVES_HPP
#define PRIMITIVES_HPP

#include <stdint.h>
#include "bitfield.hpp"

// welcome to the result of Josh's frustration with C's back-reference-only design.
// this file contains all the enums and primitive typedefs that someone might want to refer to.

enum ThingType {
    ThingType_INDIVIDUAL,
    ThingType_WAND,
    ThingType_POTION,
    ThingType_BOOK,
    ThingType_WEAPON,

    ThingType_COUNT,
};

enum WandDescriptionId {
    WandDescriptionId_BONE_WAND,
    WandDescriptionId_GOLD_WAND,
    WandDescriptionId_PLASTIC_WAND,
    WandDescriptionId_COPPER_WAND,
    WandDescriptionId_PURPLE_WAND,
    WandDescriptionId_SHINY_BONE_WAND,
    WandDescriptionId_SHINY_GOLD_WAND,
    WandDescriptionId_SHINY_PLASTIC_WAND,
    WandDescriptionId_SHINY_COPPER_WAND,
    WandDescriptionId_SHINY_PURPLE_WAND,

    WandDescriptionId_COUNT,
    WandDescriptionId_UNSEEN,
};
enum WandId {
    WandId_WAND_OF_CONFUSION,
    WandId_WAND_OF_DIGGING,
    WandId_WAND_OF_MAGIC_MISSILE,
    WandId_WAND_OF_SPEED,
    WandId_WAND_OF_REMEDY,
    WandId_WAND_OF_BLINDING,
    WandId_WAND_OF_FORCE,
    WandId_WAND_OF_INVISIBILITY,
    WandId_WAND_OF_MAGIC_BULLET,
    WandId_WAND_OF_SLOWING,

    WandId_COUNT,
    WandId_UNKNOWN,
};

enum PotionDescriptionId {
    PotionDescriptionId_BLUE_POTION,
    PotionDescriptionId_GREEN_POTION,
    PotionDescriptionId_RED_POTION,
    PotionDescriptionId_YELLOW_POTION,
    PotionDescriptionId_ORANGE_POTION,
    PotionDescriptionId_PURPLE_POTION,
    PotionDescriptionId_GLITTERY_BLUE_POTION,
    PotionDescriptionId_GLITTERY_GREEN_POTION,

    PotionDescriptionId_COUNT,
    PotionDescriptionId_UNSEEN,
};
enum PotionId {
    PotionId_POTION_OF_HEALING,
    PotionId_POTION_OF_POISON,
    PotionId_POTION_OF_ETHEREAL_VISION,
    PotionId_POTION_OF_COGNISCOPY,
    PotionId_POTION_OF_BLINDNESS,
    PotionId_POTION_OF_INVISIBILITY,
    PotionId_POTION_OF_BURROWING,
    PotionId_POTION_OF_LEVITATION,

    PotionId_COUNT,
    PotionId_UNKNOWN,
};

enum BookDescriptionId {
    BookDescriptionId_PURPLE_BOOK,
    BookDescriptionId_BLUE_BOOK,
    BookDescriptionId_RED_BOOK,
    BookDescriptionId_GREEN_BOOK,
    BookDescriptionId_YELLOW_BOOK,

    BookDescriptionId_COUNT,
    BookDescriptionId_UNSEEN,
};
enum BookId {
    BookId_SPELLBOOK_OF_MAGIC_BULLET,
    BookId_SPELLBOOK_OF_SPEED,
    BookId_SPELLBOOK_OF_MAPPING,
    BookId_SPELLBOOK_OF_FORCE,
    BookId_SPELLBOOK_OF_ASSUME_FORM,

    BookId_COUNT,
    BookId_UNKNOWN,
};
enum WeaponId {
    WeaponId_DAGGER,
    WeaponId_BATTLEAXE,

    WeaponId_COUNT,
    WeaponId_UNKNOWN,
};

enum SpeciesId {
    SpeciesId_HUMAN,
    SpeciesId_OGRE,
    SpeciesId_LICH,
    SpeciesId_SHAPESHIFTER,
    SpeciesId_PINK_BLOB,
    SpeciesId_AIR_ELEMENTAL,
    SpeciesId_TAR_ELEMENTAL,
    SpeciesId_DOG,
    SpeciesId_ANT,
    SpeciesId_BEE,
    SpeciesId_BEETLE,
    SpeciesId_SCORPION,
    SpeciesId_SNAKE,
    SpeciesId_COBRA,

    SpeciesId_COUNT,
    SpeciesId_UNSEEN,
};

enum DecisionMakerType {
    DecisionMakerType_AI,
    DecisionMakerType_PLAYER,

    DecisionMakerType_COUNT,
};

typedef uint8_t VisionTypes;
enum VisionTypesBits {
    VisionTypes_NORMAL = 0x1,
    VisionTypes_ETHEREAL = 0x2,
    VisionTypes_COGNISCOPY = 0x4,
    VisionTypes_COLOCATION = 0x8,
    VisionTypes_REACH_AND_TOUCH = 0x10,
};

enum Mind {
    Mind_NONE,
    Mind_BEAST,
    Mind_SAVAGE,
    Mind_CIVILIZED,
};

enum AbilityId {
    AbilityId_SPIT_BLINDING_VENOM,
    AbilityId_THROW_TAR,
    AbilityId_ASSUME_FORM,
    AbilityId_LUNGE_ATTACK,

    AbilityId_COUNT,
};

enum TutorialPrompt {
    TutorialPrompt_QUIT,

    TutorialPrompt_MOVE_HIT,
    TutorialPrompt_INVENTORY,
    TutorialPrompt_ABILITY,
    TutorialPrompt_PICK_UP,
    TutorialPrompt_GO_DOWN,
    TutorialPrompt_GROUND_ACTION,

    TutorialPrompt_REST,
    TutorialPrompt_MOVE_CURSOR,
    TutorialPrompt_MOVE_CURSOR_MENU,
    TutorialPrompt_DIRECTION,
    TutorialPrompt_YOURSELF,
    TutorialPrompt_MENU_ACTION,
    TutorialPrompt_ACCEPT,
    TutorialPrompt_ACCEPT_SUBMENU,

    TutorialPrompt_CANCEL,
    TutorialPrompt_BACK,

    TutorialPrompt_WHATS_THIS,

    TutorialPrompt_COUNT,
};
typedef BitField<TutorialPrompt_COUNT>::Type TutorialPromptBits;

#endif
