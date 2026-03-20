#include "game/RoundController.hpp"
#include "phase/EnvisionPhaseHandler.hpp"
#include "phase/TraversePhaseHandler.hpp"
#include "phase/AdoptPhaseHandler.hpp"

RoundController::RoundController() {
    // Wire up the three phase handlers
    handlers[static_cast<int>(GamePhase::ENVISION)]  = std::make_unique<EnvisionPhaseHandler>();
    handlers[static_cast<int>(GamePhase::TRAVERSE)]  = std::make_unique<TraversePhaseHandler>();
    handlers[static_cast<int>(GamePhase::ADOPT)]     = std::make_unique<AdoptPhaseHandler>();

    // Initialize turn order (will be rebuilt on first processAction)
    turnOrder.fill(-1);
    initialized = false;
}

// ─────────────────────────────────────────────
//  Main entry point
// ─────────────────────────────────────────────

ActionResult RoundController::processAction(const Action& action, GameState& state) {
    // 0. Game over check
    if (state.gameOver) {
        return {ActionStatus::GAME_OVER};
    }

    // 1. Initialize turn order on first call
    if (!initialized) {
        buildTurnOrder(state.firstPlayerId);
        state.currentPlayerId = getCurrentPlayerId();
        initialized = true;
    }

    // 2. Is it this player's turn? If not, silently ignore.
    if (action.playerId != getCurrentPlayerId()) {
        return ActionResult::ignored();
    }

    // 3. Has player already passed this phase?
    if (hasPlayerPassed(action.playerId)) {
        return ActionResult::alreadyPassed();
    }

    // 4. Handle "pass" 
    if (action.isPass()) {
        passedPlayers.insert(action.playerId);

        // Check if phase is now complete
        if (isPhaseComplete()) {
            advancePhase(state);
        } else {
            advanceTurn(state);
        }
        return ActionResult::success("Player " + std::to_string(action.playerId) + " passed");
    }

    // 5. Delegate to the appropriate PhaseHandler
    PhaseHandler* handler = getHandlerForPhase(state.currentPhase);
    if (!handler) {
        return {ActionStatus::UNKNOWN_ERROR};
    }

    ActionResult result = handler->handle(action, state);

    // 6. If the handler succeeded, advance turn to next player
    //    (the current player can act again on their NEXT turn in the cycle,
    //     unless they pass)
    if (result.ok()) {
        advanceTurn(state);
    }

    return result;
}

// ─────────────────────────────────────────────
//  Turn order management
// ─────────────────────────────────────────────

void RoundController::buildTurnOrder(int firstPlayerId) {
    // Clockwise: if firstPlayer is 3, order is [3, 4, 0, 1, 2]
    for (int i = 0; i < GameState::NUM_PLAYERS; ++i) {
        turnOrder[i] = (firstPlayerId + i) % GameState::NUM_PLAYERS;
    }
    turnIndex = 0;
}

void RoundController::advanceTurn(GameState& state) {
    // Move to next non-passed player in clockwise order
    if (!advanceToNextActivePlayer()) {
        // Everyone has passed — advance phase
        advancePhase(state);
        return;
    }

    // Sync to GameState so frontend can read it
    state.currentPlayerId = getCurrentPlayerId();
}

bool RoundController::advanceToNextActivePlayer() {
    // Try each subsequent player in turn order
    for (int attempts = 0; attempts < GameState::NUM_PLAYERS; ++attempts) {
        turnIndex = (turnIndex + 1) % GameState::NUM_PLAYERS;
        int candidateId = turnOrder[turnIndex];
        if (passedPlayers.find(candidateId) == passedPlayers.end()) {
            return true;  // Found an active player
        }
    }
    return false;  // Everyone has passed
}

// ─────────────────────────────────────────────
//  Phase & Round advancement
// ─────────────────────────────────────────────

void RoundController::advancePhase(GameState& state) {
    GamePhase next = nextPhase(state.currentPhase);

    if (next == GamePhase::ENVISION && state.currentPhase == GamePhase::ADOPT) {
        // Completed all 3 phases → advance round
        advanceRound(state);
    } else {
        // Move to next phase within this round
        state.currentPhase = next;
        resetPhase(state);
    }
}

void RoundController::advanceRound(GameState& state) {
    state.currentRound++;

    if (state.currentRound > state.maxRounds) {
        state.gameOver = true;
        return;
    }

    // New round starts at Envision
    state.currentPhase = GamePhase::ENVISION;

    // First player might have changed during the round (via claim_first_player)
    // Read it from state — it was set by the PhaseHandler
    state.firstPlayerId = state.findFirstPlayer();

    resetPhase(state);
}

void RoundController::resetPhase(GameState& state) {
    passedPlayers.clear();
    buildTurnOrder(state.firstPlayerId);
    state.currentPlayerId = getCurrentPlayerId();
}

// ─────────────────────────────────────────────
//  Queries
// ─────────────────────────────────────────────

bool RoundController::hasPlayerPassed(int playerId) const {
    return passedPlayers.find(playerId) != passedPlayers.end();
}

bool RoundController::isPhaseComplete() const {
    return static_cast<int>(passedPlayers.size()) >= GameState::NUM_PLAYERS;
}

PhaseHandler* RoundController::getHandlerForPhase(GamePhase phase) {
    int idx = static_cast<int>(phase);
    if (idx < 0 || idx >= NUM_PHASES) return nullptr;
    return handlers[idx].get();
}

nlohmann::json RoundController::toJson() const {
    nlohmann::json j;
    
    // Turn order
    j["turnOrder"] = nlohmann::json::array();
    for (int i = 0; i < GameState::NUM_PLAYERS; ++i) {
        j["turnOrder"].push_back(turnOrder[i]);
    }
    j["currentTurnIndex"] = turnIndex;
    j["currentPlayerId"]  = getCurrentPlayerId();

    // Passed players
    j["passedPlayers"] = nlohmann::json::array();
    for (int id : passedPlayers) {
        j["passedPlayers"].push_back(id);
    }

    return j;
}
