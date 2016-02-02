#ifndef DECISION_HPP
#define DECISION_HPP

#include "swarkland.hpp"

extern Action (*decision_makers[DecisionMakerType_COUNT])(Thing);
// this needs to be set by the input processor
extern Action current_player_decision;
extern int start_waiting_event_count;

void init_decisions();

#endif
