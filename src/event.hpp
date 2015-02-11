#ifndef EVENT_HPP
#define EVENT_HPP

#include "individual.hpp"

struct Event {
    enum Type {
        MOVE,
        BUMP_INTO_WALL,
        BUMP_INTO_INDIVIDUAL,
        BUMP_INTO_SOMETHING,
        SOMETHING_BUMP_INTO_INDIVIDUAL,

        ATTACK,
        ATTACK_SOMETHING,
        SOMETHING_ATTACK_INDIVIDUAL,
        ATTACK_THIN_AIR,
        ATTACK_WALL,

        DIE,

        ZAP_WAND,
        WAND_HIT_NO_EFFECT,
        WAND_OF_CONFUSION_HIT,
        WAND_OF_STRIKING_HIT,
        WAND_OF_DIGGING_HIT_WALL,

        NO_LONGER_CONFUSED,

        APPEAR,
        TURN_INVISIBLE,
        DISAPPEAR,

        POLYMORPH,
    };
    Type type;
    uint256 & the_individual_data() {
        check_data_type(DataType_THE_INDIVIDUAL);
        return _data._the_individual;
    }
    struct MoveData {
        uint256 individual;
        Coord from;
        Coord to;
    };
    MoveData & move_data() {
        check_data_type(DataType_MOVE);
        return _data._move;
    }
    struct AttackData {
        uint256 attacker;
        uint256 target;
    };
    AttackData & attack_data() {
        check_data_type(DataType_ATTACK);
        return _data._attack;
    }
    struct ZapData {
        uint256 wielder;
        Item wand;
    };
    ZapData & zap_data() {
        check_data_type(DataType_ZAP);
        return _data._zap;
    }
    struct ZapHitIndividualData {
        uint256 wielder;
        Item wand;
        uint256 target;
    };
    ZapHitIndividualData & zap_hit_individual_data() {
        check_data_type(DataType_ZAP_HIT_INDIVIDUAL);
        return _data._zap_hit_individual;
    }
    struct ZapHitLocationData {
        uint256 wielder;
        Item wand;
        Coord target_location;
    };
    ZapHitLocationData & zap_hit_location_data() {
        check_data_type(DataType_ZAP_HIT_LOCATION);
        return _data._zap_hit_location;
    }
    struct PolymorphData {
        uint256 individual;
        SpeciesId old_species;
    };
    PolymorphData & polymorph_data() {
        check_data_type(DataType_POLYMORPH);
        return _data._polymorph;
    };

    static inline Event move(Individual mover, Coord from, Coord to) {
        return move_type_event(MOVE, mover->id, from, to);
    }

    static inline Event attack(Individual attacker, Individual target) {
        return attack_type_event(ATTACK, attacker, target);
    }
    static inline Event attack_something(uint256 attacker_id, Coord attacker_location, Coord target_location) {
        return move_type_event(ATTACK_SOMETHING, attacker_id, attacker_location, target_location);
    }
    static inline Event something_attack_individual(uint256 target_id, Coord attacker_location, Coord target_location) {
        return move_type_event(SOMETHING_ATTACK_INDIVIDUAL, target_id, attacker_location, target_location);
    }
    static Event attack_thin_air(Individual actor, Coord target_location) {
        return move_type_event(ATTACK_THIN_AIR, actor->id, actor->location, target_location);
    }
    static Event attack_wall(Individual actor, Coord target_location) {
        return move_type_event(ATTACK_WALL, actor->id, actor->location, target_location);
    }

    static inline Event die(Individual deceased) {
        return event_individual(DIE, deceased->id);
    }

    static inline Event zap_wand(Individual wand_wielder, Item item) {
        Event result;
        result.type = ZAP_WAND;
        result.zap_data() = {
            wand_wielder->id,
            item,
        };
        return result;
    }
    static inline Event wand_hit_no_effect(Individual wand_wielder, Item item, Individual target) {
        return zap_hit_individual(WAND_HIT_NO_EFFECT, wand_wielder, target, item);
    }
    static inline Event wand_of_confusion_hit(Individual wand_wielder, Item item, Individual target) {
        return zap_hit_individual(WAND_OF_CONFUSION_HIT, wand_wielder, target, item);
    }
    static inline Event wand_of_striking_hit(Individual wand_wielder, Item item, Individual target) {
        return zap_hit_individual(WAND_OF_STRIKING_HIT, wand_wielder, target, item);
    }
    static inline Event wand_of_digging_hit_wall(Individual wand_wielder, Item item, Coord wall_location) {
        Event result;
        result.type = WAND_OF_DIGGING_HIT_WALL;
        result.zap_hit_location_data() = {
            wand_wielder->id,
            item,
            wall_location,
        };
        return result;
    }

