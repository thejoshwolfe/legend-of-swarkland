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
    union {
        uint256 _the_individual;
        struct {
            uint256 individual;
            Coord from;
            Coord to;
        } _move;
        struct {
            uint256 attacker;
            uint256 target;
        } _attack;
        struct {
            uint256 attacker;
        } _fail_to_attack;
        struct {
            uint256 deceased;
        } _die;
        struct {
            uint256 wielder;
            Item wand;
        } _zap;
        struct {
            uint256 wielder;
            Item wand;
            uint256 target;
        } _zap_hit_individual;
        struct {
            uint256 wielder;
            Item wand;
            Coord target_location;
        } _zap_hit_location;
        struct {
            uint256 individual;
            SpeciesId old_species;
        } _polymorph;
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
        result._zap.wielder = wand_wielder->id;
        result._zap.wand = item;
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
        result._zap_hit_location.wielder = wand_wielder->id;
        result._zap_hit_location.wand = item;
        result._zap_hit_location.target_location = wall_location;
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
        return event_individual(TURN_INVISIBLE, individual_id);
    }
    static inline Event polymorph(Individual shapeshifter) {
        return event_individual(POLYMORPH, shapeshifter->id);
    }

private:
    static inline Event move_type_event(Type type, uint256 mover_id, Coord from, Coord to) {
        Event result;
        result.type = type;
        result._move.individual = mover_id;
        result._move.from = from;
        result._move.to = to;
        return result;
    }
    static inline Event event_individual(Type type, uint256 individual_id) {
        Event result;
        result.type = type;
        result._the_individual = individual_id;
        return result;
    }
    static Event attack_type_event(Type type, Individual individual1, Individual individual2) {
        Event result;
        result.type = type;
        result._attack.attacker = individual1->id;
        result._attack.target = individual2->id;
        return result;
    }
    static inline Event zap_hit_individual(Type type, Individual individual1, Individual individual2, Item item) {
        Event result;
        result.type = type;
        result._zap_hit_individual.wielder = individual1->id;
        result._zap_hit_individual.wand = item;
        result._zap_hit_individual.target = individual2->id;
        return result;
    }
};

bool can_see_individual(Individual observer, uint256 target_id, Coord target_location);
bool can_see_individual(Individual observer, uint256 target_id);
void publish_event(Event event);

#endif
