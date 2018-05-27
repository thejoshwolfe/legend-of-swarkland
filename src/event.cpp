#include "event.hpp"

#include "display.hpp"
#include "swarkland.hpp"

static VisionTypes get_vision_for_thing(Thing observer, uint256 target_id, bool ignore_invisibility) {
    Thing target = game->actual_things.get(target_id);
    Thing container = get_top_level_container(target);
    VisionTypes vision = observer->life()->knowledge.tile_is_visible[container->location];
    if (!ignore_invisibility && has_status(container, StatusEffect::INVISIBILITY))
        vision &= ~VisionTypes_NORMAL;
    if (!(target->thing_type == ThingType_INDIVIDUAL && individual_has_mind(target)))
        vision &= ~VisionTypes_COGNISCOPY;
    // nothing can hide from touch or ethereal vision
    return vision;
}
static VisionTypes get_vision_for_thing(Thing observer, uint256 target_id) {
    return get_vision_for_thing(observer, target_id, false);
}
bool can_see_thing(Thing observer, uint256 target_id, Coord target_location) {
    // you can always see yourself
    if (observer->id == target_id)
        return true;
    // you can't see anything else while dead
    if (!observer->still_exists)
        return false;
    Thing actual_target = game->actual_things.get(target_id);
    // you can't see dead things
    if (!actual_target->still_exists)
        return false;
    VisionTypes vision = observer->life()->knowledge.tile_is_visible[target_location];
    if (vision & VisionTypes_ETHEREAL) {
        // ethereal vision sees all
        return true;
    }
    if (vision & VisionTypes_NORMAL) {
        // we're looking right at it
        Thing container = actual_target;
        if (actual_target->container_id != uint256::zero())
            container = game->actual_things.get(actual_target->container_id);
        if (!has_status(container, StatusEffect::INVISIBILITY)) {
            // see normally
            return true;
        }
    }
    if (vision & VisionTypes_COLOCATION) {
        // we're on top of it
        return true;
    }
    // cogniscopy can see minds
    if (vision & VisionTypes_COGNISCOPY) {
        if (actual_target->thing_type == ThingType_INDIVIDUAL && individual_has_mind(actual_target))
            return true;
    }

    return false;
}
bool can_see_thing(Thing observer, uint256 target_id) {
    assert(target_id != uint256::zero());
    Thing thing = game->actual_things.get(target_id, nullptr);
    if (thing == nullptr || !thing->still_exists) {
        // i'm sure you can't see it, because it doesn't exist anymore.
        return false;
    }
    Coord location = thing->location;
    if (thing->container_id != uint256::zero()) {
        // it's being carried
        location = game->actual_things.get(thing->container_id)->location;
    }

    return can_see_thing(observer, target_id, location);
}
static Coord get_individual_location(uint256 individual_id) {
    assert(individual_id != uint256::zero());
    Coord result = game->actual_things.get(individual_id)->location;
    assert(result != Coord::nowhere());
    return result;
}
static bool see_thing(Thing observer, uint256 target_id, Coord location) {
    if (!can_see_thing(observer, target_id, location))
        return false;
    PerceivedThing target = observer->life()->knowledge.perceived_things.get(target_id, nullptr);
    if (target == nullptr) {
        record_perception_of_thing(observer, target_id);
    }
    return true;
}
static bool see_thing(Thing observer, uint256 target_id) {
    assert(target_id != uint256::zero());
    Thing thing = game->actual_things.get(target_id, nullptr);
    if (thing == nullptr || !thing->still_exists)
        return false;
    Coord location = thing->location;
    if (thing->container_id != uint256::zero()) {
        // it's being carried
        location = game->actual_things.get(thing->container_id)->location;
    }
    return see_thing(observer, target_id, location);
}

static PerceivedThing find_placeholder_individual(Thing observer, Coord location) {
    PerceivedThing thing;
    for (auto iterator = observer->life()->knowledge.perceived_things.value_iterator(); iterator.next(&thing);)
        if (thing->thing_type == ThingType_INDIVIDUAL && thing->is_placeholder && thing->location == location)
            return thing;
    return nullptr;
}
static void clear_placeholder_individual_at(Thing observer, Coord location) {
    PerceivedThing thing = find_placeholder_individual(observer, location);
    if (thing != nullptr) {
        observer->life()->knowledge.perceived_things.remove(thing->id);
        List<PerceivedThing> inventory;
        find_items_in_inventory(observer, thing->id, &inventory);
        for (int i = 0; i < inventory.length(); i++)
            observer->life()->knowledge.perceived_things.remove(inventory[i]->id);
    }
}

