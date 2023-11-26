const std = @import("std");
const ArrayListUnmanaged = std.ArrayListUnmanaged;
const allocator = std.heap.c_allocator;

const sdl = @import("sdl.zig");
const core = @import("core");
const textures = @import("textures.zig");
const gui = @import("gui.zig");

const RunningState = @import("./gui_main.zig").RunningState;
const isFloorItemSuggestedEquip = @import("./gui_main.zig").isFloorItemSuggestedEquip;

const Coord = core.geometry.Coord;
const makeCoord = core.geometry.makeCoord;
const Rect = core.geometry.Rect;
const directionToRotation = core.geometry.directionToRotation;
const deltaToCardinalDirection = core.geometry.deltaToCardinalDirection;

const PerceivedFrame = core.protocol.PerceivedFrame;
const PerceivedThing = core.protocol.PerceivedThing;
const Species = core.protocol.Species;
const EquippedItem = core.protocol.EquippedItem;
const Action = core.protocol.Action;

const getHeadPosition = core.game_logic.getHeadPosition;
const getPhysicsLayer = core.game_logic.getPhysicsLayer;
const canKick = core.game_logic.canKick;

const aesthetic_seed = 0xbee894fc;

pub fn renderThings(state: *RunningState, renderer: *sdl.Renderer, now: i32) !void {
    // at one point in what frame should we render?
    var frame = state.client_state.?;
    var progress: i32 = 0;
    var move_frame_time: i32 = 1;
    var display_any_input_prompt = true;
    if (state.animations) |animations| {
        if (animations.frameAtTime(now)) |data| {
            // animating
            frame = animations.frames.items[data.frame_index];
            progress = data.progress;
            move_frame_time = data.time_per_frame;
            display_any_input_prompt = false;
        } else {
            // stale
            state.animations = null;
        }
    }

    // top left corner of the screen in world coordinates * 32
    const screen_display_position = b: {
        const self_display_rect = getDisplayRect(progress, move_frame_time, frame.self);
        const self_display_center = self_display_rect.position().plus(self_display_rect.size().scaledDivTrunc(2));
        const screen_half_size = makeCoord(16, 16).scaled(32 / 2);
        break :b self_display_center.minus(screen_half_size);
    };

    // render terrain
    {
        const my_head_coord = getHeadPosition(frame.self.position);
        const render_bounding_box = Rect{
            .x = my_head_coord.x - 8,
            .y = my_head_coord.y - 8,
            .width = 18,
            .height = 18,
        };
        var cursor = render_bounding_box.position();
        while (cursor.y < render_bounding_box.bottom()) : (cursor.y += 1) {
            cursor.x = render_bounding_box.x;
            while (cursor.x < render_bounding_box.right()) : (cursor.x += 1) {
                const cell = state.client.remembered_terrain.getCoord(cursor);
                const render_position = cursor.scaled(32).minus(screen_display_position);

                render_floor: {
                    const floor_texture = switch (cell.floor) {
                        .unknown => break :render_floor,
                        .dirt => selectAesthetic(textures.sprites.dirt_floor[0..], aesthetic_seed, cursor),
                        .grass,
                        .grass_and_water_edge_east,
                        .grass_and_water_edge_southeast,
                        .grass_and_water_edge_south,
                        .grass_and_water_edge_southwest,
                        .grass_and_water_edge_west,
                        .grass_and_water_edge_northwest,
                        .grass_and_water_edge_north,
                        .grass_and_water_edge_northeast,
                        => selectAesthetic(textures.sprites.grass[0..], aesthetic_seed, cursor),
                        .sand => selectAestheticBiasedLow(textures.sprites.sand[0..], aesthetic_seed, cursor, 5),
                        .sandstone => selectAestheticBiasedLow(textures.sprites.sandstone_floor[0..], aesthetic_seed, cursor, 5),
                        .marble => selectAesthetic(textures.sprites.marble_floor[0..], aesthetic_seed, cursor),
                        .lava => selectAesthetic(textures.sprites.lava[0..], aesthetic_seed, cursor),
                        .hatch => textures.sprites.hatch,
                        .stairs_down => textures.sprites.stairs_down,
                        .water, .water_bloody => selectAesthetic(textures.sprites.water[0..], aesthetic_seed, cursor),
                        .water_deep => selectAesthetic(textures.sprites.water_deep[0..], aesthetic_seed, cursor),
                        .unknown_floor => textures.sprites.unknown_floor,
                    };
                    textures.renderSprite(renderer, floor_texture, render_position);
                    const second_floor_texture = switch (cell.floor) {
                        .grass_and_water_edge_east => textures.sprites.water_edge[0],
                        .grass_and_water_edge_southeast => textures.sprites.water_edge[1],
                        .grass_and_water_edge_south => textures.sprites.water_edge[2],
                        .grass_and_water_edge_southwest => textures.sprites.water_edge[3],
                        .grass_and_water_edge_west => textures.sprites.water_edge[4],
                        .grass_and_water_edge_northwest => textures.sprites.water_edge[5],
                        .grass_and_water_edge_north => textures.sprites.water_edge[6],
                        .grass_and_water_edge_northeast => textures.sprites.water_edge[7],
                        .water_bloody => textures.sprites.bloody_water_overlay,
                        else => break :render_floor,
                    };
                    textures.renderSprite(renderer, second_floor_texture, render_position);
                }
                render_wall: {
                    const wall_texture = switch (cell.wall) {
                        .unknown => break :render_wall,
                        .air => break :render_wall,
                        .dirt => selectAesthetic(textures.sprites.brown_brick[0..], aesthetic_seed, cursor),
                        .tree_northwest => textures.sprites.tree[0],
                        .tree_northeast => textures.sprites.tree[1],
                        .tree_southwest => textures.sprites.tree[2],
                        .tree_southeast => textures.sprites.tree[3],
                        .bush => selectAesthetic(textures.sprites.bush[0..], aesthetic_seed, cursor),
                        .door_open => textures.sprites.open_door,
                        .door_closed => textures.sprites.closed_door,
                        .stone => selectAesthetic(textures.sprites.gray_brick[0..], aesthetic_seed, cursor),
                        .sandstone => selectAestheticBiasedLow(textures.sprites.sandstone_wall[0..], aesthetic_seed, cursor, 5),
                        .sandstone_cracked => textures.sprites.sandstone_wall_cracked,
                        .angel_statue => textures.sprites.statue_angel,
                        .chest => textures.sprites.chest,
                        .polymorph_trap_centaur, .polymorph_trap_kangaroo, .polymorph_trap_turtle, .polymorph_trap_blob, .polymorph_trap_human, .unknown_polymorph_trap => textures.sprites.polymorph_trap,
                        .polymorph_trap_rhino_west, .polymorph_trap_blob_west, .unknown_polymorph_trap_west => textures.sprites.polymorph_trap_wide[0],
                        .polymorph_trap_rhino_east, .polymorph_trap_blob_east, .unknown_polymorph_trap_east => textures.sprites.polymorph_trap_wide[1],
                        .unknown_wall => textures.sprites.unknown_wall,
                    };
                    textures.renderSprite(renderer, wall_texture, render_position);
                }

                if (!state.client.terrain_is_currently_in_view.getCoord(cursor)) {
                    // Gray out the space
                    textures.renderSprite(renderer, textures.sprites.black_alpha_square, render_position);
                }
            }
        }
    }

    // render the things
    for ([4]u2{ 0, 1, 2, 3 }) |physics_layer| {
        for (frame.others) |other| {
            switch (other.kind) {
                .individual => {
                    if (getPhysicsLayer(other.kind.individual.species) != physics_layer) continue;
                },
                .item => {
                    // put items on layer 0?
                    if (physics_layer != 0) continue;
                },
            }
            _ = renderThing(renderer, progress, move_frame_time, screen_display_position, other);
        }
    }
    const render_position = renderThing(renderer, progress, move_frame_time, screen_display_position, frame.self);
    // render input prompt
    if (display_any_input_prompt) {
        switch (state.input_prompt) {
            .none => {},
            .attack => {
                var attack_prompt_sprite = textures.sprites.dagger;
                if (frame.self.kind.individual.equipment.is_equipped(.axe)) {
                    attack_prompt_sprite = textures.sprites.axe;
                } else if (frame.self.kind.individual.equipment.is_equipped(.torch)) {
                    attack_prompt_sprite = textures.sprites.torch;
                } else if (frame.self.kind.individual.equipment.is_equipped(.dagger)) {
                    attack_prompt_sprite = textures.sprites.dagger;
                }
                textures.renderSprite(renderer, attack_prompt_sprite, render_position);
            },
            .kick => {
                textures.renderSprite(renderer, textures.sprites.kick, render_position);
            },
            .open_close => {
                textures.renderSprite(renderer, textures.sprites.open_close, render_position);
            },
            .defend => {
                textures.renderSprite(renderer, textures.sprites.shield, render_position);
            },
        }
    }
    // render activity effects
    for (frame.others) |other| {
        renderActivity(renderer, progress, move_frame_time, screen_display_position, other);
    }
    renderActivity(renderer, progress, move_frame_time, screen_display_position, frame.self);

    // sidebar
    const anatomy_coord = makeCoord(512, 0);
    {
        const sidebard_render_rect = sdl.makeRect(Rect{
            .x = anatomy_coord.x,
            .y = anatomy_coord.y,
            .width = 200,
            .height = 512,
        });
        sdl.assertZero(sdl.c.SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255));
        sdl.assertZero(sdl.c.SDL_RenderFillRect(renderer, &sidebard_render_rect));

        const AnatomySprites = struct {
            diagram: Rect,
            being_digested: ?Rect = null,
            leg_wound: ?Rect = null,
            grappled: ?Rect = null,
            limping: ?Rect = null,
            grappling_back: ?Rect = null,
            digesting: ?Rect = null,
            grappling_front: ?Rect = null,
            malaise: ?Rect = null,
            pain: ?Rect = null,
            shielded: ?Rect = null,
            equipped_axe: ?Rect = null,
            equipped_torch: ?Rect = null,
            equipped_dagger: ?Rect = null,
        };
        const anatomy_sprites = switch (core.game_logic.getAnatomy(frame.self.kind.individual.species)) {
            .humanoid => AnatomySprites{
                .diagram = textures.large_sprites.humanoid,
                .being_digested = textures.large_sprites.humanoid_being_digested,
                .leg_wound = textures.large_sprites.humanoid_leg_wound,
                .grappled = textures.large_sprites.humanoid_grappled,
                .limping = textures.large_sprites.humanoid_limping,
                .malaise = textures.large_sprites.humanoid_malaise,
                .pain = textures.large_sprites.humanoid_pain,
                .shielded = textures.large_sprites.humanoid_shielded,
                .equipped_axe = textures.large_sprites.humanoid_equipped_axe,
                .equipped_torch = textures.large_sprites.humanoid_equipped_torch,
                .equipped_dagger = textures.large_sprites.humanoid_equipped_dagger,
            },
            .centauroid => AnatomySprites{
                .diagram = textures.large_sprites.centauroid,
                .being_digested = textures.large_sprites.centauroid_being_digested,
                .leg_wound = textures.large_sprites.centauroid_leg_wound,
                .grappled = textures.large_sprites.centauroid_grappled,
                .limping = textures.large_sprites.centauroid_limping,
            },
            .bloboid => AnatomySprites{
                .diagram = switch (frame.self.kind.individual.species.blob) {
                    .small_blob => switch (frame.self.position) {
                        .small => textures.large_sprites.bloboid_small,
                        .large => textures.large_sprites.bloboid_small_wide,
                    },
                    .large_blob => switch (frame.self.position) {
                        .small => textures.large_sprites.bloboid_large,
                        .large => textures.large_sprites.bloboid_large_wide,
                    },
                },
                .grappling_back = textures.large_sprites.bloboid_grappling_back,
                .digesting = textures.large_sprites.bloboid_digesting,
                .grappling_front = textures.large_sprites.bloboid_grappling_front,
            },
            .kangaroid => AnatomySprites{
                .diagram = textures.large_sprites.kangaroid,
                .being_digested = textures.large_sprites.kangaroid_being_digested,
                .leg_wound = textures.large_sprites.kangaroid_leg_wound,
                .grappled = textures.large_sprites.kangaroid_grappled,
                .limping = textures.large_sprites.kangaroid_limping,
            },
            .quadruped => AnatomySprites{
                .diagram = textures.large_sprites.quadruped,
                .being_digested = textures.large_sprites.quadruped_being_digested,
                .leg_wound = textures.large_sprites.quadruped_leg_wound,
                .grappled = textures.large_sprites.quadruped_grappled,
                .limping = textures.large_sprites.quadruped_limping,
            },
            .scorpionoid => @panic("TODO"),
            .serpentine => @panic("TODO"),
            .insectoid => @panic("TODO"),
            .minotauroid => @panic("TODO"),
            .mermoid => @panic("TODO"),
        };
        textures.renderLargeSprite(renderer, anatomy_sprites.diagram, anatomy_coord);

        if (frame.self.kind.individual.equipment.is_equipped(.shield)) {
            textures.renderLargeSprite(renderer, anatomy_sprites.shielded.?, anatomy_coord);
        }
        if (frame.self.kind.individual.equipment.is_equipped(.axe)) {
            textures.renderLargeSprite(renderer, anatomy_sprites.equipped_axe.?, anatomy_coord);
        }
        if (frame.self.kind.individual.equipment.is_equipped(.torch)) {
            textures.renderLargeSprite(renderer, anatomy_sprites.equipped_torch.?, anatomy_coord);
        }
        if (frame.self.kind.individual.equipment.is_equipped(.dagger)) {
            textures.renderLargeSprite(renderer, anatomy_sprites.equipped_dagger.?, anatomy_coord);
        }
        // explicit integer here to provide a compile error when new items get added.
        const status_conditions: u9 = frame.self.kind.individual.status_conditions;
        if (0 != status_conditions & core.protocol.StatusCondition_being_digested) {
            textures.renderLargeSprite(renderer, anatomy_sprites.being_digested.?, anatomy_coord);
        }
        if (0 != status_conditions & core.protocol.StatusCondition_wounded_leg) {
            textures.renderLargeSprite(renderer, anatomy_sprites.leg_wound.?, anatomy_coord);
        }
        if (0 != status_conditions & core.protocol.StatusCondition_grappled) {
            textures.renderLargeSprite(renderer, anatomy_sprites.grappled.?, anatomy_coord);
        }
        if (0 != status_conditions & core.protocol.StatusCondition_limping) {
            textures.renderLargeSprite(renderer, anatomy_sprites.limping.?, anatomy_coord);
        }
        if (0 != status_conditions & core.protocol.StatusCondition_grappling) {
            textures.renderLargeSprite(renderer, anatomy_sprites.grappling_back.?, anatomy_coord);
        }
        if (0 != status_conditions & core.protocol.StatusCondition_digesting) {
            textures.renderLargeSprite(renderer, anatomy_sprites.digesting.?, anatomy_coord);
        }
        if (0 != status_conditions & core.protocol.StatusCondition_grappling) {
            textures.renderLargeSprite(renderer, anatomy_sprites.grappling_front.?, anatomy_coord);
        }
        if (0 != status_conditions & core.protocol.StatusCondition_malaise) {
            textures.renderLargeSprite(renderer, anatomy_sprites.malaise.?, anatomy_coord);
        }
        if (0 != status_conditions & core.protocol.StatusCondition_pain) {
            textures.renderLargeSprite(renderer, anatomy_sprites.pain.?, anatomy_coord);
        }
    }
    // input options
    const inventory_coord = makeCoord(anatomy_coord.x, anatomy_coord.y + 200);
    var inventory_height: i32 = 0;
    {
        const myself = state.client_state.?.self;
        const equipment = myself.kind.individual.equipment;

        var unequipped = equipment.held & ~equipment.equipped;
        var cursor_x = inventory_coord.x;
        while (unequipped != 0) {
            const item_int = @as(@TypeOf(@intFromEnum(@as(EquippedItem, undefined))), @intCast(@ctz(unequipped)));
            unequipped &= ~(@as(@TypeOf(unequipped), 1) << item_int);
            const item = @as(EquippedItem, @enumFromInt(item_int));

            textures.renderSprite(renderer, switch (item) {
                .shield => textures.sprites.shield,
                .axe => textures.sprites.axe,
                .torch => textures.sprites.torch,
                .dagger => textures.sprites.dagger,
            }, makeCoord(cursor_x, inventory_coord.y));
            cursor_x += 32;
            inventory_height = 32;
        }
    }
    {
        const myself = state.client_state.?.self;
        var g = gui.Gui.init(renderer);
        g.seek(anatomy_coord.x, inventory_coord.y + inventory_height);
        g.font(.small);
        g.marginBottom(1);
        g.text("Inputs:");
        switch (state.input_prompt) {
            .none => {
                g.text(" Space: Wait");
                if (can(myself, .move)) {
                    g.text(" Arrows: Move");
                    g.text(" Shift+Arrows: Move repeatedly");
                } else if (can(myself, .grow)) {
                    g.text(" Arrows: Grow");
                    g.text(" Shift+Arrows: Grow/shrink repeatedly");
                } else if (can(myself, .shrink)) {
                    if (myself.position.large[0].x == myself.position.large[1].x) {
                        g.text(" Up/Down: Shrink");
                        g.text(" Shift+Up/Down: Shrink/grow repeatedly");
                    } else {
                        g.text(" Left/Right: Shrink");
                        g.text(" Shift+Left/Right: Shrink/grow repeatedly");
                    }
                }
                if (can(myself, .attack)) {
                    g.text(" F: Start attack...");
                }
                if (can(myself, .defend)) {
                    g.text(" D: Start defend...");
                }
                if (can(myself, .kick)) {
                    g.text(" K: Start kick...");
                }
                if (can(myself, .stomp)) {
                    g.text(" S: Stomp");
                }
                if (can(myself, .open_close) and isNextToDoor(state.client_state.?)) {
                    g.text(" O: Start open/close door...");
                }
                if (can(myself, .pick_up_unequipped) and isFloorItemSuggestedEquip(state.client_state.?) != null) {
                    g.text(" G: Pick up item");
                }
                if (can(myself, .charge)) {
                    g.text(" C: Charge");
                }
                g.text(" Backspace: Undo");
                if (state.animations != null) {
                    g.text(" Esc: Skip animation");
                }
            },
            .attack => {
                if (can(myself, .attack)) {
                    g.text(" Arrows: Attack");
                }
                g.text(" Esc/Backspace: Cancel");
            },
            .kick => {
                if (can(myself, .kick)) {
                    g.text(" Arrows: Kick");
                }
                g.text(" Esc/Backspace: Cancel");
            },
            .open_close => {
                if (can(myself, .open_close)) {
                    g.text(" Arrows: Open/close");
                }
                g.text(" Esc/Backspace: Cancel");
            },
            .defend => {
                if (can(myself, .defend)) {
                    g.text(" Arrows: Defend");
                }
                g.text(" Esc/Backspace: Cancel");
            },
        }
    }

    // tutorials
    var maybe_tutorial_text: ?[]const u8 = null;
    if (frame.self.kind.individual.activity == .death) {
        maybe_tutorial_text = "You died. Use Backspace to undo.";
    } else if (state.new_game_settings == .puzzle_level and state.consecutive_undos >= 6 and !state.performed_restart) {
        maybe_tutorial_text = "Press R to restart the current level.";
    } else if (state.input_tutorial) |text| {
        maybe_tutorial_text = text;
        // } else if (state.new_game_settings == .puzzle_level and frame.completed_levels == the_levels.len - 1) {
        //     maybe_tutorial_text = "You're are win. Use Ctrl+R to quit.";
    } else if (state.kick_observed and canKick(frame.self.kind.individual.species) and !state.kick_performed) {
        maybe_tutorial_text = "You learned to kick! Use K then a direction.";
    } else if (frame.self.kind.individual.species == .rhino and !state.charge_performed) {
        maybe_tutorial_text = "Press C to charge.";
    } else if (state.moves_performed < 2) {
        maybe_tutorial_text = "Use Arrow Keys to move.";
    } else if (state.attacks_performed < 2) {
        maybe_tutorial_text = "Press F then a direction to attack.";
    } else if (!state.wait_performed and frame.self.kind.individual.status_conditions & core.protocol.StatusCondition_digesting != 0) {
        maybe_tutorial_text = "Use Spacebar to wait.";
    }
    if (maybe_tutorial_text) |tutorial_text| {
        // gentle up/down bob
        var animated_y: i32 = @divFloor(@mod(now, 2000), 100);
        if (animated_y > 10) animated_y = 20 - animated_y;
        const coord = makeCoord(512 / 2 - 384 / 2, 512 - 32 + animated_y);
        _ = textures.renderTextScaled(renderer, tutorial_text, coord, .large_bold, 1);
    }
}

