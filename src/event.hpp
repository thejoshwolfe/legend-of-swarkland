#ifndef EVENT_HPP
#define EVENT_HPP

#include "thing.hpp"

struct Event {
    enum Type {
        THE_INDIVIDUAL,
        INDIVIDUAL_AND_LOCATION,
        MOVE,
        TWO_INDIVIDUAL,
        INDIVIDUAL_AND_ITEM,
        WAND_HIT,
        USE_POTION,
        POLYMORPH,
        ITEM_AND_LOCATION,
    };
    Type type;

    struct IndividualAndLocationData {
        enum Id {
            BUMP_INTO_LOCATION,
            ATTACK_LOCATION,
        };
        Id id;
        uint256 actor;
        Coord location;
        bool is_air;
    };
    struct MoveData {
        uint256 actor;
        Coord old_location;
        Coord new_location;
    };
    struct TwoIndividualData {
        enum Id {
            BUMP_INTO_INDIVIDUAL,
            ATTACK_INDIVIDUAL,
            MELEE_KILL,
        };
        Id id;
        uint256 actor;
        uint256 target;
    };
    struct IndividualAndItemData {
        enum Id {
            ZAP_WAND,
            ZAP_WAND_NO_CHARGES,
            WAND_DISINTEGRATES,
            THROW_ITEM,
            ITEM_HITS_INDIVIDUAL,
            INDIVIDUAL_PICKS_UP_ITEM,
            INDIVIDUAL_SUCKS_UP_ITEM,
        };
        Id id;
        uint256 individual;
        uint256 item;
        Coord location;
    };
    struct ItemAndLocationData {
        enum Id {
            WAND_EXPLODES,
            ITEM_HITS_WALL,
            ITEM_DROPS_TO_THE_FLOOR,
        };
        Id id;
        uint256 item;
        Coord location;
    };
    struct TheIndividualData {
        enum Id {
            POISONED,
            NO_LONGER_CONFUSED,
            NO_LONGER_FAST,
            NO_LONGER_HAS_ETHEREAL_VISION,
            NO_LONGER_COGNISCOPIC,
            NO_LONGER_BLIND,
            NO_LONGER_POISONED,
            NO_LONGER_INVISIBLE,
            APPEAR,
            TURN_INVISIBLE,
            LEVEL_UP,
            DIE,
        };
        Id id;
        uint256 individual;
    };

    TheIndividualData & the_individual_data() {
        check_data_type(THE_INDIVIDUAL);
        return _data._the_individual;
    }

    IndividualAndLocationData & individual_and_location_data() {
        check_data_type(INDIVIDUAL_AND_LOCATION);
        return _data._individual_and_location;
    }

    MoveData & move_data() {
        check_data_type(MOVE);
        return _data._move;
    }

    TwoIndividualData & two_individual_data() {
        check_data_type(TWO_INDIVIDUAL);
        return _data._two_individual;
    }

    IndividualAndItemData & individual_and_item_data() {
        check_data_type(INDIVIDUAL_AND_ITEM);
        return _data._individual_and_item;
    }

    struct WandHitData {
        WandId observable_effect;
        bool is_explosion;
        uint256 target;
        Coord location;
    };
    WandHitData & wand_hit_data() {
        check_data_type(WAND_HIT);
        return _data._wand_hit;
    }

    struct UsePotionData {
        uint256 item_id;
        PotionId effect;
        bool is_breaking;
        uint256 target_id;
        Coord location;
    };
    UsePotionData & use_potion_data() {
        check_data_type(USE_POTION);
        return _data._use_potion;
    }

    struct PolymorphData {
        uint256 individual;
        SpeciesId new_species;
    };
    PolymorphData & polymorph_data() {
        check_data_type(POLYMORPH);
        return _data._polymorph;
    };

    ItemAndLocationData & item_and_location_data() {
        check_data_type(ITEM_AND_LOCATION);
        return _data._item_and_location;
    }

    static inline Event move(Thing mover, Coord from) {
        return move_event(mover->id, from, mover->location);
    }

