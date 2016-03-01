#ifndef ACTION_HPP
#define ACTION_HPP

#include "hashtable.hpp"
#include "geometry.hpp"

struct Game;

struct Action {
    enum Id {
        MOVE,
        WAIT,
        ATTACK,
        ZAP,
        POSITION_ITEM,
        QUAFF,
        READ_BOOK,
        THROW,
        ABILITY,
        GO_DOWN,

        CHEATCODE_HEALTH_BOOST,
        CHEATCODE_KILL,
        CHEATCODE_POLYMORPH,
        CHEATCODE_GENERATE_MONSTER,
        CHEATCODE_WISH,
        CHEATCODE_IDENTIFY,
        CHEATCODE_GO_DOWN,
        CHEATCODE_GAIN_LEVEL,

        COUNT,
        // only a player can be undecided
        UNDECIDED,
        // this is a pseudo-action that activates an intermediate AI
        // that waits until something interesting happens.
        AUTO_WAIT,
    };
    struct ItemAndSlot {
        uint256 item;
        InventorySlot slot;
    };
    struct CoordAndItem {
        uint256 item;
        Coord coord;
    };
    struct Thing {
        ThingType thing_type;
        union {
            SpeciesId species_id;
            WandId wand_id;
            PotionId potion_id;
            BookId book_id;
        };
    };
    struct GenerateMonster {
        SpeciesId species;
        DecisionMakerType decision_maker;
        Coord location;
    };
    struct AbilityData {
        AbilityId ability_id;
        Coord direction;
    };
    enum Layout {
        Layout_VOID,
        Layout_COORD,
        Layout_ITEM,
        Layout_ITEM_AND_SLOT,
        Layout_COORD_AND_ITEM,
        Layout_THING,
        Layout_SPECIES,
        Layout_GENERATE_MONSTER,
        Layout_ABILITY,
    };

    Id id;

    Layout get_layout() const {
        return get_layout(id);
    }
    Coord           const & coord()            const { assert(get_layout() == Layout_COORD);            return _coord; }
    Coord                 & coord()                  { assert(get_layout() == Layout_COORD);            return _coord; }
    uint256         const & item()             const { assert(get_layout() == Layout_ITEM);             return _item; }
    uint256               & item()                   { assert(get_layout() == Layout_ITEM);             return _item; }
    ItemAndSlot     const & item_and_slot()    const { assert(get_layout() == Layout_ITEM_AND_SLOT);    return _item_and_slot; }
    ItemAndSlot           & item_and_slot()          { assert(get_layout() == Layout_ITEM_AND_SLOT);    return _item_and_slot; }
    CoordAndItem    const & coord_and_item()   const { assert(get_layout() == Layout_COORD_AND_ITEM);   return _coord_and_item; }
    CoordAndItem          & coord_and_item()         { assert(get_layout() == Layout_COORD_AND_ITEM);   return _coord_and_item; }
    Thing           const & thing()            const { assert(get_layout() == Layout_THING);            return _thing; }
    Thing                 & thing()                  { assert(get_layout() == Layout_THING);            return _thing; }
    SpeciesId       const & species()          const { assert(get_layout() == Layout_SPECIES);          return _species; }
    SpeciesId             & species()                { assert(get_layout() == Layout_SPECIES);          return _species; }
    GenerateMonster const & generate_monster() const { assert(get_layout() == Layout_GENERATE_MONSTER); return _generate_monster; }
    GenerateMonster       & generate_monster()       { assert(get_layout() == Layout_GENERATE_MONSTER); return _generate_monster; }
    AbilityData     const & ability()          const { assert(get_layout() == Layout_ABILITY);          return _ability; }
    AbilityData           & ability()                { assert(get_layout() == Layout_ABILITY);          return _ability; }

