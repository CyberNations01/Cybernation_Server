#include "game/RoundController.hpp"
#include "phase/EnvisionPhaseHandler.hpp"
#include "phase/TraversePhaseHandler.hpp"
#include "phase/AdoptPhaseHandler.hpp"

RoundController::RoundController() {
    turnOrder.fill(0);
}

ActionResult RoundController::processAction(const Action& action, GameState& state) {
    PhaseHandler* handler = getHandlerForPhase(currentPhase);
    if (!handler)
        return ActionResult::invalid("No handler for current phase");
    return handler->handle(action, state);
}

PhaseHandler* RoundController::getHandlerForPhase(GamePhase phase) {
    switch (phase) {
        case GamePhase::ENVISION: return &envisionHandler;
        case GamePhase::TRAVERSE: return &traverseHandler;
        case GamePhase::ADOPT:    return &adoptHandler;
    }
    return nullptr;
}

int  RoundController::getCurrentPlayerId() const { return currentPlayerId; }
GamePhase RoundController::getCurrentPhase() const { return currentPhase; }
int  RoundController::getCurrentRound() const { return currentRound; }
bool RoundController::isGameOver() const { return gameOver; }

void RoundController::buildTurnOrder(int) {}
void RoundController::advanceTurn(GameState&) {}
void RoundController::advancePhase(GameState&) {}
void RoundController::advanceRound(GameState&) {}
void RoundController::resetPhase() {}
bool RoundController::advanceToNextActivePlayer() { return false; }

nlohmann::json RoundController::toJson() const {
    return {
        {"round", currentRound},
        {"phase", gamePhaseToStr(currentPhase)},
        {"currentPlayerId", currentPlayerId},
        {"gameOver", gameOver}
    };
}