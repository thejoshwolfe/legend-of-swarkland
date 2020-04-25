const std = @import("std");
const ArrayList = std.ArrayList;
const core = @import("../index.zig");
const Coord = core.geometry.Coord;
const makeCoord = core.geometry.makeCoord;

const Species = core.protocol.Species;
const Floor = core.protocol.Floor;
const Wall = core.protocol.Wall;
const TerrainSpace = core.protocol.TerrainSpace;

const game_model = @import("./game_model.zig");
const Individual = game_model.Individual;
const Terrain = game_model.Terrain;
const Level = game_model.Level;
const IdMap = game_model.IdMap;
const oob_terrain = game_model.oob_terrain;

fn makeIndividual(allocator: *std.mem.Allocator, small_position: Coord, species: Species) !*Individual {
    return (Individual{
        .species = species,
        .abs_position = .{ .small = small_position },
    }).clone(allocator);
}

pub fn generate(allocator: *std.mem.Allocator, terrain: *Terrain, individuals: *IdMap(*Individual)) !void {
    terrain.* = try Terrain.initFill(allocator, 1, 1, oob_terrain);
    try individuals.putNoClobber(1, try makeIndividual(allocator, makeCoord(0, 0), .human));
}
