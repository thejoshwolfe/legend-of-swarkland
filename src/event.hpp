#ifndef EVENT_HPP
#define EVENT_HPP

#include "thing.hpp"

struct Event {
    enum Type {
        MOVE,
        BUMP_INTO,
        ATTACK,

        ZAP_WAND,
        ZAP_WAND_NO_CHARGES,
        WAND_DISINTEGRATES,
        WAND_EXPLODES,
        WAND_HIT,

        THROW_ITEM,
        ITEM_HITS_INDIVIDUAL,
        ITEM_HITS_WALL,
        ITEM_HITS_SOMETHING,

        USE_POTION,

        POISONED,
        NO_LONGER_CONFUSED,
        NO_LONGER_FAST,
        NO_LONGER_HAS_ETHEREAL_VISION,
        NO_LONGER_POISONED,

        APPEAR,
        TURN_INVISIBLE,
        DISAPPEAR,
        LEVEL_UP,
        DIE,

        POLYMORPH,

        ITEM_DROPS_TO_THE_FLOOR,
        INDIVIDUAL_PICKS_UP_ITEM,
        SOMETHING_PICKS_UP_ITEM,
        INDIVIDUAL_SUCKS_UP_ITEM,
        SOMETHING_SUCKS_UP_ITEM,
    };
    Type type;

    uint256 & the_individual_data() {
        check_data_type(DataType_THE_INDIVIDUAL);
        return _data._the_individual;
    }

    Coord & the_location_data() {
        check_data_type(DataType_THE_LOCATION);
        return _data._the_location;
    }

    struct TwoIndividualData {
        uint256 actor;
        Coord actor_location;
        uint256 target;
        Coord target_location;
    };
    TwoIndividualData & two_individual_data() {
        check_data_type(DataType_TWO_INDIVIDUAL);
        return _data._two_individual;
    }

    struct ZapWandData {
        uint256 wielder;
        uint256 wand;
    };
    ZapWandData & zap_wand_data() {
        check_data_type(DataType_ZAP_WAND);
        return _data._zap_wand;
    }

    struct WandHitData {
        WandId observable_effect;
        bool is_explosion;
        uint256 target;
        Coord location;
    };
    WandHitData & wand_hit_data() {
        check_data_type(DataType_WAND_HIT);
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
        check_data_type(DataType_USE_POTION);
        return _data._use_potion;
    }

    struct PolymorphData {
        uint256 individual;
        SpeciesId old_species;
    };
    PolymorphData & polymorph_data() {
        check_data_type(DataType_POLYMORPH);
        return _data._polymorph;
    };

    struct ItemAndLocationData {
        uint256 item;
        Coord location;
    };
    ItemAndLocationData & item_and_location_data() {
        check_data_type(DataType_ITEM_AND_LOCATION);
        return _data._item_and_location;
    }

    static inline Event move(Thing mover, Coord from, Coord to) {
        return two_individual_type_event(MOVE, mover->id, from, uint256::zero(), to);
    }

    static inline Event attack(Thing attacker, uint256 target_id, Coord target_location) {
        return two_individual_type_event(ATTACK, attacker->id, attacker->location, target_id, target_location);
    }

    static inline Event zap_wand(Thing wand_wielder, Thing item) {
        return zap_wand_type_event(ZAP_WAND, wand_wielder->id, item->id);
    }
    static inline Event wand_zap_no_charges(Thing wand_wielder, Thing item) {
        return zap_wand_type_event(ZAP_WAND_NO_CHARGES, wand_wielder->id, item->id);
    }
    static inline Event wand_disintegrates(Thing wand_wielder, Thing item) {
        return zap_wand_type_event(WAND_DISINTEGRATES, wand_wielder->id, item->id);
    }
    static inline Event wand_explodes(uint256 item_id, Coord location) {
        return item_and_location_type_event(WAND_EXPLODES, item_id, location);
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
        return two_individual_type_event(BUMP_INTO, actor_id, from_location, bumpee_id, bumpee_location);
    }

