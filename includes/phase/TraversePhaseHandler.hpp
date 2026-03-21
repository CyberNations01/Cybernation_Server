#ifndef TRAVERSE_PHASE_HANDLER_HPP
#define TRAVERSE_PHASE_HANDLER_HPP

#include "game/PhaseHandler.hpp"

/*
 * TraversePhaseHandler
 * 
 * Valid actions during Traverse phase:
 * 
 * - `walkPath`: Walk People token and claim resource
 * - `Solve Disruption`: Draw Disruption and handle the effect
 */

class TraversePhaseHandler : public PhaseHandler {
public:
    ActionResult handle(const Action& action, GameState& state) override;
    GamePhase    getPhase() const override { return GamePhase::TRAVERSE; }

private:
    nlohmann::json 
    restoJson(const int& tile, const int& side, 
                const std::vector<std::string> & resources, 
                const std::string& layer);

    ActionResult handleCancelDisruption(const Action& action, GameState& state);
    ActionResult handleWalkPath(GameState &state);
    ActionResult handleDisruptionEffect();

};

#endif
