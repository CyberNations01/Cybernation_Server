#ifndef TRAVERSE_PHASE_HANDLER_HPP
#define TRAVERSE_PHASE_HANDLER_HPP

#include "game/PhaseHandler.hpp"

/*
 * TraversePhaseHandler
 * 
 * Valid actions during Traverse phase:
 *   - "place_token"        : Place a feedback token on a stack
 *   - "cancel_disruption"  : Pay cost to cancel a disruption card effect
 *   - "pass"
 */

class TraversePhaseHandler : public PhaseHandler {
public:
    ActionResult handle(const Action& action, GameState& state) override;
    GamePhase    getPhase() const override { return GamePhase::TRAVERSE; }

private:
    ActionResult handlePlaceToken(const Action& action, GameState& state);
    ActionResult handleCancelDisruption(const Action& action, GameState& state);
};

#endif
