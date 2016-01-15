#ifndef ACTION_HPP
#define ACTION_HPP

struct Action {
    enum Id {
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

        COUNT,
        // only a player can be undecided
        UNDECIDED,
    };
    struct CoordAndItem {
        uint256 item;
        Coord coord;
    };
    enum Layout {
        Layout_VOID,
        Layout_COORD,
        Layout_ITEM,
        Layout_COORD_AND_ITEM,
    };

    Id id;

    Layout get_layout() const {
        return get_layout(id);
    }
    const Coord        & coord()          const { assert(get_layout() == Layout_COORD);           return _coord; }
          Coord        & coord()                { assert(get_layout() == Layout_COORD);           return _coord; }
    const uint256      & item()           const { assert(get_layout() == Layout_ITEM);            return _item; }
          uint256      & item()                 { assert(get_layout() == Layout_ITEM);            return _item; }
    const CoordAndItem & coord_and_item() const { assert(get_layout() == Layout_COORD_AND_ITEM);  return _coord_and_item; }
          CoordAndItem & coord_and_item()       { assert(get_layout() == Layout_COORD_AND_ITEM);  return _coord_and_item; }

    static Action move(Coord direction) {
        return init(MOVE, direction);
    }
    static Action wait() {
        return init(WAIT);
    }
    static Action attack(Coord direction) {
        return init(ATTACK, direction);
    }
    static Action undecided() {
        return init(UNDECIDED);
    }
    static Action zap(uint256 item_id, Coord direction) {
        return init(ZAP, direction, item_id);
    }
    static Action pickup(uint256 item_id) {
        return init(PICKUP, item_id);
    }
    static Action drop(uint256 item_id) {
        return init(DROP, item_id);
    }
    static Action quaff(uint256 item_id) {
        return init(QUAFF, item_id);
    }
    static Action throw_(uint256 item_id, Coord direction) {
        return init(THROW, direction, item_id);
    }

    static Action go_down() {
        return init(GO_DOWN);
    }

    static Action cheatcode_health_boost() {
        return init(CHEATCODE_HEALTH_BOOST);
    }
    static Action cheatcode_kill_everybody_in_the_world() {
        return init(CHEATCODE_KILL_EVERYBODY_IN_THE_WORLD);
    }
    static Action cheatcode_polymorph() {
        return init(CHEATCODE_POLYMORPH);
    }
    static Action cheatcode_generate_monster() {
        return init(CHEATCODE_GENERATE_MONSTER);
    }
    static Action cheatcode_create_item() {
        return init(CHEATCODE_CREATE_ITEM);
    }
    static Action cheatcode_identify() {
        return init(CHEATCODE_IDENTIFY);
    }
    static Action cheatcode_go_down() {
        return init(CHEATCODE_GO_DOWN);
    }
    static Action cheatcode_gain_level() {
        return init(CHEATCODE_GAIN_LEVEL);
    }

    static Action init(Id id) {
        assert(get_layout(id) == Layout_VOID);
        Action result;
        result.id = id;
        return result;
    }
    static Action init(Id id, Coord coord) {
        assert(get_layout(id) == Layout_COORD);
        Action result;
        result.id = id;
        result._coord = coord;
        return result;
    }
    static Action init(Id id, uint256 item) {
        assert(get_layout(id) == Layout_ITEM);
        Action result;
        result.id = id;
        result._item = item;
        return result;
    }
    static Action init(Id id, Coord coord, uint256 item) {
        assert(get_layout(id) == Layout_COORD_AND_ITEM);
        Action result;
        result.id = id;
        result._coord_and_item.coord = coord;
        result._coord_and_item.item = item;
        return result;
    }

private:
    union {
        uint256 _item;
        Coord _coord;
        CoordAndItem _coord_and_item;
    };

    static Layout get_layout(Id id) {
        switch (id) {
            case WAIT:
            case GO_DOWN:
            case UNDECIDED:
                return Layout_VOID;
            case MOVE:
            case ATTACK:
                return Layout_COORD;
            case PICKUP:
            case DROP:
            case QUAFF:
                return Layout_ITEM;
            case ZAP:
            case THROW:
                return Layout_COORD_AND_ITEM;

            case CHEATCODE_HEALTH_BOOST:
            case CHEATCODE_KILL_EVERYBODY_IN_THE_WORLD:
            case CHEATCODE_POLYMORPH:
            case CHEATCODE_GENERATE_MONSTER:
            case CHEATCODE_CREATE_ITEM:
            case CHEATCODE_IDENTIFY:
            case CHEATCODE_GO_DOWN:
            case CHEATCODE_GAIN_LEVEL:
                return Layout_VOID;

            case COUNT:
                unreachable();
        }
        unreachable();
    }
};
static inline bool operator==(const Action & a, const Action &  b) {
    if (a.id != b.id)
        return false;
    Action::Layout layout = a.get_layout();
    if (layout != b.get_layout())
        return false;
    switch (layout) {
        case Action::Layout_VOID:
            return true;
        case Action::Layout_COORD:
            return a.coord() == b.coord();
        case Action::Layout_ITEM:
            return a.item() == b.item();
        case Action::Layout_COORD_AND_ITEM:
            if (a.coord_and_item().coord != b.coord_and_item().coord)
                return false;
            if (a.coord_and_item().item != b.coord_and_item().item)
                return false;
            return true;
    }
    unreachable();
}
static inline bool operator!=(const Action &  a, const Action &  b) {
    return !(a == b);
}

#endif