    static inline Event attack_individual(Thing attacker, Thing target) {
        return two_individual_type_event(TwoIndividualData::ATTACK_INDIVIDUAL, attacker->id, target->id);
    }
    static inline Event attack_location(Thing attacker, Coord location, bool is_air) {
        return individual_and_location_event(IndividualAndLocationData::ATTACK_LOCATION, attacker->id, location, is_air);
    }

    static inline Event zap_wand(Thing wand_wielder, Thing item) {
        return individual_and_item_type_event(IndividualAndItemData::ZAP_WAND, wand_wielder->id, item->id, wand_wielder->location);
    }
    static inline Event wand_zap_no_charges(Thing wand_wielder, Thing item) {
        return individual_and_item_type_event(IndividualAndItemData::ZAP_WAND_NO_CHARGES, wand_wielder->id, item->id, wand_wielder->location);
    }
    static inline Event wand_disintegrates(Thing wand_wielder, Thing item) {
        return individual_and_item_type_event(IndividualAndItemData::WAND_DISINTEGRATES, wand_wielder->id, item->id, wand_wielder->location);
    }
    static inline Event wand_explodes(uint256 item_id, Coord location) {
        return item_and_location_type_event(ItemAndLocationData::WAND_EXPLODES, item_id, location);
    }
    static inline Event wand_hit(WandId observable_effect, bool is_explosion, uint256 target, Coord location) {
        Event result;
        result.type = WAND_HIT;
        result.wand_hit_data() = {
            observable_effect,
            is_explosion,
            target,
            location,
        };
        return result;
    }

    static Event bump_into_individual(Thing actor, Thing target) {
        return two_individual_type_event(TwoIndividualData::BUMP_INTO_INDIVIDUAL, actor->id, target->id);
    }
    static Event bump_into_location(Thing actor, Coord location, bool is_air) {
        return individual_and_location_event(IndividualAndLocationData::BUMP_INTO_LOCATION, actor->id, location, is_air);
    }

    static Event throw_item(Thing thrower, uint256 item_id) {
        return individual_and_item_type_event(IndividualAndItemData::THROW_ITEM, thrower->id, item_id, thrower->location);
    }
    static Event item_hits_individual(uint256 item_id, Thing individual) {
        return individual_and_item_type_event(IndividualAndItemData::ITEM_HITS_INDIVIDUAL, individual->id, item_id, individual->location);
    }
    static Event item_hits_wall(uint256 item_id, Coord location) {
        return item_and_location_type_event(ItemAndLocationData::ITEM_HITS_WALL, item_id, location);
    }

    static Event use_potion(uint256 item_id, PotionId effect, bool is_breaking, uint256 target_id, Coord location) {
        Event result;
        result.type = USE_POTION;
        result.use_potion_data() = {
            item_id,
            effect,
            is_breaking,
            target_id,
            location,
        };
        return result;
    }

    static inline Event poisoned(Thing individual) {
        return event_individual(TheIndividualData::POISONED, individual->id);
    }
    static inline Event no_longer_confused(Thing individual) {
        return event_individual(TheIndividualData::NO_LONGER_CONFUSED, individual->id);
    }
    static inline Event no_longer_fast(Thing individual) {
        return event_individual(TheIndividualData::NO_LONGER_FAST, individual->id);
    }
    static inline Event no_longer_has_ethereal_vision(Thing individual) {
        return event_individual(TheIndividualData::NO_LONGER_HAS_ETHEREAL_VISION, individual->id);
    }
    static inline Event no_longer_cogniscopic(Thing individual) {
        return event_individual(TheIndividualData::NO_LONGER_COGNISCOPIC, individual->id);
    }
    static inline Event no_longer_blind(Thing individual) {
        return event_individual(TheIndividualData::NO_LONGER_BLIND, individual->id);
    }
    static inline Event no_longer_poisoned(Thing individual) {
        return event_individual(TheIndividualData::NO_LONGER_POISONED, individual->id);
    }
    static inline Event no_longer_invisible(Thing individual) {
        return event_individual(TheIndividualData::NO_LONGER_INVISIBLE, individual->id);
    }

