#ifndef EVENT_HPP
#define EVENT_HPP

#include "individual.hpp"

struct Event {
    enum Type {
        MOVE,
        ATTACK,
        DIE,
        ZAP_WAND,
        WAND_HIT_NO_EFFECT,
        WAND_OF_CONFUSION_HIT,
        WAND_OF_STRIKING_HIT,
        WAND_OF_DIGGING_HIT_WALL,
        NO_LONGER_CONFUSED,
        BUMP_INTO_WALL,
        BUMP_INTO_INDIVIDUAL,
        ATTACK_THIN_AIR,
        // spawn or become visible
        APPEAR,
        // these are possible with cheatcodes
        DISAPPEAR,
        POLYMORPH,
    };
    Type type;
    Individual individual1;
    Individual individual2;
    Coord coord1;
    Coord coord2;
    Item item1;

    static inline Event move(Individual mover, Coord from, Coord to) {
        return {
            MOVE,
            mover,
            NULL,
            from,
            to,
            Item::none(),
        };
    }
    static inline Event attack(Individual attacker, Individual target) {
        return event_individual_individual(ATTACK, attacker, target);
    }
    static inline Event die(Individual deceased) {
        return event_individual(DIE, deceased);
    }

    static inline Event zap_wand(Individual wand_wielder, Item item) {
        return {
            ZAP_WAND,
            wand_wielder,
            NULL,
            wand_wielder->location,
            Coord::nowhere(),
            item,
        };
    }
    static inline Event wand_hit_no_effect(Individual wand_wielder, Item item, Individual target) {
        return event_individual_individual_item(WAND_HIT_NO_EFFECT, wand_wielder, target, item);
    }
    static inline Event wand_of_confusion_hit(Individual wand_wielder, Item item, Individual target) {
        return event_individual_individual_item(WAND_OF_CONFUSION_HIT, wand_wielder, target, item);
    }
    static inline Event wand_of_striking_hit(Individual wand_wielder, Item item, Individual target) {
        return event_individual_individual_item(WAND_OF_STRIKING_HIT, wand_wielder, target, item);
    }
    static inline Event wand_of_digging_hit_wall(Individual wand_wielder, Item item, Coord wall_location) {
        return {
            WAND_OF_DIGGING_HIT_WALL,
            wand_wielder,
            NULL,
            wand_wielder->location,
            wall_location,
            item,
        };
    }

    static Event bump_into_wall(Individual actor) {
        return event_individual(BUMP_INTO_WALL, actor);
    }
    static Event bump_into_individual(Individual actor, Individual target) {
        return event_individual_individual(BUMP_INTO_INDIVIDUAL, actor, target);
    }
    static Event attack_thin_air(Individual actor) {
        return event_individual(ATTACK_THIN_AIR, actor);
    }

    static inline Event no_longer_confused(Individual individual) {
        return event_individual(NO_LONGER_CONFUSED, individual);
    }
    static inline Event appear(Individual new_guy) {
        return event_individual(APPEAR, new_guy);
    }
    static inline Event disappear(Individual cant_see_me) {
        return event_individual(DISAPPEAR, cant_see_me);
    }
    static inline Event polymorph(Individual shapeshifter) {
        return event_individual(POLYMORPH, shapeshifter);
    }

private:
    static inline Event event_individual(Type type, Individual individual) {
        return {
            type,
            individual,
            NULL,
            individual->location,
            Coord::nowhere(),
            Item::none(),
        };
    }
    static Event event_individual_individual(Type type, Individual individual1, Individual individual2) {
        return {
            type,
            individual1,
            individual2,
            individual1->location,
            individual2->location,
            Item::none(),
        };
    }
    static inline Event event_individual_individual_item(Type type, Individual individual1, Individual individual2, Item item) {
        return {
            type,
            individual1,
            individual2,
            individual1->location,
            individual2->location,
            item,
        };
    }
};

void publish_event(Event event);

#endif
