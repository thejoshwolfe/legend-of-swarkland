#include "event.hpp"

#include "display.hpp"
#include "swarkland.hpp"


static bool can_see_location(Thing observer, Coord location) {
    if (!observer->still_exists)
        return false;
    if (!is_in_bounds(location))
        return false;
    return observer->life()->knowledge.tile_is_visible[location].any();
}
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
        if (!has_status(actual_target, StatusEffect::INVISIBILITY)) {
            // see normally
            return true;
        }
    }
    // cogniscopy can see minds
    if (has_status(observer, StatusEffect::COGNISCOPY)) {
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
    if (thing->location == Coord::nowhere()) {
        // it's being carried
        Thing container = actual_things.get(thing->container_id);
        // if the container is invisible, so is its contents
        if (!can_see_thing(observer, container->id))
            return false;
        // cogniscopy doesn't show items
        if (!can_see_location(observer, container->location))
            return false;
        return true;
    }

    return can_see_thing(observer, target_id, thing->location);
}
static Coord location_of(uint256 individual_id) {
    return actual_things.get(individual_id)->location;
}

static PerceivedThing find_unseen_individual(Thing observer, Coord location) {
    PerceivedThing thing;
    for (auto iterator = observer->life()->knowledge.perceived_things.value_iterator(); iterator.next(&thing);)
        if (thing->thing_type == ThingType_INDIVIDUAL && thing->life()->species_id == SpeciesId_UNSEEN && thing->location == location)
            return thing;
    return nullptr;
}

static uint256 make_unseen_item(Thing observer, uint256 actual_item_id, uint256 supposed_container_id) {
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
        }
    }
    // invent an item
    uint256 id = random_uint256();
    PerceivedThing thing;
    switch (thing_type) {
        case ThingType_INDIVIDUAL:
            unreachable();
        case ThingType_WAND:
            thing = create<PerceivedThingImpl>(id, WandDescriptionId_UNSEEN, Coord::nowhere(), supposed_container_id, 0, time_counter);
            break;
        case ThingType_POTION:
            thing = create<PerceivedThingImpl>(id, PotionDescriptionId_UNSEEN, Coord::nowhere(), supposed_container_id, 0, time_counter);
            break;
    }
    observer->life()->knowledge.perceived_things.put(id, thing);
    fix_perceived_z_orders(observer, container->id);
    return id;
}
static uint256 make_unseen_individual(Thing observer, uint256 actual_target_id) {
    Thing actual_target = actual_things.get(actual_target_id);
    PerceivedThing thing = find_unseen_individual(observer, actual_target->location);
    if (thing == nullptr) {
        // invent an unseen individual here
        uint256 id = random_uint256();
        thing = create<PerceivedThingImpl>(id, SpeciesId_UNSEEN, actual_target->location, time_counter);
        observer->life()->knowledge.perceived_things.put(id, thing);
    }
    if (can_see_location(observer, actual_target->location)) {
        // hmmm. this guy is probably invisible
        put_status(thing, StatusEffect::INVISIBILITY);
    }
    return thing->id;
}

