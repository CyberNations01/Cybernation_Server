#include "phase/TraversePhaseHandler.hpp"

ActionResult TraversePhaseHandler::handle(const Action& action, GameState& state) {

    if (action.type == "draw_disruption")
        return handleResolveDisruption(action, state);
    if (action.type == "cancel_disruption")
        return handleCancelDisruption(state);
    if (action.type == "walkPath")
        return handleWalkPath(state);

    return {ActionStatus::INVALID_ACTION};
}

ActionResult TraversePhaseHandler::handleWalkPath(GameState &state)
{
    return GameUtility::walkPath(state);
}

ActionResult TraversePhaseHandler::handleDrawDisruption(GameState &state)
{
    return GameUtility::drawDisruption(state);
}

ActionResult TraversePhaseHandler::handleResolveDisruption(const Action& action, GameState& state)
{
    return GameUtility::applyDisruptionEffect(state, action);
}

ActionResult TraversePhaseHandler::handleCancelDisruption(GameState &state)
{
    return GameUtility::cancelDisruptionEffect(state);
}
