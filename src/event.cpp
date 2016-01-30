#include "event.hpp"

#include "display.hpp"
#include "swarkland.hpp"

bool can_see_thing(Thing observer, uint256 target_id, Coord target_location) {
    // you can always see yourself
    if (observer->id == target_id)
        return true;
    // you can't see anything else while dead
    if (!observer->still_exists)
        return false;
    Thing actual_target = actual_things.get(target_id);
    // you can't see dead things
    if (!actual_target->still_exists)
        return false;
    VisionTypes vision = observer->life()->knowledge.tile_is_visible[target_location];
    if (vision.ethereal) {
        // ethereal vision sees all
        return true;
    }
    if (vision.normal) {
        // we're looking right at it
        Thing container = actual_target;
        if (actual_target->location == Coord::nowhere())
            container = actual_things.get(actual_target->container_id);
        if (!has_status(container, StatusEffect::INVISIBILITY)) {
            // see normally
            return true;
        }
    }
    if (vision.touch) {
        // we're on top of it
        return true;
    }
    // cogniscopy can see minds
    if (vision.cogniscopy) {
        if (actual_target->thing_type == ThingType_INDIVIDUAL && individual_has_mind(actual_target))
            return true;
    }

    return false;
}
bool can_see_thing(Thing observer, uint256 target_id) {
    assert(target_id != uint256::zero());
    Thing thing = actual_things.get(target_id, nullptr);
    if (thing == nullptr || !thing->still_exists) {
        // i'm sure you can't see it, because it doesn't exist anymore.
        return false;
    }
    Coord location = thing->location;
    if (location == Coord::nowhere()) {
        // it's being carried
        location = actual_things.get(thing->container_id)->location;
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
     Thing thing = actual_things.get(target_id, nullptr);
     if (thing == nullptr || !thing->still_exists)
         return false;
     Coord location = thing->location;
     if (location == Coord::nowhere()) {
         // it's being carried
         location = actual_things.get(thing->container_id)->location;
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
        find_items_in_inventory(observer, thing, &inventory);
        for (int i = 0; i < inventory.length(); i++)
            observer->life()->knowledge.perceived_things.remove(inventory[i]->id);
    }
}

static uint256 make_placeholder_item(Thing observer, uint256 actual_item_id, uint256 supposed_container_id) {
    Thing actual_item = actual_things.get(actual_item_id);
    ThingType thing_type = actual_item->thing_type;
    PerceivedThing container = observer->life()->knowledge.perceived_things.get(supposed_container_id);
    List<PerceivedThing> inventory;
    find_items_in_inventory(observer, container, &inventory);
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
            thing = create<PerceivedThingImpl>(id, true, WandDescriptionId_UNSEEN, Coord::nowhere(), supposed_container_id, 0, time_counter);
            break;
        case ThingType_POTION:
            thing = create<PerceivedThingImpl>(id, true, PotionDescriptionId_UNSEEN, Coord::nowhere(), supposed_container_id, 0, time_counter);
            break;

        case ThingType_COUNT:
            unreachable();
    }
    observer->life()->knowledge.perceived_things.put(id, thing);
    fix_perceived_z_orders(observer, container->id);
    return id;
}
static uint256 make_placeholder_individual(Thing observer, uint256 actual_target_id) {
    Thing actual_target = actual_things.get(actual_target_id);
    PerceivedThing thing = find_placeholder_individual(observer, actual_target->location);
    if (thing == nullptr) {
        // invent a placeholder here
        uint256 id = random_id();
        thing = create<PerceivedThingImpl>(id, true, SpeciesId_UNSEEN, actual_target->location, time_counter);
        observer->life()->knowledge.perceived_things.put(id, thing);
    }
    VisionTypes vision = observer->life()->knowledge.tile_is_visible[actual_target->location];
    if (vision.normal) {
        // hmmm. this guy is probably invisible
        assert(!can_see_invisible(vision));
        put_status(thing, StatusEffect::INVISIBILITY);
    }
    return thing->id;
}