static uint256 make_placeholder_item(Thing observer, uint256 actual_item_id, uint256 supposed_container_id) {
    Thing actual_item = game->actual_things.get(actual_item_id);
    ThingType thing_type = actual_item->thing_type;
    PerceivedThing container = observer->life()->knowledge.perceived_things.get(supposed_container_id);
    List<PerceivedThing> inventory;
    find_items_in_inventory(observer, container->id, &inventory);
    for (int i = 0; i < inventory.length(); i++) {
        PerceivedThing thing = inventory[i];
        if (thing->thing_type != thing_type)
            continue;
        switch (thing->thing_type) {
            case ThingType_INDIVIDUAL:
                unreachable();
            case ThingType_WAND:
                if (thing->wand_info()->description_id == WandDescriptionId_UNSEEN)
                    return thing->id; // already got one
                break;
            case ThingType_POTION:
                if (thing->potion_info()->description_id == PotionDescriptionId_UNSEEN)
                    return thing->id; // already got one
                break;
            case ThingType_BOOK:
                if (thing->book_info()->description_id == BookDescriptionId_UNSEEN)
                    return thing->id; // already got one
                break;
            case ThingType_WEAPON:
                if (thing->weapon_info()->weapon_id == WeaponId_UNKNOWN)
                    return thing->id; // already got one
                break;

            case ThingType_COUNT:
                unreachable();
        }
    }
    // invent an item
    uint256 id = random_id();
    PerceivedThing thing;
    switch (thing_type) {
        case ThingType_INDIVIDUAL:
            unreachable();
        case ThingType_WAND:
            thing = create<PerceivedThingImpl>(id, true, WandDescriptionId_UNSEEN, game->time_counter);
            break;
        case ThingType_POTION:
            thing = create<PerceivedThingImpl>(id, true, PotionDescriptionId_UNSEEN, game->time_counter);
            break;
        case ThingType_BOOK:
            thing = create<PerceivedThingImpl>(id, true, BookDescriptionId_UNSEEN, game->time_counter);
            break;
        case ThingType_WEAPON:
            thing = create<PerceivedThingImpl>(id, true, WeaponId_UNKNOWN, game->time_counter);
            break;

        case ThingType_COUNT:
            unreachable();
    }
    thing->container_id = supposed_container_id;
    observer->life()->knowledge.perceived_things.put(id, thing);
    fix_perceived_z_orders(observer, container->id);
    return id;
}
static uint256 make_placeholder_individual(Thing observer, uint256 actual_target_id) {
    Thing actual_target = game->actual_things.get(actual_target_id);
    PerceivedThing thing = find_placeholder_individual(observer, actual_target->location);
    if (thing == nullptr) {
        // invent a placeholder here
        uint256 id = random_id();
        thing = create<PerceivedThingImpl>(id, true, SpeciesId_UNSEEN, game->time_counter);
        thing->location = actual_target->location;
        observer->life()->knowledge.perceived_things.put(id, thing);
    }
    VisionTypes vision = observer->life()->knowledge.tile_is_visible[actual_target->location];
    if (vision & VisionTypes_NORMAL) {
        // hmmm. this guy is probably invisible
        assert(!can_see_physical_presence(vision));
        put_status(thing, StatusEffect::INVISIBILITY);
    }
    return thing->id;
}