static bool true_event_to_observed_event(Thing observer, Event event, Event * output_event) {
    switch (event.type) {
        case Event::THE_INDIVIDUAL: {
            const Event::TheIndividualData & data = event.the_individual_data();
            if (data.id == Event::TheIndividualData::TURN_INVISIBLE) {
                if (!can_see_location(observer, location_of(data.individual)))
                    return false;
                *output_event = event;
                return true;
            }
            if (!can_see_thing(observer, data.individual))
                return false;
            *output_event = event;
            return true;
        }
        case Event::INDIVIDUAL_AND_LOCATION: {
            const Event::IndividualAndLocationData & data = event.individual_and_location_data();
            switch (data.id) {
                case Event::IndividualAndLocationData::MOVE:
                    // for moving, you get to see the individual in either location
                    if (!(can_see_thing(observer, data.actor, data.location) || can_see_thing(observer, data.actor)))
                        return false;
                    *output_event = event;
                    return true;
                case Event::IndividualAndLocationData::BUMP_INTO_LOCATION:
                case Event::IndividualAndLocationData::ATTACK_LOCATION:
                    if (!can_see_thing(observer, data.actor))
                        return false;
                    *output_event = event;
                    return true;
            }
        }
        case Event::TWO_INDIVIDUAL: {
            const Event::TwoIndividualData & data = event.two_individual_data();
            switch (data.id) {
                case Event::TwoIndividualData::BUMP_INTO_INDIVIDUAL:
                case Event::TwoIndividualData::ATTACK_INDIVIDUAL:
                case Event::TwoIndividualData::MELEE_KILL: {
                    // maybe replace one of the individuals with an unseen one.
                    *output_event = event;
                    if (can_see_thing(observer, data.actor)) {
                        if (can_see_thing(observer, data.target)) {
                            return true;
                        } else {
                            output_event->two_individual_data().target = make_unseen_individual(observer, data.target);
                            return true;
                        }
                    } else {
                        if (can_see_thing(observer, data.target)) {
                            output_event->two_individual_data().actor = make_unseen_individual(observer, data.actor);
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
                    // the item is not in anyone's hand, so if you can see the location, you can see the event.
                    if (!observer->life()->knowledge.tile_is_visible[data.location].any())
                        return false;
                    *output_event = event;
                    // the individual might be invisible
                    if (!can_see_thing(observer, data.individual))
                        output_event->individual_and_item_data().individual = make_unseen_individual(observer, data.individual);
                    return true;
                case Event::IndividualAndItemData::ZAP_WAND:
                    // the magic beam gives away the location, so if you can see the location, you can see the event.
                    if (!observer->life()->knowledge.tile_is_visible[data.location].any())
                        return false;
                    *output_event = event;
                    // the individual might be invisible
                    if (!can_see_thing(observer, data.individual)) {
                        Event::IndividualAndItemData * output_data = &output_event->individual_and_item_data();
                        output_data->individual = make_unseen_individual(observer, data.individual);
                        output_data->item = make_unseen_item(observer, data.item, output_data->individual);
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
            if (data.target != uint256::zero()) {
                if (!can_see_thing(observer, data.target))
                    return false;
            } else {
                if (!observer->life()->knowledge.tile_is_visible[data.location].any())
                    return false;
            }
            *output_event = event;
            return true;
        }
        case Event::USE_POTION: {
            const Event::UsePotionData & data = event.use_potion_data();
            if (!can_see_location(observer, data.location))
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
            // TODO: gaining and losing a mind should interact with cogniscopy. missing this feature can cause crashes.
            *output_event = event;
            return true;
        }
        case Event::ITEM_AND_LOCATION: {
            const Event::ItemAndLocationData & data = event.item_and_location_data();
            if (!can_see_location(observer, data.location))
                return false;
            if (!can_see_thing(observer, data.item))
                return false;
            *output_event = event;
            return true;
        }
    }
    unreachable();
}

static void record_perception_of_location(Thing observer, Coord location, bool see_items) {
    // don't set tile_is_visible, because it might actually not be.
    observer->life()->knowledge.tiles[location] = actual_map_tiles[location];

    if (see_items) {
        List<Thing> items;
        find_items_on_floor(location, &items);
        for (int i = 0; i < items.length(); i++)
            record_perception_of_thing(observer, items[i]->id);
    }
}

PerceivedThing record_perception_of_thing(Thing observer, uint256 target_id) {
    PerceivedThing target = observer->life()->knowledge.perceived_things.get(target_id, nullptr);
    if (target != nullptr) {
        switch(target->thing_type) {
            case ThingType_INDIVIDUAL:
                if (target->life()->species_id == SpeciesId_UNSEEN) {
                    // still looking at an unseen marker
                    target->last_seen_time = time_counter;
                    return target;
                } else {
                    // clear any unseen markers here
                    PerceivedThing thing = find_unseen_individual(observer, target->location);
                    if (thing != nullptr)
                        observer->life()->knowledge.perceived_things.remove(thing->id);
                }
                break;
            case ThingType_WAND:
                if (target->wand_info()->description_id == WandDescriptionId_UNSEEN) {
                    // still looking at an unseen marker
                    target->last_seen_time = time_counter;
                    return target;
                }
                break;
            case ThingType_POTION:
                if (target->potion_info()->description_id == PotionDescriptionId_UNSEEN) {
                    // still looking at an unseen marker
                    target->last_seen_time = time_counter;
                    return target;
                }
                break;
        }
    }
    target = to_perceived_thing(target_id);
    observer->life()->knowledge.perceived_things.put(target_id, target);
    // cogniscopy doesn't see items.
    if (can_see_location(observer, target->location)) {
        List<Thing> inventory;
        find_items_in_inventory(target_id, &inventory);
        for (int i = 0; i < inventory.length(); i++)
            record_perception_of_thing(observer, inventory[i]->id);
    }
    return target;
}

static void identify_wand(Thing observer, WandDescriptionId description_id, WandId id) {
    observer->life()->knowledge.wand_identities[description_id] = id;
}
static void identify_potion(Thing observer, PotionDescriptionId description_id, PotionId id) {
    observer->life()->knowledge.potion_identities[description_id] = id;
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
                case Event::TheIndividualData::TURN_INVISIBLE:
                    remembered_event->span->format("%s turns invisible!", get_thing_description(observer, data.individual));
                    put_status(individual, StatusEffect::INVISIBILITY);
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
                case Event::IndividualAndLocationData::MOVE:
                    remembered_event = nullptr;
                    record_perception_of_thing(observer, data.actor);
                    if (data.actor == observer->id) {
                        // moving into a space while blind explores the space
                        record_perception_of_location(observer, observer->location, true);
                    }
                    break;
                case Event::IndividualAndLocationData::BUMP_INTO_LOCATION:
                case Event::IndividualAndLocationData::ATTACK_LOCATION: {
                    record_perception_of_location(observer, data.location, false);
                    // there's no individual there to attack
                    List<PerceivedThing> perceived_things;
                    find_perceived_things_at(observer, data.location, &perceived_things);
                    for (int i = 0; i < perceived_things.length(); i++) {
                        if (perceived_things[i]->thing_type == ThingType_INDIVIDUAL)
                            delete_ids.append(perceived_things[i]->id);
                    }
                    Span actor_description = get_thing_description(observer, data.actor);
                    Span bumpee_description;
                    if (!is_open_space(observer->life()->knowledge.tiles[data.location].tile_type))
                        bumpee_description = new_span("a wall");
                    else
                        bumpee_description = new_span("thin air");
                    const char * fmt;
                    switch (data.id) {
                        case Event::IndividualAndLocationData::MOVE:
                            unreachable();
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
            } else if (!is_open_space(observer->life()->knowledge.tiles[data.location].tile_type)) {
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
                    identify_wand(observer, wand_description, true_id);
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
                identify_potion(observer, description_id, effect);
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
