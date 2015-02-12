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
        ZAP_WAND_NO_CHARGES,
        WAND_DISINTEGRATES,
        BEAM_HIT_INDIVIDUAL_NO_EFFECT,
        BEAM_HIT_WALL_NO_EFFECT,
        BEAM_OF_CONFUSION_HIT_INDIVIDUAL,
        BEAM_OF_STRIKING_HIT_INDIVIDUAL,
        BEAM_OF_DIGGING_HIT_WALL,

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
    Coord & the_location_data() {
        check_data_type(DataType_THE_LOCATION);
        return _data._the_location;
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
    struct ZapWandData {
        uint256 wielder;
        uint256 wand;
    };
    ZapWandData & zap_wand_data() {
        check_data_type(DataType_ZAP_WAND);
        return _data._zap_wand;
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
        return zap_wand_type_event(ZAP_WAND, wand_wielder, item);
    }
    static inline Event wand_zap_no_charges(Individual wand_wielder, Item item) {
        return zap_wand_type_event(ZAP_WAND_NO_CHARGES, wand_wielder, item);
    }
    static inline Event wand_disintegrates(Individual wand_wielder, Item item) {
        return zap_wand_type_event(WAND_DISINTEGRATES, wand_wielder, item);
    }

    static inline Event beam_hit_individual_no_effect(Individual target) {
        return event_individual(BEAM_HIT_INDIVIDUAL_NO_EFFECT, target->id);
    }
    static inline Event beam_of_confusion_hit_individual(Individual target) {
        return event_individual(BEAM_OF_CONFUSION_HIT_INDIVIDUAL, target->id);
    }
    static inline Event beam_of_striking_hit_individual(Individual target) {
        return event_individual(BEAM_OF_STRIKING_HIT_INDIVIDUAL, target->id);
    }
    static inline Event beam_of_digging_hit_wall(Coord wall_location) {
        Event result;
        result.type = BEAM_OF_DIGGING_HIT_WALL;
        result.the_location_data() = wall_location;
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
    static inline Event event_individual(Type type, uint256 individual_id) {
        Event result;
        result.type = type;
        result.the_individual_data() = individual_id;
        return result;
    }
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
    static Event attack_type_event(Type type, Individual individual1, Individual individual2) {
        Event result;
        result.type = type;
        result.attack_data() = {
            individual1->id,
            individual2->id,
        };
        return result;
    }
    static inline Event zap_wand_type_event(Type type, Individual wand_wielder, Item item) {
        Event result;
        result.type = type;
        result.zap_wand_data() = {
            wand_wielder->id,
            item->id,
        };
        return result;
    }

    union {
        uint256 _the_individual;
        Coord _the_location;
        MoveData _move;
        AttackData _attack;
        ZapWandData _zap_wand;
        PolymorphData _polymorph;
    } _data;
    enum DataType {
        DataType_THE_INDIVIDUAL,
        DataType_THE_LOCATION,
        DataType_MOVE,
        DataType_ATTACK,
        DataType_ZAP_WAND,
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
                return DataType_ZAP_WAND;
            case ZAP_WAND_NO_CHARGES:
                return DataType_ZAP_WAND;
            case WAND_DISINTEGRATES:
                return DataType_ZAP_WAND;

            case BEAM_HIT_INDIVIDUAL_NO_EFFECT:
                return DataType_THE_INDIVIDUAL;
            case BEAM_HIT_WALL_NO_EFFECT:
                return DataType_THE_LOCATION;
            case BEAM_OF_CONFUSION_HIT_INDIVIDUAL:
                return DataType_THE_INDIVIDUAL;
            case BEAM_OF_STRIKING_HIT_INDIVIDUAL:
                return DataType_THE_INDIVIDUAL;
            case BEAM_OF_DIGGING_HIT_WALL:
                return DataType_THE_LOCATION;

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
        panic("event type");
    }
};

bool can_see_individual(Individual observer, uint256 target_id, Coord target_location);
bool can_see_individual(Individual observer, uint256 target_id);
void publish_event(Event event);

#endif
