#include "event.hpp"

#include "display.hpp"
#include "swarkland.hpp"

static RememberedEvent to_remembered_event(Thing observer, Event event) {
    RememberedEvent result = create<RememberedEventImpl>();
    switch (event.type) {
        case Event::MOVE:
            // unremarkable
            return nullptr;
        case Event::BUMP_INTO:
        case Event::ATTACK: {
            Event::TwoIndividualData & data = event.two_individual_data();
            Span actor_description = data.actor != uint256::zero() ? get_thing_description(observer, data.actor) : new_span("something unseen");
            // what did it bump into? whatever we think is there
            Span bumpee_description;
            if (data.target != uint256::zero()) {
                bumpee_description = get_thing_description(observer, data.target);
            } else {
                // can't see anybody there. what are we bumping into?
                if (!is_open_space(observer->life()->knowledge.tiles[data.target_location].tile_type))
                    bumpee_description = new_span("a wall");
                else if (!observer->life()->knowledge.tile_is_visible[data.target_location].any())
                    bumpee_description = new_span("something");
                else
                    bumpee_description = new_span("thin air");
            }
            const char * fmt = event.type == Event::BUMP_INTO ? "%s bumps into %s" : "%s hits %s";
            result->span->format(fmt, actor_description, bumpee_description);
            return result;
        }

        case Event::ZAP_WAND: {
            Event::ZapWandData & data = event.zap_wand_data();
            result->span->format("%s zaps %s.", get_thing_description(observer, data.wielder), get_thing_description(observer, data.wand));
            return result;
        }
        case Event::ZAP_WAND_NO_CHARGES: {
            Event::ZapWandData & data = event.zap_wand_data();
            Span wand_description = get_thing_description(observer, data.wand);
            result->span->format("%s zaps %s, but %s just sputters.", get_thing_description(observer, data.wielder), wand_description, wand_description);
            return result;
        }
        case Event::WAND_DISINTEGRATES: {
            Event::ZapWandData & data = event.zap_wand_data();
            Span wand_description = get_thing_description(observer, data.wand);
            result->span->format("%s tries to zap %s, but %s disintegrates.", get_thing_description(observer, data.wielder), wand_description, wand_description);
            return result;
        }
        case Event::WAND_EXPLODES:
            result->span->format("%s explodes!", get_thing_description(observer, event.item_and_location_data().item));
            return result;

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

        case Event::THROW_ITEM:
            result->span->format("%s throws %s.",
                    get_thing_description(observer, event.zap_wand_data().wielder),
                    get_thing_description(observer, event.zap_wand_data().wand));
            return result;
        case Event::ITEM_HITS_INDIVIDUAL:
            result->span->format("%s hits %s!",
                    get_thing_description(observer, event.zap_wand_data().wand),
                    get_thing_description(observer, event.zap_wand_data().wielder));
            return result;
        case Event::ITEM_HITS_WALL:
            result->span->format("%s hits a wall.", get_thing_description(observer, event.item_and_location_data().item));
            return result;
        case Event::ITEM_HITS_SOMETHING:
            result->span->format("%s hits something.", get_thing_description(observer, event.item_and_location_data().item));
            return result;

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

                case PotionId_UNKNOWN:
                    result->span->append(", but nothing happens.");
                    break;
                case PotionId_COUNT:
                    panic("not a real id");
            }
            return result;
        }

        case Event::POISONED:
            result->span->format("%s is poisoned!", get_thing_description(observer, event.the_individual_data()));
            return result;
        case Event::NO_LONGER_CONFUSED:
            result->span->format("%s is no longer confused.", get_thing_description(observer, event.the_individual_data()));
            return result;
        case Event::NO_LONGER_FAST:
            result->span->format("%s slows back down to normal speed.", get_thing_description(observer, event.the_individual_data()));
            return result;
        case Event::NO_LONGER_HAS_ETHEREAL_VISION:
            result->span->format("%s no longer has ethereal vision.", get_thing_description(observer, event.the_individual_data()));
            return result;
        case Event::NO_LONGER_POISONED:
            result->span->format("%s is no longer poisoned.", get_thing_description(observer, event.the_individual_data()));
            return result;

        case Event::APPEAR:
            result->span->format("%s appears out of nowhere!", get_thing_description(observer, event.the_individual_data()));
            return result;
        case Event::TURN_INVISIBLE:
            result->span->format("%s turns invisible!", get_thing_description(observer, event.the_individual_data()));
            return result;
        case Event::DISAPPEAR:
            result->span->format("%s vanishes out of sight!", get_thing_description(observer, event.the_individual_data()));
            return result;
        case Event::DIE:
            result->span->format("%s dies.", get_thing_description(observer, event.the_individual_data()));
            return result;
        case Event::LEVEL_UP:
            result->span->format("%s levels up.", get_thing_description(observer, event.the_individual_data()));
            return result;

        case Event::POLYMORPH:
            result->span->format("%s transforms into %s!",
                    get_species_name(event.polymorph_data().old_species),
                    get_thing_description(observer, event.polymorph_data().individual));
            return result;

        case Event::ITEM_DROPS_TO_THE_FLOOR:
            result->span->format("%s drops to the floor.", get_thing_description(observer, event.item_and_location_data().item));
            return result;
        case Event::INDIVIDUAL_PICKS_UP_ITEM:
            result->span->format("%s picks up %s.",
                    get_thing_description(observer, event.zap_wand_data().wielder),
                    get_thing_description(observer, event.zap_wand_data().wand));
            return result;
        case Event::SOMETHING_PICKS_UP_ITEM:
            result->span->format("something unseen picks up %s.", get_thing_description(observer, event.item_and_location_data().item));
            return result;
        case Event::INDIVIDUAL_SUCKS_UP_ITEM:
            result->span->format("%s sucks up %s.",
                    get_thing_description(observer, event.zap_wand_data().wielder),
                    get_thing_description(observer, event.zap_wand_data().wand));
            return result;
        case Event::SOMETHING_SUCKS_UP_ITEM:
            result->span->format("something unseen sucks up %s.", get_thing_description(observer, event.item_and_location_data().item));
            return result;
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
bool can_see_individual(Thing observer, uint256 target_id, Coord target_location) {
    // you can always see yourself
    if (observer->id == target_id)
        return true;
    // you can't see anything else while dead
    if (!observer->still_exists)
        return false;
    Thing actual_target = actual_things.get(target_id);
    // nobody can see invisible people
    if (actual_target->status_effects.invisible)
        return false;
    // we can see someone if they're in our line of sight
    if (!observer->life()->knowledge.tile_is_visible[target_location].any())
        return false;
    return true;
}
bool can_see_individual(Thing observer, uint256 target_id) {
    return can_see_individual(observer, target_id, actual_things.get(target_id)->location);
}
static Coord location_of(uint256 individual_id) {
    return actual_things.get(individual_id)->location;
}