fn can(me: PerceivedThing, action: std.meta.Tag(Action)) bool {
    if (core.game_logic.validateAction(me.kind.individual.species, me.position, me.kind.individual.status_conditions, me.kind.individual.equipment, action)) {
        return true;
    } else |_| {
        return false;
    }
}

fn positionedRect32(position: Coord) Rect {
    return core.geometry.makeRect(position, makeCoord(32, 32));
}

fn getDisplayRect(progress: i32, progress_denominator: i32, thing: PerceivedThing) Rect {
    const head_coord = getHeadPosition(thing.position);
    var activity: core.protocol.PerceivedActivity = .none;
    switch (thing.kind) {
        .individual => {
            activity = thing.kind.individual.activity;
        },
        else => {},
    }
    switch (activity) {
        .movement => |move_delta| {
            return positionedRect32(core.geometry.bezierMove(
                head_coord.scaled(32),
                head_coord.scaled(32).plus(move_delta.scaled(32)),
                progress,
                progress_denominator,
            ));
        },
        .failed_movement => |move_delta| {
            return positionedRect32(core.geometry.bezierBounce(
                head_coord.scaled(32),
                head_coord.scaled(32).plus(move_delta.scaled(32)),
                progress,
                progress_denominator,
            ));
        },
        .growth => |delta| {
            const start_position = thing.position.small;
            const end_position = makeCoord(
                if (delta.x < 0) start_position.x - 1 else start_position.x,
                if (delta.y < 0) start_position.y - 1 else start_position.y,
            );
            const start_size = makeCoord(1, 1);
            const end_size = makeCoord(
                if (delta.x == 0) 1 else 2,
                if (delta.y == 0) 1 else 2,
            );
            return core.geometry.makeRect(
                core.geometry.bezierMove(
                    start_position.scaled(32),
                    end_position.scaled(32),
                    progress,
                    progress_denominator,
                ),
                core.geometry.bezierMove(
                    start_size.scaled(32),
                    end_size.scaled(32),
                    progress,
                    progress_denominator,
                ),
            );
        },
        .failed_growth => |delta| {
            const start_position = thing.position.small;
            const end_position = makeCoord(
                if (delta.x < 0) start_position.x - 1 else start_position.x,
                if (delta.y < 0) start_position.y - 1 else start_position.y,
            );
            const start_size = makeCoord(1, 1);
            const end_size = makeCoord(
                if (delta.x == 0) 1 else 2,
                if (delta.y == 0) 1 else 2,
            );
            return core.geometry.makeRect(
                core.geometry.bezierBounce(
                    start_position.scaled(32),
                    end_position.scaled(32),
                    progress,
                    progress_denominator,
                ),
                core.geometry.bezierBounce(
                    start_size.scaled(32),
                    end_size.scaled(32),
                    progress,
                    progress_denominator,
                ),
            );
        },
        .shrink => |index| {
            const large_position = thing.position.large;
            const position_delta = large_position[0].minus(large_position[1]);
            const start_position = core.geometry.min(large_position[0], large_position[1]);
            const end_position = large_position[index];
            const start_size = position_delta.abs().plus(makeCoord(1, 1));
            const end_size = makeCoord(1, 1);
            return core.geometry.makeRect(
                core.geometry.bezierMove(
                    start_position.scaled(32),
                    end_position.scaled(32),
                    progress,
                    progress_denominator,
                ),
                core.geometry.bezierMove(
                    start_size.scaled(32),
                    end_size.scaled(32),
                    progress,
                    progress_denominator,
                ),
            );
        },
        else => {
            if (thing.kind == .individual and thing.kind.individual.species == .blob and thing.position == .large) {
                const position_delta = thing.position.large[0].minus(thing.position.large[1]);
                return core.geometry.makeRect(
                    core.geometry.min(thing.position.large[0], thing.position.large[1]).scaled(32),
                    position_delta.abs().plus(makeCoord(1, 1)).scaled(32),
                );
            }
            return positionedRect32(head_coord.scaled(32));
        },
    }
}

