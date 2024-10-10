const RunningState = @import("./gui_main.zig").RunningState;

const std = @import("std");

const InputEngine = @import("InputEngine.zig");

const core = @import("core");
const Coord = core.geometry.Coord;
const makeCoord = core.geometry.makeCoord;
const Rect = core.geometry.Rect;
const deltaToCardinalDirection = core.geometry.deltaToCardinalDirection;
const GameEngineClient = @import("client").GameEngineClient;
const FancyClient = @import("client").FancyClient;
const Species = core.protocol.Species;
const Floor = core.protocol.Floor;
const Wall = core.protocol.Wall;
const TerrainSpace = core.protocol.TerrainSpace;
const Response = core.protocol.Response;
const Event = core.protocol.Event;
const Action = core.protocol.Action;
const PerceivedFrame = core.protocol.PerceivedFrame;
const Equipment = core.protocol.Equipment;
const EquippedItem = core.protocol.EquippedItem;
const getHeadPosition = core.game_logic.getHeadPosition;
const canCharge = core.game_logic.canCharge;
const canUseDoors = core.game_logic.canUseDoors;

pub fn updateTutorialData(state: *RunningState, frames: []PerceivedFrame) void {
    state.consecutive_undos = 0;
    for (frames) |frame| {
        for (frame.others) |other| {
            if (other.kind == .individual and other.kind.individual.activity == .kick) {
                state.kick_observed = true;
            }
        }
        switch (frame.self.kind.individual.activity) {
            .kick => {
                state.kick_performed = true;
            },
            .movement => |delta| {
                if (core.geometry.isOrthogonalVectorOfMagnitude(delta, 2)) {
                    state.charge_performed = true;
                } else {
                    state.moves_performed +|= 1;
                }
            },
            .attack => {
                state.attacks_performed +|= 1;
            },
            else => {},
        }
    }
}

pub fn handleGameInput(state: *RunningState, input: InputEngine.Input) !?enum { back } {
    switch (input) {
        .left => try doDirectionInput(state, makeCoord(-1, 0)),
        .right => try doDirectionInput(state, makeCoord(1, 0)),
        .up => try doDirectionInput(state, makeCoord(0, -1)),
        .down => try doDirectionInput(state, makeCoord(0, 1)),

        .shift_left => try doAutoDirectionInput(state, makeCoord(-1, 0)),
        .shift_right => try doAutoDirectionInput(state, makeCoord(1, 0)),
        .shift_up => try doAutoDirectionInput(state, makeCoord(0, -1)),
        .shift_down => try doAutoDirectionInput(state, makeCoord(0, 1)),

        .greaterthan => {
            if (state.input_prompt == .kick) {
                clearInputState(state);
                try doActionOrShowTutorialForError(state, .stomp);
            }
        },

        .start_attack => {
            if (validateAndShowTotorialForError(state, .attack)) {
                state.input_prompt = .attack;
            }
        },
        .start_kick => {
            _ = validateAndShowTotorialForError(state, .kick);
            state.input_prompt = .kick;
        },
        .start_open_close => {
            _ = validateAndShowTotorialForError(state, .open_close);
            state.input_prompt = .open_close;
        },
        .start_defend => {
            _ = validateAndShowTotorialForError(state, .defend);
            state.input_prompt = .defend;
        },
        .charge => {
            try doActionOrShowTutorialForError(state, .charge);
            state.input_prompt = .none;
        },
        .stomp => {
            try doActionOrShowTutorialForError(state, .stomp);
            state.input_prompt = .none;
        },
        .pick_up => {
            if (isFloorItemSuggestedEquip(state.client_state.?)) |should_equip| {
                if (should_equip) {
                    try doActionOrShowTutorialForError(state, .pick_up_and_equip);
                } else {
                    try doActionOrShowTutorialForError(state, .pick_up_unequipped);
                }
            }
            state.input_prompt = .none;
        },

        .equip_0 => try doEquipmentAction(state, .dagger, false),
        .force_equip_0 => try doEquipmentAction(state, .dagger, true),
        .equip_1 => try doEquipmentAction(state, .torch, false),
        .force_equip_1 => try doEquipmentAction(state, .torch, true),

        .backspace => {
            if (state.input_prompt == .none) {
                try state.client.rewind();
                state.consecutive_undos +|= 1;
            }
            clearInputState(state);
        },
        .spacebar => {
            if (state.input_prompt == .none) {
                clearInputState(state);
                try state.client.act(.wait);
                state.wait_performed = true;
            }
        },
        .escape => {
            clearInputState(state);
            state.animations = null;
        },
        .restart => {
            if (state.new_game_settings == .puzzle_level) {
                try state.client.restartLevel();
                clearInputState(state);
                state.performed_restart = true;
            }
        },
        .quit => {
            state.client.client.stopEngine();
            return .back;
        },

        .beat_level => {
            try state.client.beatLevelMacro(1);
        },
        .beat_level_5 => {
            try state.client.beatLevelMacro(5);
        },
        .unbeat_level => {
            try state.client.unbeatLevelMacro(1);
        },
        .unbeat_level_5 => {
            try state.client.unbeatLevelMacro(5);
        },
        .warp_0 => try state.client.act(Action{ .cheatcode_warp = 0 }),
        .warp_1 => try state.client.act(Action{ .cheatcode_warp = 1 }),
        .warp_2 => try state.client.act(Action{ .cheatcode_warp = 2 }),
        .warp_3 => try state.client.act(Action{ .cheatcode_warp = 3 }),
        .warp_4 => try state.client.act(Action{ .cheatcode_warp = 4 }),
        .warp_5 => try state.client.act(Action{ .cheatcode_warp = 5 }),
        .warp_6 => try state.client.act(Action{ .cheatcode_warp = 6 }),
        .warp_7 => try state.client.act(Action{ .cheatcode_warp = 7 }),

        else => {},
    }
    return null;
}

