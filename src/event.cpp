#include "event.hpp"

#include "display.hpp"
#include "swarkland.hpp"

static RememberedEvent to_remembered_event(Thing observer, Event event) {
    ByteBuffer buffer1;
    ByteBuffer buffer2;
    RememberedEvent result = create<RememberedEventImpl>();
    switch (event.type) {
        case Event::MOVE:
            // unremarkable
            return NULL;
        case Event::BUMP_INTO_WALL:
            get_individual_description(observer, event.move_data().individual, &buffer1);
            result->bytes.format("%s bumps into a wall.", buffer1.raw());
            return result;
        case Event::BUMP_INTO_INDIVIDUAL:
            get_individual_description(observer, event.attack_data().attacker, &buffer1);
            get_individual_description(observer, event.attack_data().target, &buffer2);
            result->bytes.format("%s bumps into %s.", buffer1.raw(), buffer2.raw());
            return result;
        case Event::BUMP_INTO_SOMETHING:
            get_individual_description(observer, event.move_data().individual, &buffer1);
            result->bytes.format("%s bumps into something.", buffer1.raw());
            return result;
        case Event::SOMETHING_BUMP_INTO_INDIVIDUAL:
            get_individual_description(observer, event.move_data().individual, &buffer1);
            result->bytes.format("something unseen bumps into %s.", buffer1.raw());
            return result;

        case Event::ATTACK:
            get_individual_description(observer, event.attack_data().attacker, &buffer1);
            get_individual_description(observer, event.attack_data().target, &buffer2);
            result->bytes.format("%s hits %s!", buffer1.raw(), buffer2.raw());
            return result;
        case Event::ATTACK_SOMETHING:
            get_individual_description(observer, event.move_data().individual, &buffer1);
            result->bytes.format("%s attacks something.", buffer1.raw());
            return result;
        case Event::SOMETHING_ATTACK_INDIVIDUAL:
            get_individual_description(observer, event.move_data().individual, &buffer1);
            result->bytes.format("something unseen attacks %s!", buffer1.raw());
            return result;
        case Event::ATTACK_THIN_AIR:
            get_individual_description(observer, event.move_data().individual, &buffer1);
            result->bytes.format("%s attacks thin air.", buffer1.raw());
            return result;
        case Event::ATTACK_WALL:
            get_individual_description(observer, event.move_data().individual, &buffer1);
            result->bytes.format("%s attacks a wall.", buffer1.raw());
            return result;

        case Event::DIE:
            get_individual_description(observer, event.the_individual_data(), &buffer1);
            result->bytes.format("%s dies.", buffer1.raw());
            return result;

        case Event::ZAP_WAND: {
            Event::ZapWandData & data = event.zap_wand_data();
            get_individual_description(observer, data.wielder, &buffer1);
            get_item_description(observer, data.wielder, data.wand, &buffer2);
            result->bytes.format("%s zaps %s.", buffer1.raw(), buffer2.raw());
            return result;
        }
        case Event::ZAP_WAND_NO_CHARGES: {
            Event::ZapWandData & data = event.zap_wand_data();
            get_individual_description(observer, data.wielder, &buffer1);
            get_item_description(observer, data.wielder, data.wand, &buffer2);
            result->bytes.format("%s zaps %s, but %s just sputters.", buffer1.raw(), buffer2.raw(), buffer2.raw());
            return result;
        }
        case Event::WAND_DISINTEGRATES: {
            Event::ZapWandData & data = event.zap_wand_data();
            get_individual_description(observer, data.wielder, &buffer1);
            get_item_description(observer, data.wielder, data.wand, &buffer2);
            result->bytes.format("%s tries to zap %s, but %s disintegrates.", buffer1.raw(), buffer2.raw(), buffer2.raw());
            return result;
        }

        case Event::BEAM_HIT_INDIVIDUAL_NO_EFFECT:
            get_individual_description(observer, event.the_individual_data(), &buffer1);
            result->bytes.format("a magic beam hits %s, but nothing happens.", buffer1.raw());
            return result;
        case Event::BEAM_HIT_WALL_NO_EFFECT:
            result->bytes.format("a magic beam hits the wall, but nothing happens.");
            return result;
        case Event::BEAM_OF_CONFUSION_HIT_INDIVIDUAL:
            get_individual_description(observer, event.the_individual_data(), &buffer1);
            result->bytes.format("a magic beam hits %s; %s is confused!", buffer1.raw(), buffer1.raw());
            return result;
        case Event::BEAM_OF_STRIKING_HIT_INDIVIDUAL:
            get_individual_description(observer, event.the_individual_data(), &buffer1);
            result->bytes.format("a magic beam strikes %s!", buffer1.raw());
            return result;
        case Event::BEAM_OF_DIGGING_HIT_WALL:
            result->bytes.format("the wall magically crumbles away!");
            return result;

        case Event::NO_LONGER_CONFUSED:
            get_individual_description(observer, event.the_individual_data(), &buffer1);
            result->bytes.format("%s is no longer confused.", buffer1.raw());
            return result;

        case Event::APPEAR:
            get_individual_description(observer, event.the_individual_data(), &buffer1);
            result->bytes.format("%s appears out of nowhere!", buffer1.raw());
            return result;
        case Event::TURN_INVISIBLE:
            get_individual_description(observer, event.the_individual_data(), &buffer1);
            result->bytes.format("%s turns invisible!", buffer1.raw());
            return result;
        case Event::DISAPPEAR:
            get_individual_description(observer, event.the_individual_data(), &buffer1);
            result->bytes.format("%s vanishes out of sight!", buffer1.raw());
            return result;
        case Event::POLYMORPH:
            get_individual_description(observer, event.polymorph_data().individual, &buffer1);
            result->bytes.format("%s transforms into %s!", get_species_name(event.polymorph_data().old_species), buffer1.raw());
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

        case Event::DIE:
            if (!can_see_individual(observer, event.the_individual_data()))
                return false;
            *output_event = event;
            return true;

        case Event::ZAP_WAND:
        case Event::ZAP_WAND_NO_CHARGES:
        case Event::WAND_DISINTEGRATES:
            if (!can_see_individual(observer, event.zap_wand_data().wielder))
                return false;
            *output_event = event;
            return true;

        case Event::BEAM_HIT_INDIVIDUAL_NO_EFFECT:
        case Event::BEAM_OF_CONFUSION_HIT_INDIVIDUAL:
        case Event::BEAM_OF_STRIKING_HIT_INDIVIDUAL: {
            if (!can_see_individual(observer, event.the_individual_data()))
                return false;
            *output_event = event;
            return true;
        }
        case Event::BEAM_HIT_WALL_NO_EFFECT:
        case Event::BEAM_OF_DIGGING_HIT_WALL: {
            if (!can_see_location(observer, event.the_location_data()))
                return false;
            *output_event = event;
            return true;
        }

        case Event::NO_LONGER_CONFUSED:
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

        case Event::POLYMORPH:
            if (!can_see_location(observer, location_of(event.polymorph_data().individual)))
                return false;
            *output_event = event;
            return true;
    }
    panic("see event");
}

