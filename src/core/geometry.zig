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

    pub fn euclideanDistanceSquared(a: Coord) i32 {
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

pub fn isOrthogonalUnitVector(direction: Coord) bool {
    return isOrthogonalVectorOfMagnitude(direction, 1);
}
pub fn isOrthogonalVectorOfMagnitude(direction: Coord, scale: i32) bool {
    return direction.isOrthogonalOrZero() and direction.euclideanDistanceSquared() == scale * scale;
}

/// rotation is a number 0 <= r < 8
/// representing the number of 45 degree clockwise turns from right.
/// assert(isOrthogonalUnitVector(direction)).
pub fn directionToRotation(direction: Coord) u3 {
    return @as(u3, @enumToInt(deltaToCardinalDirection(direction))) * 2;
}

pub const CardinalDirection = enum(u2) {
    east = 0,
    south = 1,
    west = 2,
    north = 3,
};

/// assert(isOrthogonalUnitVector(direction)).
pub fn deltaToCardinalDirection(direction: Coord) CardinalDirection {
    if (direction.x == 1 and direction.y == 0) return .east;
    if (direction.x == 0 and direction.y == 1) return .south;
    if (direction.x == -1 and direction.y == 0) return .west;
    if (direction.x == 0 and direction.y == -1) return .north;
    unreachable;
}
pub fn cardinalDirectionToDelta(cardinal_index: CardinalDirection) Coord {
    return switch (cardinal_index) {
        .east => makeCoord(1, 0),
        .south => makeCoord(0, 1),
        .west => makeCoord(-1, 0),
        .north => makeCoord(0, -1),
    };
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

/// Ramps up speed in the first half of the animation, then slows down to the end.
pub fn bezierMove(x0: Coord, x1: Coord, s: i32, max_s: i32) Coord {
    const x_mid = x0.plus(x1.minus(x0).scaledDivTrunc(2));
    const s_mid = @divFloor(max_s, 2);
    if (s < s_mid) {
        // in the first half, speed up toward the halfway point.
        return bezier3(x0, x0, x_mid, s, s_mid);
    } else {
        // in the second half, slow down from the halfway point.
        return bezier3(x_mid, x1, x1, s - s_mid, s_mid);
    }
}

/// Ramps up speed in the first half, but then bounces backwards to x0.
/// x1 is the would-be destination if the movement didn't bounce.
pub fn bezierBounce(x0: Coord, x1: Coord, s: i32, max_s: i32) Coord {
    const x_mid = x0.plus(x1.minus(x0).scaledDivTrunc(2));
    const s_mid = @divFloor(max_s, 2);
    if (s < s_mid) {
        // in the first half, speed up toward the halfway point of the would-be movement.
        return bezier3(x0, x0, x_mid, s, s_mid);
    } else {
        // in the second half, abruptly reverse course and do the opposite of the above.
        return bezier3(x_mid, x0, x0, s - s_mid, s_mid);
    }
}

pub fn min(a: Coord, b: Coord) Coord {
    return makeCoord(
        if (a.x < b.x) a.x else b.x,
        if (a.y < b.y) a.y else b.y,
    );
}

pub const Rect = struct {
    x: i32,
    y: i32,
    width: i32,
    height: i32,

    pub fn position(self: @This()) Coord {
        return makeCoord(self.x, self.y);
    }
    pub fn size(self: @This()) Coord {
        return makeCoord(self.width, self.height);
    }
    pub fn right(self: @This()) i32 {
        return self.x + self.width;
    }
    pub fn bottom(self: @This()) i32 {
        return self.y + self.height;
    }

    pub fn translated(self: @This(), offset: Coord) Rect {
        return makeRect(self.position().plus(offset), self.size());
    }

    pub fn rowMajorIterator(self: @This()) RowMajorIterator {
        return RowMajorIterator{
            .r = &self,
            .x = self.x,
            .y = self.y,
        };
    }
};
const RowMajorIterator = struct {
    r: *const Rect,
    x: i32,
    y: i32,

    pub fn next(self: *@This()) ?Coord {
        if (self.y >= self.r.bottom()) return null;
        if (self.x >= self.r.right()) {
            self.y += 1;
            if (self.y >= self.r.bottom()) return null;
            self.x = self.r.x;
            defer self.x += 1;
            return makeCoord(self.x, self.y);
        }
        defer self.x += 1;
        return makeCoord(self.x, self.y);
    }
};

pub fn makeRect(position: Coord, size: Coord) Rect {
    return Rect{
        .x = position.x,
        .y = position.y,
        .width = size.x,
        .height = size.y,
    };
}
