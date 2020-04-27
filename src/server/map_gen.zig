const std = @import("std");
const ArrayList = std.ArrayList;

const core = @import("../index.zig");
const Coord = core.geometry.Coord;
const makeCoord = core.geometry.makeCoord;

const Species = core.protocol.Species;

const game_model = @import("./game_model.zig");
const Individual = game_model.Individual;
const Terrain = game_model.Terrain;
const IdMap = game_model.IdMap;
const oob_terrain = game_model.oob_terrain;

/// overwrites terrain. populates individuals.
pub fn generate(allocator: *std.mem.Allocator, terrain: *Terrain, individuals: *IdMap(*Individual)) !void {
    var generator = MapGenerator{
        .allocator = allocator,
        .terrain = terrain,
        .individuals = individuals,
        .id_cursor = 1,
        ._r = undefined,
        .random = undefined,
    };

    // TODO: accept seed parameter
    var buf: [8]u8 = undefined;
    try std.crypto.randomBytes(&buf);
    const seed = std.mem.readIntLittle(u64, &buf);
    generator._r = std.rand.DefaultPrng.init(seed);
    generator.random = &generator._r.random;

    return generator.generate();
}

const MapGenerator = struct {
    allocator: *std.mem.Allocator,
    terrain: *Terrain,
    individuals: *IdMap(*Individual),
    id_cursor: u32,
    _r: std.rand.DefaultPrng,
    random: *std.rand.Random,

    fn nextId(self: *@This()) u32 {
        defer {
            self.id_cursor += 1;
        }
        return self.id_cursor;
    }

    fn makeIndividual(self: *@This(), small_position: Coord, species: Species) !*Individual {
        return (Individual{
            .species = species,
            .abs_position = .{ .small = small_position },
        }).clone(self.allocator);
    }

    fn generate(self: *@This()) !void {
        const width = 30;
        const height = 30;
        self.terrain.* = try Terrain.initFill(self.allocator, width, height, .{
            .floor = .dirt,
            .wall = .air,
        });

        // You are the human.
        try self.individuals.putNoClobber(self.nextId(), try self.makeIndividual(makeCoord(0, 0), .human));

        const enemy_count = self.random.intRangeAtMost(usize, 10, 20);
        var i: usize = 0;
        while (i < enemy_count) : (i += 1) {
            var location = makeCoord(self.random.intRangeLessThan(i32, 1, width), self.random.intRangeLessThan(i32, 1, height));
            try self.individuals.putNoClobber(self.nextId(), try self.makeIndividual(location, .orc));
        }
    }
};
