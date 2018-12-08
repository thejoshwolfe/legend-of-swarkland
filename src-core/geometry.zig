pub const Rect = struct {
    x: i32,
    y: i32,
    width: i32,
    height: i32,
};

pub const Coord = struct {
    x: i32,
    y: i32,

    pub fn plus(a: Coord, b: Coord) Coord {
        return Coord{
            .x = a.x + b.x,
            .y = a.y + b.y,
        };
    }
    pub fn minus(a: Coord, b: Coord) Coord {
        return Coord{
            .x = a.x - b.x,
            .y = a.y - b.y,
        };
    }

    pub fn scaled(a: Coord, s: i32) Coord {
        return Coord{
            .x = a.x * s,
            .y = a.y * s,
        };
    }
    pub fn scaledDivTrunc(a: Coord, s: i32) Coord {
        return Coord{
            .x = @divTrunc(a.x, s),
            .y = @divTrunc(a.y, s),
        };
    }
};

pub fn makeCoord(x: i32, y: i32) Coord {
    return Coord{
        .x = x,
        .y = y,
    };
}

pub fn isCardinalDirection(direction: Coord) bool {
    if (direction.x * direction.y != 0) return false;
    if ((direction.x + direction.y) * (direction.x + direction.y) != 1) return false;
    return true;
}

pub fn hashU32(input: u32) u32 {
    // https://nullprogram.com/blog/2018/07/31/
    var x = input;
    x ^= x >> 17;
    x *%= 0xed5ad4bb;
    x ^= x >> 11;
    x *%= 0xac4c1b51;
    x ^= x >> 15;
    x *%= 0x31848bab;
    x ^= x >> 14;
    return x;
}

pub fn hashCoord(coord: Coord) u32 {
    var x = hashU32(@bitCast(u32, coord.x));
    x ^= hashU32(@bitCast(u32, coord.y));
    return x;
}

pub fn eqlCoord(a: Coord, b: Coord) bool {
    return a.x == b.x and a.y == b.y;
}

pub fn directionToRotation(direction: Coord) u3 {
    if (direction.x == 1 and direction.y == 0) return 0;
    if (direction.x == 0 and direction.y == 1) return 2;
    if (direction.x == -1 and direction.y == 0) return 4;
    if (direction.x == 0 and direction.y == -1) return 6;
    unreachable;
}
