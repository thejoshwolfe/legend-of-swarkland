pub const Rect = struct.{
    x: i32,
    y: i32,
    width: i32,
    height: i32,
};

pub const Coord = struct.{
    x: i32,
    y: i32,
};

pub fn makeCoord(x: i32, y: i32) Coord {
    return Coord.{
        .x = x,
        .y = y,
    };
}