    static Action move(Coord direction) {
        return init(MOVE, direction);
    }
    static Action wait() {
        return init(WAIT);
    }
    static Action attack(Coord direction) {
        return init(ATTACK, direction);
    }
    static Action undecided() {
        return init(UNDECIDED);
    }
    static Action auto_wait() {
        return init(AUTO_WAIT);
    }
    static Action zap(uint256 item_id, Coord direction) {
        return init(ZAP, direction, item_id);
    }
    static Action read_book(uint256 item_id, Coord direction) {
        return init(READ_BOOK, direction, item_id);
    }
    static Action position_item(uint256 item_id, InventorySlot slot) {
        return init(POSITION_ITEM, item_id, slot);
    }
    static Action quaff(uint256 item_id) {
        return init(QUAFF, item_id);
    }
    static Action throw_(uint256 item_id, Coord direction) {
        return init(THROW, direction, item_id);
    }
    static Action ability(AbilityId ability_id, Coord direction) {
        return init(ABILITY, ability_id, direction);
    }

    static Action go_down() {
        return init(GO_DOWN);
    }

    static Action cheatcode_health_boost() {
        return init(CHEATCODE_HEALTH_BOOST);
    }
    static Action cheatcode_kill(uint256 individual) {
        return init(CHEATCODE_KILL, individual);
    }
    static Action cheatcode_polymorph(SpeciesId species_id) {
        return init(CHEATCODE_POLYMORPH, species_id);
    }
    static Action cheatcode_generate_monster(SpeciesId species_id, DecisionMakerType decision_maker, Coord location) {
        return init(CHEATCODE_GENERATE_MONSTER, species_id, decision_maker, location);
    }
    static Action cheatcode_wish(WandId wand_id) {
        return init(CHEATCODE_WISH, wand_id);
    }
    static Action cheatcode_wish(PotionId potion_id) {
        return init(CHEATCODE_WISH, potion_id);
    }
    static Action cheatcode_wish(BookId book_id) {
        return init(CHEATCODE_WISH, book_id);
    }
    static Action cheatcode_identify() {
        return init(CHEATCODE_IDENTIFY);
    }
    static Action cheatcode_go_down() {
        return init(CHEATCODE_GO_DOWN);
    }
    static Action cheatcode_gain_level() {
        return init(CHEATCODE_GAIN_LEVEL);
    }

    static Action init(Id id) {
        assert(get_layout(id) == Layout_VOID);
        Action result;
        result.id = id;
        return result;
    }
    static Action init(Id id, Coord coord) {
        assert(get_layout(id) == Layout_COORD);
        Action result;
        result.id = id;
        result._coord = coord;
        return result;
    }
    static Action init(Id id, uint256 item) {
        assert(get_layout(id) == Layout_ITEM);
        Action result;
        result.id = id;
        result._item = item;
        return result;
    }
    static Action init(Id id, uint256 item, InventorySlot slot) {
        assert(get_layout(id) == Layout_ITEM_AND_SLOT);
        Action result;
        result.id = id;
        result._item_and_slot.item = item;
        result._item_and_slot.slot = slot;
        return result;
    }
    static Action init(Id id, Coord coord, uint256 item) {
        assert(get_layout(id) == Layout_COORD_AND_ITEM);
        Action result;
        result.id = id;
        result._coord_and_item.coord = coord;
        result._coord_and_item.item = item;
        return result;
    }
    static Action init(Id id, SpeciesId species_id) {
        assert(get_layout(id) == Layout_SPECIES);
        Action result;
        result.id = id;
        result._species = species_id;
        return result;
    }
    static Action init(Id id, WandId wand_id) {
        assert(get_layout(id) == Layout_THING);
        Action result;
        result.id = id;
        result._thing.thing_type = ThingType_WAND;
        result._thing.wand_id = wand_id;
        return result;
    }
    static Action init(Id id, PotionId potion_id) {
        assert(get_layout(id) == Layout_THING);
        Action result;
        result.id = id;
        result._thing.thing_type = ThingType_POTION;
        result._thing.potion_id = potion_id;
        return result;
    }
    static Action init(Id id, BookId book_id) {
        assert(get_layout(id) == Layout_THING);
        Action result;
        result.id = id;
        result._thing.thing_type = ThingType_BOOK;
        result._thing.book_id = book_id;
        return result;
    }
    static Action init(Id id, SpeciesId species_id, DecisionMakerType decision_maker, Coord location) {
        assert(get_layout(id) == Layout_GENERATE_MONSTER);
        Action result;
        result.id = id;
        result._generate_monster.species = species_id;
        result._generate_monster.decision_maker = decision_maker;
        result._generate_monster.location = location;
        return result;
    }
    static Action init(Id id, AbilityId ability_id, Coord direction) {
        assert(get_layout(id) == Layout_ABILITY);
        Action result;
        result.id = id;
        result._ability.ability_id = ability_id;
        result._ability.direction = direction;
        return result;
    }

private:
    union {
        uint256 _item;
        Coord _coord;
        ItemAndSlot _item_and_slot;
        CoordAndItem _coord_and_item;
        Thing _thing;
        SpeciesId _species;
        GenerateMonster _generate_monster;
        AbilityData _ability;
    };

