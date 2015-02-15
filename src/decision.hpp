#ifndef DECISION_HPP
#define DECISION_HPP

#include "swarkland.hpp"

extern Action (*decision_makers[DecisionMakerType_COUNT])(Thing);
// this needs to be set by the input processor
extern Action current_player_decision;

void init_decisions();

#endif