static void perceive_individual(Thing observer, uint256 target_id) {
    observer->life()->knowledge.perceived_individuals.put(target_id, to_perceived_individual(target_id));
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
                perceive_individual(observer, event.move_data().individual);
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

            case Event::DIE:
                delete_ids.append(event.the_individual_data());
                break;

            case Event::ZAP_WAND:
                perceived_current_zapper->put(observer->id, actual_things.get(event.zap_wand_data().wand)->wand_info()->description_id);
                break;
            case Event::ZAP_WAND_NO_CHARGES:
                // boring
                break;
            case Event::WAND_DISINTEGRATES:
                perceive_individual(observer, event.zap_wand_data().wielder);
                break;

            case Event::BEAM_HIT_INDIVIDUAL_NO_EFFECT:
            case Event::BEAM_HIT_WALL_NO_EFFECT:
                // no state change
                break;
            case Event::BEAM_OF_CONFUSION_HIT_INDIVIDUAL:
                perceive_individual(observer, event.the_individual_data());
                id_item(observer, perceived_current_zapper->get(observer->id, WandDescriptionId_COUNT), WandId_WAND_OF_CONFUSION);
                break;
            case Event::BEAM_OF_STRIKING_HIT_INDIVIDUAL:
                id_item(observer, perceived_current_zapper->get(observer->id, WandDescriptionId_COUNT), WandId_WAND_OF_STRIKING);
                break;
            case Event::BEAM_OF_DIGGING_HIT_WALL:
                id_item(observer, perceived_current_zapper->get(observer->id, WandDescriptionId_COUNT), WandId_WAND_OF_DIGGING);
                break;

            case Event::NO_LONGER_CONFUSED:
                perceive_individual(observer, event.the_individual_data());
                break;

            case Event::APPEAR:
                perceive_individual(observer, event.the_individual_data());
                break;
            case Event::TURN_INVISIBLE:
                perceive_individual(observer, event.the_individual_data());
                break;
            case Event::DISAPPEAR:
                delete_ids.append(event.the_individual_data());
                break;

            case Event::POLYMORPH:
                perceive_individual(observer, event.polymorph_data().individual);
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
            observer->life()->knowledge.perceived_individuals.remove(delete_ids[i]);
    }
}