static bool see_event(Thing observer, Event event, Event * output_event) {
    switch (event.type) {
        case Event::MOVE: {
            Event::TwoIndividualData & data = event.two_individual_data();
            if (!(can_see_individual(observer, data.actor, data.actor_location) || can_see_individual(observer, data.actor, data.target_location)))
                return false;
            *output_event = event;
            return true;
        }
        case Event::BUMP_INTO:
        case Event::ATTACK: {
            Event::TwoIndividualData & data = event.two_individual_data();

            uint256 actor = data.actor;
            if (data.actor != uint256::zero() && !can_see_individual(observer, data.actor))
                actor = uint256::zero();

            uint256 target = data.target;
            if (data.target != uint256::zero() && !can_see_individual(observer, data.target))
                target = uint256::zero();

            if (actor == uint256::zero() && target == uint256::zero())
                return false;
            *output_event = event;
            output_event->two_individual_data().actor = actor;
            output_event->two_individual_data().target = target;
            return true;
        }

        case Event::ZAP_WAND:
        case Event::ZAP_WAND_NO_CHARGES:
        case Event::WAND_DISINTEGRATES:
            if (!can_see_individual(observer, event.zap_wand_data().wielder))
                return false;
            *output_event = event;
            return true;
        case Event::WAND_EXPLODES:
            if (!can_see_location(observer, event.item_and_location_data().location))
                return false;
            *output_event = event;
            return true;

        case Event::WAND_HIT: {
            Event::WandHitData & data = event.wand_hit_data();
            if (data.target != uint256::zero()) {
                if (!can_see_individual(observer, data.target))
                    return false;
            } else {
                if (!observer->life()->knowledge.tile_is_visible[data.location].any())
                    return false;
            }
            *output_event = event;
            return true;
        }

        case Event::THROW_ITEM:
        case Event::ITEM_HITS_INDIVIDUAL:
            if (!can_see_individual(observer, event.zap_wand_data().wielder))
                return false;
            *output_event = event;
            return true;
        case Event::ITEM_HITS_SOMETHING:
        case Event::ITEM_HITS_WALL:
            if (!can_see_location(observer, event.item_and_location_data().location))
                return false;
            *output_event = event;
            return true;

        case Event::USE_POTION:
            if (!can_see_location(observer, event.use_potion_data().location))
                return false;
            if (!can_see_individual(observer, event.use_potion_data().target_id)) {
                if (event.use_potion_data().is_breaking) {
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

        case Event::POISONED:
        case Event::NO_LONGER_CONFUSED:
        case Event::NO_LONGER_FAST:
        case Event::NO_LONGER_HAS_ETHEREAL_VISION:
        case Event::NO_LONGER_POISONED:
        case Event::APPEAR:
            if (!can_see_individual(observer, event.the_individual_data()))
                return false;
            *output_event = event;
            return true;
        case Event::TURN_INVISIBLE:
            if (!can_see_location(observer, location_of(event.the_individual_data())))
                return false;
            if (!can_see_individual(observer, event.the_individual_data())) {
                *output_event = Event::disappear(event.the_individual_data());
                return true;
            }
            *output_event = event;
            return true;
        case Event::DISAPPEAR:
            panic("not a real event");
        case Event::LEVEL_UP:
        case Event::DIE:
            if (!can_see_individual(observer, event.the_individual_data()))
                return false;
            *output_event = event;
            return true;

        case Event::POLYMORPH:
            if (!can_see_location(observer, location_of(event.polymorph_data().individual)))
                return false;
            *output_event = event;
            return true;

        case Event::ITEM_DROPS_TO_THE_FLOOR:
            if (!can_see_location(observer, event.item_and_location_data().location))
                return false;
            *output_event = event;
            return true;
        case Event::INDIVIDUAL_PICKS_UP_ITEM:
        case Event::INDIVIDUAL_SUCKS_UP_ITEM:
            if (!can_see_location(observer, location_of(event.zap_wand_data().wielder)))
                return false;
            if (!can_see_individual(observer, event.zap_wand_data().wielder)) {
                if (event.type == Event::INDIVIDUAL_PICKS_UP_ITEM)
                    *output_event = Event::something_picks_up_item(event.zap_wand_data().wand, location_of(event.zap_wand_data().wielder));
                else
                    *output_event = Event::something_sucks_up_item(event.zap_wand_data().wand, location_of(event.zap_wand_data().wielder));
                return true;
            }
            *output_event = event;
            return true;
        case Event::SOMETHING_PICKS_UP_ITEM:
        case Event::SOMETHING_SUCKS_UP_ITEM:
            panic("not a real event");
    }
    panic("see event");
}

static void record_perception_of_thing(Thing observer, uint256 target_id) {
    PerceivedThing target = to_perceived_thing(target_id);
    if (target == nullptr) {
        observer->life()->knowledge.perceived_things.remove(target_id);
        return;
    }
    observer->life()->knowledge.perceived_things.put(target_id, target);
    List<Thing> inventory;
    find_items_in_inventory(target_id, &inventory);
    for (int i = 0; i < inventory.length(); i++)
        record_perception_of_thing(observer, inventory[i]->id);
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
            case Event::MOVE:
                record_perception_of_thing(observer, event.two_individual_data().actor);
                break;
            case Event::BUMP_INTO:
            case Event::ATTACK:
                // no state change
                break;

            case Event::ZAP_WAND:
                perceived_current_zapper->put(observer->id, actual_things.get(event.zap_wand_data().wand)->wand_info()->description_id);
                break;
            case Event::ZAP_WAND_NO_CHARGES:
                // boring
                break;
            case Event::WAND_DISINTEGRATES:
                delete_ids.append(event.zap_wand_data().wand);
                break;
            case Event::WAND_EXPLODES:
                perceived_current_zapper->put(observer->id, actual_things.get(event.item_and_location_data().item)->wand_info()->description_id);
                delete_ids.append(event.item_and_location_data().item);
                break;

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
            }

            case Event::THROW_ITEM:
                // item is now in the air, i guess
                break;
            case Event::ITEM_HITS_INDIVIDUAL:
            case Event::ITEM_HITS_SOMETHING:
            case Event::ITEM_HITS_WALL:
                // no state change
                break;

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

                        case PotionId_UNKNOWN:
                        case PotionId_COUNT:
                            panic("not a real id");
                    }
                }
                delete_ids.append(event.use_potion_data().item_id);
                break;
            }

            case Event::POISONED:
                observer->life()->knowledge.perceived_things.get(event.the_individual_data())->status_effects.poison_expiration_time = 0x7fffffffffffffffLL;
                break;
            case Event::NO_LONGER_CONFUSED:
            case Event::NO_LONGER_FAST:
            case Event::NO_LONGER_HAS_ETHEREAL_VISION:
            case Event::NO_LONGER_POISONED:
                record_perception_of_thing(observer, event.the_individual_data());
                break;

            case Event::APPEAR:
                record_perception_of_thing(observer, event.the_individual_data());
                break;
            case Event::TURN_INVISIBLE:
                record_perception_of_thing(observer, event.the_individual_data());
                break;
            case Event::DISAPPEAR:
                delete_ids.append(event.the_individual_data());
                break;
            case Event::LEVEL_UP:
                // no state change
                break;
            case Event::DIE:
                delete_ids.append(event.the_individual_data());
                break;

            case Event::POLYMORPH:
                record_perception_of_thing(observer, event.polymorph_data().individual);
                break;

            case Event::ITEM_DROPS_TO_THE_FLOOR:
            case Event::SOMETHING_PICKS_UP_ITEM:
            case Event::SOMETHING_SUCKS_UP_ITEM:
                record_perception_of_thing(observer, event.item_and_location_data().item);
                break;
            case Event::INDIVIDUAL_PICKS_UP_ITEM:
            case Event::INDIVIDUAL_SUCKS_UP_ITEM:
                record_perception_of_thing(observer, event.zap_wand_data().wand);
                break;
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
