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
    if (vision & VisionTypes_TOUCH) {
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
        assert(!can_see_invisible(vision));
        find_or_put_status(thing, StatusEffect::INVISIBILITY);
    }
    return thing->id;
}

static bool true_event_to_observed_event(Thing observer, Event event, Event * output_event) {
    switch (event.type) {
        case Event::THE_INDIVIDUAL: {
            const Event::TheIndividualData & data = event.the_individual_data();
            VisionTypes vision = get_vision_for_thing(observer, data.individual);
            switch (data.id) {
                case Event::TheIndividualData::DELETE_THING:
                    // special case. everyone can see this. it prevents memory leaks (or whatever).
                    *output_event = event;
                    return true;
                case Event::TheIndividualData::APPEAR:
                case Event::TheIndividualData::DIE:
                    // any vision can see this
                    if (vision == 0)
                        return false;
                    *output_event = event;
                    return true;
                case Event::TheIndividualData::LEVEL_UP:
                case Event::TheIndividualData::SPIT_BLINDING_VENOM:
                case Event::TheIndividualData::INDIVIDUAL_IS_HEALED:
                    if (!can_see_shape(vision))
                        return false;
                    *output_event = event;
                    return true;
                case Event::TheIndividualData::ACTIVATED_MAPPING:
                case Event::TheIndividualData::FAIL_TO_CAST_SPELL:
                    // it's all in the mind
                    if (!can_see_thoughts(get_vision_for_thing(observer, data.individual)))
                        return false;
                    *output_event = event;
                    return true;
                case Event::TheIndividualData::BLINDING_VENOM_HIT_INDIVIDUAL:
                case Event::TheIndividualData::MAGIC_BEAM_HIT_INDIVIDUAL:
                case Event::TheIndividualData::MAGIC_MISSILE_HIT_INDIVIDUAL:
                case Event::TheIndividualData::MAGIC_BULLET_HIT_INDIVIDUAL: {
                    Coord location = game->actual_things.get(data.individual)->location;
                    if (!can_see_shape(observer->life()->knowledge.tile_is_visible[location]))
                        return false;
                    *output_event = event;
                    // the individual might be invisible
                    if (!can_see_thing(observer, data.individual))
                        output_event->the_individual_data().individual = make_placeholder_individual(observer, data.individual);
                    return true;
                }
                case Event::TheIndividualData::MAGIC_BEAM_PUSH_INDIVIDUAL:
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
                case Event::TheLocationData::MAGIC_BEAM_HIT_WALL:
                case Event::TheLocationData::BEAM_OF_DIGGING_DIGS_WALL:
                case Event::TheLocationData::MAGIC_BEAM_PASS_THROUGH_AIR:
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
            if (had_status == data.is_gain)
                return false; // no state change. no one can tell. not even yourself.
            bool ignore_invisibility = data.is_gain == false && data.status == StatusEffect::INVISIBILITY;
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
                case Event::IndividualAndLocationData::BUMP_INTO_LOCATION:
                case Event::IndividualAndLocationData::ATTACK_LOCATION:
                    if (!see_thing(observer, data.actor))
                        return false;
                    *output_event = event;
                    return true;
            }
        }
        case Event::MOVE: {
            const Event::MoveData & data = event.move_data();
            // for moving, you get to see the individual in either location
            if (!(see_thing(observer, data.actor, data.old_location) || see_thing(observer, data.actor)))
                return false;
            *output_event = event;
            return true;
        }
        case Event::TWO_INDIVIDUAL: {
            const Event::TwoIndividualData & data = event.two_individual_data();
            switch (data.id) {
                case Event::TwoIndividualData::BUMP_INTO_INDIVIDUAL:
                case Event::TwoIndividualData::ATTACK_INDIVIDUAL:
                case Event::TwoIndividualData::MELEE_KILL: {
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
                case Event::IndividualAndItemData::INDIVIDUAL_PICKS_UP_ITEM:
                case Event::IndividualAndItemData::INDIVIDUAL_SUCKS_UP_ITEM:
                case Event::IndividualAndItemData::ITEM_HITS_INDIVIDUAL:
                case Event::IndividualAndItemData::POTION_HITS_INDIVIDUAL:
                case Event::IndividualAndItemData::THROW_ITEM:
                    // the item is not in anyone's hand, so if you can see the item, you can see the event.
                    if (!can_see_shape(observer->life()->knowledge.tile_is_visible[location]))
                        return false;
                    *output_event = event;
                    // the individual might be invisible
                    if (!can_see_thing(observer, data.individual))
                        output_event->individual_and_item_data().individual = make_placeholder_individual(observer, data.individual);
                    return true;
                case Event::IndividualAndItemData::ZAP_WAND:
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
                case Event::IndividualAndItemData::READ_BOOK:
                    // this event is noticing that the person is staring into the pages of the book.
                    // the audible spell cast happens later.
                    if (!can_see_shape(get_vision_for_thing(observer, data.individual)))
                        return false;
                    *output_event = event;
                    return true;
                case Event::IndividualAndItemData::WAND_DISINTEGRATES:
                case Event::IndividualAndItemData::ZAP_WAND_NO_CHARGES:
                case Event::IndividualAndItemData::QUAFF_POTION:
                    // you need to see the individual AND the item to see this event
                    if (!can_see_thing(observer, data.individual))
                        return false;
                    if (!can_see_thing(observer, data.item))
                        return false;
                    *output_event = event;
                    return true;
            }
            unreachable();
        }
        case Event::POLYMORPH: {
            if (!can_see_thing(observer, event.polymorph_data().individual))
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

        case ThingType_COUNT:
            unreachable();
    }

    for (int i = target->status_effects.length() - 1; i >= 0; i--) {
        StatusEffect effect = target->status_effects[i];
        if (!can_have_status(actual_target, effect.type) || can_see_status_effect(effect.type, vision))
            target->status_effects.swap_remove(i);
    }
    for (int i = 0; i < actual_target->status_effects.length(); i++) {
        StatusEffect::Id effect = actual_target->status_effects[i].type;
        if (can_have_status(actual_target, effect) && can_see_status_effect(effect, vision))
            target->status_effects.append(StatusEffect { effect, -1, -1, uint256::zero(), SpeciesId_COUNT });
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

        case ThingType_COUNT:
            unreachable();
    }
}

static void observe_event(Thing observer, Event event) {
    // make changes to our knowledge
    RememberedEvent remembered_event = create<RememberedEventImpl>();
    switch (event.type) {
        case Event::THE_INDIVIDUAL: {
            const Event::TheIndividualData & data = event.the_individual_data();
            PerceivedThing individual = observer->life()->knowledge.perceived_things.get(data.individual, nullptr);
            Span individual_description;
            switch (data.id) {
                case Event::TheIndividualData::APPEAR:
                    individual = record_perception_of_thing(observer, data.individual);
                    maybe_remove_status(&individual->status_effects, StatusEffect::INVISIBILITY);
                    remembered_event->span->format("%s appears out of nowhere!", get_thing_description(observer, data.individual));
                    break;
                case Event::TheIndividualData::LEVEL_UP:
                    // no state change
                    remembered_event->span->format("%s levels up.", get_thing_description(observer, data.individual));
                    break;
                case Event::TheIndividualData::DIE:
                    remembered_event->span->format("%s dies.", get_thing_description(observer, data.individual));
                    individual->location = Coord::nowhere();
                    break;
                case Event::TheIndividualData::DELETE_THING: {
                    remembered_event = nullptr;
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
                    break;
                }
                case Event::TheIndividualData::SPIT_BLINDING_VENOM:
                    remembered_event->span->format("%s spits blinding venom!", get_thing_description(observer, data.individual));
                    break;
                case Event::TheIndividualData::BLINDING_VENOM_HIT_INDIVIDUAL:
                    remembered_event->span->format("%s is hit by blinding venom!", get_thing_description(observer, data.individual));
                    // status is included in a different event
                    break;
                case Event::TheIndividualData::MAGIC_BEAM_HIT_INDIVIDUAL:
                    remembered_event->span->format("a magic beam hits %s.", get_thing_description(observer, data.individual));
                    // status is included in a different event
                    break;
                case Event::TheIndividualData::MAGIC_MISSILE_HIT_INDIVIDUAL:
                    remembered_event->span->format("a magic missile hits %s!", get_thing_description(observer, data.individual));
                    identify_active_item(observer, WandId_WAND_OF_MAGIC_MISSILE, PotionId_COUNT, BookId_COUNT);
                    break;
                case Event::TheIndividualData::MAGIC_BULLET_HIT_INDIVIDUAL:
                    remembered_event->span->format("a magic bullet hits %s!", get_thing_description(observer, data.individual));
                    identify_active_item(observer, WandId_WAND_OF_MAGIC_BULLET, PotionId_COUNT, BookId_SPELLBOOK_OF_MAGIC_BULLET);
                    break;
                case Event::TheIndividualData::INDIVIDUAL_IS_HEALED:
                    remembered_event->span->format("%s is healed!", get_thing_description(observer, data.individual));
                    identify_active_item(observer, WandId_COUNT, PotionId_POTION_OF_HEALING, BookId_COUNT);
                    break;
                case Event::TheIndividualData::ACTIVATED_MAPPING:
                    remembered_event->span->format("%s gets a vision of a map of the area!", get_thing_description(observer, data.individual));
                    identify_active_item(observer, WandId_COUNT, PotionId_COUNT, BookId_SPELLBOOK_OF_MAPPING);
                    break;
                case Event::TheIndividualData::MAGIC_BEAM_PUSH_INDIVIDUAL:
                    remembered_event->span->format("a magic beam pushes %s!", get_thing_description(observer, data.individual));
                    identify_active_item(observer, WandId_WAND_OF_FORCE, PotionId_COUNT, BookId_SPELLBOOK_OF_FORCE);
                    break;
                case Event::TheIndividualData::FAIL_TO_CAST_SPELL:
                    remembered_event->span->format("%s must not understand how to cast that spell.", get_thing_description(observer, data.individual));
                    break;
            }
            break;
        }
        case Event::THE_LOCATION: {
            const Event::TheLocationData & data = event.the_location_data();
            switch (data.id) {
                case Event::TheLocationData::MAGIC_BEAM_HIT_WALL:
                    remembered_event->span->format("a magic beam hits a wall.");
                    break;
                case Event::TheLocationData::BEAM_OF_DIGGING_DIGS_WALL:
                    remembered_event->span->format("a magic beam digs away a wall!");
                    identify_active_item(observer, WandId_WAND_OF_DIGGING, PotionId_COUNT, BookId_COUNT);
                    break;
                case Event::TheLocationData::MAGIC_BEAM_PASS_THROUGH_AIR:
                    remembered_event = nullptr;
                    // we know there's no one here
                    clear_placeholder_individual_at(observer, data.location);
                    break;
            }
            break;
        }
        case Event::INDIVIDUAL_AND_STATUS: {
            const Event::IndividualAndStatusData & data = event.individual_and_status_data();
            PerceivedThing individual = observer->life()->knowledge.perceived_things.get(data.individual);
            Span individual_description;
            const char * is_no_longer;
            const char * punctuation;
            if (data.is_gain) {
                is_no_longer = "is";
                punctuation = "!";
                individual_description = get_thing_description(observer, data.individual);
                find_or_put_status(individual, data.status);
            } else {
                is_no_longer = "is no longer";
                punctuation = ".";
                maybe_remove_status(&individual->status_effects, data.status);
                individual_description = get_thing_description(observer, data.individual);
            }
            const char * status_description = nullptr;
            WandId gain_wand_id = WandId_COUNT;
            WandId lose_wand_id = WandId_COUNT;
            PotionId gain_potion_id = PotionId_COUNT;
            PotionId lose_potion_id = PotionId_COUNT;
            BookId gain_book_id = BookId_COUNT;
            BookId lose_book_id = BookId_COUNT;
            switch (data.status) {
                case StatusEffect::CONFUSION:
                    status_description = "confused";
                    gain_wand_id = WandId_WAND_OF_CONFUSION;
                    lose_wand_id = WandId_WAND_OF_REMEDY;
                    break;
                case StatusEffect::SPEED:
                    if (data.is_gain) {
                        remembered_event->span->format("%s speeds up!", individual_description);
                    } else {
                        remembered_event->span->format("%s slows back down to normal speed.", individual_description);
                    }
                    gain_wand_id = WandId_WAND_OF_SPEED;
                    gain_book_id = BookId_SPELLBOOK_OF_SPEED;
                    lose_wand_id = WandId_WAND_OF_SLOWING;
                    break;
                case StatusEffect::ETHEREAL_VISION:
                    if (data.is_gain) {
                        remembered_event->span->format("%s gains ethereal vision!", individual_description);
                    } else {
                        remembered_event->span->format("%s no longer has ethereal vision.", individual_description);
                    }
                    gain_potion_id = PotionId_POTION_OF_ETHEREAL_VISION;
                    break;
                case StatusEffect::COGNISCOPY:
                    status_description = "cogniscopic";
                    gain_potion_id = PotionId_POTION_OF_COGNISCOPY;
                    break;
                case StatusEffect::BLINDNESS:
                    status_description = "blind";
                    gain_wand_id = WandId_WAND_OF_BLINDING;
                    gain_potion_id = PotionId_POTION_OF_BLINDNESS;
                    lose_wand_id = WandId_WAND_OF_REMEDY;
                    break;
                case StatusEffect::POISON:
                    status_description = "poisoned";
                    gain_potion_id = PotionId_POTION_OF_POISON;
                    lose_wand_id = WandId_WAND_OF_REMEDY;
                    break;
                case StatusEffect::INVISIBILITY:
                    status_description = "invisible";
                    gain_wand_id = WandId_WAND_OF_INVISIBILITY;
                    gain_potion_id = PotionId_POTION_OF_INVISIBILITY;
                    break;
                case StatusEffect::POLYMORPH:
                    // there's a different event for polymorph since you can't tell if someone is gaining it or losing it.
                    unreachable();
                case StatusEffect::SLOWING:
                    status_description = "slow";
                    gain_wand_id = WandId_WAND_OF_SLOWING;
                    lose_wand_id = WandId_WAND_OF_SPEED;
                    lose_book_id = BookId_SPELLBOOK_OF_SPEED;
                    break;

                case StatusEffect::COUNT:
                    unreachable();
            }
            if (status_description != nullptr) {
                // TODO: we should be able to pass const char * to span->format()
                String string = new_string();
                string->format("%s %s%s", is_no_longer, status_description, punctuation);
                remembered_event->span->format("%s %s", individual_description, new_span(string));
            }
            if (data.is_gain) {
                identify_active_item(observer, gain_wand_id, gain_potion_id, gain_book_id);
            } else {
                identify_active_item(observer, lose_wand_id, lose_potion_id, lose_book_id);
            }
            break;
        }
        case Event::INDIVIDUAL_AND_LOCATION: {
            const Event::IndividualAndLocationData & data = event.individual_and_location_data();
            record_solidity_of_location(observer, data.location, data.is_air);
            // there's no individual there to attack
            List<PerceivedThing> perceived_things;
            find_perceived_things_at(observer, data.location, &perceived_things);
            for (int i = 0; i < perceived_things.length(); i++)
                if (perceived_things[i]->thing_type == ThingType_INDIVIDUAL)
                    perceived_things[i]->location = Coord::nowhere();
            Span actor_description = get_thing_description(observer, data.actor);
            Span bumpee_description;
            if (!is_open_space(observer->life()->knowledge.tiles[data.location]))
                bumpee_description = new_span("a wall");
            else
                bumpee_description = new_span("thin air");
            const char * fmt;
            switch (data.id) {
                case Event::IndividualAndLocationData::BUMP_INTO_LOCATION:
                    fmt = "%s bumps into %s.";
                    break;
                case Event::IndividualAndLocationData::ATTACK_LOCATION:
                    fmt = "%s hits %s.";
                    break;
            }
            remembered_event->span->format(fmt, actor_description, bumpee_description);
            break;
        }
        case Event::MOVE: {
            const Event::MoveData & data = event.move_data();
            remembered_event = nullptr;
            const MapMatrix<VisionTypes> & tile_is_visible = observer->life()->knowledge.tile_is_visible;
            VisionTypes vision = tile_is_visible[data.old_location] | tile_is_visible[data.new_location];
            record_perception_of_thing(observer, data.actor, vision);
            break;
        }
        case Event::TWO_INDIVIDUAL: {
            const Event::TwoIndividualData & data = event.two_individual_data();
            switch (data.id) {
                case Event::TwoIndividualData::BUMP_INTO_INDIVIDUAL:
                case Event::TwoIndividualData::ATTACK_INDIVIDUAL:
                case Event::TwoIndividualData::MELEE_KILL: {
                    record_perception_of_thing(observer, data.actor);
                    record_perception_of_thing(observer, data.target);
                    Span actor_description = get_thing_description(observer, data.actor);
                    // what did it bump into? whatever we think is there
                    Span bumpee_description = get_thing_description(observer, data.target);
                    const char * fmt;
                    switch (data.id) {
                        case Event::TwoIndividualData::BUMP_INTO_INDIVIDUAL:
                            fmt = "%s bumps into %s.";
                            break;
                        case Event::TwoIndividualData::ATTACK_INDIVIDUAL:
                            fmt = "%s hits %s.";
                            break;
                        case Event::TwoIndividualData::MELEE_KILL:
                            fmt = "%s kills %s.";
                            break;
                    }
                    remembered_event->span->format(fmt, actor_description, bumpee_description);
                    if (data.id == Event::TwoIndividualData::MELEE_KILL)
                        observer->life()->knowledge.perceived_things.get(data.target)->location = Coord::nowhere();
                    break;
                }
            }
            break;
        }
        case Event::INDIVIDUAL_AND_ITEM: {
            const Event::IndividualAndItemData & data = event.individual_and_item_data();
            record_perception_of_thing(observer, data.individual);
            Span individual_description = get_thing_description(observer, data.individual);
            PerceivedThing item = record_perception_of_thing(observer, data.item);
            Span item_description = get_thing_description(observer, data.item);
            switch (data.id) {
                case Event::IndividualAndItemData::ZAP_WAND: {
                    PerceivedThing wand = observer->life()->knowledge.perceived_things.get(data.item);
                    WandDescriptionId wand_description = wand->wand_info()->description_id;
                    if (wand_description != WandDescriptionId_UNSEEN)
                        game->observer_to_active_identifiable_item.put(observer->id, wand->id);
                    remembered_event->span->format("%s zaps %s.", individual_description, item_description);
                    wand->wand_info()->used_count += 1;
                    break;
                }
                case Event::IndividualAndItemData::ZAP_WAND_NO_CHARGES:
                    remembered_event->span->format("%s zaps %s, but %s just sputters.", individual_description, item_description, item_description);
                    observer->life()->knowledge.perceived_things.get(data.item)->wand_info()->used_count = -1;
                    break;
                case Event::IndividualAndItemData::WAND_DISINTEGRATES:
                    remembered_event->span->format("%s tries to zap %s, but %s disintegrates.", individual_description, item_description, item_description);
                    item->location = Coord::nowhere();
                    item->container_id = uint256::zero();
                    break;
                case Event::IndividualAndItemData::READ_BOOK: {
                    PerceivedThing book = observer->life()->knowledge.perceived_things.get(data.item);
                    if (book->book_info()->description_id != BookDescriptionId_UNSEEN)
                        game->observer_to_active_identifiable_item.put(observer->id, book->id);
                    remembered_event->span->format("%s reads %s.", individual_description, item_description);
                    break;
                }
                case Event::IndividualAndItemData::QUAFF_POTION: {
                    PerceivedThing potion = observer->life()->knowledge.perceived_things.get(data.item);
                    if (potion->potion_info()->description_id != PotionDescriptionId_UNSEEN)
                        game->observer_to_active_identifiable_item.put(observer->id, potion->id);
                    remembered_event->span->format("%s quaffs %s.", individual_description, item_description);
                    break;
                }
                case Event::IndividualAndItemData::THROW_ITEM:
                    remembered_event->span->format("%s throws %s.", individual_description, item_description);
                    break;
                case Event::IndividualAndItemData::ITEM_HITS_INDIVIDUAL:
                    remembered_event->span->format("%s hits %s!", item_description, individual_description);
                    break;
                case Event::IndividualAndItemData::POTION_HITS_INDIVIDUAL: {
                    remembered_event->span->format("%s shatters and splashes on %s!", item_description, individual_description);
                    PerceivedThing potion = observer->life()->knowledge.perceived_things.get(data.item);
                    if (potion->potion_info()->description_id != PotionDescriptionId_UNSEEN)
                        game->observer_to_active_identifiable_item.put(observer->id, potion->id);
                    break;
                }
                case Event::IndividualAndItemData::INDIVIDUAL_PICKS_UP_ITEM:
                case Event::IndividualAndItemData::INDIVIDUAL_SUCKS_UP_ITEM: {
                    const char * fmt = data.id == Event::IndividualAndItemData::INDIVIDUAL_PICKS_UP_ITEM ? "%s picks up %s." : "%s sucks up %s.";
                    remembered_event->span->format(fmt, individual_description, item_description);
                    PerceivedThing item = observer->life()->knowledge.perceived_things.get(data.item);
                    item->location = Coord::nowhere();
                    item->container_id = data.individual;
                    fix_perceived_z_orders(observer, data.individual);
                    break;
                }
            }
            break;
        }
        case Event::POLYMORPH: {
            const Event::PolymorphData & data = event.polymorph_data();
            remembered_event->span->format("%s transforms into a %s!", get_thing_description(observer, data.individual), get_species_name(data.new_species));
            record_perception_of_thing(observer, data.individual);
            identify_active_item(observer, WandId_COUNT, PotionId_COUNT, BookId_SPELLBOOK_OF_ASSUME_FORM);
            break;
        }
        case Event::ITEM_AND_LOCATION: {
            const Event::ItemAndLocationData & data = event.item_and_location_data();
            PerceivedThing item = record_perception_of_thing(observer, data.item);
            Span item_description = get_thing_description(observer, data.item);
            switch (data.id) {
                case Event::ItemAndLocationData::WAND_EXPLODES:
                    remembered_event->span->format("%s explodes!", item_description);
                    if (item->wand_info()->description_id != WandDescriptionId_UNSEEN)
                        game->observer_to_active_identifiable_item.put(observer->id, data.item);
                    item->location = Coord::nowhere();
                    item->container_id = uint256::zero();
                    break;
                case Event::ItemAndLocationData::ITEM_HITS_WALL:
                    // the item may have been thrown from out of view, so make sure we know what it is.
                    remembered_event->span->format("%s hits a wall.", item_description);
                    break;
                case Event::ItemAndLocationData::ITEM_DROPS_TO_THE_FLOOR:
                    remembered_event->span->format("%s drops to the floor.", item_description);
                    break;
                case Event::ItemAndLocationData::POTION_BREAKS:
                    remembered_event->span->format("%s shatters!", item_description);
                    break;
            }
            break;
        }
    }

    if (remembered_event != nullptr)
        observer->life()->knowledge.remembered_events.append(remembered_event);
}

void publish_event(Event actual_event) {
    Thing observer;
    for (auto iterator = actual_individuals(); iterator.next(&observer);) {
        Event event;
        if (!true_event_to_observed_event(observer, actual_event, &event))
            continue;
        observe_event(observer, event);
    }
}
