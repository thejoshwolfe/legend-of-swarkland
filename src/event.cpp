#include "event.hpp"

#include "display.hpp"
#include "swarkland.hpp"

static RememberedEvent to_remembered_event(Individual observer, Event event) {
    ByteBuffer buffer1;
    ByteBuffer buffer2;
    RememberedEvent result = new RememberedEventImpl;
    switch (event.type) {
        case Event::MOVE:
            // unremarkable
            return NULL;
        case Event::BUMP_INTO_WALL:
            get_individual_description(observer, event._the_individual, &buffer1);
            result->bytes.format("%s bumps into a wall.", buffer1.raw());
            return result;
        case Event::BUMP_INTO_INDIVIDUAL:
            get_individual_description(observer, event._attack.attacker, &buffer1);
            get_individual_description(observer, event._attack.target, &buffer2);
            result->bytes.format("%s bumps into %s.", buffer1.raw(), buffer2.raw());
            return result;
        case Event::BUMP_INTO_SOMETHING:
            get_individual_description(observer, event._move.individual, &buffer1);
            result->bytes.format("%s bumps into something.", buffer1.raw());
            return result;
        case Event::SOMETHING_BUMP_INTO_INDIVIDUAL:
            get_individual_description(observer, event._move.individual, &buffer1);
            result->bytes.format("something unseen bumps into %s.", buffer1.raw());
            return result;

        case Event::ATTACK:
            get_individual_description(observer, event._attack.attacker, &buffer1);
            get_individual_description(observer, event._attack.target, &buffer2);
            result->bytes.format("%s hits %s!", buffer1.raw(), buffer2.raw());
            return result;
        case Event::ATTACK_SOMETHING:
            get_individual_description(observer, event._move.individual, &buffer1);
            result->bytes.format("%s attacks something.", buffer1.raw());
            return result;
        case Event::SOMETHING_ATTACK_INDIVIDUAL:
            get_individual_description(observer, event._move.individual, &buffer1);
            result->bytes.format("something unseen attacks %s!", buffer1.raw());
            return result;
        case Event::ATTACK_THIN_AIR:
            get_individual_description(observer, event._move.individual, &buffer1);
            result->bytes.format("%s attacks thin air.", buffer1.raw());
            return result;
        case Event::ATTACK_WALL:
            get_individual_description(observer, event._move.individual, &buffer1);
            result->bytes.format("%s attacks a wall.", buffer1.raw());
            return result;

        case Event::DIE:
            get_individual_description(observer, event._the_individual, &buffer1);
            result->bytes.format("%s dies.", buffer1.raw());
            return result;

        case Event::ZAP_WAND:
            get_individual_description(observer, event._zap.wielder, &buffer1);
            get_item_description(observer, event._zap.wielder, event._zap.wand, &buffer2);
            result->bytes.format("%s zaps %s.", buffer1.raw(), buffer2.raw());
            return result;
        case Event::WAND_HIT_NO_EFFECT:
            get_item_description(observer, event._zap_hit_individual.wielder, event._zap_hit_individual.wand, &buffer1);
            get_individual_description(observer, event._zap_hit_individual.target, &buffer2);
            result->bytes.format("%s hits %s, but nothing happens.", buffer1.raw(), buffer2.raw());
            return result;
        case Event::WAND_OF_CONFUSION_HIT:
            get_item_description(observer, event._zap_hit_individual.wielder, event._zap_hit_individual.wand, &buffer1);
            get_individual_description(observer, event._zap_hit_individual.target, &buffer2);
            result->bytes.format("%s hits %s; %s is confused!", buffer1.raw(), buffer2.raw(), buffer2.raw());
            return result;
        case Event::WAND_OF_STRIKING_HIT:
            get_item_description(observer, event._zap_hit_individual.wielder, event._zap_hit_individual.wand, &buffer1);
            get_individual_description(observer, event._zap_hit_individual.target, &buffer2);
            result->bytes.format("%s strikes %s!", buffer1.raw(), buffer2.raw());
            return result;
        case Event::WAND_OF_DIGGING_HIT_WALL:
            get_item_description(observer, event._zap_hit_location.wielder, event._zap_hit_location.wand, &buffer1);
            result->bytes.format("%s digs through the wall!", buffer1.raw());
            return result;

        case Event::NO_LONGER_CONFUSED:
            get_individual_description(observer, event._the_individual, &buffer1);
            result->bytes.format("%s is no longer confused.", buffer1.raw());
            return result;

        case Event::APPEAR:
            get_individual_description(observer, event._the_individual, &buffer1);
            result->bytes.format("%s appears out of nowhere!", buffer1.raw());
            return result;
        case Event::TURN_INVISIBLE:
            get_individual_description(observer, event._the_individual, &buffer1);
            result->bytes.format("%s turns invisible!", buffer1.raw());
            return result;
        case Event::DISAPPEAR:
            get_individual_description(observer, event._the_individual, &buffer1);
            result->bytes.format("%s vanishes out of sight!", buffer1.raw());
            return result;
        case Event::POLYMORPH:
            get_individual_description(observer, event._polymorph.individual, &buffer1);
            result->bytes.format("%s transforms into %s!", "TODO: pre-transform description", buffer1.raw());
            return result;
    }
    panic("remembered_event");
}

