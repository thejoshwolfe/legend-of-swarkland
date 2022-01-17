client: GameEngineClient,

// client-side memory
remembered_terrain: PerceivedTerrain,
terrain_is_currently_in_view: core.matrix.SparseChunkedMatrix(bool, false),

// convenience things
auto_move: ?Coord = null,

// puzzle level control stuff
current_level: usize = 0,
starting_level: usize = 0,
moves_per_level: []usize,
pending_forward_actions: usize = 0,
pending_backward_actions: usize = 0,

const std = @import("std");
const core = @import("../index.zig");
const game_server = @import("../server/game_server.zig");
const debugPrintAction = game_server.debugPrintAction;
const Coord = core.geometry.Coord;
const deltaToCardinalDirection = core.geometry.deltaToCardinalDirection;
const Request = core.protocol.Request;
const NewGameSettings = core.protocol.NewGameSettings;
const Action = core.protocol.Action;
const Response = core.protocol.Response;
const unseen_terrain = core.game_logic.unseen_terrain;
const PerceivedHappening = core.protocol.PerceivedHappening;
const PerceivedTerrain = core.game_logic.PerceivedTerrain;
const PerceivedFrame = core.protocol.PerceivedFrame;
const TerrainSpace = core.protocol.TerrainSpace;
const Wall = core.protocol.Wall;
const Floor = core.protocol.Floor;
const cheatcodes = struct {
    pub const beat_level_actions = [_][]const Action{}; // TODO: fix or delete.
};
const GameEngineClient = @import("game_engine_client.zig").GameEngineClient;

const the_levels = @import("../server/map_gen.zig").the_levels;
const allocator = std.heap.c_allocator;

pub fn init(client: GameEngineClient) !@This() {
    const moves_per_level = try allocator.alloc(usize, the_levels.len);
    std.mem.set(usize, moves_per_level, 0);
    moves_per_level[0] = 1;

    return @This(){
        .client = client,
        .remembered_terrain = PerceivedTerrain.init(allocator),
        .terrain_is_currently_in_view = core.matrix.SparseChunkedMatrix(bool, false).init(allocator),
        .moves_per_level = moves_per_level,
    };
}

pub fn startGame(self: *@This(), new_game_settings: NewGameSettings) !void {
    try self.enqueueRequest(Request{ .start_game = new_game_settings });
    // starting a new game looks like undoing.
    self.pending_backward_actions += 1;
}

pub fn act(self: *@This(), action: Action) !void {
    try self.enqueueRequest(Request{ .act = action });
    debugPrintAction(0, action);
}
pub fn rewind(self: *@This()) !void {
    const data = self.getMoveCounterInfo();
    if (data.moves_into_current_level == 0 and data.current_level <= self.starting_level) {
        core.debug.move_counter.print("can't undo the start of the starting level", .{});
        return;
    }

    try self.enqueueRequest(.rewind);
    core.debug.actions.print("[rewind]", .{});
}

pub fn autoMove(self: *@This(), direction: Coord) !void {
    self.auto_move = direction;
    try self.act(Action{ .move = deltaToCardinalDirection(direction) });
}
pub fn cancelAutoAction(self: *@This()) void {
    self.auto_move = null;
}
fn maybeContinueAutoAction(self: *@This(), happening: PerceivedHappening) !void {
    const direction = self.auto_move orelse return;

    if (happening.frames.len == 1) {
        core.debug.auto_action.print("nothing happened. last move was rejected or player hit undo or something.", .{});
        return self.cancelAutoAction();
    }
    for (happening.frames) |frame| {
        if (frame.others.len > 0) {
            core.debug.auto_action.print("i see someone. stopping.", .{});
            return self.cancelAutoAction();
        }
    }
    if (core.game_logic.positionEquals(happening.frames[0].self.position, last(happening.frames).self.position)) {
        core.debug.auto_action.print("movement didn't get us anywhere. stopping.", .{});
        return self.cancelAutoAction();
    }

    core.debug.auto_action.print("continuing auto move: {},{}", .{ direction.x, direction.y });
    try self.act(Action{ .move = deltaToCardinalDirection(direction) });
}

pub fn beatLevelMacro(self: *@This(), how_many: usize) !void {
    const data = self.getMoveCounterInfo();
    if (data.moves_into_current_level > 0) {
        core.debug.move_counter.print("beat level is restarting first with {} undos.", .{data.moves_into_current_level});
        for (times(data.moves_into_current_level)) |_| {
            try self.rewind();
        }
    }
    for (times(how_many)) |_, i| {
        if (data.current_level + i >= cheatcodes.beat_level_actions.len) {
            core.debug.move_counter.print("beat level is stopping at the end of the levels.", .{});
            return;
        }
        for (cheatcodes.beat_level_actions[data.current_level + i]) |action| {
            try self.enqueueRequest(Request{ .act = action });
        }
    }
}
pub fn unbeatLevelMacro(self: *@This(), how_many: usize) !void {
    for (times(how_many)) |_| {
        const data = self.getMoveCounterInfo();
        if (data.moves_into_current_level > 0) {
            core.debug.move_counter.print("unbeat level is restarting first with {} undos.", .{data.moves_into_current_level});
            for (times(data.moves_into_current_level)) |_| {
                try self.rewind();
            }
        } else if (data.current_level <= self.starting_level) {
            core.debug.move_counter.print("unbeat level reached the first level.", .{});
            return;
        } else {
            const unbeat_level = data.current_level - 1;
            const moves_counter = self.moves_per_level[unbeat_level];
            core.debug.move_counter.print("unbeat level is undoing level {} with {} undos.", .{ unbeat_level, moves_counter });
            for (times(moves_counter)) |_| {
                try self.rewind();
            }
        }
    }
}

