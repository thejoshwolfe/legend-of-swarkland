#ifndef DECISION_HPP
#define DECISION_HPP

#include "swarkland.hpp"

Action get_ai_decision(Thing actor);
Action get_player_decision(Thing actor);

static constexpr Action (*decision_makers[DecisionMakerType_COUNT])(Thing) = {
    get_ai_decision,
    get_player_decision,
};
static_assert(decision_makers[DecisionMakerType_AI] == get_ai_decision, "missed a spot");
static_assert(decision_makers[DecisionMakerType_PLAYER] == get_player_decision, "missed a spot");

void start_auto_wait();
void assess_auto_wait_situation(List<uint256> * output_scary_individuals, List<StatusEffect::Id> * output_annoying_status_effects, bool * output_stop_for_other_reasons);

#endif
