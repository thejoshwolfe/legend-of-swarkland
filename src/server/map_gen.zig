const std = @import("std");
const Allocator = std.mem.Allocator;
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
pub fn generate(allocator: Allocator, terrain: *Terrain, individuals: *IdMap(*Individual)) !void {
    var generator = MapGenerator{
        .allocator = allocator,
        .terrain = terrain,
        .individuals = individuals,
        .id_cursor = 1,
        ._r = undefined,
        .random = undefined,
    };

    var buf: [8]u8 = undefined;
    std.crypto.random.bytes(&buf);
    const seed = std.mem.readIntLittle(u64, &buf);
    generator._r = std.rand.DefaultPrng.init(seed);
    generator.random = &generator._r.random();

    return generator.generate();
}

const MapGenerator = struct {
    allocator: Allocator,
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
    fn makeWideBoi(self: *@This(), position0: Coord, position1: Coord, species: Species) !*Individual {
        return (Individual{
            .species = species,
            .abs_position = .{ .large = .{ position0, position1 } },
        }).clone(self.allocator);
    }

    fn generate(self: *@This()) !void {
        const width = 10;
        const height = 10;
        self.terrain.* = try Terrain.initFill(self.allocator, width, height, .{
            .floor = .dirt,
            .wall = .air,
        });

        var free_spaces = try ArrayList(Coord).initCapacity(self.allocator, width * height);
        var y: i32 = 0;
        while (y < height) : (y += 1) {
            var x: i32 = 0;
            while (x < width) : (x += 1) {
                free_spaces.append(makeCoord(x, y)) catch unreachable;
            }
        }
        // You are the human.
        try self.individuals.putNoClobber(self.nextId(), try self.makeIndividual(self.popRandom(&free_spaces), .human));

        // throw enemies around
        {
            const count = 1;
            var i: usize = 0;
            while (i < count) : (i += 1) {
                const fella = try self.makeIndividual(self.popRandom(&free_spaces), .orc);
                fella.has_shield = self.random.boolean() and false; // TODO
                try self.individuals.putNoClobber(self.nextId(), fella);
            }
            try self.individuals.putNoClobber(self.nextId(), try self.makeIndividual(self.popRandom(&free_spaces), Species{ .blob = .small_blob }));
            try self.individuals.putNoClobber(self.nextId(), try self.makeIndividual(self.popRandom(&free_spaces), .turtle));
        }

        // let's throw around some lava.
        {
            const count = 1;
            var i: usize = 0;
            while (i < count) : (i += 1) {
                self.terrain.atCoord(self.popRandom(&free_spaces)).?.* = .{
                    .floor = .lava,
                    .wall = .air,
                };
            }
        }

        // maybe a heal spot
        {
            const count = 1;
            var i: usize = 0;
            while (i < count) : (i += 1) {
                self.terrain.atCoord(self.popRandom(&free_spaces)).?.* = .{
                    .floor = .marble,
                    .wall = .air,
                };
            }
        }

        // and some walls
        {
            const count = 1;
            var i: usize = 0;
            while (i < count) : (i += 1) {
                self.terrain.atCoord(self.popRandom(&free_spaces)).?.* = .{
                    .floor = .dirt,
                    .wall = .dirt,
                };
            }
        }

        // have fun
        {
            const count = 1;
            var i: usize = 0;
            while (i < count) : (i += 1) {
                self.terrain.atCoord(self.popRandom(&free_spaces)).?.* = .{
                    .floor = .dirt,
                    .wall = .centaur_transformer,
                };
            }
        }
    }

    fn popRandom(self: *@This(), array: *ArrayList(Coord)) Coord {
        return array.swapRemove(self.random.uintLessThan(usize, array.items.len));
    }
};
