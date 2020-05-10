const std = @import("std");
const ArrayList = std.ArrayList;

const core = @import("../index.zig");
const Coord = core.geometry.Coord;
const makeCoord = core.geometry.makeCoord;

const Species = core.protocol.Species;

const game_model = @import("./game_model.zig");
const Individual = game_model.Individual;
const Item = game_model.Item;
const Terrain = game_model.Terrain;
const IdMap = game_model.IdMap;
const oob_terrain = game_model.oob_terrain;

/// overwrites terrain. populates individuals.
pub fn generate(allocator: *std.mem.Allocator, terrain: *Terrain, individuals: *IdMap(*Individual), items: *IdMap(*Item)) !void {
    var generator = MapGenerator{
        .allocator = allocator,
        .terrain = terrain,
        .individuals = individuals,
        .items = items,
        .individual_id_cursor = 1,
        .item_id_cursor = 1,
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
    items: *IdMap(*Item),
    individual_id_cursor: u32,
    item_id_cursor: u32,
    _r: std.rand.DefaultPrng,
    random: *std.rand.Random,

    fn nextIndividualId(self: *@This()) u32 {
        defer {
            self.individual_id_cursor += 1;
        }
        return self.individual_id_cursor;
    }
    fn nextItemId(self: *@This()) u32 {
        defer {
            self.item_id_cursor += 1;
        }
        return self.item_id_cursor;
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

        var free_spaces = try ArrayList(Coord).initCapacity(self.allocator, width * height);
        var y: i32 = 0;
        while (y < height) : (y += 1) {
            var x: i32 = 0;
            while (x < width) : (x += 1) {
                free_spaces.append(makeCoord(x, y)) catch unreachable;
            }
        }

        // You are the human.
        try self.individuals.putNoClobber(self.nextIndividualId(), try self.makeIndividual(self.popRandom(&free_spaces), .human));

        // throw enemies around
        {
            const count = self.random.intRangeAtMost(usize, 10, 20);
            var i: usize = 0;
            while (i < count) : (i += 1) {
                const fella = try self.makeIndividual(self.popRandom(&free_spaces), .orc);
                const individual_id = self.nextIndividualId();
                try self.individuals.putNoClobber(individual_id, fella);

                const has_shield = self.random.boolean();
                if (has_shield) {
                    const item = Item{
                        .position = .{ .held_by_individual = individual_id },
                    };
                    const item_id = self.nextItemId();
                    try self.items.putNoClobber(item_id, try item.clone(self.allocator));
                }
            }
        }

        // let's throw around some lava.
        {
            const count = self.random.intRangeAtMost(usize, 40, 80);
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
            const count = self.random.intRangeAtMost(usize, 0, 1);
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
            const count = self.random.intRangeAtMost(usize, 20, 40);
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
            const count = self.random.intRangeAtMost(usize, 1, 2);
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