    static Event bump_into_wall(Individual actor, Coord wall_location) {
        return move_type_event(BUMP_INTO_WALL, actor->id, actor->location, wall_location);
    }
    static Event bump_into_individual(Individual actor, Individual target) {
        return attack_type_event(BUMP_INTO_INDIVIDUAL, actor, target);
    }
    static Event bump_into_something(uint256 actor_id, Coord from_location, Coord something_location) {
        return move_type_event(BUMP_INTO_SOMETHING, actor_id, from_location, something_location);
    }
    static Event something_bump_into_individual(uint256 target, Coord from_location, Coord target_location) {
        return move_type_event(SOMETHING_BUMP_INTO_INDIVIDUAL, target, from_location, target_location);
    }

    static inline Event no_longer_confused(Individual individual) {
        return event_individual(NO_LONGER_CONFUSED, individual->id);
    }
    static inline Event appear(Individual new_guy) {
        return event_individual(APPEAR, new_guy->id);
    }
    static inline Event turn_invisible(Individual individual) {
        return event_individual(TURN_INVISIBLE, individual->id);
    }
    static inline Event disappear(uint256 individual_id) {
        return event_individual(DISAPPEAR, individual_id);
    }
    static inline Event polymorph(Individual shapeshifter, SpeciesId old_species) {
        Event result;
        result.type = POLYMORPH;
        PolymorphData & data = result.polymorph_data();
        data.individual = shapeshifter->id;
        data.old_species = old_species;
        return result;
    }

private:
    static inline Event move_type_event(Type type, uint256 mover_id, Coord from, Coord to) {
        Event result;
        result.type = type;
        result.move_data() = {
            mover_id,
            from,
            to,
        };
        return result;
    }
    static inline Event event_individual(Type type, uint256 individual_id) {
        Event result;
        result.type = type;
        result.the_individual_data() = individual_id;
        return result;
    }
    static Event attack_type_event(Type type, Individual individual1, Individual individual2) {
        Event result;
        result.type = type;
        result.attack_data() = {
            individual1->id,
            individual2->id,
        };
        return result;
    }
    static inline Event zap_hit_individual(Type type, Individual individual1, Individual individual2, Item item) {
        Event result;
        result.type = type;
        result.zap_hit_individual_data() = {
            individual1->id,
            item,
            individual2->id,
        };
        return result;
    }

    union {
        uint256 _the_individual;
        MoveData _move;
        AttackData _attack;
        ZapData _zap;
        ZapHitIndividualData _zap_hit_individual;
        ZapHitLocationData _zap_hit_location;
        PolymorphData _polymorph;
    } _data;
    enum DataType {
        DataType_THE_INDIVIDUAL,
        DataType_MOVE,
        DataType_ATTACK,
        DataType_ZAP,
        DataType_ZAP_HIT_INDIVIDUAL,
        DataType_ZAP_HIT_LOCATION,
        DataType_POLYMORPH,
    };

    void check_data_type(DataType supposed_data_type) {
        DataType correct_data_type = get_correct_data_type();
        if (supposed_data_type != correct_data_type)
            panic("wrong data type");
    }
    DataType get_correct_data_type() {
        switch (type) {
            case MOVE:
                return DataType_MOVE;
            case BUMP_INTO_WALL:
                return DataType_MOVE;
            case BUMP_INTO_INDIVIDUAL:
                return DataType_ATTACK;
            case BUMP_INTO_SOMETHING:
                return DataType_MOVE;
            case SOMETHING_BUMP_INTO_INDIVIDUAL:
                return DataType_MOVE;

            case ATTACK:
                return DataType_ATTACK;
            case ATTACK_SOMETHING:
                return DataType_MOVE;
            case SOMETHING_ATTACK_INDIVIDUAL:
                return DataType_MOVE;
            case ATTACK_THIN_AIR:
                return DataType_MOVE;
            case ATTACK_WALL:
                return DataType_MOVE;

            case DIE:
                return DataType_THE_INDIVIDUAL;

            case ZAP_WAND:
                return DataType_ZAP;
            case WAND_HIT_NO_EFFECT:
                return DataType_ZAP_HIT_INDIVIDUAL;
            case WAND_OF_CONFUSION_HIT:
                return DataType_ZAP_HIT_INDIVIDUAL;
            case WAND_OF_STRIKING_HIT:
                return DataType_ZAP_HIT_INDIVIDUAL;
            case WAND_OF_DIGGING_HIT_WALL:
                return DataType_ZAP_HIT_LOCATION;

            case NO_LONGER_CONFUSED:
                return DataType_THE_INDIVIDUAL;

            case APPEAR:
                return DataType_THE_INDIVIDUAL;
            case TURN_INVISIBLE:
                return DataType_THE_INDIVIDUAL;
            case DISAPPEAR:
                return DataType_THE_INDIVIDUAL;

            case POLYMORPH:
                return DataType_POLYMORPH;
        }
    }
};

bool can_see_individual(Individual observer, uint256 target_id, Coord target_location);
bool can_see_individual(Individual observer, uint256 target_id);
void publish_event(Event event);

#endif
