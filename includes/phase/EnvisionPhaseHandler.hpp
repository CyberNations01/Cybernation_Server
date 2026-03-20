#ifndef ENVISION_PHASE_HANDLER_HPP
#define ENVISION_PHASE_HANDLER_HPP

#include "game/PhaseHandler.hpp"
#include "game/GameState.hpp"

/*
 * EnvisionPhaseHandler
 * 
 * Valid actions during Envision phase:
 * 
 * - Walk People token and claim resource
 * - Draw Disruption and handle the effect
 * 
 * Add more actions here as the game design solidifies.
 */

class EnvisionPhaseHandler : public PhaseHandler {
public:
    ActionResult handle(const Action& action, GameState& state) override;
    GamePhase    getPhase() const override { return GamePhase::ENVISION; }

private:
    ActionResult handleWalkPath(GameState &state);
    ActionResult handleDisruptionEffect();

};

#endif
