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
        QUAFF,
        THROW,
        GO_DOWN,

        CHEATCODE_HEALTH_BOOST,
        CHEATCODE_KILL_EVERYBODY_IN_THE_WORLD,
        CHEATCODE_POLYMORPH,
        CHEATCODE_GENERATE_MONSTER,
        CHEATCODE_CREATE_ITEM,
        CHEATCODE_IDENTIFY,
        CHEATCODE_GO_DOWN,
        CHEATCODE_GAIN_LEVEL,

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
    static inline Action quaff(uint256 item_id) {
        return {QUAFF, item_id, {0, 0}};
    }
    static inline Action throw_(uint256 item_id, Coord direction) {
        return {THROW, item_id, direction};
    }

    static inline Action go_down() {
        return {GO_DOWN, uint256::zero(), Coord::nowhere()};
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
    static inline Action cheatcode_generate_monster() {
        return {CHEATCODE_GENERATE_MONSTER, uint256::zero(), {0, 0}};
    }
    static inline Action cheatcode_create_item() {
        return {CHEATCODE_CREATE_ITEM, uint256::zero(), {0, 0}};
    }
    static inline Action cheatcode_identify() {
        return {CHEATCODE_IDENTIFY, uint256::zero(), {0, 0}};
    }
    static inline Action cheatcode_go_down() {
        return {CHEATCODE_GO_DOWN, uint256::zero(), {0, 0}};
    }
    static inline Action cheatcode_gain_level() {
        return {CHEATCODE_GAIN_LEVEL, uint256::zero(), {0, 0}};
    }
};
static inline bool operator==(const Action & a, const Action &  b) {
    return a.type == b.type && a.coord == b.coord;
}
static inline bool operator!=(const Action &  a, const Action &  b) {
    return !(a == b);
}

#endif
