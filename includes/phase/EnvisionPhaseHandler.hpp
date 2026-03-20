#ifndef ENVISION_PHASE_HANDLER_HPP
#define ENVISION_PHASE_HANDLER_HPP

#include "game/PhaseHandler.hpp"
#include "game/GameState.hpp"

/*
 * EnvisionPhaseHandler
 * 
 * Valid actions during Envision phase:
 * 
 *   - "claim_first_player" : Trade resources to acquire first-player token
 *   - "trade"              : Trade resources between params
 *   - "commit"   : Commit/finalize changes for the round
 *   - "pass"
 * 
 * Add more actions here as the game design solidifies.
 */

class EnvisionPhaseHandler : public PhaseHandler {
public:
    ActionResult handle(const Action& action, GameState& state) override;
    GamePhase    getPhase() const override { return GamePhase::ENVISION; }

private:
    ActionResult handleTrade(const Action& action, GameState& state);
    ActionResult handleClaimFirstPlayer(const Action& action, GameState& state);
    ActionResult handleCommit(const Action& action, GameState& state);
    
};

#endif
