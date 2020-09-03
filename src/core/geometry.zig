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
    pub fn signumed(a: Coord) Coord {
        return Coord{
            .x = sign(a.x),
            .y = sign(a.y),
        };
    }

    /// How many orthogonal steps to get from a to b.
    pub fn distanceOrtho(a: Coord, b: Coord) i32 {
        return b.minus(a).magnitudeOrtho();
    }
    pub fn magnitudeOrtho(a: Coord) i32 {
        const abs_vec = a.abs();
        return abs_vec.x + abs_vec.y;
    }

    /// How many diagonal or orthogonal steps to get from a to b.
    pub fn distanceDiag(a: Coord, b: Coord) i32 {
        return b.minus(a).magnitudeDiag();
    }
    pub fn magnitudeDiag(a: Coord) i32 {
        const abs_delta = a.abs();
        return if (abs_delta.x < abs_delta.y) abs_delta.y else abs_delta.x;
    }

    /// Returns true iff at least one dimension is 0.
    pub fn isOrthogonalOrZero(a: Coord) bool {
        return a.x * a.y == 0;
    }

    /// Euclidean distance squared.
    pub fn magnitudeSquared(a: Coord) i32 {
        return a.x * a.x + a.y * a.y;
    }

    pub fn equals(a: Coord, b: Coord) bool {
        return a.x == b.x and a.y == b.y;
    }
};

pub fn makeCoord(x: i32, y: i32) Coord {
    return Coord{
        .x = x,
        .y = y,
    };
}

pub fn isCardinalDirection(direction: Coord) bool {
    return isScaledCardinalDirection(direction, 1);
}
pub fn isScaledCardinalDirection(direction: Coord, scale: i32) bool {
    return direction.isOrthogonalOrZero() and direction.magnitudeSquared() == scale * scale;
}

/// rotation is a number 0 <= r < 8
/// representing the number of 45 degree clockwise turns from right.
/// assert(isCardinalDirection(direction)).
pub fn directionToRotation(direction: Coord) u3 {
    return @as(u3, directionToCardinalIndex(direction)) * 2;
}

/// index in .{right, down, left, up}.
/// assert(isCardinalDirection(direction)).
pub fn directionToCardinalIndex(direction: Coord) u2 {
    if (direction.x == 1 and direction.y == 0) return 0;
    if (direction.x == 0 and direction.y == 1) return 1;
    if (direction.x == -1 and direction.y == 0) return 2;
    if (direction.x == 0 and direction.y == -1) return 3;
    unreachable;
}

pub fn sign(x: i32) i32 {
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
