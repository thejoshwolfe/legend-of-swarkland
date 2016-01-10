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
        case Event::INDIVIDUAL_AND_LOCATION: {
            Event::IndividualAndLocationData & data = event.individual_and_location_data();
            switch (data.id) {
                case Event::IndividualAndLocationData::MOVE:
                    // unremarkable
                    return nullptr;
                case Event::IndividualAndLocationData::BUMP_INTO_LOCATION:
                case Event::IndividualAndLocationData::ATTACK_LOCATION: {
                    Span actor_description = get_thing_description(observer, data.actor);
                    Span bumpee_description;
                    if (!is_open_space(observer->life()->knowledge.tiles[data.location].tile_type))
                        bumpee_description = new_span("a wall");
                    else
                        bumpee_description = new_span("thin air");
                    const char * fmt;
                    switch (data.id) {
                        case Event::IndividualAndLocationData::MOVE:
                            panic("unreachable");
                        case Event::IndividualAndLocationData::BUMP_INTO_LOCATION:
                            fmt = "%s bumps into %s.";
                            break;
                        case Event::IndividualAndLocationData::ATTACK_LOCATION:
                            fmt = "%s hits %s.";
                            break;
                    }
                    result->span->format(fmt, actor_description, bumpee_description);
                    return result;
                }
            }
        }
        case Event::TWO_INDIVIDUAL: {
            Event::TwoIndividualData & data = event.two_individual_data();
            switch (data.id) {
                case Event::TwoIndividualData::BUMP_INTO_INDIVIDUAL:
                case Event::TwoIndividualData::ATTACK_INDIVIDUAL:
                case Event::TwoIndividualData::MELEE_KILL: {
                    assert(data.actor != uint256::zero());
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
                case PotionId_POTION_OF_INVISIBILITY:
                    result->span->format("; %s disappears!", get_thing_description(observer, data.target_id));
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
    if (has_status(actual_target, StatusEffect::INVISIBILITY))
        return false;
    // cogniscopy can see minds
    if (has_status(observer, StatusEffect::COGNISCOPY)) {
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

static uint256 to_unseen(Thing observer, uint256 actual_target_id) {
    Thing actual_target = actual_things.get(actual_target_id);
    PerceivedThing thing = find_perceived_individual_at(observer, actual_target->location);
    if (thing != nullptr) {
        // we're already aware of something here
        if (thing->life()->species_id != SpeciesId_UNSEEN) {
            // we're trying to place an unseen marker. leave this guy alone.
            thing = nullptr;
        }
    }
    if (thing == nullptr) {
        // invent an unseen individual here
        uint256 id = random_uint256();
        Team opposite_team = observer->life()->team == Team_BAD_GUYS ? Team_GOOD_GUYS :Team_BAD_GUYS;
        thing = create<PerceivedThingImpl>(id, SpeciesId_UNSEEN, actual_target->location, opposite_team);
        observer->life()->knowledge.perceived_things.put(id, thing);
    }
    return thing->id;
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
        case Event::INDIVIDUAL_AND_LOCATION: {
            Event::IndividualAndLocationData & data = event.individual_and_location_data();
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
            Event::TwoIndividualData & data = event.two_individual_data();
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
                            output_event->two_individual_data().target = to_unseen(observer, data.target);
                            return true;
                        }
                    } else {
                        if (can_see_thing(observer, data.target)) {
                            output_event->two_individual_data().actor = to_unseen(observer, data.actor);
                            return true;
                        } else {
                            return false;
                        }
                    }
                }
            }
            panic("unreachable");
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

void record_perception_of_thing(Thing observer, uint256 target_id) {
    PerceivedThing target = observer->life()->knowledge.perceived_things.get(target_id, nullptr);
    if (target != nullptr && target->thing_type == ThingType_INDIVIDUAL && target->life()->species_id == SpeciesId_UNSEEN)
        return;
    target = to_perceived_thing(target_id);
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
                        put_status(observer->life()->knowledge.perceived_things.get(data.individual), StatusEffect::POISON);
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
            case Event::INDIVIDUAL_AND_LOCATION: {
                Event::IndividualAndLocationData & data = event.individual_and_location_data();
                switch (data.id) {
                    case Event::IndividualAndLocationData::MOVE:
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
                    PerceivedThing target = observer->life()->knowledge.perceived_things.get(data.target);
                    switch (data.observable_effect) {
                        case WandId_WAND_OF_CONFUSION:
                            put_status(target, StatusEffect::CONFUSION);
                            break;
                        case WandId_WAND_OF_SPEED:
                            put_status(target, StatusEffect::SPEED);
                            break;
                        case WandId_WAND_OF_REMEDY:
                            maybe_remove_status(target, StatusEffect::CONFUSION);
                            maybe_remove_status(target, StatusEffect::POISON);
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

                    PerceivedThing target = observer->life()->knowledge.perceived_things.get(data.target_id);
                    switch (effect) {
                        case PotionId_POTION_OF_HEALING:
                            // no state change
                            break;
                        case PotionId_POTION_OF_POISON:
                            put_status(target, StatusEffect::POISON);
                            break;
                        case PotionId_POTION_OF_ETHEREAL_VISION:
                            put_status(target, StatusEffect::ETHEREAL_VISION);
                            break;
                        case PotionId_POTION_OF_COGNISCOPY:
                            put_status(target, StatusEffect::COGNISCOPY);
                            break;
                        case PotionId_POTION_OF_BLINDNESS:
                            put_status(target, StatusEffect::BLINDNESS);
                            break;
                        case PotionId_POTION_OF_INVISIBILITY:
                            put_status(target, StatusEffect::INVISIBILITY);
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
}
