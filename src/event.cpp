#include "event.hpp"

#include "display.hpp"
#include "swarkland.hpp"

static RememberedEvent to_remembered_event(Thing observer, Event event) {
    RememberedEvent result = create<RememberedEventImpl>();
    switch (event.type) {
        case Event::THE_INDIVIDUAL: {
            Event::TheIndividualData & data = event.the_individual_data();
            Span individual_description = get_thing_description(observer, data.individual);
            switch (data.id) {
                case Event::TheIndividualData::POISONED:
                    result->span->format("%s is poisoned!", individual_description);
                    return result;
                case Event::TheIndividualData::NO_LONGER_CONFUSED:
                    result->span->format("%s is no longer confused.", individual_description);
                    return result;
                case Event::TheIndividualData::NO_LONGER_FAST:
                    result->span->format("%s slows back down to normal speed.", individual_description);
                    return result;
                case Event::TheIndividualData::NO_LONGER_HAS_ETHEREAL_VISION:
                    result->span->format("%s no longer has ethereal vision.", individual_description);
                    return result;
                case Event::TheIndividualData::NO_LONGER_COGNISCOPIC:
                    result->span->format("%s is no longer cogniscopic.", individual_description);
                    return result;
                case Event::TheIndividualData::NO_LONGER_BLIND:
                    result->span->format("%s is no longer blind.", individual_description);
                    return result;
                case Event::TheIndividualData::NO_LONGER_POISONED:
                    result->span->format("%s is no longer poisoned.", individual_description);
                    return result;
                case Event::TheIndividualData::APPEAR:
                    result->span->format("%s appears out of nowhere!", individual_description);
                    return result;
                case Event::TheIndividualData::TURN_INVISIBLE:
                    result->span->format("%s turns invisible!", individual_description);
                    return result;
                case Event::TheIndividualData::DISAPPEAR:
                    result->span->format("%s vanishes out of sight!", individual_description);
                    return result;
                case Event::TheIndividualData::LEVEL_UP:
                    result->span->format("%s levels up.", individual_description);
                    return result;
                case Event::TheIndividualData::DIE:
                    result->span->format("%s dies.", individual_description);
                    return result;
            }
            panic("switch");
        }
        case Event::TWO_INDIVIDUAL: {
            Event::TwoIndividualData & data = event.two_individual_data();
            switch (data.id) {
                case Event::TwoIndividualData::MOVE:
                    // unremarkable
                    return nullptr;
                case Event::TwoIndividualData::BUMP_INTO:
                case Event::TwoIndividualData::ATTACK:
                case Event::TwoIndividualData::KILL: {
                    Span actor_description = data.actor != uint256::zero() ? get_thing_description(observer, data.actor) : new_span("something unseen");
                    // what did it bump into? whatever we think is there
                    Span bumpee_description;
                    if (data.target != uint256::zero()) {
                        bumpee_description = get_thing_description(observer, data.target);
                    } else {
                        // can't see anybody there. what are we bumping into?
                        if (data.actor != observer->id && !observer->life()->knowledge.tile_is_visible[data.target_location].any())
                            bumpee_description = new_span("something");
                        else if (!is_open_space(observer->life()->knowledge.tiles[data.target_location].tile_type))
                            bumpee_description = new_span("a wall");
                        else
                            bumpee_description = new_span("thin air");
                    }
                    const char * fmt;
                    switch (data.id) {
                        case Event::TwoIndividualData::MOVE:
                            panic("unreachable");
                        case Event::TwoIndividualData::BUMP_INTO:
                            fmt = "%s bumps into %s.";
                            break;
                        case Event::TwoIndividualData::ATTACK:
                            fmt = "%s hits %s.";
                            break;
                        case Event::TwoIndividualData::KILL:
                            fmt = "%s kills %s.";
                            break;
                    }
                    result->span->format(fmt, actor_description, bumpee_description);
                    return result;
                }
            }
            panic("switch");
        }
        case Event::INDIVIDUAL_AND_ITEM: {
            Event::IndividualAndItemData & data = event.individual_and_item_data();
            Span individual_description = get_thing_description(observer, data.individual);
            Span item_description = get_thing_description(observer, data.item);
            switch (data.id) {
                case Event::IndividualAndItemData::ZAP_WAND:
                    result->span->format("%s zaps %s.", individual_description, item_description);
                    return result;
                case Event::IndividualAndItemData::ZAP_WAND_NO_CHARGES:
                    result->span->format("%s zaps %s, but %s just sputters.", individual_description, item_description, item_description);
                    return result;
                case Event::IndividualAndItemData::WAND_DISINTEGRATES:
                    result->span->format("%s tries to zap %s, but %s disintegrates.", individual_description, item_description, item_description);
                    return result;
                case Event::IndividualAndItemData::THROW_ITEM:
                    result->span->format("%s throws %s.", individual_description, item_description);
                    return result;
                case Event::IndividualAndItemData::ITEM_HITS_INDIVIDUAL:
                    result->span->format("%s hits %s!", item_description, individual_description);
                    return result;
                case Event::IndividualAndItemData::INDIVIDUAL_PICKS_UP_ITEM:
                    result->span->format("%s picks up %s.", individual_description, item_description);
                    return result;
                case Event::IndividualAndItemData::INDIVIDUAL_SUCKS_UP_ITEM:
                    result->span->format("%s sucks up %s.", individual_description, item_description);
                    return result;
            }
            panic("switch");
        }
        case Event::WAND_HIT: {
            Event::WandHitData & data = event.wand_hit_data();
            Span beam_description = new_span(data.is_explosion ? "an explosion" : "a magic beam");
            Span target_description;
            if (data.target != uint256::zero()) {
                target_description = get_thing_description(observer, data.target);
            } else if (!is_open_space(observer->life()->knowledge.tiles[data.location].tile_type)) {
                target_description = new_span("a wall");
            } else {
                panic("wand hit something that's not an individual or wall");
            }
            switch (data.observable_effect) {
                case WandId_WAND_OF_CONFUSION:
                    result->span->format("%s hits %s; %s is confused!", beam_description, target_description, target_description);
                    return result;
                case WandId_WAND_OF_DIGGING:
                    result->span->format("%s digs away %s!", beam_description, target_description);
                    return result;
                case WandId_WAND_OF_STRIKING:
                    result->span->format("%s strikes %s!", beam_description, target_description);
                    return result;
                case WandId_WAND_OF_SPEED:
                    result->span->format("%s hits %s; %s speeds up!", beam_description, target_description, target_description);
                    return result;
                case WandId_WAND_OF_REMEDY:
                    result->span->format("%s hits %s; the magic beam soothes %s.", beam_description, target_description, target_description);
                    return result;

                case WandId_UNKNOWN:
                    // nothing happens
                    result->span->format("%s hits %s, but nothing happens.", beam_description, target_description);
                    return result;
                case WandId_COUNT:
                    panic("not a real wand id");
            }
            panic("wand id");
        }
        case Event::USE_POTION: {
            Event::UsePotionData & data = event.use_potion_data();
            if (data.is_breaking) {
                result->span->format("%s breaks", get_thing_description(observer, data.item_id));
                if (data.target_id != uint256::zero())
                    result->span->format(" and splashes on %s", get_thing_description(observer, data.target_id));
            } else {
                result->span->format("%s drinks %s",
                        get_thing_description(observer, data.target_id),
                        get_thing_description(observer, data.item_id));
            }
            switch (data.effect) {
                case PotionId_POTION_OF_HEALING:
                    result->span->format("; %s is healed!", get_thing_description(observer, data.target_id));
                    break;
                case PotionId_POTION_OF_POISON:
                    result->span->format("; %s is poisoned!", get_thing_description(observer, data.target_id));
                    break;
                case PotionId_POTION_OF_ETHEREAL_VISION:
                    result->span->format("; %s gains ethereal vision!", get_thing_description(observer, data.target_id));
                    break;
                case PotionId_POTION_OF_COGNISCOPY:
                    result->span->format("; %s gains cogniscopy!", get_thing_description(observer, data.target_id));
                    break;
                case PotionId_POTION_OF_BLINDNESS:
                    result->span->format("; %s is blinded!", get_thing_description(observer, data.target_id));
                    break;

                case PotionId_UNKNOWN:
                    result->span->append(", but nothing happens.");
                    break;
                case PotionId_COUNT:
                    panic("not a real id");
            }
            return result;
        }
        case Event::POLYMORPH: {
            Event::PolymorphData & data = event.polymorph_data();
            result->span->format("%s transforms into a %s!", get_thing_description(observer, data.individual), get_species_name(data.new_species));
            return result;
        }
        case Event::ITEM_AND_LOCATION: {
            Event::ItemAndLocationData & data = event.item_and_location_data();
            Span item_description = get_thing_description(observer, data.item);
            switch (data.id) {
                case Event::ItemAndLocationData::WAND_EXPLODES:
                    result->span->format("%s explodes!", item_description);
                    return result;
                case Event::ItemAndLocationData::ITEM_HITS_WALL:
                    result->span->format("%s hits a wall.", item_description);
                    return result;
                case Event::ItemAndLocationData::ITEM_HITS_SOMETHING:
                    result->span->format("%s hits something.", item_description);
                    return result;
                case Event::ItemAndLocationData::ITEM_DROPS_TO_THE_FLOOR:
                    result->span->format("%s drops to the floor.", item_description);
                    return result;
                case Event::ItemAndLocationData::SOMETHING_PICKS_UP_ITEM:
                    result->span->format("something unseen picks up %s.", item_description);
                    return result;
                case Event::ItemAndLocationData::SOMETHING_SUCKS_UP_ITEM:
                    result->span->format("something unseen sucks up %s.", item_description);
                    return result;
            }
        }
    }
    panic("remembered_event");
}

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
    // nobody can see invisible people, because that would be cheating :)
    if (actual_target->status_effects.invisible)
        return false;
    // cogniscopy can see minds
    if (observer->status_effects.cogniscopy_expiration_time > time_counter) {
        if (actual_target->thing_type == ThingType_INDIVIDUAL && actual_target->life()->species()->has_mind)
            return true;
    }
    // we can see someone if they're in our line of sight
    if (!observer->life()->knowledge.tile_is_visible[target_location].any())
        return false;
    return true;
}
bool can_see_thing(Thing observer, uint256 target_id) {
    Thing thing = actual_things.get(target_id, nullptr);
    if (thing == nullptr) {
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
static uint256 check_visible(Thing observer, uint256 thing_id) {
    if (thing_id != uint256::zero() && can_see_thing(observer, thing_id))
        return thing_id;
    return uint256::zero();
}

static bool see_event(Thing observer, Event event, Event * output_event) {
    switch (event.type) {
        case Event::THE_INDIVIDUAL: {
            Event::TheIndividualData & data = event.the_individual_data();
            if (data.id == Event::TheIndividualData::TURN_INVISIBLE) {
                if (!can_see_location(observer, location_of(data.individual)))
                    return false;
                if (!can_see_thing(observer, data.individual)) {
                    // you don't know what they disappeared.
                    *output_event = Event::disappear(data.individual);
                    return true;
                }
                *output_event = event;
                return true;
            }
            if (!can_see_thing(observer, data.individual))
                return false;
            *output_event = event;
            return true;
        }
        case Event::TWO_INDIVIDUAL: {
            Event::TwoIndividualData & data = event.two_individual_data();
            if (data.id == Event::TwoIndividualData::MOVE) {
                // for moving, you get to see the individual in either location
                if (!(can_see_thing(observer, data.actor, data.actor_location) || can_see_thing(observer, data.actor, data.target_location)))
                    return false;
                *output_event = event;
                return true;
            }
            uint256 actor;
            uint256 target;
            if (data.actor == observer->id || data.target == observer->id) {
                // we can always observe attacking/bumping involving ourself
                actor = data.actor;
                target = data.target;
            } else {
                // we may not be privy to this happening
                actor = check_visible(observer, data.actor);
                target = check_visible(observer, data.target);
            }

            if (actor == uint256::zero() && target == uint256::zero())
                return false;
            *output_event = event;
            output_event->two_individual_data().actor = actor;
            output_event->two_individual_data().target = target;
            return true;
        }
        case Event::INDIVIDUAL_AND_ITEM: {
            Event::IndividualAndItemData & data = event.individual_and_item_data();
            if (observer->id == data.individual) {
                // you're always aware of what you're doing
                *output_event = event;
                return true;
            }
            switch (data.id) {
                case Event::IndividualAndItemData::INDIVIDUAL_PICKS_UP_ITEM:
                case Event::IndividualAndItemData::INDIVIDUAL_SUCKS_UP_ITEM:
                case Event::IndividualAndItemData::ITEM_HITS_INDIVIDUAL:
                case Event::IndividualAndItemData::THROW_ITEM: {
                    // the item is not in anyone's hand, so if you can see the location, you can see the event.
                    // note that we do this check after the pick-up type events go through,
                    // so we can't rely on check_visible in case the holder is invisible now.
                    if (!observer->life()->knowledge.tile_is_visible[data.location].any())
                        return false;
                    *output_event = event;
                    // you might have just gotten a clue about an invisible monster
                    return true;
                }
                case Event::IndividualAndItemData::WAND_DISINTEGRATES:
                case Event::IndividualAndItemData::ZAP_WAND:
                case Event::IndividualAndItemData::ZAP_WAND_NO_CHARGES:
                    // you need to see the individual to see the event
                    if (check_visible(observer, data.individual) == uint256::zero())
                        return false;
                    // you also need to see the item, not via cogniscopy
                    if (check_visible(observer, data.item) == uint256::zero())
                        return false;
                    *output_event = event;
                    return true;
            }
            panic("unreachable");
        }
        case Event::WAND_HIT: {
            Event::WandHitData & data = event.wand_hit_data();
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
            Event::UsePotionData & data = event.use_potion_data();
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
            Event::ItemAndLocationData & data = event.item_and_location_data();
            if (!can_see_location(observer, data.location))
                return false;
            uint256 item = check_visible(observer, data.item);
            if (item == uint256::zero())
                return false;
            *output_event = event;
            return true;
        }
    }
    panic("see event");
}

static void record_perception_of_location(Thing observer, Coord location) {
    // don't set tile_is_visible, because it might actually not be.
    observer->life()->knowledge.tiles[location] = actual_map_tiles[location];
    // we also get to see items here
    List<Thing> items;
    find_items_on_floor(location, &items);
    for (int i = 0; i < items.length(); i++)
        record_perception_of_thing(observer, items[i]->id);
}

static void become_aware_of_something_at_location(Thing observer, uint256 target_id, Coord location) {
    if (can_see_thing(observer, target_id)) {
        // yeah, hello there.
        record_perception_of_thing(observer, target_id);
    } else {
        // unseen thing
        PerceivedThing perceived_thing = observer->life()->knowledge.perceived_things.get(target_id, nullptr);
        if (perceived_thing != nullptr) {
            // there you are!
            perceived_thing->location = location;
        } else {
            // who are you? what are you?
            perceived_thing = create<PerceivedThingImpl>(target_id, SpeciesId_UNSEEN, location, Team_BAD_GUYS, StatusEffects());
            observer->life()->knowledge.perceived_things.put(target_id, perceived_thing);
        }
    }
}

void record_perception_of_thing(Thing observer, uint256 target_id) {
    PerceivedThing target = to_perceived_thing(target_id);
    observer->life()->knowledge.perceived_things.put(target_id, target);
    // cogniscopy doesn't see items.
    if (can_see_location(observer, target->location)) {
        List<Thing> inventory;
        find_items_in_inventory(target_id, &inventory);
        for (int i = 0; i < inventory.length(); i++)
            record_perception_of_thing(observer, inventory[i]->id);
    }
}

static void id_item(Thing observer, WandDescriptionId description_id, WandId id) {
    if (description_id == WandDescriptionId_COUNT)
        return; // can't see it
    observer->life()->knowledge.wand_identities[description_id] = id;
}
static void id_item(Thing observer, PotionDescriptionId description_id, PotionId id) {
    observer->life()->knowledge.potion_identities[description_id] = id;
}

void publish_event(Event event) {
    publish_event(event, nullptr);
}
void publish_event(Event actual_event, IdMap<WandDescriptionId> * perceived_current_zapper) {
    Thing observer;
    for (auto iterator = actual_individuals(); iterator.next(&observer);) {
        Event event;
        if (!see_event(observer, actual_event, &event))
            continue;
        // make changes to our knowledge
        List<uint256> delete_ids;
        switch (event.type) {
            case Event::THE_INDIVIDUAL: {
                Event::TheIndividualData & data = event.the_individual_data();
                switch (data.id) {
                    case Event::TheIndividualData::POISONED:
                        observer->life()->knowledge.perceived_things.get(data.individual)->status_effects.poison_expiration_time = 0x7fffffffffffffffLL;
                        break;
                    case Event::TheIndividualData::NO_LONGER_CONFUSED:
                    case Event::TheIndividualData::NO_LONGER_FAST:
                    case Event::TheIndividualData::NO_LONGER_HAS_ETHEREAL_VISION:
                    case Event::TheIndividualData::NO_LONGER_COGNISCOPIC:
                    case Event::TheIndividualData::NO_LONGER_BLIND:
                    case Event::TheIndividualData::NO_LONGER_POISONED:
                    case Event::TheIndividualData::APPEAR:
                    case Event::TheIndividualData::TURN_INVISIBLE:
                        record_perception_of_thing(observer, data.individual);
                        break;
                    case Event::TheIndividualData::DISAPPEAR: {
                        delete_ids.append(data.individual);
                        // the equipment goes too
                        PerceivedThing item;
                        for (auto iterator = observer->life()->knowledge.perceived_things.value_iterator(); iterator.next(&item);) {
                            if (item->location == Coord::nowhere() && item->container_id == data.individual)
                                delete_ids.append(item->id);
                        }
                        break;
                    }
                    case Event::TheIndividualData::LEVEL_UP:
                        // no state change
                        break;
                    case Event::TheIndividualData::DIE:
                        delete_ids.append(data.individual);
                        break;
                }
                break;
            }
            case Event::TWO_INDIVIDUAL: {
                Event::TwoIndividualData & data = event.two_individual_data();
                switch (data.id) {
                    case Event::TwoIndividualData::MOVE:
                        record_perception_of_thing(observer, data.actor);
                        if (data.actor == observer->id) {
                            // moving into a space while blind explores the space
                            record_perception_of_location(observer, data.target_location);
                        }
                        break;
                    case Event::TwoIndividualData::BUMP_INTO:
                    case Event::TwoIndividualData::ATTACK:
                    case Event::TwoIndividualData::KILL: {
                        if (data.actor == observer->id) {
                            // you're doing something, possibly blind
                            if (data.target != uint256::zero()) {
                                become_aware_of_something_at_location(observer, data.target, data.target_location);
                            } else {
                                // you bumps into a wall or attacks a wall
                                record_perception_of_location(observer, data.target_location);
                                // and there's no individual there to attack
                                List<PerceivedThing> perceived_things;
                                find_perceived_things_at(observer, data.target_location, &perceived_things);
                                for (int i = 0; i < perceived_things.length(); i++) {
                                    if (perceived_things[i]->thing_type == ThingType_INDIVIDUAL)
                                        observer->life()->knowledge.perceived_things.remove(perceived_things[i]->id);
                                }
                            }
                        } else if (data.target == observer->id) {
                            // something's happening to you, possibly while you're blind
                            become_aware_of_something_at_location(observer, data.actor, data.actor_location);
                        } else {
                            // we already know what's going on here.
                        }
                        if (data.id == Event::TwoIndividualData::KILL && data.target != uint256::zero()) {
                            delete_ids.append(data.target);
                        }
                        break;
                    }
                }
                break;
            }
            case Event::INDIVIDUAL_AND_ITEM: {
                Event::IndividualAndItemData & data = event.individual_and_item_data();
                become_aware_of_something_at_location(observer, data.individual, data.location);
                switch (data.id) {
                    case Event::IndividualAndItemData::ZAP_WAND:
                        perceived_current_zapper->put(observer->id, actual_things.get(data.item)->wand_info()->description_id);
                        break;
                    case Event::IndividualAndItemData::ZAP_WAND_NO_CHARGES:
                        // boring
                        break;
                    case Event::IndividualAndItemData::WAND_DISINTEGRATES:
                        delete_ids.append(data.item);
                        break;
                    case Event::IndividualAndItemData::THROW_ITEM:
                        // TODO: should we delete the item if it flies out of view?
                        break;
                    case Event::IndividualAndItemData::ITEM_HITS_INDIVIDUAL:
                        // no state change
                        break;
                    case Event::IndividualAndItemData::INDIVIDUAL_PICKS_UP_ITEM:
                    case Event::IndividualAndItemData::INDIVIDUAL_SUCKS_UP_ITEM:
                        if (!can_see_thing(observer, data.individual, data.location)) {
                            // where'd it go?
                            delete_ids.append(data.item);
                        } else {
                            record_perception_of_thing(observer, data.item);
                        }
                        break;
                }
                break;
            }
            case Event::WAND_HIT: {
                Event::WandHitData & data = event.wand_hit_data();
                WandId true_id = data.observable_effect;
                if (true_id != WandId_UNKNOWN)
                    id_item(observer, perceived_current_zapper->get(observer->id, WandDescriptionId_COUNT), true_id);

                if (data.target != uint256::zero()) {
                    // notice confusion, speed, etc.
                    StatusEffects & status_effects = observer->life()->knowledge.perceived_things.get(data.target)->status_effects;
                    switch (data.observable_effect) {
                        case WandId_WAND_OF_CONFUSION:
                            status_effects.confused_expiration_time = 0x7fffffffffffffffLL;
                            break;
                        case WandId_WAND_OF_SPEED:
                            status_effects.speed_up_expiration_time = 0x7fffffffffffffffLL;
                            break;
                        case WandId_WAND_OF_REMEDY:
                            status_effects.confused_expiration_time = -1;
                            status_effects.poison_expiration_time = -1;
                            break;
                        case WandId_UNKNOWN:
                        case WandId_WAND_OF_DIGGING:
                        case WandId_WAND_OF_STRIKING:
                            // no change
                            break;
                        case WandId_COUNT:
                            panic("not a real wand id");
                    }
                }
                break;
            }
            case Event::USE_POTION: {
                Event::UsePotionData & data = event.use_potion_data();
                PotionId effect = data.effect;
                if (effect != PotionId_UNKNOWN) {
                    // ah hah!
                    PotionDescriptionId description_id = observer->life()->knowledge.perceived_things.get(data.item_id)->potion_info()->description_id;
                    id_item(observer, description_id, effect);

                    StatusEffects & status_effects = observer->life()->knowledge.perceived_things.get(data.target_id)->status_effects;
                    switch (effect) {
                        case PotionId_POTION_OF_HEALING:
                            // no state change
                            break;
                        case PotionId_POTION_OF_POISON:
                            status_effects.poison_expiration_time = 0x7fffffffffffffffLL;
                            break;
                        case PotionId_POTION_OF_ETHEREAL_VISION:
                            status_effects.ethereal_vision_expiration_time = 0x7fffffffffffffffLL;
                            break;
                        case PotionId_POTION_OF_COGNISCOPY:
                            status_effects.cogniscopy_expiration_time = 0x7fffffffffffffffLL;
                            break;
                        case PotionId_POTION_OF_BLINDNESS:
                            status_effects.blindness_expiration_time = 0x7fffffffffffffffLL;
                            break;

                        case PotionId_UNKNOWN:
                        case PotionId_COUNT:
                            panic("not a real id");
                    }
                }
                delete_ids.append(data.item_id);
                break;
            }
            case Event::POLYMORPH: {
                Event::PolymorphData & data = event.polymorph_data();
                record_perception_of_thing(observer, data.individual);
                break;
            }
            case Event::ITEM_AND_LOCATION: {
                Event::ItemAndLocationData & data = event.item_and_location_data();
                switch (data.id) {
                    case Event::ItemAndLocationData::WAND_EXPLODES:
                        perceived_current_zapper->put(observer->id, actual_things.get(data.item)->wand_info()->description_id);
                        delete_ids.append(data.item);
                        break;
                    case Event::ItemAndLocationData::ITEM_HITS_WALL:
                    case Event::ItemAndLocationData::ITEM_HITS_SOMETHING:
                        // the item may have been thrown from out of view, so make sure we know what it is.
                        record_perception_of_thing(observer, data.item);
                        break;
                    case Event::ItemAndLocationData::ITEM_DROPS_TO_THE_FLOOR:
                    case Event::ItemAndLocationData::SOMETHING_PICKS_UP_ITEM:
                    case Event::ItemAndLocationData::SOMETHING_SUCKS_UP_ITEM:
                        record_perception_of_thing(observer, data.item);
                        break;
                }
                break;
            }
        }
        if (observer->life()->species()->has_mind) {
            // we need to log the event before the monster disappears from our knowledge
            RememberedEvent remembered_event = to_remembered_event(observer, event);
            if (remembered_event != nullptr)
                observer->life()->knowledge.remembered_events.append(remembered_event);
        }
        // now that we've had a chance to talk about it, delete it if we should
        for (int i = 0; i < delete_ids.length(); i++)
            observer->life()->knowledge.perceived_things.remove(delete_ids[i]);
    }
}