static bool can_see_location(Individual observer, Coord location) {
    if (!observer->is_alive)
        return false;
    if (!is_in_bounds(location))
        return false;
    return observer->knowledge.tile_is_visible[location].any();
}
bool can_see_individual(Individual observer, uint256 target_id, Coord target_location) {
    // you can always see yourself
    if (observer->id == target_id)
        return true;
    // you can't see anything else while dead
    if (!observer->is_alive)
        return false;
    Individual actual_target = actual_individuals.get(target_id);
    // nobody can see invisible people
    if (actual_target->status_effects.invisible)
        return false;
    // we can see someone if they're in our line of sight
    if (!observer->knowledge.tile_is_visible[target_location].any())
        return false;
    return true;
}
bool can_see_individual(Individual observer, uint256 target_id) {
    return can_see_individual(observer, target_id, actual_individuals.get(target_id)->location);
}
static Coord location_of(uint256 individual_id) {
    return actual_individuals.get(individual_id)->location;
}

static bool see_event(Individual observer, Event event, Event * output_event) {
    switch (event.type) {
        case Event::MOVE:
            if (!(can_see_individual(observer, event._move.individual, event._move.from) || can_see_individual(observer, event._move.individual, event._move.to)))
                return false;
            *output_event = event;
            return true;
        case Event::BUMP_INTO_WALL:
            if (!can_see_individual(observer, event._move.individual))
                return false;
            if (can_see_location(observer, event._move.to)) {
                *output_event = event;
            } else {
                *output_event = Event::bump_into_something(event._move.individual, event._move.from, event._move.to);
            }
            return true;
        case Event::BUMP_INTO_INDIVIDUAL:
            if (can_see_individual(observer, event._attack.attacker)) {
                if (can_see_individual(observer, event._attack.target)) {
                    *output_event = event;
                } else {
                    *output_event = Event::bump_into_something(event._attack.attacker, location_of(event._attack.attacker), location_of(event._attack.target));
                }
            } else {
                if (can_see_individual(observer, event._attack.target)) {
                    *output_event = Event::something_bump_into_individual(event._attack.target, location_of(event._attack.attacker), location_of(event._attack.target));
                } else {
                    return false;
                }
            }
            return true;
        case Event::BUMP_INTO_SOMETHING:
        case Event::SOMETHING_BUMP_INTO_INDIVIDUAL:
            panic("not a real event");

        case Event::ATTACK:
            if (can_see_individual(observer, event._attack.attacker)) {
                if (can_see_individual(observer, event._attack.target)) {
                    *output_event = event;
                } else {
                    *output_event = Event::attack_something(event._attack.attacker, location_of(event._attack.attacker), location_of(event._attack.target));
                }
            } else {
                if (can_see_individual(observer, event._attack.target)) {
                    *output_event = Event::something_attack_individual(event._attack.target, location_of(event._attack.attacker), location_of(event._attack.target));
                } else {
                    return false;
                }
            }
            return true;
        case Event::ATTACK_THIN_AIR:
        case Event::ATTACK_WALL:
            if (!can_see_individual(observer, event._move.individual))
                return false;
            *output_event = event;
            return true;
        case Event::ATTACK_SOMETHING:
        case Event::SOMETHING_ATTACK_INDIVIDUAL:
            panic("not a real event");

        case Event::DIE:
            if (!can_see_individual(observer, event._the_individual))
                return false;
            *output_event = event;
            return true;

        case Event::ZAP_WAND:
            if (!can_see_individual(observer, event._zap.wielder))
                return false;
            *output_event = event;
            return true;
        case Event::WAND_HIT_NO_EFFECT:
        case Event::WAND_OF_CONFUSION_HIT:
        case Event::WAND_OF_STRIKING_HIT: {
            if (!can_see_individual(observer, event._zap_hit_individual.target))
                return false;
            // TODO: if you don't see the wielder, you shouldn't know what the wand looks like
            *output_event = event;
            return true;
        }
        case Event::WAND_OF_DIGGING_HIT_WALL: {
            if (!can_see_location(observer, event._zap_hit_location.target_location))
                return false;
            // TODO: if you don't see the wielder, you shouldn't know what the wand looks like
            *output_event = event;
            return true;
        }

        case Event::NO_LONGER_CONFUSED:
        case Event::APPEAR:
            if (!can_see_individual(observer, event._the_individual))
                return false;
            *output_event = event;
            return true;
        case Event::TURN_INVISIBLE:
            if (!can_see_location(observer, location_of(event._the_individual)))
                return false;
            if (!can_see_individual(observer, event._the_individual)) {
                *output_event = Event::disappear(event._the_individual);
                return true;
            }
            *output_event = event;
            return true;
        case Event::DISAPPEAR:
            panic("not a real event");

        case Event::POLYMORPH:
            if (!can_see_location(observer, location_of(event._the_individual)))
                return false;
            *output_event = event;
            return true;
    }
    panic("see event");
}