fn renderThing(renderer: *sdl.Renderer, progress: i32, progress_denominator: i32, screen_display_position: Coord, thing: PerceivedThing) Coord {
    // compute position
    const display_rect = getDisplayRect(progress, progress_denominator, thing);
    const render_rect = display_rect.translated(screen_display_position.scaled(-1));
    const render_position = display_rect.position().minus(screen_display_position);

    // render main sprite
    switch (thing.kind) {
        .individual => {
            switch (thing.kind.individual.species) {
                else => {
                    const is_arrow_nocked = thing.kind.individual.status_conditions & core.protocol.StatusCondition_arrow_nocked != 0;
                    textures.renderSpriteScaled(renderer, speciesToSprite(thing.kind.individual.species, is_arrow_nocked), render_rect);
                },
                .rhino => {
                    const oriented_delta = if (progress < @divTrunc(progress_denominator, 2))
                        thing.position.large[1].minus(thing.position.large[0])
                    else blk: {
                        const future_position = core.game_logic.applyMovementFromActivity(thing.kind.individual.activity, thing.position);
                        break :blk future_position.large[1].minus(future_position.large[0]);
                    };
                    const tail_render_position = render_position.plus(oriented_delta.scaled(32));
                    const rhino_sprite_normalizing_rotation = 0;
                    const rotation = directionToRotation(oriented_delta) +% rhino_sprite_normalizing_rotation;
                    textures.renderSpriteRotated45Degrees(renderer, speciesToSprite(thing.kind.individual.species, false), render_position, rotation);
                    textures.renderSpriteRotated45Degrees(renderer, speciesToTailSprite(thing.kind.individual.species), tail_render_position, rotation);
                },
            }

            // render status effects
            if (thing.kind.individual.status_conditions & (core.protocol.StatusCondition_wounded_leg | core.protocol.StatusCondition_being_digested) != 0) {
                textures.renderSprite(renderer, textures.sprites.wounded, render_position);
            }
            if (thing.kind.individual.status_conditions & (core.protocol.StatusCondition_limping | core.protocol.StatusCondition_grappled) != 0) {
                textures.renderSprite(renderer, textures.sprites.limping, render_position);
            }
            if (thing.kind.individual.status_conditions & core.protocol.StatusCondition_pain != 0) {
                textures.renderSprite(renderer, textures.sprites.pain, render_position);
            }
            if (thing.kind.individual.equipment.is_equipped(.shield)) {
                textures.renderSprite(renderer, textures.sprites.equipped_shield, render_position);
            }
            if (thing.kind.individual.equipment.is_equipped(.axe) and thing.kind.individual.activity != .attack) {
                textures.renderSprite(renderer, textures.sprites.equipped_axe, render_position);
            }
            if (thing.kind.individual.equipment.is_equipped(.torch) and thing.kind.individual.activity != .attack) {
                textures.renderSprite(renderer, textures.sprites.equipped_torch, render_position);
            }
            if (thing.kind.individual.equipment.is_equipped(.dagger) and thing.kind.individual.activity != .attack) {
                textures.renderSprite(renderer, textures.sprites.equipped_dagger, render_position);
            }
        },
        .item => |item| {
            textures.renderSprite(renderer, switch (item) {
                .shield => textures.sprites.shield,
                .axe => textures.sprites.axe,
                .torch => textures.sprites.torch,
                .dagger => textures.sprites.dagger,
            }, render_position);
        },
    }

    return render_position;
}

