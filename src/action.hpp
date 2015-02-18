#ifndef ACTION_HPP
#define ACTION_HPP

struct Action {
    enum Type {
        MOVE,
        WAIT,
        ATTACK,
        ZAP,
        PICKUP,
        DROP,
        THROW,

        CHEATCODE_HEALTH_BOOST,
        CHEATCODE_KILL_EVERYBODY_IN_THE_WORLD,
        CHEATCODE_POLYMORPH,
        CHEATCODE_INVISIBILITY,
        CHEATCODE_GENERATE_MONSTER,

        // only a player can be undecided
        UNDECIDED,
    };
    Type type;
    uint256 item;
    Coord coord;

    static inline Action move(Coord direction) {
        return {MOVE, uint256::zero(), direction};
    }
    static inline Action wait() {
        return {WAIT, uint256::zero(), {0, 0}};
    }
    static inline Action attack(Coord direction) {
        return {ATTACK, uint256::zero(), direction};
    }
    static inline Action undecided() {
        return {UNDECIDED, uint256::zero(), {0, 0}};
    }
    static inline Action zap(uint256 item_id, Coord direction) {
        return {ZAP, item_id, direction};
    }
    static inline Action pickup(uint256 item_id) {
        return {PICKUP, item_id, {0, 0}};
    }
    static inline Action drop(uint256 item_id) {
        return {DROP, item_id, {0, 0}};
    }
    static inline Action throw_(uint256 item_id, Coord direction) {
        return {THROW, item_id, direction};
    }

    static inline Action cheatcode_health_boost() {
        return {CHEATCODE_HEALTH_BOOST, uint256::zero(), {0, 0}};
    }
    static inline Action cheatcode_kill_everybody_in_the_world() {
        return {CHEATCODE_KILL_EVERYBODY_IN_THE_WORLD, uint256::zero(), {0, 0}};
    }
    static inline Action cheatcode_polymorph() {
        return {CHEATCODE_POLYMORPH, uint256::zero(), {0, 0}};
    }
    static inline Action cheatcode_invisibility() {
        return {CHEATCODE_INVISIBILITY, uint256::zero(), {0, 0}};
    }
    static inline Action cheatcode_generate_monster() {
        return {CHEATCODE_GENERATE_MONSTER, uint256::zero(), {0, 0}};
    }
};
static inline bool operator==(const Action & a, const Action &  b) {
    return a.type == b.type && a.coord == b.coord;
}
static inline bool operator!=(const Action &  a, const Action &  b) {
    return !(a == b);
}

#endif