fn doDirectionInput(state: *RunningState, delta: Coord) !void {
    const action = switch (state.input_prompt) {
        .none => blk: {
            // The default input behavior is a move-like action.
            const myself = state.client_state.?.self;
            if (core.game_logic.canMoveNormally(myself.kind.individual.species)) {
                break :blk Action{ .move = deltaToCardinalDirection(delta) };
            } else if (core.game_logic.canGrowAndShrink(myself.kind.individual.species)) {
                switch (myself.position) {
                    .small => {
                        break :blk Action{ .grow = deltaToCardinalDirection(delta) };
                    },
                    .large => |large_position| {
                        // which direction should we shrink?
                        const position_delta = large_position[0].minus(large_position[1]);
                        if (position_delta.equals(delta)) {
                            // foward
                            break :blk Action{ .shrink = 0 };
                        } else if (position_delta.equals(delta.scaled(-1))) {
                            // backward
                            break :blk Action{ .shrink = 1 };
                        } else {
                            // You cannot shrink sideways.
                            return;
                        }
                    },
                }
            } else {
                // You're not a moving one.
                return;
            }
        },
        .attack => Action{ .attack = deltaToCardinalDirection(delta) },
        .kick => Action{ .kick = deltaToCardinalDirection(delta) },
        .open_close => Action{ .open_close = deltaToCardinalDirection(delta) },
        .defend => Action{ .defend = deltaToCardinalDirection(delta) },
    };
    clearInputState(state);
    try doActionOrShowTutorialForError(state, action);
}

fn doAutoDirectionInput(state: *RunningState, delta: Coord) !void {
    clearInputState(state);

    const myself = state.client_state.?.self;
    if (core.game_logic.canMoveNormally(myself.kind.individual.species)) {
        return state.client.autoMove(delta);
    }
}

fn doActionOrShowTutorialForError(state: *RunningState, action: Action) !void {
    if (validateAndShowTotorialForError(state, action)) {
        try state.client.act(action);
    }
}

var tutorial_text_buffer: [0x100]u8 = undefined;
fn validateAndShowTotorialForError(state: *RunningState, action: std.meta.Tag(Action)) bool {
    const me = state.client_state.?.self;
    if (core.game_logic.validateAction(me.kind.individual.species, me.position, me.kind.individual.status_conditions, me.kind.individual.equipment, action)) {
        state.input_tutorial = null;
        return true;
    } else |err| switch (err) {
        error.StatusForbids => {
            state.input_tutorial = std.fmt.bufPrint(&tutorial_text_buffer, "You cannot {s}. Try Spacebar to wait.", .{@tagName(action)}) catch unreachable;
        },
        error.SpeciesIncapable => {
            state.input_tutorial = std.fmt.bufPrint(&tutorial_text_buffer, "A {s} cannot {s}.", .{ @tagName(me.kind.individual.species), @tagName(action) }) catch unreachable;
        },
        error.MissingItem => {
            state.input_tutorial = std.fmt.bufPrint(&tutorial_text_buffer, "You are missing an item.", .{}) catch unreachable;
        },
        error.TooBig, error.TooSmall => unreachable,
    }
    return false;
}

fn clearInputState(state: *RunningState) void {
    state.input_prompt = .none;
    state.input_tutorial = null;
    state.client.cancelAutoAction();
}

fn getUnequipAction(equipment: Equipment) ?Action {
    if (equipment.is_equipped(.axe)) {
        return .{ .unequip = .axe };
    }
    if (equipment.is_equipped(.dagger)) {
        return .{ .unequip = .dagger };
    }
    if (equipment.is_equipped(.torch)) {
        return .{ .unequip = .torch };
    }
    return null;
}

pub fn isFloorItemSuggestedEquip(frame: PerceivedFrame) ?bool {
    const coord = getHeadPosition(frame.self.position);
    for (frame.others) |other| {
        switch (other.kind) {
            .individual => continue,
            .item => |item| {
                if (!coord.equals(other.position.small)) continue;
                return switch (item) {
                    .dagger, .axe => true,
                    .shield => true,
                    .torch => false,
                };
            },
        }
    }
    return null;
}

fn doEquipmentAction(state: *RunningState, item: EquippedItem, force: bool) !void {
    if (state.input_prompt != .none) return;
    const myself = state.client_state.?.self;
    const equipment = myself.kind.individual.equipment;
    if (equipment.is_equipped(item)) {
        try doActionOrShowTutorialForError(state, .{ .unequip = item });
    } else if (equipment.is_held(item)) {
        if (!force) {
            if (getUnequipAction(equipment)) |unequip_action| {
                return doActionOrShowTutorialForError(state, unequip_action);
            }
        }
        try doActionOrShowTutorialForError(state, .{ .equip = item });
    }
}