static bool true_event_to_observed_event(Thing observer, Event event, Event * output_event) {
    switch (event.type) {
        case Event::THE_INDIVIDUAL: {
            const Event::TheIndividualData & data = event.the_individual_data();
            if (!see_thing(observer, data.individual))
                return false;
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
            switch (data.id) {
                case Event::IndividualAndItemData::INDIVIDUAL_PICKS_UP_ITEM:
                case Event::IndividualAndItemData::INDIVIDUAL_SUCKS_UP_ITEM:
                case Event::IndividualAndItemData::ITEM_HITS_INDIVIDUAL:
                case Event::IndividualAndItemData::THROW_ITEM:
                    // the item is not in anyone's hand, so if you can see the item, you can see the event.
                    if (!can_see_shape(observer->life()->knowledge.tile_is_visible[data.location]))
                        return false;
                    *output_event = event;
                    // the individual might be invisible
                    if (!can_see_thing(observer, data.individual))
                        output_event->individual_and_item_data().individual = make_placeholder_individual(observer, data.individual);
                    return true;
                case Event::IndividualAndItemData::ZAP_WAND:
                    // the magic beam gives away the location, so if you can see the beam, you can see the event.
                    if (!can_see_shape(observer->life()->knowledge.tile_is_visible[data.location]))
                        return false;
                    *output_event = event;
                    // the individual might be invisible
                    if (!can_see_thing(observer, data.individual)) {
                        Event::IndividualAndItemData * output_data = &output_event->individual_and_item_data();
                        output_data->individual = make_placeholder_individual(observer, data.individual);
                        output_data->item = make_placeholder_item(observer, data.item, output_data->individual);
                    }
                    return true;
                case Event::IndividualAndItemData::WAND_DISINTEGRATES:
                case Event::IndividualAndItemData::ZAP_WAND_NO_CHARGES:
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
        case Event::WAND_HIT: {
            const Event::WandHitData & data = event.wand_hit_data();
            if (!can_see_shape(observer->life()->knowledge.tile_is_visible[data.location]))
                return false;
            *output_event = event;
            if (data.target != uint256::zero() && !can_see_thing(observer, data.target))
                output_event->wand_hit_data().target = make_placeholder_individual(observer, data.target);
            return true;
        }
        case Event::USE_POTION: {
            const Event::UsePotionData & data = event.use_potion_data();
            if (!can_see_shape(observer->life()->knowledge.tile_is_visible[data.location]))
                return false;
            if (data.target_id != uint256::zero() && !can_see_thing(observer, data.target_id)) {
                if (data.is_breaking) {
                    // i see that it broke, but it looks like it hit nobody
                    *output_event = event;
                    output_event->use_potion_data().target_id = uint256::zero();
                    output_event->use_potion_data().effect = PotionId_UNKNOWN;
                    return true;
                } else {
                    // can't see the quaffer
                    return false;
                }
            }
            *output_event = event;
            return true;
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

static void record_location_of_thing(Thing observer, uint256 thing_id, Coord new_location) {
    PerceivedThing target = observer->life()->knowledge.perceived_things.get(thing_id);
    target->location = new_location;
    if (target->thing_type == ThingType_INDIVIDUAL)
        clear_placeholder_individual_at(observer, new_location);
}
static PerceivedThing to_perceived_thing(uint256 target_id, VisionTypes vision) {
    can_see_shape(vision); // TODO: hide information for different types of vision
    Thing target = actual_things.get(target_id);

    Coord location = target->location;
    uint256 container_id = target->container_id;
    int z_order = target->z_order;
    if (location != Coord::nowhere()) {
        container_id = uint256::zero();
        z_order = 0;
    }

    PerceivedThing result;
    switch (target->thing_type) {
        case ThingType_INDIVIDUAL: {
            SpeciesId species_id = SpeciesId_UNSEEN;
            if (can_see_shape(vision))
                species_id = target->life()->species_id;
            result = create<PerceivedThingImpl>(target->id, false, species_id, target->location, time_counter);
            break;
        }
        case ThingType_WAND: {
            WandDescriptionId description_id = WandDescriptionId_UNSEEN;
            if (can_see_color(vision))
                description_id = target->wand_info()->description_id;
            result = create<PerceivedThingImpl>(target->id, false, description_id, location, container_id, z_order, time_counter);
            break;
        }
        case ThingType_POTION: {
            PotionDescriptionId description_id = PotionDescriptionId_UNSEEN;
            if (can_see_color(vision))
                description_id = target->potion_info()->description_id;
            result = create<PerceivedThingImpl>(target->id, false, description_id, location, container_id, z_order, time_counter);
            break;
        }

        case ThingType_COUNT:
            unreachable();
    }
    // TODO: limited perception of status effects
    for (int i = 0; i < target->status_effects.length(); i++)
        result->status_effects.append(target->status_effects[i].type);
    return result;
}
PerceivedThing record_perception_of_thing(Thing observer, uint256 target_id) {
    Knowledge & knowledge = observer->life()->knowledge;
    PerceivedThing target = knowledge.perceived_things.get(target_id, nullptr);
    if (target != nullptr && target->is_placeholder) {
        // still looking at an unseen marker at this location
        target->last_seen_time = time_counter;
        return target;
    }

    Thing actual_target = actual_things.get(target_id);
    Coord location = actual_target->location;
    if (location == Coord::nowhere())
        location = actual_things.get(actual_target->container_id)->location;
    VisionTypes vision = knowledge.tile_is_visible[location];
    if (!vision.any())
        return nullptr;

    target = to_perceived_thing(target_id, vision);
    knowledge.perceived_things.put(target_id, target);
    record_location_of_thing(observer, target_id, target->location);
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

static void observe_event(Thing observer, Event event, IdMap<WandDescriptionId> * perceived_current_zapper) {
    // make changes to our knowledge
    RememberedEvent remembered_event = create<RememberedEventImpl>();
    List<uint256> delete_ids;
    switch (event.type) {
        case Event::THE_INDIVIDUAL: {
            Event::TheIndividualData & data = event.the_individual_data();
            PerceivedThing individual = observer->life()->knowledge.perceived_things.get(data.individual, nullptr);
            Span individual_description;
            switch (data.id) {
                case Event::TheIndividualData::POISONED:
                    remembered_event->span->format("%s is poisoned!", get_thing_description(observer, data.individual));
                    put_status(individual, StatusEffect::POISON);
                    break;
                case Event::TheIndividualData::NO_LONGER_CONFUSED:
                    maybe_remove_status(individual, StatusEffect::CONFUSION);
                    remembered_event->span->format("%s is no longer confused.", get_thing_description(observer, data.individual));
                    break;
                case Event::TheIndividualData::NO_LONGER_FAST:
                    maybe_remove_status(individual, StatusEffect::SPEED);
                    remembered_event->span->format("%s slows back down to normal speed.", get_thing_description(observer, data.individual));
                    break;
                case Event::TheIndividualData::NO_LONGER_HAS_ETHEREAL_VISION:
                    maybe_remove_status(individual, StatusEffect::ETHEREAL_VISION);
                    remembered_event->span->format("%s no longer has ethereal vision.", get_thing_description(observer, data.individual));
                    break;
                case Event::TheIndividualData::NO_LONGER_COGNISCOPIC:
                    maybe_remove_status(individual, StatusEffect::COGNISCOPY);
                    remembered_event->span->format("%s is no longer cogniscopic.", get_thing_description(observer, data.individual));
                    break;
                case Event::TheIndividualData::NO_LONGER_BLIND:
                    maybe_remove_status(individual, StatusEffect::BLINDNESS);
                    remembered_event->span->format("%s is no longer blind.", get_thing_description(observer, data.individual));
                    break;
                case Event::TheIndividualData::NO_LONGER_POISONED:
                    maybe_remove_status(individual, StatusEffect::POISON);
                    remembered_event->span->format("%s is no longer poisoned.", get_thing_description(observer, data.individual));
                    break;
                case Event::TheIndividualData::NO_LONGER_INVISIBLE:
                    maybe_remove_status(individual, StatusEffect::INVISIBILITY);
                    remembered_event->span->format("%s is no longer invisible.", get_thing_description(observer, data.individual));
                    break;
                case Event::TheIndividualData::APPEAR:
                    // TODO: this seems wrong to use here
                    individual = record_perception_of_thing(observer, data.individual);
                    maybe_remove_status(individual, StatusEffect::INVISIBILITY);
                    remembered_event->span->format("%s appears out of nowhere!", get_thing_description(observer, data.individual));
                    break;
                case Event::TheIndividualData::LEVEL_UP:
                    // no state change
                    remembered_event->span->format("%s levels up.", get_thing_description(observer, data.individual));
                    break;
                case Event::TheIndividualData::DIE:
                    remembered_event->span->format("%s dies.", get_thing_description(observer, data.individual));
                    delete_ids.append(data.individual);
                    break;
            }
            break;
        }
        case Event::INDIVIDUAL_AND_LOCATION: {
            Event::IndividualAndLocationData & data = event.individual_and_location_data();
            switch (data.id) {
                case Event::IndividualAndLocationData::BUMP_INTO_LOCATION:
                case Event::IndividualAndLocationData::ATTACK_LOCATION: {
                    record_solidity_of_location(observer, data.location, data.is_air);
                    // there's no individual there to attack
                    List<PerceivedThing> perceived_things;
                    find_perceived_things_at(observer, data.location, &perceived_things);
                    for (int i = 0; i < perceived_things.length(); i++) {
                        if (perceived_things[i]->thing_type == ThingType_INDIVIDUAL)
                            delete_ids.append(perceived_things[i]->id);
                    }
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
            }
            break;
        }
        case Event::MOVE: {
            const Event::MoveData & data = event.move_data();
            remembered_event = nullptr;
            record_location_of_thing(observer, data.actor, data.new_location);
            break;
        }
        case Event::TWO_INDIVIDUAL: {
            Event::TwoIndividualData & data = event.two_individual_data();
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
                    if (data.id == Event::TwoIndividualData::MELEE_KILL) {
                        delete_ids.append(data.target);
                    }
                    break;
                }
            }
            break;
        }
        case Event::INDIVIDUAL_AND_ITEM: {
            Event::IndividualAndItemData & data = event.individual_and_item_data();
            record_perception_of_thing(observer, data.individual);
            Span individual_description = get_thing_description(observer, data.individual);
            record_perception_of_thing(observer, data.item);
            Span item_description = get_thing_description(observer, data.item);
            switch (data.id) {
                case Event::IndividualAndItemData::ZAP_WAND: {
                    WandDescriptionId wand_description = observer->life()->knowledge.perceived_things.get(data.item)->wand_info()->description_id;
                    if (wand_description != WandDescriptionId_UNSEEN)
                        perceived_current_zapper->put(observer->id, wand_description);
                    remembered_event->span->format("%s zaps %s.", individual_description, item_description);
                    break;
                }
                case Event::IndividualAndItemData::ZAP_WAND_NO_CHARGES:
                    remembered_event->span->format("%s zaps %s, but %s just sputters.", individual_description, item_description, item_description);
                    break;
                case Event::IndividualAndItemData::WAND_DISINTEGRATES:
                    remembered_event->span->format("%s tries to zap %s, but %s disintegrates.", individual_description, item_description, item_description);
                    delete_ids.append(data.item);
                    break;
                case Event::IndividualAndItemData::THROW_ITEM:
                    remembered_event->span->format("%s throws %s.", individual_description, item_description);
                    break;
                case Event::IndividualAndItemData::ITEM_HITS_INDIVIDUAL:
                    remembered_event->span->format("%s hits %s!", item_description, individual_description);
                    break;
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
        case Event::WAND_HIT: {
            Event::WandHitData & data = event.wand_hit_data();
            Span beam_description = new_span(data.is_explosion ? "an explosion" : "a magic beam");
            PerceivedThing target;
            Span target_description;
            if (data.target != uint256::zero()) {
                target = observer->life()->knowledge.perceived_things.get(data.target);
                target_description = get_thing_description(observer, data.target);
            } else if (!is_open_space(observer->life()->knowledge.tiles[data.location])) {
                target = nullptr;
                target_description = new_span("a wall");
            } else {
                panic("wand hit something that's not an individual or wall");
            }
            switch (data.observable_effect) {
                case WandId_WAND_OF_CONFUSION:
                    remembered_event->span->format("%s hits %s; %s is confused!", beam_description, target_description, target_description);
                    put_status(target, StatusEffect::CONFUSION);
                    break;
                case WandId_WAND_OF_DIGGING:
                    remembered_event->span->format("%s digs away %s!", beam_description, target_description);
                    break;
                case WandId_WAND_OF_STRIKING:
                    remembered_event->span->format("%s strikes %s!", beam_description, target_description);
                    break;
                case WandId_WAND_OF_SPEED:
                    remembered_event->span->format("%s hits %s; %s speeds up!", beam_description, target_description, target_description);
                    put_status(target, StatusEffect::SPEED);
                    break;
                case WandId_WAND_OF_REMEDY:
                    remembered_event->span->format("%s hits %s; the magic beam soothes %s.", beam_description, target_description, target_description);
                    maybe_remove_status(target, StatusEffect::CONFUSION);
                    maybe_remove_status(target, StatusEffect::POISON);
                    maybe_remove_status(target, StatusEffect::BLINDNESS);
                    break;

                case WandId_UNKNOWN:
                    // nothing happens
                    remembered_event->span->format("%s hits %s, but nothing happens.", beam_description, target_description);
                    break;
                case WandId_COUNT:
                    unreachable();
            }

            WandId true_id = data.observable_effect;
            if (true_id != WandId_UNKNOWN) {
                // cool, i can id this wand.
                WandDescriptionId wand_description = perceived_current_zapper->get(observer->id, WandDescriptionId_COUNT);
                if (wand_description != WandDescriptionId_COUNT) {
                    observer->life()->knowledge.wand_identities[wand_description] = true_id;
                } else {
                    // if only i could see it!!
                }
            }

            break;
        }
        case Event::USE_POTION: {
            Event::UsePotionData & data = event.use_potion_data();
            PerceivedThing target = nullptr;
            if (data.is_breaking) {
                remembered_event->span->format("%s breaks", get_thing_description(observer, data.item_id));
                if (data.target_id != uint256::zero()) {
                    remembered_event->span->format(" and splashes on %s", get_thing_description(observer, data.target_id));
                    target = observer->life()->knowledge.perceived_things.get(data.target_id);
                }
            } else {
                remembered_event->span->format("%s drinks %s",
                        get_thing_description(observer, data.target_id),
                        get_thing_description(observer, data.item_id));
                target = observer->life()->knowledge.perceived_things.get(data.target_id);
            }

            PotionId effect = data.effect;
            switch (effect) {
                case PotionId_POTION_OF_HEALING:
                    remembered_event->span->format("; %s is healed!", get_thing_description(observer, data.target_id));
                    break;
                case PotionId_POTION_OF_POISON:
                    remembered_event->span->format("; %s is poisoned!", get_thing_description(observer, data.target_id));
                    put_status(target, StatusEffect::POISON);
                    break;
                case PotionId_POTION_OF_ETHEREAL_VISION:
                    remembered_event->span->format("; %s gains ethereal vision!", get_thing_description(observer, data.target_id));
                    put_status(target, StatusEffect::ETHEREAL_VISION);
                    break;
                case PotionId_POTION_OF_COGNISCOPY:
                    remembered_event->span->format("; %s gains cogniscopy!", get_thing_description(observer, data.target_id));
                    put_status(target, StatusEffect::COGNISCOPY);
                    break;
                case PotionId_POTION_OF_BLINDNESS:
                    remembered_event->span->format("; %s is blinded!", get_thing_description(observer, data.target_id));
                    put_status(target, StatusEffect::BLINDNESS);
                    break;
                case PotionId_POTION_OF_INVISIBILITY:
                    remembered_event->span->format("; %s turns invisible!", get_thing_description(observer, data.target_id));
                    put_status(target, StatusEffect::INVISIBILITY);
                    break;

                case PotionId_UNKNOWN:
                    remembered_event->span->append(", but nothing happens.");
                    break;
                case PotionId_COUNT:
                    unreachable();
            }
            if (effect != PotionId_UNKNOWN) {
                // ah hah!
                PotionDescriptionId description_id = observer->life()->knowledge.perceived_things.get(data.item_id)->potion_info()->description_id;
                if (description_id != PotionDescriptionId_UNSEEN) {
                    observer->life()->knowledge.potion_identities[description_id] = effect;
                } else {
                    // ...awe
                }
            }
            delete_ids.append(data.item_id);
            break;
        }
        case Event::POLYMORPH: {
            Event::PolymorphData & data = event.polymorph_data();
            remembered_event->span->format("%s transforms into a %s!", get_thing_description(observer, data.individual), get_species_name(data.new_species));
            record_perception_of_thing(observer, data.individual);
            break;
        }
        case Event::ITEM_AND_LOCATION: {
            Event::ItemAndLocationData & data = event.item_and_location_data();
            record_perception_of_thing(observer, data.item);
            Span item_description = get_thing_description(observer, data.item);
            switch (data.id) {
                case Event::ItemAndLocationData::WAND_EXPLODES:
                    remembered_event->span->format("%s explodes!", item_description);
                    perceived_current_zapper->put(observer->id, actual_things.get(data.item)->wand_info()->description_id);
                    delete_ids.append(data.item);
                    break;
                case Event::ItemAndLocationData::ITEM_HITS_WALL:
                    // the item may have been thrown from out of view, so make sure we know what it is.
                    remembered_event->span->format("%s hits a wall.", item_description);
                    break;
                case Event::ItemAndLocationData::ITEM_DROPS_TO_THE_FLOOR:
                    remembered_event->span->format("%s drops to the floor.", item_description);
                    break;
            }
            break;
        }
    }

    if (remembered_event != nullptr)
        observer->life()->knowledge.remembered_events.append(remembered_event);

    if (delete_ids.length() > 0) {
        // also delete everything that these people are holding.
        // TODO: this smells like a bad pattern. we shouldn't be deleting things when they go out of view.
        PerceivedThing thing;
        for (auto iterator = observer->life()->knowledge.perceived_things.value_iterator(); iterator.next(&thing);) {
            for (int i = 0; i < delete_ids.length(); i++) {
                if (thing->container_id == delete_ids[i]) {
                    delete_ids.append(thing->id);
                }
            }
        }
        for (int i = 0; i < delete_ids.length(); i++)
            observer->life()->knowledge.perceived_things.remove(delete_ids[i]);
    }
}

void publish_event(Event event) {
    publish_event(event, nullptr);
}
void publish_event(Event actual_event, IdMap<WandDescriptionId> * perceived_current_zapper) {
    Thing observer;
    for (auto iterator = actual_individuals(); iterator.next(&observer);) {
        Event event;
        if (!true_event_to_observed_event(observer, actual_event, &event))
            continue;
        observe_event(observer, event, perceived_current_zapper);
    }
}