    static inline Event appear(Thing new_guy) {
        return event_individual(TheIndividualData::APPEAR, new_guy->id);
    }
    static inline Event turn_invisible(Thing individual) {
        return event_individual(TheIndividualData::TURN_INVISIBLE, individual->id);
    }
    static inline Event level_up(uint256 individual_id) {
        return event_individual(TheIndividualData::LEVEL_UP, individual_id);
    }
    static inline Event die(uint256 deceased_id) {
        return event_individual(TheIndividualData::DIE, deceased_id);
    }
    static inline Event melee_kill(Thing attacker, Thing deceased) {
        return two_individual_type_event(TwoIndividualData::MELEE_KILL, attacker->id, deceased->id);
    }

    static inline Event polymorph(Thing shapeshifter, SpeciesId new_species) {
        Event result;
        result.type = POLYMORPH;
        PolymorphData & data = result.polymorph_data();
        data.individual = shapeshifter->id;
        data.new_species = new_species;
        return result;
    }


    static inline Event item_drops_to_the_floor(Thing item) {
        return item_and_location_type_event(ItemAndLocationData::ITEM_DROPS_TO_THE_FLOOR, item->id, item->location);
    }
    static inline Event individual_picks_up_item(Thing individual, uint256 item_id) {
        return individual_and_item_type_event(IndividualAndItemData::INDIVIDUAL_PICKS_UP_ITEM, individual->id, item_id, individual->location);
    }
    static inline Event individual_sucks_up_item(Thing individual, Thing item) {
        return individual_and_item_type_event(IndividualAndItemData::INDIVIDUAL_SUCKS_UP_ITEM, individual->id, item->id, individual->location);
    }

private:
    static inline Event event_individual(TheIndividualData::Id id, uint256 individual_id) {
        Event result;
        result.type = THE_INDIVIDUAL;
        result.the_individual_data() = {
            id,
            individual_id,
        };
        return result;
    }
    static inline Event individual_and_location_event(IndividualAndLocationData::Id id, uint256 actor, Coord location, bool is_air) {
        Event result;
        result.type = INDIVIDUAL_AND_LOCATION;
        result.individual_and_location_data() = {
            id,
            actor,
            location,
            is_air,
        };
        return result;
    }
    static inline Event move_event(uint256 actor, Coord old_location, Coord new_location) {
        Event result;
        result.type = MOVE;
        result.move_data() = {
            actor,
            old_location,
            new_location,
        };
        return result;
    }
    static inline Event two_individual_type_event(TwoIndividualData::Id id, uint256 actor, uint256 target) {
        Event result;
        result.type = TWO_INDIVIDUAL;
        result.two_individual_data() = {
            id,
            actor,
            target,
        };
        return result;
    }
    static inline Event individual_and_item_type_event(IndividualAndItemData::Id id, uint256 individual_id, uint256 item_id, Coord location) {
        Event result;
        result.type = INDIVIDUAL_AND_ITEM;
        result.individual_and_item_data() = {
            id,
            individual_id,
            item_id,
            location,
        };
        return result;
    }
    static inline Event item_and_location_type_event(ItemAndLocationData::Id id, uint256 item, Coord location) {
        Event result;
        result.type = ITEM_AND_LOCATION;
        result.item_and_location_data() = {
            id,
            item,
            location,
        };
        return result;
    }

    union {
        TheIndividualData _the_individual;
        IndividualAndLocationData _individual_and_location;
        MoveData _move;
        TwoIndividualData _two_individual;
        IndividualAndItemData _individual_and_item;
        WandHitData _wand_hit;
        UsePotionData _use_potion;
        PolymorphData _polymorph;
        ItemAndLocationData _item_and_location;
    } _data;

    void check_data_type(Type supposed_data_type) const {
        if (supposed_data_type != type)
            panic("wrong data type");
    }
};

bool can_see_thing(Thing observer, uint256 target_id, Coord target_location);
bool can_see_thing(Thing observer, uint256 target_id);
PerceivedThing record_perception_of_thing(Thing observer, uint256 target_id);
void publish_event(Event event);
void publish_event(Event event, IdMap<WandDescriptionId> * perceived_current_zapper);

#endif