    static Layout get_layout(Id id) {
        switch (id) {
            case WAIT:
            case GO_DOWN:
            case UNDECIDED:
            case AUTO_WAIT:
                return Layout_VOID;
            case MOVE:
            case ATTACK:
                return Layout_COORD;
            case POSITION_ITEM:
                return Layout_ITEM_AND_SLOT;
            case QUAFF:
                return Layout_ITEM;
            case ZAP:
            case READ_BOOK:
            case THROW:
                return Layout_COORD_AND_ITEM;
            case ABILITY:
                return Layout_ABILITY;

            case CHEATCODE_HEALTH_BOOST:
            case CHEATCODE_IDENTIFY:
            case CHEATCODE_GO_DOWN:
            case CHEATCODE_GAIN_LEVEL:
                return Layout_VOID;
            case CHEATCODE_POLYMORPH:
                return Layout_SPECIES;
            case CHEATCODE_KILL:
                return Layout_ITEM;
            case CHEATCODE_WISH:
                return Layout_THING;
            case CHEATCODE_GENERATE_MONSTER:
                return Layout_GENERATE_MONSTER;

            case COUNT:
                unreachable();
        }
        unreachable();
    }
};
static inline bool operator==(const Action & a, const Action &  b) {
    if (a.id != b.id)
        return false;
    Action::Layout layout = a.get_layout();
    if (layout != b.get_layout())
        return false;
    switch (layout) {
        case Action::Layout_VOID:
            return true;
        case Action::Layout_COORD:
            return a.coord() == b.coord();
        case Action::Layout_ITEM:
            return a.item() == b.item();
        case Action::Layout_ITEM_AND_SLOT:
            if (a.item_and_slot().item != b.item_and_slot().item)
                return false;
            if (a.item_and_slot().slot != b.item_and_slot().slot)
                return false;
            return true;
        case Action::Layout_COORD_AND_ITEM:
            if (a.coord_and_item().coord != b.coord_and_item().coord)
                return false;
            if (a.coord_and_item().item != b.coord_and_item().item)
                return false;
            return true;
        case Action::Layout_THING: {
            const Action::Thing & a_data = a.thing();
            const Action::Thing & b_data = b.thing();
            if (a_data.thing_type != b_data.thing_type)
                return false;
            switch (a_data.thing_type) {
                case ThingType_INDIVIDUAL:
                    if (a_data.species_id != b_data.species_id)
                        return false;
                    return true;
                case ThingType_WAND:
                    if (a_data.wand_id != b_data.wand_id)
                        return false;
                    return true;
                case ThingType_POTION:
                    if (a_data.potion_id != b_data.potion_id)
                        return false;
                    return true;
                case ThingType_BOOK:
                    if (a_data.book_id != b_data.book_id)
                        return false;
                    return true;

                case ThingType_COUNT:
                    unreachable();
            }
            unreachable();
        }
        case Action::Layout_SPECIES: {
            if (a.species() != b.species())
                return false;
            return true;
        }
        case Action::Layout_GENERATE_MONSTER: {
            const Action::GenerateMonster & a_data = a.generate_monster();
            const Action::GenerateMonster & b_data = b.generate_monster();
            if (a_data.species != b_data.species)
                return false;
            if (a_data.decision_maker != b_data.decision_maker)
                return false;
            if (a_data.location != b_data.location)
                return false;
            return true;
        }
        case Action::Layout_ABILITY: {
            const Action::AbilityData & a_data = a.ability();
            const Action::AbilityData & b_data = b.ability();
            if (a_data.ability_id != b_data.ability_id)
                return false;
            if (a_data.direction != b_data.direction)
                return false;
            return true;
        }
    }
    unreachable();
}
static inline bool operator!=(const Action &  a, const Action &  b) {
    return !(a == b);
}

#endif