static bool true_event_to_observed_event(Thing observer, Event event, Event * output_event) {
    switch (event.type) {
        case Event::THE_INDIVIDUAL: {
            const Event::TheIndividualData & data = event.the_individual_data();
            VisionTypes vision = get_vision_for_thing(observer, data.individual);
            switch (data.id) {
                case Event::DELETE_THING:
                    // special case. everyone can see this. it prevents memory leaks (or whatever).
                    *output_event = event;
                    return true;
                case Event::APPEAR:
                case Event::DIE:
                    // any vision can see this
                    if (vision == 0)
                        return false;
                    *output_event = event;
                    return true;
                case Event::LEVEL_UP:
                case Event::SPIT_BLINDING_VENOM:
                case Event::THROW_TAR:
                case Event::INDIVIDUAL_IS_HEALED:
                case Event::SEARED_BY_LAVA:
                case Event::INDIVIDUAL_FLOATS_UNCONTROLLABLY:
                    if (!can_see_shape(vision))
                        return false;
                    *output_event = event;
                    return true;
                case Event::ACTIVATED_MAPPING:
                case Event::FAIL_TO_CAST_SPELL:
                    // it's all in the mind
                    if (!can_see_thoughts(vision))
                        return false;
                    *output_event = event;
                    return true;
                case Event::BLINDING_VENOM_HIT_INDIVIDUAL:
                case Event::TAR_HIT_INDIVIDUAL:
                case Event::MAGIC_BEAM_HIT_INDIVIDUAL:
                case Event::MAGIC_MISSILE_HIT_INDIVIDUAL:
                case Event::MAGIC_BULLET_HIT_INDIVIDUAL: {
                    Coord location = game->actual_things.get(data.individual)->location;
                    if (!can_see_shape(observer->life()->knowledge.tile_is_visible[location]))
                        return false;
                    *output_event = event;
                    // the individual might be invisible
                    if (!can_see_thing(observer, data.individual))
                        output_event->the_individual_data().individual = make_placeholder_individual(observer, data.individual);
                    return true;
                }
                case Event::MAGIC_BEAM_PUSH_INDIVIDUAL:
                case Event::MAGIC_BEAM_RECOILS_AND_PUSHES_INDIVIDUAL:
                case Event::INDIVIDUAL_DODGES_MAGIC_BEAM:
                case Event::LUNGE:
                    // TODO: consider when this individual is unseen
                    if (!see_thing(observer, data.individual))
                        return false;
                    *output_event = event;
                    return true;
            }
            unreachable();
        }
        case Event::THE_LOCATION: {
            const Event::TheLocationData & data = event.the_location_data();
            VisionTypes vision = observer->life()->knowledge.tile_is_visible[data.location];
            switch (data.id) {
                case Event::MAGIC_BEAM_HIT_WALL:
                case Event::BEAM_OF_DIGGING_DIGS_WALL:
                case Event::MAGIC_BEAM_PASS_THROUGH_AIR:
                    if (!can_see_shape(vision))
                        return false;
                    break;
            }
            *output_event = event;
            return true;
        }
        case Event::INDIVIDUAL_AND_STATUS: {
            const Event::IndividualAndStatusData & data = event.individual_and_status_data();
            Thing actual_individual = game->actual_things.get(data.individual);
            if (!can_have_status(actual_individual, data.status))
                return false; // not even you can tell you have this status
            bool had_status = has_status(actual_individual, data.status);
            bool is_gain = data.id == Event::GAIN_STATUS;
            if (had_status == is_gain)
                return false; // no state change. no one can tell. not even yourself.
            bool ignore_invisibility = is_gain == false && data.status == StatusEffect::INVISIBILITY;
            VisionTypes vision = get_vision_for_thing(observer, data.individual, ignore_invisibility);
            if (!can_see_status_effect(data.status, vision))
                return false;
            // make sure things that stop being invisible are recordered
            if (ignore_invisibility)
                record_perception_of_thing(observer, data.individual);
            *output_event = event;
            return true;
        }
        case Event::INDIVIDUAL_AND_LOCATION: {
            const Event::IndividualAndLocationData & data = event.individual_and_location_data();
            switch (data.id) {
                case Event::BUMP_INTO_LOCATION:
                case Event::ATTACK_LOCATION:
                case Event::INDIVIDUAL_BURROWS_THROUGH_WALL:
                    if (!see_thing(observer, data.actor))
                        return false;
                    *output_event = event;
                    return true;
            }
            unreachable();
        }
        case Event::INDIVIDUAL_AND_TWO_LOCATION: {
            const Event::IndividualAndTwoLocationData & data = event.individual_and_two_location_data();
            // for moving, you get to see the individual in either location
            if (!(see_thing(observer, data.actor, data.old_location) || see_thing(observer, data.actor)))
                return false;
            *output_event = event;
            return true;
        }
        case Event::TWO_INDIVIDUAL: {
            const Event::TwoIndividualData & data = event.two_individual_data();
            switch (data.id) {
                case Event::BUMP_INTO_INDIVIDUAL:
                case Event::ATTACK_INDIVIDUAL:
                case Event::MELEE_KILL:
                case Event::PUSH_INDIVIDUAL:
                    // maybe replace one of the individuals with a placeholder.
                    *output_event = event;
                    if (see_thing(observer, data.actor)) {
                        if (see_thing(observer, data.target)) {
                            return true;
                        } else {
                            output_event->two_individual_data().target = make_placeholder_individual(observer, data.target);
                            return true;
                        }
                    } else {
                        if (see_thing(observer, data.target)) {
                            output_event->two_individual_data().actor = make_placeholder_individual(observer, data.actor);
                            return true;
                        } else {
                            return false;
                        }
                    }
                case Event::DODGE_ATTACK:
                    *output_event = event;
                    if (see_thing(observer, data.actor)) {
                        if (see_thing(observer, data.target)) {
                            return true;
                        } else {
                            // if you can't see the target, it looks like attacking thin air.
                            Coord location = get_individual_location(data.target);
                            *output_event = Event::create(Event::ATTACK_LOCATION, data.actor, location, is_open_space(game->actual_map_tiles[location]));
                            return true;
                        }
                    } else {
                        if (see_thing(observer, data.target)) {
                            output_event->two_individual_data().actor = make_placeholder_individual(observer, data.actor);
                            return true;
                        } else {
                            return false;
                        }
                    }
            }
            unreachable();
        }
        case Event::INDIVIDUAL_AND_ITEM: {
            const Event::IndividualAndItemData & data = event.individual_and_item_data();
            if (observer->id == data.individual) {
                // you're always aware of what you're doing
                *output_event = event;
                return true;
            }
            Coord location = game->actual_things.get(data.individual)->location;
            switch (data.id) {
                case Event::INDIVIDUAL_PICKS_UP_ITEM:
                case Event::INDIVIDUAL_SUCKS_UP_ITEM:
                case Event::ITEM_HITS_INDIVIDUAL:
                case Event::ITEM_SINKS_INTO_INDIVIDUAL:
                case Event::POTION_HITS_INDIVIDUAL:
                case Event::THROW_ITEM:
                    // the item is not in anyone's hand, so if you can see the item, you can see the event.
                    if (!can_see_shape(observer->life()->knowledge.tile_is_visible[location]))
                        return false;
                    *output_event = event;
                    // the individual might be invisible
                    if (!can_see_thing(observer, data.individual))
                        output_event->individual_and_item_data().individual = make_placeholder_individual(observer, data.individual);
                    return true;
                case Event::ZAP_WAND:
                    // the magic beam gives away the location, so if you can see the beam, you can see the event.
                    if (!can_see_shape(observer->life()->knowledge.tile_is_visible[location]))
                        return false;
                    *output_event = event;
                    // the individual might be invisible
                    if (!can_see_thing(observer, data.individual)) {
                        Event::IndividualAndItemData * output_data = &output_event->individual_and_item_data();
                        output_data->individual = make_placeholder_individual(observer, data.individual);
                        output_data->item = make_placeholder_item(observer, data.item, output_data->individual);
                    }
                    return true;
                case Event::READ_BOOK:
                    // this event is noticing that the person is staring into the pages of the book.
                    // the audible spell cast happens later.
                case Event::EQUIP_ITEM:
                case Event::UNEQUIP_ITEM:
                    // the item is as visible as the wielder
                    if (!can_see_shape(get_vision_for_thing(observer, data.individual)))
                        return false;
                    *output_event = event;
                    return true;
                case Event::WAND_DISINTEGRATES:
                case Event::ZAP_WAND_NO_CHARGES:
                case Event::QUAFF_POTION:
                    // you need to see the individual AND the item to see this event
                    if (!can_see_thing(observer, data.individual))
                        return false;
                    if (!can_see_thing(observer, data.item))
                        return false;
                    *output_event = event;
                    return true;
                case Event::INDIVIDUAL_DODGES_THROWN_ITEM:
                    // you need to see the individual
                    if (!can_see_shape(get_vision_for_thing(observer, data.individual)))
                        return false;
                    *output_event = event;
                    return true;
            }
            unreachable();
        }
        case Event::INDIVIDUAL_AND_TWO_ITEM: {
            const Event::IndividualAndTwoItemData & data = event.individual_and_two_item_data();
            if (observer->id == data.individual) {
                // you're always aware of what you're doing
                *output_event = event;
                return true;
            }
            switch (data.id) {
                case Event::SWAP_EQUIPPED_ITEM:
                    if (!can_see_shape(get_vision_for_thing(observer, data.individual)))
                        return false;
                    *output_event = event;
                    return true;
            }
            unreachable();
        }
        case Event::INDIVIDUAL_AND_SPECIES: {
            const Event::IndividualAndSpeciesData & data = event.individual_and_species_data();
            if (!can_see_shape(get_vision_for_thing(observer, data.individual)))
                return false;
            *output_event = event;
            return true;
        }
        case Event::ITEM_AND_LOCATION: {
            const Event::ItemAndLocationData & data = event.item_and_location_data();
            if (!can_see_shape(observer->life()->knowledge.tile_is_visible[data.location]))
                return false;
            *output_event = event;
            return true;
        }
    }
    unreachable();
}

