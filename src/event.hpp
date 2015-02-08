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
        return {
            ATTACK,
            attacker,
            target,
            attacker->location,
            target->location,
            Item::none(),
        };
    }
    static inline Event die(Individual deceased) {
        return single_individual_event(DIE, deceased);
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
        return {
            WAND_HIT_NO_EFFECT,
            wand_wielder,
            target,
            wand_wielder->location,
            target->location,
            item,
        };
    }
    static inline Event wand_of_confusion_hit(Individual wand_wielder, Item item, Individual target) {
        return {
            WAND_OF_CONFUSION_HIT,
            wand_wielder,
            target,
            wand_wielder->location,
            target->location,
            item,
        };
    }

    static Event bump_into_wall(Individual actor) {
        return single_individual_event(BUMP_INTO_WALL, actor);
    }
    static Event bump_into_individual(Individual actor, Individual target) {
        return {
            BUMP_INTO_INDIVIDUAL,
            actor,
            target,
            actor->location,
            target->location,
            Item::none(),
        };
    }
    static Event attack_thin_air(Individual actor) {
        return single_individual_event(ATTACK_THIN_AIR, actor);
    }

    static inline Event no_longer_confused(Individual individual) {
        return single_individual_event(NO_LONGER_CONFUSED, individual);
    }
    static inline Event appear(Individual new_guy) {
        return single_individual_event(APPEAR, new_guy);
    }
    static inline Event disappear(Individual cant_see_me) {
        return single_individual_event(DISAPPEAR, cant_see_me);
    }
    static inline Event polymorph(Individual shapeshifter) {
        return single_individual_event(POLYMORPH, shapeshifter);
    }
private:
    static inline Event single_individual_event(Type type, Individual individual) {
        return {
            type,
            individual,
            NULL,
            individual->location,
            Coord::nowhere(),
            Item::none(),
        };
    }
};

void publish_event(Event event);

#endif
