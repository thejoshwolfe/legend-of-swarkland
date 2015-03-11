#include "event.hpp"

#include "display.hpp"
#include "swarkland.hpp"

static RememberedEvent to_remembered_event(Thing observer, Event event) {
    RememberedEvent result = create<RememberedEventImpl>();
    switch (event.type) {
        case Event::MOVE:
            // unremarkable
            return NULL;
        case Event::BUMP_INTO_WALL:
            result->span->format("%s bumps into a wall.", get_individual_description(observer, event.move_data().individual));
            return result;
        case Event::BUMP_INTO_INDIVIDUAL:
            result->span->format("%s bumps into %s.",
                    get_individual_description(observer, event.attack_data().attacker),
                    get_individual_description(observer, event.attack_data().target));
            return result;
        case Event::BUMP_INTO_SOMETHING:
            result->span->format("%s bumps into something.", get_individual_description(observer, event.move_data().individual));
            return result;
        case Event::SOMETHING_BUMP_INTO_INDIVIDUAL:
            result->span->format("something unseen bumps into %s.", get_individual_description(observer, event.move_data().individual));
            return result;

        case Event::ATTACK:
            result->span->format("%s hits %s!",
                    get_individual_description(observer, event.attack_data().attacker),
                    get_individual_description(observer, event.attack_data().target));
            return result;
        case Event::ATTACK_SOMETHING:
            result->span->format("%s attacks something.", get_individual_description(observer, event.move_data().individual));
            return result;
        case Event::SOMETHING_ATTACK_INDIVIDUAL:
            result->span->format("something unseen attacks %s!", get_individual_description(observer, event.move_data().individual));
            return result;
        case Event::ATTACK_THIN_AIR:
            result->span->format("%s attacks thin air.", get_individual_description(observer, event.move_data().individual));
            return result;
        case Event::ATTACK_WALL:
            result->span->format("%s attacks a wall.", get_individual_description(observer, event.move_data().individual));
            return result;

        case Event::ZAP_WAND: {
            Event::ZapWandData & data = event.zap_wand_data();
            result->span->format("%s zaps %s.", get_individual_description(observer, data.wielder), get_item_description(observer, data.wand));
            return result;
        }
        case Event::ZAP_WAND_NO_CHARGES: {
            Event::ZapWandData & data = event.zap_wand_data();
            Span wand_description = get_item_description(observer, data.wand);
            result->span->format("%s zaps %s, but %s just sputters.", get_individual_description(observer, data.wielder), wand_description, wand_description);
            return result;
        }
        case Event::WAND_DISINTEGRATES: {
            Event::ZapWandData & data = event.zap_wand_data();
            Span wand_description = get_item_description(observer, data.wand);
            result->span->format("%s tries to zap %s, but %s disintegrates.", get_individual_description(observer, data.wielder), wand_description, wand_description);
            return result;
        }
        case Event::WAND_EXPLODES:
            result->span->format("%s explodes!", get_item_description(observer, event.item_and_location_data().item));
            return result;

        case Event::BEAM_HIT_INDIVIDUAL_NO_EFFECT:
            result->span->format("a magic beam hits %s, but nothing happens.", get_individual_description(observer, event.the_individual_data()));
            return result;
        case Event::BEAM_HIT_WALL_NO_EFFECT:
            result->span->format("a magic beam hits the wall, but nothing happens.");
            return result;
        case Event::BEAM_OF_CONFUSION_HIT_INDIVIDUAL: {
            Span individual_description = get_individual_description(observer, event.the_individual_data());
            result->span->format("a magic beam hits %s; %s is confused!", individual_description, individual_description);
            return result;
        }
        case Event::BEAM_OF_STRIKING_HIT_INDIVIDUAL:
            result->span->format("a magic beam strikes %s!", get_individual_description(observer, event.the_individual_data()));
            return result;
        case Event::BEAM_OF_DIGGING_HIT_WALL:
            result->span->format("the wall magically crumbles away!");
            return result;
        case Event::BEAM_OF_SPEED_HIT_INDIVIDUAL:
            result->span->format("%s speeds up!", get_individual_description(observer, event.the_individual_data()));
            return result;
        case Event::EXPLOSION_HIT_INDIVIDUAL_NO_EFFECT:
            result->span->format("an explosion hits %s, but nothing happens.", get_individual_description(observer, event.the_individual_data()));
            return result;
        case Event::EXPLOSION_HIT_WALL_NO_EFFECT:
            result->span->format("an explosion hits the wall, but nothing happens.");
            return result;
        case Event::EXPLOSION_OF_CONFUSION_HIT_INDIVIDUAL: {
            Span individual_description = get_individual_description(observer, event.the_individual_data());
            result->span->format("an explosion hits %s; %s is confused!", individual_description, individual_description);
            return result;
        }
        case Event::EXPLOSION_OF_STRIKING_HIT_INDIVIDUAL:
            result->span->format("an explosion strikes %s!", get_individual_description(observer, event.the_individual_data()));
            return result;
        case Event::EXPLOSION_OF_DIGGING_HIT_WALL:
            result->span->format("the wall magically crumbles away!");
            return result;
        case Event::EXPLOSION_OF_SPEED_HIT_INDIVIDUAL: {
            Span individual_description = get_individual_description(observer, event.the_individual_data());
            result->span->format("an explosion hits %s; %s speeds up!", individual_description, individual_description);
            return result;
        }

        case Event::THROW_ITEM:
            result->span->format("%s throws %s.",
                    get_individual_description(observer, event.zap_wand_data().wielder),
                    get_item_description(observer, event.zap_wand_data().wand));
            return result;
        case Event::ITEM_HITS_INDIVIDUAL:
            result->span->format("%s hits %s!",
                    get_item_description(observer, event.zap_wand_data().wand),
                    get_individual_description(observer, event.zap_wand_data().wielder));
            return result;
        case Event::ITEM_HITS_WALL:
            result->span->format("%s hits a wall.", get_item_description(observer, event.item_and_location_data().item));
            return result;
        case Event::ITEM_HITS_SOMETHING:
            result->span->format("%s hits something.", get_item_description(observer, event.item_and_location_data().item));
            return result;

        case Event::NO_LONGER_CONFUSED:
            result->span->format("%s is no longer confused.", get_individual_description(observer, event.the_individual_data()));
            return result;
        case Event::NO_LONGER_FAST:
            result->span->format("%s slows back down to normal speed.", get_individual_description(observer, event.the_individual_data()));
            return result;

        case Event::APPEAR:
            result->span->format("%s appears out of nowhere!", get_individual_description(observer, event.the_individual_data()));
            return result;
        case Event::TURN_INVISIBLE:
            result->span->format("%s turns invisible!", get_individual_description(observer, event.the_individual_data()));
            return result;
        case Event::DISAPPEAR:
            result->span->format("%s vanishes out of sight!", get_individual_description(observer, event.the_individual_data()));
            return result;
        case Event::DIE:
            result->span->format("%s dies.", get_individual_description(observer, event.the_individual_data()));
            return result;
        case Event::LEVEL_UP:
            result->span->format("%s levels up.", get_individual_description(observer, event.the_individual_data()));
            return result;

        case Event::POLYMORPH:
            result->span->format("%s transforms into %s!",
                    get_species_name(event.polymorph_data().old_species),
                    get_individual_description(observer, event.polymorph_data().individual));
            return result;

        case Event::ITEM_DROPS_TO_THE_FLOOR:
            result->span->format("%s drops to the floor.", get_item_description(observer, event.item_and_location_data().item));
            return result;
        case Event::INDIVIDUAL_PICKS_UP_ITEM:
            result->span->format("%s picks up %s.",
                    get_individual_description(observer, event.zap_wand_data().wielder),
                    get_item_description(observer, event.zap_wand_data().wand));
            return result;
        case Event::SOMETHING_PICKS_UP_ITEM:
            result->span->format("something unseen picks up %s.", get_item_description(observer, event.item_and_location_data().item));
            return result;
        case Event::INDIVIDUAL_SUCKS_UP_ITEM:
            result->span->format("%s sucks up %s.",
                    get_individual_description(observer, event.zap_wand_data().wielder),
                    get_item_description(observer, event.zap_wand_data().wand));
            return result;
        case Event::SOMETHING_SUCKS_UP_ITEM:
            result->span->format("something unseen sucks up %s.", get_item_description(observer, event.item_and_location_data().item));
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
        case Event::MOVE:
            if (!(can_see_individual(observer, event.move_data().individual, event.move_data().from) || can_see_individual(observer, event.move_data().individual, event.move_data().to)))
                return false;
            *output_event = event;
            return true;
        case Event::BUMP_INTO_WALL:
            if (!can_see_individual(observer, event.move_data().individual))
                return false;
            if (can_see_location(observer, event.move_data().to)) {
                *output_event = event;
            } else {
                *output_event = Event::bump_into_something(event.move_data().individual, event.move_data().from, event.move_data().to);
            }
            return true;
        case Event::BUMP_INTO_INDIVIDUAL:
            if (can_see_individual(observer, event.attack_data().attacker)) {
                if (can_see_individual(observer, event.attack_data().target)) {
                    *output_event = event;
                } else {
                    *output_event = Event::bump_into_something(event.attack_data().attacker, location_of(event.attack_data().attacker), location_of(event.attack_data().target));
                }
            } else {
                if (can_see_individual(observer, event.attack_data().target)) {
                    *output_event = Event::something_bump_into_individual(event.attack_data().target, location_of(event.attack_data().attacker), location_of(event.attack_data().target));
                } else {
                    return false;
                }
            }
            return true;
        case Event::BUMP_INTO_SOMETHING:
        case Event::SOMETHING_BUMP_INTO_INDIVIDUAL:
            panic("not a real event");

        case Event::ATTACK:
            if (can_see_individual(observer, event.attack_data().attacker)) {
                if (can_see_individual(observer, event.attack_data().target)) {
                    *output_event = event;
                } else {
                    *output_event = Event::attack_something(event.attack_data().attacker, location_of(event.attack_data().attacker), location_of(event.attack_data().target));
                }
            } else {
                if (can_see_individual(observer, event.attack_data().target)) {
                    *output_event = Event::something_attack_individual(event.attack_data().target, location_of(event.attack_data().attacker), location_of(event.attack_data().target));
                } else {
                    return false;
                }
            }
            return true;
        case Event::ATTACK_THIN_AIR:
        case Event::ATTACK_WALL:
            if (!can_see_individual(observer, event.move_data().individual))
                return false;
            *output_event = event;
            return true;
        case Event::ATTACK_SOMETHING:
        case Event::SOMETHING_ATTACK_INDIVIDUAL:
            panic("not a real event");

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

        case Event::BEAM_HIT_INDIVIDUAL_NO_EFFECT:
        case Event::BEAM_OF_CONFUSION_HIT_INDIVIDUAL:
        case Event::BEAM_OF_STRIKING_HIT_INDIVIDUAL:
        case Event::BEAM_OF_SPEED_HIT_INDIVIDUAL:
        case Event::EXPLOSION_HIT_INDIVIDUAL_NO_EFFECT:
        case Event::EXPLOSION_OF_CONFUSION_HIT_INDIVIDUAL:
        case Event::EXPLOSION_OF_STRIKING_HIT_INDIVIDUAL:
        case Event::EXPLOSION_OF_SPEED_HIT_INDIVIDUAL:
            if (!can_see_individual(observer, event.the_individual_data()))
                return false;
            *output_event = event;
            return true;
        case Event::BEAM_HIT_WALL_NO_EFFECT:
        case Event::BEAM_OF_DIGGING_HIT_WALL:
        case Event::EXPLOSION_HIT_WALL_NO_EFFECT:
        case Event::EXPLOSION_OF_DIGGING_HIT_WALL:
            if (!can_see_location(observer, event.the_location_data()))
                return false;
            *output_event = event;
            return true;

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

        case Event::NO_LONGER_CONFUSED:
        case Event::NO_LONGER_FAST:
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
    if (target == NULL) {
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

void publish_event(Event event) {
    publish_event(event, NULL);
}
void publish_event(Event event, IdMap<WandDescriptionId> * perceived_current_zapper) {
    Thing observer;
    for (auto iterator = actual_individuals(); iterator.next(&observer);) {
        Event apparent_event;
        if (!see_event(observer, event, &apparent_event))
            continue;
        // make changes to our knowledge
        List<uint256> delete_ids;
        switch (apparent_event.type) {
            case Event::MOVE:
                record_perception_of_thing(observer, apparent_event.move_data().individual);
                break;
            case Event::BUMP_INTO_WALL:
            case Event::BUMP_INTO_INDIVIDUAL:
                // no state change
                break;
            case Event::BUMP_INTO_SOMETHING:
            case Event::SOMETHING_BUMP_INTO_INDIVIDUAL:
                // TODO: mark unseen things
                break;

            case Event::ATTACK:
                // no state change
                break;
            case Event::ATTACK_SOMETHING:
            case Event::SOMETHING_ATTACK_INDIVIDUAL:
                // TODO: mark unseen things
                break;
            case Event::ATTACK_THIN_AIR:
            case Event::ATTACK_WALL:
                // no state change
                break;

            case Event::ZAP_WAND:
                perceived_current_zapper->put(observer->id, actual_things.get(apparent_event.zap_wand_data().wand)->wand_info()->description_id);
                break;
            case Event::ZAP_WAND_NO_CHARGES:
                // boring
                break;
            case Event::WAND_DISINTEGRATES:
                delete_ids.append(apparent_event.zap_wand_data().wand);
                break;
            case Event::WAND_EXPLODES:
                perceived_current_zapper->put(observer->id, actual_things.get(apparent_event.item_and_location_data().item)->wand_info()->description_id);
                delete_ids.append(apparent_event.item_and_location_data().item);
                break;

            case Event::BEAM_HIT_INDIVIDUAL_NO_EFFECT:
            case Event::BEAM_HIT_WALL_NO_EFFECT:
            case Event::EXPLOSION_HIT_INDIVIDUAL_NO_EFFECT:
            case Event::EXPLOSION_HIT_WALL_NO_EFFECT:
                // no state change
                break;
            case Event::BEAM_OF_CONFUSION_HIT_INDIVIDUAL:
            case Event::EXPLOSION_OF_CONFUSION_HIT_INDIVIDUAL:
                record_perception_of_thing(observer, apparent_event.the_individual_data());
                id_item(observer, perceived_current_zapper->get(observer->id, WandDescriptionId_COUNT), WandId_WAND_OF_CONFUSION);
                break;
            case Event::BEAM_OF_STRIKING_HIT_INDIVIDUAL:
            case Event::EXPLOSION_OF_STRIKING_HIT_INDIVIDUAL:
                id_item(observer, perceived_current_zapper->get(observer->id, WandDescriptionId_COUNT), WandId_WAND_OF_STRIKING);
                break;
            case Event::BEAM_OF_DIGGING_HIT_WALL:
            case Event::EXPLOSION_OF_DIGGING_HIT_WALL:
                id_item(observer, perceived_current_zapper->get(observer->id, WandDescriptionId_COUNT), WandId_WAND_OF_DIGGING);
                break;
            case Event::BEAM_OF_SPEED_HIT_INDIVIDUAL:
            case Event::EXPLOSION_OF_SPEED_HIT_INDIVIDUAL:
                id_item(observer, perceived_current_zapper->get(observer->id, WandDescriptionId_COUNT), WandId_WAND_OF_SPEED);
                break;

            case Event::THROW_ITEM:
                // item is now in the air, i guess
                break;
            case Event::ITEM_HITS_INDIVIDUAL:
            case Event::ITEM_HITS_SOMETHING:
            case Event::ITEM_HITS_WALL:
                // no state change
                break;

            case Event::NO_LONGER_CONFUSED:
                record_perception_of_thing(observer, apparent_event.the_individual_data());
                break;
            case Event::NO_LONGER_FAST:
                record_perception_of_thing(observer, apparent_event.the_individual_data());
                break;

            case Event::APPEAR:
                record_perception_of_thing(observer, apparent_event.the_individual_data());
                break;
            case Event::TURN_INVISIBLE:
                record_perception_of_thing(observer, apparent_event.the_individual_data());
                break;
            case Event::DISAPPEAR:
                delete_ids.append(apparent_event.the_individual_data());
                break;
            case Event::LEVEL_UP:
                // no state change
                break;
            case Event::DIE:
                delete_ids.append(apparent_event.the_individual_data());
                break;

            case Event::POLYMORPH:
                record_perception_of_thing(observer, apparent_event.polymorph_data().individual);
                break;

            case Event::ITEM_DROPS_TO_THE_FLOOR:
            case Event::SOMETHING_PICKS_UP_ITEM:
            case Event::SOMETHING_SUCKS_UP_ITEM:
                record_perception_of_thing(observer, apparent_event.item_and_location_data().item);
                break;
            case Event::INDIVIDUAL_PICKS_UP_ITEM:
            case Event::INDIVIDUAL_SUCKS_UP_ITEM:
                record_perception_of_thing(observer, apparent_event.zap_wand_data().wand);
                break;
        }
        if (observer->life()->species()->has_mind) {
            // we need to log the event before the monster disappears from our knowledge
            RememberedEvent remembered_event = to_remembered_event(observer, apparent_event);
            if (remembered_event != NULL)
                observer->life()->knowledge.remembered_events.append(remembered_event);
        }
        // now that we've had a chance to talk about it, delete it if we should
        for (int i = 0; i < delete_ids.length(); i++)
            observer->life()->knowledge.perceived_things.remove(delete_ids[i]);
    }
}
