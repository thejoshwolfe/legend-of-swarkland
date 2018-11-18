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
