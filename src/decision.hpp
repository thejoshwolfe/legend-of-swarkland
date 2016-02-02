#ifndef DECISION_HPP
#define DECISION_HPP

#include "swarkland.hpp"

extern Action (*decision_makers[DecisionMakerType_COUNT])(Thing);
// this needs to be set by the input processor
extern Action current_player_decision;
void start_auto_wait();
void assess_auto_wait_situation(List<uint256> * output_scary_individuals, List<StatusEffect::Id> * output_annoying_status_effects, bool * output_stop_for_other_reasons);

void init_decisions();

#endif
