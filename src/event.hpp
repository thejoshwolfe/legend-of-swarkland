#ifndef EVENT_HPP
#define EVENT_HPP

#include "thing.hpp"

struct Event {
    enum Type {
        THE_INDIVIDUAL,
        TWO_INDIVIDUAL,
        INDIVIDUAL_AND_ITEM,
        WAND_HIT,
        USE_POTION,
        POLYMORPH,
        ITEM_AND_LOCATION,
    };
    Type type;

    struct TwoIndividualData {
        enum Id {
            MOVE,
            BUMP_INTO,
            ATTACK,
        };
        Id id;
        uint256 actor;
        Coord actor_location;
        uint256 target;
        Coord target_location;
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
    };
    struct ItemAndLocationData {
        enum Id {
            WAND_EXPLODES,
            ITEM_HITS_WALL,
            ITEM_HITS_SOMETHING,
            ITEM_DROPS_TO_THE_FLOOR,
            SOMETHING_PICKS_UP_ITEM,
            SOMETHING_SUCKS_UP_ITEM,
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
            NO_LONGER_POISONED,
            APPEAR,
            TURN_INVISIBLE,
            DISAPPEAR,
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

    static inline Event move(Thing mover, Coord from, Coord to) {
        return two_individual_type_event(TwoIndividualData::MOVE, mover->id, from, uint256::zero(), to);
    }

    static inline Event attack(Thing attacker, uint256 target_id, Coord target_location) {
        return two_individual_type_event(TwoIndividualData::ATTACK, attacker->id, attacker->location, target_id, target_location);
    }

    static inline Event zap_wand(Thing wand_wielder, Thing item) {
        return individual_and_item_type_event(IndividualAndItemData::ZAP_WAND, wand_wielder->id, item->id);
    }
    static inline Event wand_zap_no_charges(Thing wand_wielder, Thing item) {
        return individual_and_item_type_event(IndividualAndItemData::ZAP_WAND_NO_CHARGES, wand_wielder->id, item->id);
    }
    static inline Event wand_disintegrates(Thing wand_wielder, Thing item) {
        return individual_and_item_type_event(IndividualAndItemData::WAND_DISINTEGRATES, wand_wielder->id, item->id);
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

    static Event bump_into(uint256 actor_id, Coord from_location, uint256 bumpee_id, Coord bumpee_location) {
        return two_individual_type_event(TwoIndividualData::BUMP_INTO, actor_id, from_location, bumpee_id, bumpee_location);
    }

    static Event throw_item(uint256 thrower_id, uint256 item_id) {
        return individual_and_item_type_event(IndividualAndItemData::THROW_ITEM, thrower_id, item_id);
    }
    static Event item_hits_individual(uint256 item_id, uint256 target_id) {
        return individual_and_item_type_event(IndividualAndItemData::ITEM_HITS_INDIVIDUAL, target_id, item_id);
    }
    static Event item_hits_wall(uint256 item_id, Coord location) {
        return item_and_location_type_event(ItemAndLocationData::ITEM_HITS_WALL, item_id, location);
    }
    static Event item_hits_something(uint256 item_id, Coord location) {
        return item_and_location_type_event(ItemAndLocationData::ITEM_HITS_SOMETHING, item_id, location);
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
    static inline Event no_longer_poisoned(Thing individual) {
        return event_individual(TheIndividualData::NO_LONGER_POISONED, individual->id);
    }

    static inline Event appear(Thing new_guy) {
        return event_individual(TheIndividualData::APPEAR, new_guy->id);
    }
    static inline Event turn_invisible(Thing individual) {
        return event_individual(TheIndividualData::TURN_INVISIBLE, individual->id);
    }
    static inline Event disappear(uint256 individual_id) {
        return event_individual(TheIndividualData::DISAPPEAR, individual_id);
    }
    static inline Event level_up(uint256 individual_id) {
        return event_individual(TheIndividualData::LEVEL_UP, individual_id);
    }
    static inline Event die(Thing deceased) {
        return event_individual(TheIndividualData::DIE, deceased->id);
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
    static inline Event individual_picks_up_item(Thing individual, Thing item) {
        return individual_and_item_type_event(IndividualAndItemData::INDIVIDUAL_PICKS_UP_ITEM, individual->id, item->id);
    }
    static inline Event something_picks_up_item(uint256 item, Coord location) {
        return item_and_location_type_event(ItemAndLocationData::SOMETHING_PICKS_UP_ITEM, item, location);
    }
    static inline Event individual_sucks_up_item(Thing individual, Thing item) {
        return individual_and_item_type_event(IndividualAndItemData::INDIVIDUAL_SUCKS_UP_ITEM, individual->id, item->id);
    }
    static inline Event something_sucks_up_item(uint256 item, Coord location) {
        return item_and_location_type_event(ItemAndLocationData::SOMETHING_SUCKS_UP_ITEM, item, location);
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
    static inline Event two_individual_type_event(TwoIndividualData::Id id, uint256 actor, Coord actor_location, uint256 target, Coord target_location) {
        Event result;
        result.type = TWO_INDIVIDUAL;
        result.two_individual_data() = {
            id,
            actor,
            actor_location,
            target,
            target_location,
        };
        return result;
    }
    static inline Event individual_and_item_type_event(IndividualAndItemData::Id id, uint256 individual_id, uint256 item_id) {
        Event result;
        result.type = INDIVIDUAL_AND_ITEM;
        result.individual_and_item_data() = {
            id,
            individual_id,
            item_id,
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
void record_perception_of_thing(Thing observer, uint256 target_id);
void publish_event(Event event);
void publish_event(Event event, IdMap<WandDescriptionId> * perceived_current_zapper);

#endif