static void record_solidity_of_location(Thing observer, Coord location, bool is_air) {
    // don't set tile_is_visible, because it might actually not be.
    MapMatrix<TileType> & tiles = observer->life()->knowledge.tiles;
    if (tiles[location] == TileType_UNKNOWN || is_open_space(tiles[location]) != is_air)
        tiles[location] = is_air ? TileType_UNKNOWN_FLOOR : TileType_UNKNOWN_WALL;
}

static PerceivedThing new_perceived_thing(uint256 id, ThingType thing_type) {
    switch (thing_type) {
        case ThingType_INDIVIDUAL:
            return create<PerceivedThingImpl>(id, false, SpeciesId_UNSEEN, game->time_counter);
        case ThingType_WAND:
            return create<PerceivedThingImpl>(id, false, WandDescriptionId_UNSEEN, game->time_counter);
        case ThingType_POTION:
            return create<PerceivedThingImpl>(id, false, PotionDescriptionId_UNSEEN, game->time_counter);
        case ThingType_BOOK:
            return create<PerceivedThingImpl>(id, false, BookDescriptionId_UNSEEN, game->time_counter);
        case ThingType_WEAPON:
            return create<PerceivedThingImpl>(id, false, WeaponId_UNKNOWN, game->time_counter);

        case ThingType_COUNT:
            unreachable();
    }
    unreachable();
}
static void update_perception_of_thing(PerceivedThing target, VisionTypes vision) {
    Thing actual_target = game->actual_things.get(target->id);

    target->location = actual_target->location;
    target->container_id = actual_target->container_id;
    target->z_order = actual_target->z_order;
    target->is_equipped = actual_target->is_equipped;
    target->last_seen_time = game->time_counter;

    switch (target->thing_type) {
        case ThingType_INDIVIDUAL:
            if (can_see_shape(vision))
                target->life()->species_id = actual_target->physical_species_id();
            break;
        case ThingType_WAND:
            if (can_see_color(vision))
                target->wand_info()->description_id = game->actual_wand_descriptions[actual_target->wand_info()->wand_id];
            break;
        case ThingType_POTION:
            if (can_see_color(vision))
                target->potion_info()->description_id = game->actual_potion_descriptions[actual_target->potion_info()->potion_id];
            break;
        case ThingType_BOOK:
            if (can_see_color(vision))
                target->book_info()->description_id = game->actual_book_descriptions[actual_target->book_info()->book_id];
            break;
        case ThingType_WEAPON:
            if (can_see_shape(vision))
                target->weapon_info()->weapon_id = actual_target->weapon_info()->weapon_id;
            break;

        case ThingType_COUNT:
            unreachable();
    }

    if (actual_target->thing_type == ThingType_INDIVIDUAL) {
        for (int i = 1; i < StatusEffect::COUNT; i++) {
            StatusEffect::Id effect_id = (StatusEffect::Id)i;
            if (!can_have_status(actual_target, effect_id) || can_see_status_effect(effect_id, vision))
                remove_status(target, effect_id);
        }
        for (int i = 0; i < actual_target->status_effects.length(); i++) {
            StatusEffect::Id effect = actual_target->status_effects[i].type;
            if (can_have_status(actual_target, effect) && can_see_status_effect(effect, vision))
                put_status(target, actual_target->status_effects[i].type);
        }
    }
}
static PerceivedThing record_perception_of_thing(Thing observer, uint256 target_id, VisionTypes vision) {
    PerceivedThing target =  observer->life()->knowledge.perceived_things.get(target_id, nullptr);
    if (target == nullptr) {
        target = new_perceived_thing(target_id, game->actual_things.get(target_id)->thing_type);
        observer->life()->knowledge.perceived_things.put(target_id, target);
    }
    update_perception_of_thing(target, vision);

    if (target->thing_type == ThingType_INDIVIDUAL)
        clear_placeholder_individual_at(observer, target->location);

    if (can_see_shape(vision)) {
        // TODO: do this here?
        // cogniscopy doesn't see items.
        List<Thing> inventory;
        find_items_in_inventory(target_id, &inventory);
        for (int i = 0; i < inventory.length(); i++)
            record_perception_of_thing(observer, inventory[i]->id);
    }
    return target;
}
PerceivedThing record_perception_of_thing(Thing observer, uint256 target_id) {
    Knowledge & knowledge = observer->life()->knowledge;
    PerceivedThing target = knowledge.perceived_things.get(target_id, nullptr);
    if (target != nullptr && target->is_placeholder) {
        // still looking at an unseen marker at this location
        target->last_seen_time = game->time_counter;
        return target;
    }

    Thing actual_target = game->actual_things.get(target_id);
    Coord location = actual_target->location;
    if (actual_target->container_id != uint256::zero())
        location = game->actual_things.get(actual_target->container_id)->location;
    VisionTypes vision = knowledge.tile_is_visible[location];
    if (vision == 0)
        return nullptr;

    return record_perception_of_thing(observer, target_id, vision);
}

// passing in COUNT means it's impossible for the active thing to be that thing type
static void identify_active_item(Thing observer, WandId wand_id, PotionId potion_id, BookId book_id) {
    uint256 item_id = game->observer_to_active_identifiable_item.get(observer->id, uint256::zero());
    if (item_id == uint256::zero())
        return; // don't know what item is responsible
    PerceivedThing item = observer->life()->knowledge.perceived_things.get(item_id);
    switch (item->thing_type) {
        case ThingType_INDIVIDUAL:
            unreachable();
        case ThingType_WAND:
            assert(wand_id != WandId_COUNT);
            assert(wand_id != WandId_UNKNOWN);
            observer->life()->knowledge.wand_identities[item->wand_info()->description_id] = wand_id;
            return;
        case ThingType_POTION:
            assert(potion_id != PotionId_COUNT);
            assert(potion_id != PotionId_UNKNOWN);
            observer->life()->knowledge.potion_identities[item->potion_info()->description_id] = potion_id;
            return;
        case ThingType_BOOK:
            assert(book_id != BookId_COUNT);
            assert(book_id != BookId_UNKNOWN);
            observer->life()->knowledge.book_identities[item->book_info()->description_id] = book_id;
            return;
        case ThingType_WEAPON:
            unreachable();

        case ThingType_COUNT:
            unreachable();
    }
}