fn renderActivity(renderer: *sdl.Renderer, progress: i32, progress_denominator: i32, screen_display_position: Coord, thing: PerceivedThing) void {
    if (thing.kind != .individual) return;
    const display_position = getDisplayRect(progress, progress_denominator, thing).position();
    const render_position = display_position.minus(screen_display_position);

    switch (thing.kind.individual.activity) {
        .none => {},
        .movement => {},
        .failed_movement => {},
        .growth => {},
        .failed_growth => {},
        .shrink => {},

        .attack => |data| {
            const max_range = if (core.game_logic.hasBow(thing.kind.individual.species)) core.game_logic.bow_range else @as(i32, 1);
            if (max_range == 1) {
                switch (core.game_logic.getAttackFunction(thing.kind.individual.species, thing.kind.individual.equipment)) {
                    .just_wound, .wound_then_kill, .malaise => {
                        const dagger_sprite_normalizing_rotation = 1;
                        textures.renderSpriteRotated45Degrees(
                            renderer,
                            textures.sprites.dagger,
                            core.geometry.bezierBounce(
                                render_position.plus(data.direction.scaled(32 * 2 / 4)),
                                render_position.plus(data.direction.scaled(32 * 4 / 4)),
                                progress,
                                progress_denominator,
                            ),
                            directionToRotation(data.direction) +% dagger_sprite_normalizing_rotation,
                        );
                    },
                    .smash => {
                        const hammer_sprite_normalizing_rotation = 5;
                        const hammer_handle_coord = makeCoord(28, 3);
                        renderSpriteSwing(
                            renderer,
                            textures.sprites.hammer,
                            render_position,
                            directionToRotation(data.direction),
                            hammer_sprite_normalizing_rotation,
                            hammer_handle_coord,
                            progress,
                            progress_denominator,
                        );
                        if (progress >= @divTrunc(progress_denominator, 2)) {
                            // whooshes
                            const impact_render_position = render_position.plus(data.direction.scaled(32));
                            for ([_]core.geometry.CardinalDirection{ .east, .south, .west, .north }) |dir| {
                                if (@intFromEnum(dir) +% 2 == @intFromEnum(deltaToCardinalDirection(data.direction))) continue;
                                const offset = core.geometry.cardinalDirectionToDelta(dir).scaled(@divTrunc(32 * progress, progress_denominator));
                                textures.renderSpriteRotated45Degrees(
                                    renderer,
                                    textures.sprites.whoosh,
                                    impact_render_position.plus(offset),
                                    @as(u3, @intFromEnum(dir)) * 2,
                                );
                            }
                        }
                    },
                    .chop => {
                        const axe_sprite_normalizing_rotation = 1;
                        const axe_handle_coord = makeCoord(7, 24);
                        renderSpriteSwing(
                            renderer,
                            textures.sprites.axe,
                            render_position,
                            directionToRotation(data.direction),
                            axe_sprite_normalizing_rotation,
                            axe_handle_coord,
                            progress,
                            progress_denominator,
                        );
                    },
                    .burn => {
                        const torch_sprite_normalizing_rotation = 1;
                        textures.renderSpriteRotated45Degrees(
                            renderer,
                            textures.sprites.torch,
                            core.geometry.bezierBounce(
                                render_position.plus(data.direction.scaled(32 * 2 / 4)),
                                render_position.plus(data.direction.scaled(32 * 4 / 4)),
                                progress,
                                progress_denominator,
                            ),
                            directionToRotation(data.direction) +% torch_sprite_normalizing_rotation,
                        );
                    },
                }
            } else {
                // The animated bullet speed is determined by the max range,
                // but interrupt the progress if the arrow hits something.
                var clamped_progress = progress * max_range;
                if (clamped_progress > data.distance * progress_denominator) {
                    clamped_progress = data.distance * progress_denominator;
                }
                const arrow_sprite_normalizing_rotation = 4;
                textures.renderSpriteRotated45Degrees(
                    renderer,
                    textures.sprites.arrow,
                    core.geometry.bezier2(
                        render_position,
                        render_position.plus(data.direction.scaled(32 * max_range)),
                        clamped_progress,
                        progress_denominator * max_range,
                    ),
                    directionToRotation(data.direction) +% arrow_sprite_normalizing_rotation,
                );
            }
        },

        .kick => |coord| {
            const kick_sprite_normalizing_rotation = 6;
            textures.renderSpriteRotated45Degrees(
                renderer,
                textures.sprites.kick,
                core.geometry.bezierBounce(
                    render_position.plus(coord.scaled(32 * 2 / 4)),
                    render_position.plus(coord.scaled(32 * 4 / 4)),
                    progress,
                    progress_denominator,
                ),
                directionToRotation(coord) +% kick_sprite_normalizing_rotation,
            );
        },

        .polymorph => {
            textures.renderSprite(
                renderer,
                selectAnimatedFrame(
                    textures.sprites.polymorph_effect[4..],
                    progress,
                    progress_denominator,
                ),
                render_position,
            );
        },

        .nibble => {
            textures.renderSprite(
                renderer,
                selectAnimatedFrameBoomerang(
                    textures.sprites.nibble_effect[0..],
                    progress,
                    @divTrunc(progress_denominator, 4),
                ),
                render_position,
            );
        },
        .stomp => {
            textures.renderSprite(
                renderer,
                textures.sprites.kick,
                core.geometry.bezierBounce(
                    render_position.plus(makeCoord(0, 4)),
                    render_position.plus(makeCoord(0, 20)),
                    progress,
                    progress_denominator,
                ),
            );
        },
        .defend => |direction| {
            const delta = core.geometry.cardinalDirectionToDelta(direction);
            textures.renderSpriteRotated45Degrees(
                renderer,
                textures.sprites.shield,
                core.geometry.bezierBounce(
                    render_position.plus(delta.scaled(32 * 1 / 8)),
                    render_position.plus(delta.scaled(32 * 5 / 8)),
                    progress,
                    progress_denominator,
                ),
                switch (direction) {
                    .east, .west => 0,
                    .north, .south => 2,
                },
            );
        },

        .death => {
            textures.renderSprite(renderer, textures.sprites.death, render_position);
        },
    }
}

