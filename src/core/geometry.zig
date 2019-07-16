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

    pub fn negated(a: Coord) Coord {
        return Coord{
            .x = -a.x,
            .y = -a.y,
        };
    }

    pub fn abs(a: Coord) Coord {
        return Coord{
            .x = if (a.x < 0) -a.x else a.x,
            .y = if (a.y < 0) -a.y else a.y,
        };
    }

    /// How many orthogonal steps to get from a to b.
    pub fn distanceOrtho(a: Coord, b: Coord) i32 {
        const abs_delta = b.minus(a).abs();
        return abs_delta.x + abs_delta.y;
    }

    /// How many diagonal or orthoganl steps to get from a to b.
    pub fn distanceDiag(a: Coord, b: Coord) i32 {
        return b.minus(a).magnitudeDiag();
    }
    pub fn magnitudeDiag(a: Coord) i32 {
        const abs_delta = a.abs();
        return if (abs_delta.x < abs_delta.y) abs_delta.y else abs_delta.x;
    }

    pub fn equals(a: Coord, b: Coord) bool {
        return a.x == b.x and a.y == b.y;
    }

    pub fn hash(a: Coord) u32 {
        return hashU32(hashU32(@bitCast(u32, a.x)) ^ @bitCast(u32, a.y));
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

// TODO: why is this here
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

// rotation is a number 0 <= r < 8
// representing the number of 45 degree clockwise turns from right.
pub fn directionToRotation(direction: Coord) u3 {
    if (direction.x == 1 and direction.y == 0) return 0;
    if (direction.x == 0 and direction.y == 1) return 2;
    if (direction.x == -1 and direction.y == 0) return 4;
    if (direction.x == 0 and direction.y == -1) return 6;
    unreachable;
}

// cardinal bitmask is: 1 for right, 2 for down, 4 for left, and 8 for up.
// diagonal would be a combination of them, such as 3 or 9.
pub fn directionToCardinalBitmask(direction: Coord) u4 {
    var mask: u4 = 0;
    if (direction.x == 1) {
        mask |= 0b0001; // right
    } else if (direction.x == -1) {
        mask |= 0b0100; // left
    } else if (direction.x != 0) unreachable;

    if (direction.y == 1) {
        mask |= 0b0010; // down
    } else if (direction.y == -1) {
        mask |= 0b1000; // up
    } else if (direction.y != 0) unreachable;

    return mask;
}

// if there's no obvious direction, returns null
pub fn cardinalBitmaskToDirection(mask: u4) ?Coord {
    if (mask & 0b0101 == 0b0101 or mask & 0b1010 == 0b1010) return null;
    var direction = makeCoord(0, 0);
    if (mask & 0b0001 != 0) {
        direction.x = 1;
    } else if (mask & 0b0100 != 0) {
        direction.x = -1;
    }
    if (mask & 0b0010 != 0) {
        direction.y = 1;
    } else if (mask & 0b1000 != 0) {
        direction.y = -1;
    }
    return direction;
}

pub fn sign(x: var) @typeOf(x) {
    if (x > 0) return 1;
    if (x == 0) return 0;
    return -1;
}

/// Quadratic version of bezier2.
pub fn bezier3(
    x0: Coord,
    x1: Coord,
    x2: Coord,
    s: i32,
    max_s: i32,
) Coord {
    return bezier2(
        bezier2(x0, x1, s, max_s),
        bezier2(x1, x2, s, max_s),
        s,
        max_s,
    );
}

/// This is linear interpolation.
/// Normally max_s is fixed at 1.0, but we're too cool for floats.
pub fn bezier2(
    x0: Coord,
    x1: Coord,
    s: i32,
    max_s: i32,
) Coord {
    return x0.scaled(max_s - s).plus(x1.scaled(s)).scaledDivTrunc(max_s);
}