static ThingSnapshot make_thing_snapshot(Thing observer, uint256 target_id) {
    if (observer->id == target_id)
        return ThingSnapshot::create_you();
    Knowledge & knowledge = observer->life()->knowledge;
    PerceivedThing target = knowledge.perceived_things.get(target_id);
    switch (target->thing_type) {
        case ThingType_INDIVIDUAL:
            return ThingSnapshot::create(target->life()->species_id, target->status_effect_bits);
        case ThingType_WAND: {
            WandDescriptionId description_id = target->wand_info()->description_id;
            WandId identified_id = WandId_UNKNOWN;
            if (description_id != WandDescriptionId_UNSEEN)
                identified_id = knowledge.wand_identities[description_id];
            return ThingSnapshot::create(description_id, identified_id);
        }
        case ThingType_POTION: {
            PotionDescriptionId description_id = target->potion_info()->description_id;
            PotionId identified_id = PotionId_UNKNOWN;
            if (description_id != PotionDescriptionId_UNSEEN)
                identified_id = knowledge.potion_identities[description_id];
            return ThingSnapshot::create(description_id, identified_id);
        }
        case ThingType_BOOK: {
            BookDescriptionId description_id = target->book_info()->description_id;
            BookId identified_id = BookId_UNKNOWN;
            if (description_id != BookDescriptionId_UNSEEN)
                identified_id = knowledge.book_identities[description_id];
            return ThingSnapshot::create(description_id, identified_id);
        }
        case ThingType_WEAPON:
            return ThingSnapshot::create(target->weapon_info()->weapon_id);
        case ThingType_COUNT:
            unreachable();
    }
    unreachable();
}

