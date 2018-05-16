#ifndef EVENT_HPP
#define EVENT_HPP

#include "thing.hpp"

bool can_see_thing(Thing observer, uint256 target_id, Coord target_location);
bool can_see_thing(Thing observer, uint256 target_id);
PerceivedThing record_perception_of_thing(Thing observer, uint256 target_id);
void publish_event(Event event);

#endif