fn selectAnimatedFrame(sprites: []const Rect, progress: i32, progress_denominator: i32) Rect {
    const sprite_index = @as(usize, @intCast(@divTrunc(progress * @as(i32, @intCast(sprites.len)), progress_denominator)));
    return sprites[sprite_index];
}
fn selectAnimatedFrameBoomerang(sprites: []const Rect, progress: i32, progress_denominator: i32) Rect {
    const sprite_index = @as(usize, @intCast(@divTrunc(progress * @as(i32, @intCast(sprites.len)), progress_denominator))) % (sprites.len * 2);
    if (sprite_index >= sprites.len) {
        // backwards
        return sprites[sprites.len * 2 - 1 - sprite_index];
    } else {
        // forwards
        return sprites[sprite_index];
    }
}

fn selectAesthetic(array: []const Rect, seed: u32, coord: Coord) Rect {
    return array[hashCoordToRange(array.len, seed, coord)];
}
fn selectAestheticBiasedLow(array: []const Rect, seed: u32, coord: Coord, bias: usize) Rect {
    // (for bias=2)
    // /--\ heavy_len
    //     /--\ light_len
    // 01234567
    // |   \   \
    // |     \   \
    // 001122334567
    const heavy_len = array.len / 2;
    const light_len = array.len - heavy_len;
    var index = hashCoordToRange(heavy_len * bias + light_len, seed, coord);
    if (index < heavy_len * bias) {
        // 00112233____
        // 0123____
        index /= bias;
    } else {
        // ________4567
        // ____4567
        index -= heavy_len * (bias - 1);
    }
    return array[index];
}