    static Event throw_item(uint256 thrower_id, uint256 item_id) {
        return zap_wand_type_event(THROW_ITEM, thrower_id, item_id);
    }
    static Event item_hits_individual(uint256 item_id, uint256 target_id) {
        return zap_wand_type_event(ITEM_HITS_INDIVIDUAL, target_id, item_id);
    }
    static Event item_hits_wall(uint256 item_id, Coord location) {
        return item_and_location_type_event(ITEM_HITS_WALL, item_id, location);
    }
    static Event item_hits_something(uint256 item_id, Coord location) {
        return item_and_location_type_event(ITEM_HITS_SOMETHING, item_id, location);
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
        return event_individual(POISONED, individual->id);
    }
    static inline Event no_longer_confused(Thing individual) {
        return event_individual(NO_LONGER_CONFUSED, individual->id);
    }
    static inline Event no_longer_fast(Thing individual) {
        return event_individual(NO_LONGER_FAST, individual->id);
    }
    static inline Event no_longer_has_ethereal_vision(Thing individual) {
        return event_individual(NO_LONGER_HAS_ETHEREAL_VISION, individual->id);
    }
    static inline Event no_longer_poisoned(Thing individual) {
        return event_individual(NO_LONGER_POISONED, individual->id);
    }

    static inline Event appear(Thing new_guy) {
        return event_individual(APPEAR, new_guy->id);
    }
    static inline Event turn_invisible(Thing individual) {
        return event_individual(TURN_INVISIBLE, individual->id);
    }
    static inline Event disappear(uint256 individual_id) {
        return event_individual(DISAPPEAR, individual_id);
    }
    static inline Event level_up(uint256 individual_id) {
        return event_individual(LEVEL_UP, individual_id);
    }
    static inline Event die(Thing deceased) {
        return event_individual(DIE, deceased->id);
    }

    static inline Event polymorph(Thing shapeshifter, SpeciesId old_species) {
        Event result;
        result.type = POLYMORPH;
        PolymorphData & data = result.polymorph_data();
        data.individual = shapeshifter->id;
        data.old_species = old_species;
        return result;
    }


    static inline Event item_drops_to_the_floor(Thing item) {
        return item_and_location_type_event(ITEM_DROPS_TO_THE_FLOOR, item->id, item->location);
    }
    static inline Event individual_picks_up_item(Thing individual, Thing item) {
        return zap_wand_type_event(INDIVIDUAL_PICKS_UP_ITEM, individual->id, item->id);
    }
    static inline Event something_picks_up_item(uint256 item, Coord location) {
        return item_and_location_type_event(SOMETHING_PICKS_UP_ITEM, item, location);
    }
    static inline Event individual_sucks_up_item(Thing individual, Thing item) {
        return zap_wand_type_event(INDIVIDUAL_SUCKS_UP_ITEM, individual->id, item->id);
    }
    static inline Event something_sucks_up_item(uint256 item, Coord location) {
        return item_and_location_type_event(SOMETHING_SUCKS_UP_ITEM, item, location);
    }

private:
    static inline Event event_individual(Type type, uint256 individual_id) {
        Event result;
        result.type = type;
        result.the_individual_data() = individual_id;
        return result;
    }
    static inline Event location_type_event(Type type, Coord location) {
        Event result;
        result.type = type;
        result.the_location_data() = location;
        return result;
    }
    static inline Event two_individual_type_event(Type type, uint256 actor, Coord actor_location, uint256 target, Coord target_location) {
        Event result;
        result.type = type;
        result.two_individual_data() = {
            actor,
            actor_location,
            target,
            target_location,
        };
        return result;
    }
    static inline Event zap_wand_type_event(Type type, uint256 wielder_id, uint256 item_id) {
        Event result;
        result.type = type;
        result.zap_wand_data() = {
            wielder_id,
            item_id,
        };
        return result;
    }
    static inline Event item_and_location_type_event(Type type, uint256 item, Coord location) {
        Event result;
        result.type = type;
        ItemAndLocationData & data = result.item_and_location_data();
        data.item = item;
        data.location = location;
        return result;
    }

