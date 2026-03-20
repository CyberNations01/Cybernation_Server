#ifndef ADOPT_PHASE_HANDLER_HPP
#define ADOPT_PHASE_HANDLER_HPP

#include "game/PhaseHandler.hpp"

/*
 * AdoptPhaseHandler
 * 
 * Valid actions during Adopt phase:
 *   - "claim_first_player" : Trade resources to acquire first-player token
 *   - "trade"              : Trade resources between params
 *   - "commit"   : Commit/finalize changes for the round
 *   - "pass"
 * 
 * Add more actions as the game design solidifies.
 */

class AdoptPhaseHandler : public PhaseHandler {
public:
    ActionResult handle(const Action& action, GameState& state) override;
    GamePhase    getPhase() const override { return GamePhase::ADOPT; }

private:
    ActionResult handleTrade(const Action& action, GameState& state);
    ActionResult handleClaimFirstPlayer(const Action& action, GameState& state);
    ActionResult handleCommit(const Action& action, GameState& state);
};

#endif