fn hashCoordToRange(less_than_this: usize, seed: u32, coord: Coord) usize {
    var hash = seed;
    hash ^= @as(u32, @bitCast(coord.x));
    hash = hashU32(hash);
    hash ^= @as(u32, @bitCast(coord.y));
    hash = hashU32(hash);
    return std.rand.limitRangeBiased(u32, hash, @as(u32, @intCast(less_than_this)));
}

fn hashU32(input: u32) u32 {
    // https://nullprogram.com/blog/2018/07/31/
    var x = input;
    x ^= x >> 17;
    x *%= 0xed5ad4bb;
    x ^= x >> 11;
    x *%= 0xac4c1b51;
    x ^= x >> 15;
    x *%= 0x31848bab;
    x ^= x >> 14;
    return x;
}

fn speciesToSprite(species: Species, is_arrow_nocked: bool) Rect {
    return switch (species) {
        .human => textures.sprites.human,
        .orc => textures.sprites.orc,
        .centaur => |subspecies| switch (subspecies) {
            .archer => if (is_arrow_nocked) textures.sprites.centaur_archer_nocked else textures.sprites.centaur_archer,
            .warrior => textures.sprites.centaur_warrior,
        },
        .turtle => textures.sprites.turtle,
        .rhino => textures.sprites.rhino[0],
        .kangaroo => textures.sprites.kangaroo,
        .blob => |subspecies| switch (subspecies) {
            .small_blob => textures.sprites.pink_blob,
            .large_blob => textures.sprites.blob_large,
        },
        .wolf => textures.sprites.dog,
        .rat => textures.sprites.rat,
        .wood_golem => textures.sprites.wood_golem,
        .scorpion => textures.sprites.scorpion,
        .brown_snake => textures.sprites.brown_snake,
        .ant => |subspecies| switch (subspecies) {
            .worker => textures.sprites.ant_worker,
            .queen => textures.sprites.ant_queen,
        },
        .minotaur => textures.sprites.minotaur,
        .siren => |subspecies| switch (subspecies) {
            .water => textures.sprites.siren_water,
            .land => textures.sprites.siren_land,
        },
        .ogre => textures.sprites.ogre,
    };
}

