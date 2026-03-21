#include "phase/EnvisionPhaseHandler.hpp"

ActionResult EnvisionPhaseHandler::handle(const Action& action, GameState& state) {
    // "pass" is valid in every phase — RoundController handles the pass 
    // mechanics, but we still return SUCCESS so it knows to proceed.

    if (action.isPass()) {
        return ActionResult::success(ActionMessage("status", "Player passed"));
    }
    if (action.type == "commit") {
        return handleCommit(action, state);
    }
    return {ActionStatus::INVALID_ACTION};
}


ActionResult EnvisionPhaseHandler::handleClaimFirstPlayer(const Action& action, GameState& state) {
    // TODO: Define the exact cost to claim first player
    // For now: costs 1 cybernation level
    int currentLevel = state.params.getCybernationLevel();
    if (currentLevel < 1) {
        return {ActionStatus::INSUFFICIENT_RESOURCE};
    }

    // Pay the cost
    state.params.adjustParam(CyberParameter::CYBERNATION_LEVEL, -1);

    // Transfer first player token
    for (int i = 0; i < GameState::NUM_PLAYERS; ++i) {
        state.players[i].setFirstPlayer(false);
    }
    Player* claimant = state.getPlayer(action.playerId);
    if (claimant) {
        claimant->setFirstPlayer(true);
        state.firstPlayerId = action.playerId;
    }

    return ActionResult::success(ActionMessage("claimFirstPlayer","Player " + std::to_string(action.playerId) + " claimed first player"));
}

ActionResult EnvisionPhaseHandler::handleTrade(const Action& action, GameState& state) {
    // TODO: Implement trade logic
    // Expected params: "give_param", "give_amount", "receive_param", "receive_amount"
    return {ActionStatus::INVALID_ACTION};
}

ActionResult EnvisionPhaseHandler::handleCommit(const Action& action, GameState& state) {
    // TODO: Implement commit/scoring logic
    return {ActionStatus::INVALID_ACTION};
}
