#include "phase/AdoptPhaseHandler.hpp"

ActionResult AdoptPhaseHandler::handle(const Action& action, GameState& state) {
    
    if (action.type == "place_token") {
        return handlePlaceToken(action, state);
    }
    return ActionResult(ActionStatus::INVALID_ACTION);
}

ActionResult AdoptPhaseHandler::handlePlaceToken(const Action &action, GameState &state)
{
    return ActionResult(ActionStatus::INVALID_ACTION);
}