#ifndef ACTION_HPP
#define ACTION_HPP

struct Action {
    enum Type {
        MOVE,
        WAIT,
        ATTACK,
        ZAP,

        CHEATCODE_HEALTH_BOOST,
        CHEATCODE_KILL_EVERYBODY_IN_THE_WORLD,
        CHEATCODE_POLYMORPH,
        CHEATCODE_INVISIBILITY,
        CHEATCODE_GENERATE_MONSTER,

        // only a player can be undecided
        UNDECIDED,
    };
    Type type;
    Item item;
    Coord coord;

    static inline Action move(Coord direction) {
        return {MOVE, Item::none(), direction};
    }
    static inline Action wait() {
        return {WAIT, Item::none(), {0, 0}};
    }
    static inline Action attack(Coord direction) {
        return {ATTACK, Item::none(), direction};
    }
    static inline Action undecided() {
        return {UNDECIDED, Item::none(), {0, 0}};
    }
    static inline Action zap(Item item, Coord direction) {
        return {ZAP, item, direction};
    }

    static inline Action cheatcode_health_boost() {
        return {CHEATCODE_HEALTH_BOOST, Item::none(), {0, 0}};
    }
    static inline Action cheatcode_kill_everybody_in_the_world() {
        return {CHEATCODE_KILL_EVERYBODY_IN_THE_WORLD, Item::none(), {0, 0}};
    }
    static inline Action cheatcode_polymorph() {
        return {CHEATCODE_POLYMORPH, Item::none(), {0, 0}};
    }
    static inline Action cheatcode_invisibility() {
        return {CHEATCODE_INVISIBILITY, Item::none(), {0, 0}};
    }
    static inline Action cheatcode_generate_monster() {
        return {CHEATCODE_GENERATE_MONSTER, Item::none(), {0, 0}};
    }
};
static inline bool operator==(Action a, Action b) {
    return a.type == b.type && a.coord == b.coord;
}
static inline bool operator!=(Action a, Action b) {
    return !(a == b);
}

#endif