static Nullable<PerceivedEvent> observe_event(Thing observer, Event event) {
    // make changes to our knowledge
    switch (event.type) {
        case Event::THE_INDIVIDUAL: {
            const Event::TheIndividualData & data = event.the_individual_data();
            PerceivedThing individual = observer->life()->knowledge.perceived_things.get(data.individual, nullptr);
            switch (data.id) {
                case Event::APPEAR:
                    individual = record_perception_of_thing(observer, data.individual);
                    remove_status(individual, StatusEffect::INVISIBILITY);
                    return PerceivedEvent::create(PerceivedEvent::APPEAR, make_thing_snapshot(observer, data.individual));
                case Event::LEVEL_UP:
                    // no state change
                    return PerceivedEvent::create(PerceivedEvent::LEVEL_UP, make_thing_snapshot(observer, data.individual));
                case Event::DIE: {
                    PerceivedEvent result = PerceivedEvent::create(PerceivedEvent::DIE, make_thing_snapshot(observer, data.individual));
                    individual->location = Coord::nowhere();
                    return result;
                }
                case Event::DELETE_THING: {
                    PerceivedThing thing = observer->life()->knowledge.perceived_things.get(data.individual, nullptr);
                    if (thing != nullptr) {
                        observer->life()->knowledge.perceived_things.remove(data.individual);
                        // assume everything drops to the floor
                        List<PerceivedThing> inventory;
                        find_items_in_inventory(observer, thing->id, &inventory);
                        for (int i = 0; i < inventory.length(); i++) {
                            inventory[i]->location = thing->location;
                            inventory[i]->container_id = uint256::zero();
                        }
                    }
                    return nullptr;
                }
                case Event::SPIT_BLINDING_VENOM:
                    return PerceivedEvent::create(PerceivedEvent::SPIT_BLINDING_VENOM, make_thing_snapshot(observer, data.individual));
                case Event::BLINDING_VENOM_HIT_INDIVIDUAL:
                    // status is included in a different event
                    return PerceivedEvent::create(PerceivedEvent::BLINDING_VENOM_HIT_INDIVIDUAL, make_thing_snapshot(observer, data.individual));
                case Event::THROW_TAR:
                    return PerceivedEvent::create(PerceivedEvent::THROW_TAR, make_thing_snapshot(observer, data.individual));
                case Event::TAR_HIT_INDIVIDUAL:
                    // status is included in a different event
                    return PerceivedEvent::create(PerceivedEvent::TAR_HIT_INDIVIDUAL, make_thing_snapshot(observer, data.individual));
                case Event::MAGIC_BEAM_HIT_INDIVIDUAL:
                    // status is included in a different event
                    return PerceivedEvent::create(PerceivedEvent::MAGIC_BEAM_HIT_INDIVIDUAL, make_thing_snapshot(observer, data.individual));
                case Event::INDIVIDUAL_DODGES_MAGIC_BEAM:
                    return PerceivedEvent::create(PerceivedEvent::INDIVIDUAL_DODGES_MAGIC_BEAM, make_thing_snapshot(observer, data.individual));
                case Event::MAGIC_MISSILE_HIT_INDIVIDUAL: {
                    PerceivedEvent result = PerceivedEvent::create(PerceivedEvent::MAGIC_MISSILE_HIT_INDIVIDUAL, make_thing_snapshot(observer, data.individual));
                    identify_active_item(observer, WandId_WAND_OF_MAGIC_MISSILE, PotionId_COUNT, BookId_COUNT);
                    return result;
                }
                case Event::MAGIC_BULLET_HIT_INDIVIDUAL: {
                    PerceivedEvent result = PerceivedEvent::create(PerceivedEvent::MAGIC_BULLET_HIT_INDIVIDUAL, make_thing_snapshot(observer, data.individual));
                    identify_active_item(observer, WandId_WAND_OF_MAGIC_BULLET, PotionId_COUNT, BookId_SPELLBOOK_OF_MAGIC_BULLET);
                    return result;
                }
                case Event::INDIVIDUAL_IS_HEALED: {
                    PerceivedEvent result = PerceivedEvent::create(PerceivedEvent::INDIVIDUAL_IS_HEALED, make_thing_snapshot(observer, data.individual));
                    identify_active_item(observer, WandId_COUNT, PotionId_POTION_OF_HEALING, BookId_COUNT);
                    return result;
                }
                case Event::ACTIVATED_MAPPING: {
                    PerceivedEvent result = PerceivedEvent::create(PerceivedEvent::ACTIVATED_MAPPING, make_thing_snapshot(observer, data.individual));
                    identify_active_item(observer, WandId_COUNT, PotionId_COUNT, BookId_SPELLBOOK_OF_MAPPING);
                    return result;
                }
                case Event::MAGIC_BEAM_PUSH_INDIVIDUAL: {
                    PerceivedEvent result = PerceivedEvent::create(PerceivedEvent::MAGIC_BEAM_PUSH_INDIVIDUAL, make_thing_snapshot(observer, data.individual));
                    identify_active_item(observer, WandId_WAND_OF_FORCE, PotionId_COUNT, BookId_SPELLBOOK_OF_FORCE);
                    return result;
                }
                case Event::MAGIC_BEAM_RECOILS_AND_PUSHES_INDIVIDUAL: {
                    PerceivedEvent result = PerceivedEvent::create(PerceivedEvent::MAGIC_BEAM_RECOILS_AND_PUSHES_INDIVIDUAL, make_thing_snapshot(observer, data.individual));
                    identify_active_item(observer, WandId_WAND_OF_FORCE, PotionId_COUNT, BookId_SPELLBOOK_OF_FORCE);
                    return result;
                }
                case Event::FAIL_TO_CAST_SPELL:
                    return PerceivedEvent::create(PerceivedEvent::FAIL_TO_CAST_SPELL, make_thing_snapshot(observer, data.individual));
                case Event::SEARED_BY_LAVA:
                    // we learn that there is lava there
                    observer->life()->knowledge.tiles[individual->location] = TileType_LAVA_FLOOR;
                    return PerceivedEvent::create(PerceivedEvent::SEARED_BY_LAVA, make_thing_snapshot(observer, data.individual));
                case Event::INDIVIDUAL_FLOATS_UNCONTROLLABLY:
                    // we also know that the individual is levitating, but we should already know that.
                    return PerceivedEvent::create(PerceivedEvent::INDIVIDUAL_FLOATS_UNCONTROLLABLY, make_thing_snapshot(observer, data.individual));
                case Event::LUNGE:
                    return PerceivedEvent::create(PerceivedEvent::LUNGE, make_thing_snapshot(observer, data.individual));
            }
            unreachable();
        }
        case Event::THE_LOCATION: {
            const Event::TheLocationData & data = event.the_location_data();
            switch (data.id) {
                case Event::MAGIC_BEAM_HIT_WALL:
                    return PerceivedEvent::create(PerceivedEvent::MAGIC_BEAM_HIT_WALL, data.location);
                case Event::BEAM_OF_DIGGING_DIGS_WALL: {
                    PerceivedEvent result = PerceivedEvent::create(PerceivedEvent::BEAM_OF_DIGGING_DIGS_WALL, data.location);
                    identify_active_item(observer, WandId_WAND_OF_DIGGING, PotionId_COUNT, BookId_COUNT);
                    return result;
                }
                case Event::MAGIC_BEAM_PASS_THROUGH_AIR:
                    // either no one is here, or they dodged. in either case, believe no one is here.
                    clear_placeholder_individual_at(observer, data.location);
                    return nullptr;
            }
            unreachable();
        }
        case Event::INDIVIDUAL_AND_STATUS: {
            const Event::IndividualAndStatusData & data = event.individual_and_status_data();
            PerceivedThing individual = observer->life()->knowledge.perceived_things.get(data.individual);

            // snapshot the thing when it doesn't have the status
            bool is_gain = data.id == Event::GAIN_STATUS;
            if (!is_gain)
                remove_status(individual, data.status);
            ThingSnapshot individual_snapshot = make_thing_snapshot(observer, data.individual);
            if (is_gain)
                put_status(individual, data.status);

            WandId gain_wand_id = WandId_COUNT;
            WandId lose_wand_id = WandId_COUNT;
            PotionId gain_potion_id = PotionId_COUNT;
            PotionId lose_potion_id = PotionId_COUNT;
            BookId gain_book_id = BookId_COUNT;
            BookId lose_book_id = BookId_COUNT;
            switch (data.status) {
                case StatusEffect::CONFUSION:
                    gain_wand_id = WandId_WAND_OF_CONFUSION;
                    lose_wand_id = WandId_WAND_OF_REMEDY;
                    break;
                case StatusEffect::SPEED:
                    gain_wand_id = WandId_WAND_OF_SPEED;
                    gain_book_id = BookId_SPELLBOOK_OF_SPEED;
                    lose_wand_id = WandId_WAND_OF_SLOWING;
                    break;
                case StatusEffect::ETHEREAL_VISION:
                    gain_potion_id = PotionId_POTION_OF_ETHEREAL_VISION;
                    break;
                case StatusEffect::COGNISCOPY:
                    gain_potion_id = PotionId_POTION_OF_COGNISCOPY;
                    break;
                case StatusEffect::BLINDNESS:
                    gain_wand_id = WandId_WAND_OF_BLINDING;
                    gain_potion_id = PotionId_POTION_OF_BLINDNESS;
                    lose_wand_id = WandId_WAND_OF_REMEDY;
                    break;
                case StatusEffect::POISON:
                    gain_potion_id = PotionId_POTION_OF_POISON;
                    lose_wand_id = WandId_WAND_OF_REMEDY;
                    break;
                case StatusEffect::INVISIBILITY:
                    gain_wand_id = WandId_WAND_OF_INVISIBILITY;
                    gain_potion_id = PotionId_POTION_OF_INVISIBILITY;
                    break;
                case StatusEffect::POLYMORPH:
                    // there's a different event for polymorph since you can't tell if someone is gaining it or losing it.
                    unreachable();
                case StatusEffect::SLOWING:
                    gain_wand_id = WandId_WAND_OF_SLOWING;
                    lose_wand_id = WandId_WAND_OF_SPEED;
                    lose_book_id = BookId_SPELLBOOK_OF_SPEED;
                    break;
                case StatusEffect::BURROWING:
                    gain_potion_id = PotionId_POTION_OF_BURROWING;
                    break;
                case StatusEffect::LEVITATING:
                    gain_potion_id = PotionId_POTION_OF_LEVITATION;
                    break;

                case StatusEffect::PUSHED:
                case StatusEffect::COUNT:
                    unreachable();
            }
            if (is_gain) {
                identify_active_item(observer, gain_wand_id, gain_potion_id, gain_book_id);
                return PerceivedEvent::create(PerceivedEvent::GAIN_STATUS, individual_snapshot, data.status);
            } else {
                identify_active_item(observer, lose_wand_id, lose_potion_id, lose_book_id);
                return PerceivedEvent::create(PerceivedEvent::LOSE_STATUS, individual_snapshot, data.status);
            }
        }
        case Event::INDIVIDUAL_AND_LOCATION: {
            const Event::IndividualAndLocationData & data = event.individual_and_location_data();
            record_solidity_of_location(observer, data.location, data.is_air);
            ThingSnapshot actor_snapshot = make_thing_snapshot(observer, data.actor);
            switch (data.id) {
                case Event::BUMP_INTO_LOCATION:
                    return PerceivedEvent::create(PerceivedEvent::BUMP_INTO_LOCATION, actor_snapshot, data.location, data.is_air);
                case Event::ATTACK_LOCATION: {
                    // there's no individual there to attack
                    List<PerceivedThing> perceived_things;
                    find_perceived_things_at(observer, data.location, &perceived_things);
                    for (int i = 0; i < perceived_things.length(); i++)
                        if (perceived_things[i]->thing_type == ThingType_INDIVIDUAL)
                            perceived_things[i]->location = Coord::nowhere();
                    return PerceivedEvent::create(PerceivedEvent::ATTACK_LOCATION, actor_snapshot, data.location, data.is_air);
                }
                case Event::INDIVIDUAL_BURROWS_THROUGH_WALL:
                    return PerceivedEvent::create(PerceivedEvent::INDIVIDUAL_BURROWS_THROUGH_WALL, actor_snapshot, data.location, data.is_air);
            }
            unreachable();
        }
        case Event::INDIVIDUAL_AND_TWO_LOCATION: {
            const Event::IndividualAndTwoLocationData & data = event.individual_and_two_location_data();
            const MapMatrix<VisionTypes> & tile_is_visible = observer->life()->knowledge.tile_is_visible;
            // you should see them whether they're entering or exiting your view
            VisionTypes vision = tile_is_visible[data.old_location] | tile_is_visible[data.new_location];
            record_perception_of_thing(observer, data.actor, vision);
            return nullptr;
        }
        case Event::TWO_INDIVIDUAL: {
            const Event::TwoIndividualData & data = event.two_individual_data();
            switch (data.id) {
                case Event::BUMP_INTO_INDIVIDUAL:
                case Event::ATTACK_INDIVIDUAL:
                case Event::DODGE_ATTACK:
                case Event::MELEE_KILL:
                case Event::PUSH_INDIVIDUAL: {
                    record_perception_of_thing(observer, data.actor);
                    record_perception_of_thing(observer, data.target);
                    PerceivedEvent result = PerceivedEvent::create((PerceivedEvent::TwoIndividualDataId)data.id, make_thing_snapshot(observer, data.actor), make_thing_snapshot(observer, data.target));
                    if (data.id == Event::MELEE_KILL)
                        observer->life()->knowledge.perceived_things.get(data.target)->location = Coord::nowhere();
                    return result;
                }
            }
            unreachable();
        }
        case Event::INDIVIDUAL_AND_ITEM: {
            const Event::IndividualAndItemData & data = event.individual_and_item_data();
            record_perception_of_thing(observer, data.individual);
            ThingSnapshot individual_snapshot = make_thing_snapshot(observer, data.individual);
            PerceivedThing item = record_perception_of_thing(observer, data.item);
            ThingSnapshot item_snapshot = make_thing_snapshot(observer, data.item);
            switch (data.id) {
                case Event::ZAP_WAND: {
                    PerceivedThing wand = observer->life()->knowledge.perceived_things.get(data.item);
                    WandDescriptionId wand_description = wand->wand_info()->description_id;
                    if (wand_description != WandDescriptionId_UNSEEN)
                        game->observer_to_active_identifiable_item.put(observer->id, wand->id);
                    wand->wand_info()->used_count += 1;
                    return PerceivedEvent::create(PerceivedEvent::ZAP_WAND, individual_snapshot, item_snapshot);
                }
                case Event::ZAP_WAND_NO_CHARGES:
                    observer->life()->knowledge.perceived_things.get(data.item)->wand_info()->used_count = -1;
                    return PerceivedEvent::create(PerceivedEvent::ZAP_WAND_NO_CHARGES, individual_snapshot, item_snapshot);
                case Event::WAND_DISINTEGRATES:
                    item->location = Coord::nowhere();
                    item->container_id = uint256::zero();
                    return PerceivedEvent::create(PerceivedEvent::WAND_DISINTEGRATES, individual_snapshot, item_snapshot);
                case Event::READ_BOOK: {
                    PerceivedThing book = observer->life()->knowledge.perceived_things.get(data.item);
                    if (book->book_info()->description_id != BookDescriptionId_UNSEEN)
                        game->observer_to_active_identifiable_item.put(observer->id, book->id);
                    return PerceivedEvent::create(PerceivedEvent::READ_BOOK, individual_snapshot, item_snapshot);
                }
                case Event::QUAFF_POTION: {
                    PerceivedThing potion = observer->life()->knowledge.perceived_things.get(data.item);
                    if (potion->potion_info()->description_id != PotionDescriptionId_UNSEEN)
                        game->observer_to_active_identifiable_item.put(observer->id, potion->id);
                    return PerceivedEvent::create(PerceivedEvent::QUAFF_POTION, individual_snapshot, item_snapshot);
                }
                case Event::THROW_ITEM:
                case Event::ITEM_HITS_INDIVIDUAL:
                case Event::ITEM_SINKS_INTO_INDIVIDUAL:
                case Event::INDIVIDUAL_DODGES_THROWN_ITEM:
                    return PerceivedEvent::create((PerceivedEvent::IndividualAndItemDataId)data.id, individual_snapshot, item_snapshot);
                case Event::POTION_HITS_INDIVIDUAL: {
                    PerceivedThing potion = observer->life()->knowledge.perceived_things.get(data.item);
                    if (potion->potion_info()->description_id != PotionDescriptionId_UNSEEN)
                        game->observer_to_active_identifiable_item.put(observer->id, potion->id);
                    return PerceivedEvent::create(PerceivedEvent::POTION_HITS_INDIVIDUAL, individual_snapshot, item_snapshot);
                }
                case Event::INDIVIDUAL_PICKS_UP_ITEM:
                case Event::INDIVIDUAL_SUCKS_UP_ITEM: {
                    PerceivedThing item = observer->life()->knowledge.perceived_things.get(data.item);
                    item->location = Coord::nowhere();
                    item->container_id = data.individual;
                    fix_perceived_z_orders(observer, data.individual);
                    return PerceivedEvent::create((PerceivedEvent::IndividualAndItemDataId)data.id, individual_snapshot, item_snapshot);
                }
                case Event::EQUIP_ITEM: {
                    PerceivedThing item = observer->life()->knowledge.perceived_things.get(data.item);
                    item->is_equipped = true;
                    return PerceivedEvent::create((PerceivedEvent::IndividualAndItemDataId)data.id, individual_snapshot, item_snapshot);
                }
                case Event::UNEQUIP_ITEM: {
                    PerceivedThing item = observer->life()->knowledge.perceived_things.get(data.item);
                    item->is_equipped = false;
                    return PerceivedEvent::create((PerceivedEvent::IndividualAndItemDataId)data.id, individual_snapshot, item_snapshot);
                }
            }
            unreachable();
        }
        case Event::INDIVIDUAL_AND_TWO_ITEM: {
            const Event::IndividualAndTwoItemData & data = event.individual_and_two_item_data();
            record_perception_of_thing(observer, data.individual);
            ThingSnapshot individual_snapshot = make_thing_snapshot(observer, data.individual);
            PerceivedThing old_item = record_perception_of_thing(observer, data.old_item);
            ThingSnapshot old_item_snapshot = make_thing_snapshot(observer, data.old_item);
            PerceivedThing new_item = record_perception_of_thing(observer, data.new_item);
            ThingSnapshot new_item_snapshot = make_thing_snapshot(observer, data.new_item);
            switch (data.id) {
                case Event::SWAP_EQUIPPED_ITEM: {
                    // when we recorded our perception, this was already updated.
                    old_item->is_equipped = false;
                    new_item->is_equipped = true;
                    return PerceivedEvent::create(PerceivedEvent::SWAP_EQUIPPED_ITEM, individual_snapshot, old_item_snapshot, new_item_snapshot);
                }
            }
            unreachable();
        }
        case Event::INDIVIDUAL_AND_SPECIES: {
            const Event::IndividualAndSpeciesData & data = event.individual_and_species_data();
            PerceivedEvent result = PerceivedEvent::create(PerceivedEvent::POLYMORPH, make_thing_snapshot(observer, data.individual), data.new_species);
            record_perception_of_thing(observer, data.individual);
            identify_active_item(observer, WandId_COUNT, PotionId_COUNT, BookId_SPELLBOOK_OF_ASSUME_FORM);
            return result;
        }
        case Event::ITEM_AND_LOCATION: {
            const Event::ItemAndLocationData & data = event.item_and_location_data();
            PerceivedThing item = record_perception_of_thing(observer, data.item);
            ThingSnapshot item_snapshot = make_thing_snapshot(observer, data.item);
            switch (data.id) {
                case Event::WAND_EXPLODES:
                    if (item->wand_info()->description_id != WandDescriptionId_UNSEEN)
                        game->observer_to_active_identifiable_item.put(observer->id, data.item);
                    item->location = Coord::nowhere();
                    item->container_id = uint256::zero();
                    return PerceivedEvent::create(PerceivedEvent::WAND_EXPLODES, item_snapshot, data.location);
                case Event::ITEM_HITS_WALL:
                case Event::ITEM_DROPS_TO_THE_FLOOR:
                case Event::POTION_BREAKS:
                case Event::ITEM_DISINTEGRATES_IN_LAVA:
                    return PerceivedEvent::create((PerceivedEvent::ItemAndLocationDataId)data.id, item_snapshot, data.location);
            }
            unreachable();
        }
    }
    unreachable();
}

void publish_event(Event actual_event) {
    Thing observer;
    for (auto iterator = actual_individuals(); iterator.next(&observer);) {
        Event event;
        if (!true_event_to_observed_event(observer, actual_event, &event))
            continue;
        Nullable<PerceivedEvent> maybe_perceived_event = observe_event(observer, event);
        if (maybe_perceived_event != nullptr)
            observer->life()->knowledge.perceived_events.append(maybe_perceived_event);
    }
}