fn speciesToTailSprite(species: Species) Rect {
    return switch (species) {
        .rhino => textures.sprites.rhino[1],
        else => unreachable,
    };
}

const slow_animation_speed = 300;
const fast_animation_speed = 100;
const hyper_animation_speed = 10;

pub const Animations = struct {
    start_time: i32,
    turns: u32 = 0,
    last_turn_first_frame: usize = 0,
    time_per_frame: i32,
    frames: ArrayListUnmanaged(PerceivedFrame),

    const SomeData = struct { frame_index: usize, progress: i32, time_per_frame: i32 };
    pub fn frameAtTime(self: @This(), now: i32) ?SomeData {
        const animation_time = @as(u32, @bitCast(now -% self.start_time));
        const index = @divFloor(animation_time, @as(u32, @intCast(self.time_per_frame)));
        if (index >= self.last_turn_first_frame) {
            // Always show the last turn of animations slowly.
            const time_since_turn = animation_time - @as(u32, @intCast(self.time_per_frame)) * @as(u32, @intCast(self.last_turn_first_frame));
            const adjusted_index = self.last_turn_first_frame + @divFloor(time_since_turn, slow_animation_speed);
            if (adjusted_index >= self.frames.items.len - 1) {
                // The last frame is always everyone standing still.
                return null;
            }
            const progress = @as(i32, @intCast(time_since_turn - (adjusted_index - self.last_turn_first_frame) * slow_animation_speed));
            return SomeData{
                .frame_index = adjusted_index,
                .progress = progress,
                .time_per_frame = slow_animation_speed,
            };
        } else if (index >= self.frames.items.len - 1) {
            // The last frame is always everyone standing still.
            return null;
        } else {
            const progress = @as(i32, @intCast(animation_time - index * @as(u32, @intCast(self.time_per_frame))));
            return SomeData{
                .frame_index = index,
                .progress = progress,
                .time_per_frame = self.time_per_frame,
            };
        }
    }

    pub fn speedUp(self: *@This(), now: i32) void {
        const data = self.frameAtTime(now) orelse return;
        const old_time_per_frame = self.time_per_frame;
        if (self.turns >= 4) {
            // We're falling behind. Activate emergency NOS.
            self.time_per_frame = hyper_animation_speed;
        } else if (self.turns >= 2) {
            self.time_per_frame = fast_animation_speed;
        } else return;
        self.start_time = now - ( //
            self.time_per_frame * @as(i32, @intCast(data.frame_index)) + //
            @divFloor(data.progress * self.time_per_frame, old_time_per_frame) //
        );
    }
};

