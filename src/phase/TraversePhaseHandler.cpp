#include "phase/TraversePhaseHandler.hpp"

ActionResult TraversePhaseHandler::handle(const Action& action, GameState& state) {
    if (action.isPass()) {
        return ActionResult::success("Player passed");
    }

    if (action.type == "place_token") {
        return handlePlaceToken(action, state);
    }

    if (action.type == "cancel_disruption") {
        return handleCancelDisruption(action, state);
    }

    return {ActionStatus::INVALID_ACTION, 
            "Action '" + action.type + "' is not valid during Traverse phase"};
}

ActionResult TraversePhaseHandler::handlePlaceToken(const Action& action, GameState& state) {
    // TODO: Implement token placement logic
    // Expected params: "stack_id", "token_type"
    return {ActionStatus::INVALID_ACTION, "Place token not yet implemented"};
}

ActionResult TraversePhaseHandler::handleCancelDisruption(const Action& action, GameState& state) {
    // TODO: Implement disruption cancellation
    return {ActionStatus::INVALID_ACTION, "Cancel disruption not yet implemented"};
}