static void perceive_individual(Individual observer, uint256 target_id) {
    observer->knowledge.perceived_individuals.put(target_id, to_perceived_individual(target_id));
}

void publish_event(Event event) {
    for (auto iterator = actual_individuals.value_iterator(); iterator.has_next();) {
        Individual observer = iterator.next();
        Event apparent_event;
        if (!see_event(observer, event, &apparent_event))
            continue;
        // make changes to our knowledge
        List<uint256> delete_ids;
        switch (apparent_event.type) {
            case Event::MOVE:
                perceive_individual(observer, event._move.individual);
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
                delete_ids.append(event._the_individual);
                break;

            case Event::ZAP_WAND:
                // no state change
                break;
            case Event::WAND_HIT_NO_EFFECT:
                // TODO: id the wand if you can see it
                break;
            case Event::WAND_OF_CONFUSION_HIT:
                perceive_individual(observer, event._zap_hit_individual.target);
                // TODO: id the wand if you can see it
                break;
            case Event::WAND_OF_STRIKING_HIT:
            case Event::WAND_OF_DIGGING_HIT_WALL:
                // TODO: id the wand if you can see it
                break;

            case Event::NO_LONGER_CONFUSED:
                perceive_individual(observer, event._the_individual);
                break;

            case Event::APPEAR:
                perceive_individual(observer, event._the_individual);
                break;
            case Event::TURN_INVISIBLE:
                perceive_individual(observer, event._the_individual);
                break;
            case Event::DISAPPEAR:
                delete_ids.append(event._the_individual);
                break;

            case Event::POLYMORPH:
                perceive_individual(observer, event._polymorph.individual);
                break;
        }
        if (observer->species()->has_mind) {
            // we need to log the event before the monster disappears from our knowledge
            RememberedEvent remembered_event = to_remembered_event(observer, apparent_event);
            if (remembered_event != NULL)
                observer->knowledge.remembered_events.append(remembered_event);
        }
        // now that we've had a chance to talk about it, delete it if we should
        for (int i = 0; i < delete_ids.length(); i++)
            observer->knowledge.perceived_individuals.remove(delete_ids[i]);
    }
}