pub fn loadAnimations(animations: *?Animations, frames: []PerceivedFrame, now: i32) !void {
    if (animations.* == null or animations.*.?.frameAtTime(now) == null) {
        animations.* = Animations{
            .start_time = now,
            .time_per_frame = slow_animation_speed,
            .frames = .{},
        };
    }
    animations.*.?.last_turn_first_frame = animations.*.?.frames.items.len -| 1;
    animations.*.?.turns += 1;
    if (animations.*.?.turns > 1) {
        animations.*.?.speedUp(now);
    }
    var have_previous_frame = animations.*.?.frames.items.len > 0;
    for (frames, 0..) |frame, i| {
        if (have_previous_frame and i < frames.len - 1) {
            // try compressing the new frame into the previous one.
            if (try tryCompressingFrames(lastPtr(animations.*.?.frames.items), frame)) {
                continue;
            }
        } else {
            have_previous_frame = true;
        }
        try animations.*.?.frames.append(allocator, frame);
    }
}

fn tryCompressingFrames(base_frame: *PerceivedFrame, patch_frame: PerceivedFrame) !bool {
    core.debug.animation_compression.print("trying to compress frames...", .{});

    if (base_frame.others.len != patch_frame.others.len) {
        core.debug.animation_compression.print("NOPE: A change to the number of people we can is too complex to compress.", .{});
        return false;
    }

    // The order of frame.others is not meaningful, so we need to derrive continuity ourselves,
    // which is intentionally not possible sometimes.
    const others_mapping = try allocator.alloc(usize, base_frame.others.len);
    for (base_frame.others, 0..) |base_other, i| {
        var new_position = base_other.position;
        if (base_other.kind == .individual) {
            new_position = core.game_logic.applyMovementFromActivity(base_other.kind.individual.activity, new_position);
        }

        var mapped_index: ?usize = null;
        for (patch_frame.others, 0..) |patch_other, j| {
            // Are these the same person?
            const could_be = blk: {
                if (!core.game_logic.positionEquals(
                    new_position,
                    patch_other.position,
                )) break :blk false;
                switch (base_other.kind) {
                    .individual => {
                        if (patch_other.kind != .individual) break :blk false;
                        if (base_other.kind.individual.species != @as(std.meta.Tag(Species), patch_other.kind.individual.species)) break :blk false;
                    },
                    .item => |item| {
                        if (patch_other.kind != .item) break :blk false;
                        if (patch_other.kind.item != item) break :blk false;
                    },
                }
                break :blk true;
            };
            if (could_be) {
                if (mapped_index != null) {
                    core.debug.animation_compression.deepPrint("NOPE: individual has too many matches in the next frame: ", base_other);
                    return false;
                }
                // our best guess so far.
                mapped_index = j;
            }
        }

        if (mapped_index) |j| {
            others_mapping[i] = j;
        } else {
            core.debug.animation_compression.deepPrint("NOPE: individual has no matches in the next frame: ", base_other);
            core.debug.animation_compression.deepPrint("  expected position: ", new_position);
            for (patch_frame.others) |patch_other| {
                core.debug.animation_compression.deepPrint("  saw position: ", patch_other.position);
            }
            return false;
        }
    }
    core.debug.animation_compression.print("...all ({}) individuals tracked...", .{base_frame.others.len});

    // If someone does two different things in subsequent frames, then we can't compress those.
    if (base_frame.self.kind.individual.activity != .none and patch_frame.self.kind.individual.activity != .none) {
        core.debug.animation_compression.print("NOPE: self doing two different things.", .{});
        return false;
    }
    for (base_frame.others, 0..) |base_other, i| {
        if (base_other.kind != .individual) continue;
        const patch_other = patch_frame.others[others_mapping[i]];
        if (base_other.kind.individual.activity != .none and patch_other.kind.individual.activity != .none) {
            core.debug.animation_compression.print("NOPE: someone({}, {}) doing two different things.", .{ i, others_mapping[i] });
            return false;
        }
    }

    compressPerceivedThings(&base_frame.self, patch_frame.self);
    core.debug.animation_compression.print("...compressing...", .{});
    for (base_frame.others, 0..) |*base_other, i| {
        const patch_other = patch_frame.others[others_mapping[i]];
        compressPerceivedThings(base_other, patch_other);
    }
    base_frame.completed_levels = patch_frame.completed_levels;
    // uhhh. how do we compress this?
    _ = patch_frame.terrain;

    return true;
}

fn compressPerceivedThings(base_thing: *PerceivedThing, patch_thing: PerceivedThing) void {
    if (patch_thing.kind == .individual and patch_thing.kind.individual.activity == .none) {
        // That was easy.
        return;
    }
    base_thing.* = patch_thing;
}

fn lastPtr(arr: anytype) @TypeOf(&arr[arr.len - 1]) {
    return &arr[arr.len - 1];
}

fn isNextToDoor(frame: PerceivedFrame) bool {
    _ = frame; // TODO: implement this some day.
    return true;
}

fn renderSpriteSwing(
    renderer: *sdl.Renderer,
    sprite: Rect,
    wielder_location: Coord,
    rotation_45_degrees: u3,
    normalizing_rotation_45_degrees: u3,
    local_center: Coord,
    s: i32,
    max_s: i32,
) void {
    const s_mid = @divFloor(max_s, 2);
    const flip = textures.Flip.none;
    var degrees = @as(f64, @floatFromInt(rotation_45_degrees +% normalizing_rotation_45_degrees)) * 45.0;
    if (s < s_mid) {
        degrees -= 45.0 * (1.0 - @as(f64, @floatFromInt(s)) / @as(f64, @floatFromInt(s_mid)));
    }

    const sprite_location = wielder_location.minus(local_center).plus(switch (rotation_45_degrees) {
        0 => makeCoord(28, 16),
        2 => makeCoord(16, 28),
        4 => makeCoord(4, 16),
        6 => makeCoord(16, 4),
        else => unreachable,
    });

    textures.renderSpriteRotatedFlipped(
        renderer,
        sprite,
        sprite_location,
        degrees,
        local_center,
        flip,
    );
}
