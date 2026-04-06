#include "phase/TraversePhaseHandler.hpp"

ActionResult TraversePhaseHandler::handle(const Action& action, GameState& state) {

    if (action.type == "draw_disruption")
        return handleDrawDisruption(state);
    if (action.type == "resolve_disruption")
        return handleResolveDisruption(action, state);
    if (action.type == "cancel_disruption")
        return handleCancelDisruption(state);
    if (action.type == "walkPath")
        return handleWalkPath(state);

    return {ActionStatus::INVALID_ACTION};
}

ActionResult TraversePhaseHandler::handleWalkPath(GameState &state)
{
    if (!state.traverseDisruptionHandled) {
        return {ActionStatus::INVALID_ACTION, {"walkPath", "Disruption must be resolved or cancelled before walking path"}};
    }
    ActionResult res = GameUtility::walkPath(state);
    if (res.ok()) {
        state.traverseWalkCompleted = true;
    }
    return res;
}

ActionResult TraversePhaseHandler::handleDrawDisruption(GameState &state)
{
    if (state.traverseDisruptionDrawn) {
        return {ActionStatus::INVALID_ACTION, {"draw_disruption", "Disruption is already drawn this Traverse phase"}};
    }
    if (state.traverseWalkCompleted) {
        return {ActionStatus::INVALID_ACTION, {"draw_disruption", "Cannot draw disruption after walkPath"}};
    }
    ActionResult res = GameUtility::drawDisruption(state);
    if (res.ok()) {
        state.traverseDisruptionDrawn = true;
        state.traverseDisruptionHandled = false;
    }
    return res;
}

ActionResult TraversePhaseHandler::handleResolveDisruption(const Action& action, GameState& state)
{
    if (!state.traverseDisruptionDrawn) {
        return {ActionStatus::INVALID_ACTION, {"resolve_disruption", "Draw disruption before resolving"}};
    }
    if (state.traverseDisruptionHandled) {
        return {ActionStatus::INVALID_ACTION, {"resolve_disruption", "Disruption is already handled"}};
    }
    ActionResult res = GameUtility::applyDisruptionEffect(state, action);
    if (res.ok()) {
        state.activeDisruption = std::nullopt;
        state.traverseDisruptionHandled = true;
    }
    return res;
}

ActionResult TraversePhaseHandler::handleCancelDisruption(GameState &state)
{
    if (!state.traverseDisruptionDrawn) {
        return {ActionStatus::INVALID_ACTION, {"cancel_disruption", "Draw disruption before cancelling"}};
    }
    if (state.traverseDisruptionHandled) {
        return {ActionStatus::INVALID_ACTION, {"cancel_disruption", "Disruption is already handled"}};
    }
    ActionResult res = GameUtility::cancelDisruptionEffect(state);
    if (res.ok()) {
        state.traverseDisruptionHandled = true;
    }
    return res;
}
