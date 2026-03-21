#ifndef ADOPT_PHASE_HANDLER_HPP
#define ADOPT_PHASE_HANDLER_HPP

#include "game/PhaseHandler.hpp"

/*
 * AdoptPhaseHandler
 * 
 * Valid actions during Adopt phase:
 * 
 * 
 * Add more actions as the game design solidifies.
 */

class AdoptPhaseHandler : public PhaseHandler {
public:
    ActionResult handle(const Action& action, GameState& state) override;
    GamePhase    getPhase() const override { return GamePhase::ADOPT; }

private:
    ActionResult handlePlaceToken(const Action& action, GameState& state);

};

#endif