pub fn restartLevel(self: *@This()) !void {
    const data = self.getMoveCounterInfo();
    core.debug.move_counter.print("restarting is undoing {} times.", .{data.moves_into_current_level});
    for (times(data.moves_into_current_level)) |_| {
        try self.rewind();
    }
}

const MoveCounterData = struct {
    current_level: usize,
    moves_into_current_level: usize,
};
fn getMoveCounterInfo(self: @This()) MoveCounterData {
    var remaining_pending_backward_actions = self.pending_backward_actions;
    var remaining_pending_forward_actions = self.pending_forward_actions;
    var pending_current_level = self.current_level;
    while (remaining_pending_backward_actions > self.moves_per_level[pending_current_level] + remaining_pending_forward_actions) {
        core.debug.move_counter.print("before the start of a level: (m[{}]={})+{}-{} < zero,", .{
            pending_current_level,
            self.moves_per_level[pending_current_level],
            remaining_pending_forward_actions,
            remaining_pending_backward_actions,
        });
        remaining_pending_backward_actions -= self.moves_per_level[pending_current_level] + remaining_pending_forward_actions;
        remaining_pending_forward_actions = 0;
        if (pending_current_level == 0) {
            return .{
                .current_level = 0,
                .moves_into_current_level = 0,
            };
        }
        pending_current_level -= 1;
        core.debug.move_counter.print("looking at the previous level.", .{});
    }
    const moves_counter = self.moves_per_level[pending_current_level] + remaining_pending_forward_actions - remaining_pending_backward_actions;
    core.debug.move_counter.print("move counter data: (m[{}]={})+{}-{}={}", .{
        pending_current_level,
        self.moves_per_level[pending_current_level],
        remaining_pending_forward_actions,
        remaining_pending_backward_actions,
        moves_counter,
    });
    return .{
        .current_level = pending_current_level,
        .moves_into_current_level = moves_counter,
    };
}

pub fn stopUndoPastLevel(self: *@This(), starting_level: usize) void {
    self.starting_level = starting_level;
}

fn enqueueRequest(self: *@This(), request: Request) !void {
    switch (request) {
        .act => {
            self.pending_forward_actions += 1;
        },
        .rewind => {
            self.pending_backward_actions += 1;
        },
        .start_game => {},
    }
    try self.client.queues.enqueueRequest(request);
}
pub fn takeResponse(self: *@This()) !?Response {
    const response = self.client.queues.takeResponse() orelse return null;
    // Monitor the responses to count moves per level.
    switch (response) {
        .stuff_happens => |happening| {
            self.pending_forward_actions -= 1;
            self.moves_per_level[happening.frames[0].completed_levels] += 1;
            self.current_level = last(happening.frames).completed_levels;
            try self.updateRememberedTerrain(last(happening.frames));
            try self.maybeContinueAutoAction(happening);
        },
        .load_state => |frame| {
            // This can also happen for the initial game load.
            self.pending_backward_actions -= 1;
            self.moves_per_level[frame.completed_levels] -= 1;
            self.current_level = frame.completed_levels;
            try self.updateRememberedTerrain(frame);
        },
        .reject_request => |request| {
            switch (request) {
                .act => {
                    self.pending_forward_actions -= 1;
                },
                .rewind => {
                    self.pending_backward_actions -= 1;
                },
                .start_game => {},
            }
        },
    }
    return response;
}

fn updateRememberedTerrain(self: *@This(), frame: PerceivedFrame) !void {
    self.terrain_is_currently_in_view.clear();
    var y = frame.terrain.position.y;
    while (y < frame.terrain.position.y + frame.terrain.height) : (y += 1) {
        var x = frame.terrain.position.x;
        const inner_y = @intCast(u16, y - frame.terrain.position.y);
        while (x < frame.terrain.position.x + frame.terrain.width) : (x += 1) {
            const inner_x = @intCast(u16, x - frame.terrain.position.x);
            const cell = frame.terrain.matrix[inner_y * frame.terrain.width + inner_x];
            if (terrainSpaceEquals(cell, unseen_terrain)) continue;
            const cell_ptr = try self.remembered_terrain.getOrPut(x, y);
            var in_view = false;
            if (isFloorMoreKnown(cell_ptr.floor, cell.floor)) {
                cell_ptr.floor = cell.floor;
                in_view = true;
            }
            if (isWallMoreKnown(cell_ptr.wall, cell.wall)) {
                cell_ptr.wall = cell.wall;
                in_view = true;
            }
            if (in_view) {
                try self.terrain_is_currently_in_view.put(x, y, true);
            }
        }
    }
}

fn last(arr: anytype) @TypeOf(arr[arr.len - 1]) {
    return arr[arr.len - 1];
}

fn times(n: usize) []const void {
    return @as([(1 << (@sizeOf(usize) * 8) - 1)]void, undefined)[0..n];
}

fn terrainSpaceEquals(a: TerrainSpace, b: TerrainSpace) bool {
    return a.floor == b.floor and a.wall == b.wall;
}

fn isFloorMoreKnown(base: Floor, new: Floor) bool {
    return switch (new) {
        .unknown => false,
        .unknown_floor => base == .unknown,
        else => true,
    };
}
fn isWallMoreKnown(base: Wall, new: Wall) bool {
    return switch (new) {
        .unknown => false,
        .unknown_wall => base == .unknown,
        else => true,
    };
}
