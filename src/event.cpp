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
        case Event::ATTACK:
            get_individual_description(observer, event.individual1->id, &buffer1);
            get_individual_description(observer, event.individual2->id, &buffer2);
            result->bytes.format("%s hits %s!", buffer1.raw(), buffer2.raw());
            return result;
        case Event::DIE:
            get_individual_description(observer, event.individual1->id, &buffer1);
            result->bytes.format("%s dies.", buffer1.raw());
            return result;
        case Event::ZAP_WAND:
            get_individual_description(observer, event.individual1->id, &buffer1);
            get_item_description(observer, event.individual1, event.item1, &buffer2);
            result->bytes.format("%s zaps %s.", buffer1.raw(), buffer2.raw());
            return result;
        case Event::WAND_HIT_NO_EFFECT:
            get_item_description(observer, event.individual1, event.item1, &buffer1);
            get_individual_description(observer, event.individual2->id, &buffer2);
            result->bytes.format("%s hits %s, but nothing happens.", buffer1.raw(), buffer2.raw());
            return result;
        case Event::WAND_OF_CONFUSION_HIT:
            get_item_description(observer, event.individual1, event.item1, &buffer1);
            get_individual_description(observer, event.individual2->id, &buffer2);
            result->bytes.format("%s hits %s; %s is confused!", buffer1.raw(), buffer2.raw(), buffer2.raw());
            return result;
        case Event::WAND_OF_STRIKING_HIT:
            get_item_description(observer, event.individual1, event.item1, &buffer1);
            get_individual_description(observer, event.individual2->id, &buffer2);
            result->bytes.format("%s strikes %s!", buffer1.raw(), buffer2.raw());
            return result;
        case Event::WAND_OF_DIGGING_HIT_WALL:
            get_item_description(observer, event.individual1, event.item1, &buffer1);
            result->bytes.format("%s digs through the wall!", buffer1.raw());
            return result;
        case Event::NO_LONGER_CONFUSED:
            get_individual_description(observer, event.individual1->id, &buffer1);
            result->bytes.format("%s is no longer confused.", buffer1.raw());
            return result;
        case Event::BUMP_INTO_WALL:
            get_individual_description(observer, event.individual1->id, &buffer1);
            result->bytes.format("%s bumps into a wall.", buffer1.raw());
            return result;
        case Event::BUMP_INTO_INDIVIDUAL:
            get_individual_description(observer, event.individual1->id, &buffer1);
            get_individual_description(observer, event.individual2->id, &buffer2);
            result->bytes.format("%s bumps into %s.", buffer1.raw(), buffer2.raw());
            return result;
        case Event::ATTACK_THIN_AIR:
            get_individual_description(observer, event.individual1->id, &buffer1);
            result->bytes.format("%s attacks thin air.", buffer1.raw());
            return result;
        case Event::APPEAR:
            get_individual_description(observer, event.individual1->id, &buffer1);
            result->bytes.format("%s appears out of nowhere!", buffer1.raw());
            return result;
        case Event::DISAPPEAR:
            get_individual_description(observer, event.individual1->id, &buffer1);
            result->bytes.format("%s vanishes out of sight!", buffer1.raw());
            return result;
        case Event::POLYMORPH:
            get_individual_description(observer, event.individual1->id, &buffer1);
            result->bytes.format("%s transforms into %s!", "TODO: pre-transform description", buffer1.raw());
            return result;
    }
    panic("remembered_event");
}

void publish_event(Event event) {
    for (auto iterator = actual_individuals.value_iterator(); iterator.has_next();) {
        Individual observer = iterator.next();
        if (!observer->is_alive)
            continue;
        bool can_see1 = observer->knowledge.tile_is_visible[event.coord1].any();
        bool can_see2 = event.coord2 != Coord::nowhere() && observer->knowledge.tile_is_visible[event.coord2].any();
        if (!(can_see1 || can_see2))
            continue; // out of view
        if (event.individual1 != NULL) {
            // we typically can't see invisible individuals doing things
            if (observer != event.individual1 && event.individual1->status_effects.invisible) {
                // we can only see this event if it's the event made the individual invisible
                if (event.type != Event::DISAPPEAR)
                    continue;
            }
        }
        // i see what happened
        List<uint256> delete_ids;
        if (event.individual1 != NULL) {
            if (event.type == Event::DIE) {
                delete_ids.append(event.individual1->id);
            } else if (event.type == Event::DISAPPEAR) {
                // you can always see yourself
                if (observer == event.individual1) {
                    // notice yourself vanish
                    observer->knowledge.perceived_individuals.put(event.individual1->id, to_perceived_individual(event.individual1));
                } else {
                    delete_ids.append(event.individual1->id);
                }
            } else {
                observer->knowledge.perceived_individuals.put(event.individual1->id, to_perceived_individual(event.individual1));
            }
        }
        if (event.individual2 != NULL)
            observer->knowledge.perceived_individuals.put(event.individual2->id, to_perceived_individual(event.individual2));
        if (observer->species()->has_mind) {
            // we need to log the event before the monster disappears from our knowledge
            RememberedEvent remembered_event = to_remembered_event(observer, event);
            if (remembered_event != NULL)
                observer->knowledge.remembered_events.append(remembered_event);
        }
        // now that we've had a chance to talk about it, delete it if we should
        for (int i = 0; i < delete_ids.length(); i++)
            observer->knowledge.perceived_individuals.remove(delete_ids[i]);
    }

    if (event.type == Event::DIE) {
        // we didn't notice ourselves dying in the above loop
        event.individual1->knowledge.perceived_individuals.remove(event.individual1->id);
    }
}