    union {
        uint256 _the_individual;
        Coord _the_location;
        TwoIndividualData _two_individual;
        ZapWandData _zap_wand;
        WandHitData _wand_hit;
        UsePotionData _use_potion;
        PolymorphData _polymorph;
        ItemAndLocationData _item_and_location;
    } _data;
    enum DataType {
        DataType_THE_INDIVIDUAL,
        DataType_THE_LOCATION,
        DataType_TWO_INDIVIDUAL,
        DataType_ZAP_WAND,
        DataType_WAND_HIT,
        DataType_USE_POTION,
        DataType_POLYMORPH,
        DataType_ITEM_AND_LOCATION,
    };

    void check_data_type(DataType supposed_data_type) const {
        DataType correct_data_type = get_correct_data_type();
        if (supposed_data_type != correct_data_type)
            panic("wrong data type");
    }
    DataType get_correct_data_type() const {
        switch (type) {
            case MOVE:
                return DataType_TWO_INDIVIDUAL;
            case BUMP_INTO:
                return DataType_TWO_INDIVIDUAL;
            case ATTACK:
                return DataType_TWO_INDIVIDUAL;

            case ZAP_WAND:
                return DataType_ZAP_WAND;
            case ZAP_WAND_NO_CHARGES:
                return DataType_ZAP_WAND;
            case WAND_DISINTEGRATES:
                return DataType_ZAP_WAND;
            case WAND_EXPLODES:
                return DataType_ITEM_AND_LOCATION;
            case WAND_HIT:
                return DataType_WAND_HIT;

            case THROW_ITEM:
                return DataType_ZAP_WAND;
            case ITEM_HITS_INDIVIDUAL:
                return DataType_ZAP_WAND;
            case ITEM_HITS_WALL:
                return DataType_ITEM_AND_LOCATION;
            case ITEM_HITS_SOMETHING:
                return DataType_ITEM_AND_LOCATION;

            case USE_POTION:
                return DataType_USE_POTION;

            case POISONED:
                return DataType_THE_INDIVIDUAL;
            case NO_LONGER_CONFUSED:
                return DataType_THE_INDIVIDUAL;
            case NO_LONGER_FAST:
                return DataType_THE_INDIVIDUAL;
            case NO_LONGER_HAS_ETHEREAL_VISION:
                return DataType_THE_INDIVIDUAL;
            case NO_LONGER_POISONED:
                return DataType_THE_INDIVIDUAL;

            case APPEAR:
                return DataType_THE_INDIVIDUAL;
            case TURN_INVISIBLE:
                return DataType_THE_INDIVIDUAL;
            case DISAPPEAR:
                return DataType_THE_INDIVIDUAL;
            case LEVEL_UP:
                return DataType_THE_INDIVIDUAL;
            case DIE:
                return DataType_THE_INDIVIDUAL;

            case POLYMORPH:
                return DataType_POLYMORPH;

            case ITEM_DROPS_TO_THE_FLOOR:
                return DataType_ITEM_AND_LOCATION;
            case INDIVIDUAL_PICKS_UP_ITEM:
                return DataType_ZAP_WAND;
            case SOMETHING_PICKS_UP_ITEM:
                return DataType_ITEM_AND_LOCATION;
            case INDIVIDUAL_SUCKS_UP_ITEM:
                return DataType_ZAP_WAND;
            case SOMETHING_SUCKS_UP_ITEM:
                return DataType_ITEM_AND_LOCATION;
        }
        panic("event type");
    }
};

bool can_see_individual(Thing observer, uint256 target_id, Coord target_location);
bool can_see_individual(Thing observer, uint256 target_id);
void publish_event(Event event);
void publish_event(Event event, IdMap<WandDescriptionId> * perceived_current_zapper);

#endif
